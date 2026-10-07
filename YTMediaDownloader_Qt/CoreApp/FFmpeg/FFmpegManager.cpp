/**
 * @file FFmpegManager.cpp
 * @brief Implementazione del gestore asincrono FFmpegManager per l'estrazione e download con yt-dlp ed FFmpeg.
 * 
 * Architettura di download a 2 fasi:
 * - Fase 1 (Downloading): Download video+audio con player_client=all (necessario per accedere ai format ID specifici delle tracce localizzate).
 * - Fase 2 (DownloadingSubtitles): Download sottotitoli SENZA player_client=all (per evitare HTTP 429 Too Many Requests).
 * - Fase 3 (Finalizzazione): FFmpeg muxing dei sottotitoli nel contenitore video e transcodifica audio Opus→AAC per massima compatibilità.
 */

#include "FFmpegManager.h"
#include "CoreApp/IO/FileManager.h"
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QLocale>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

/**
 * @brief Costruttore della classe FFmpegManager.
 * @param parent Oggetto padre facoltativo.
 */
FFmpegManager::FFmpegManager(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_subProcess(new QProcess(this))
    , m_currentPid(0)
    , m_subPid(0)
    , m_isCancelled(false)
    , m_mode(Idle)
    , m_totalStages(3)
    , m_currentStage(1)
    , m_downloadDestinationCount(0)
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &FFmpegManager::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &FFmpegManager::onProcessReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &FFmpegManager::onProcessFinished);
}

/**
 * @brief Distruttore della classe FFmpegManager.
 */
FFmpegManager::~FFmpegManager()
{
    cancelCurrentTask();
}

/**
 * @brief Termina ricorsivamente un processo e tutti i relativi processi figli/discendenti.
 * @param pid ID del processo principale da terminare.
 */
void FFmpegManager::killProcessTree(qint64 pid)
{
    if (pid <= 0) return;

    // Invia SIGKILL al gruppo di processi (se è process group leader)
    ::kill(-static_cast<pid_t>(pid), SIGKILL);

    // Scansione di /proc per identificare e terminare ricorsivamente tutti i processi figli
    QDir procDir("/proc");
    QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        bool ok = false;
        qint64 cpid = entry.toLongLong(&ok);
        if (ok && cpid > 0 && cpid != pid) {
            QFile statFile(QString("/proc/%1/stat").arg(cpid));
            if (statFile.open(QIODevice::ReadOnly)) {
                QByteArray content = statFile.readAll();
                statFile.close();
                int lastParen = content.lastIndexOf(')');
                if (lastParen != -1) {
                    QByteArray rest = content.mid(lastParen + 2);
                    QList<QByteArray> parts = rest.split(' ');
                    if (parts.size() >= 2) {
                        qint64 ppid = parts.at(1).toLongLong();
                        if (ppid == pid) {
                            killProcessTree(cpid);
                        }
                    }
                }
            }
        }
    }

    // Esegue pkill -9 -P come ulteriore protezione contro processi orfani
    QProcess::execute("pkill", QStringList() << "-9" << "-P" << QString::number(pid));

    // Termina il processo stesso con SIGKILL
    ::kill(static_cast<pid_t>(pid), SIGKILL);
}

/**
 * @brief Interrompe immediatamente tutti i processi attivi yt-dlp, FFmpeg e ffprobe.
 * Metodo thread-safe richiamabile direttamente anche dal thread GUI.
 */
void FFmpegManager::killActiveProcesses()
{
    m_isCancelled.store(true);
    qint64 pid = m_currentPid.load();
    if (pid > 0) {
        killProcessTree(pid);
    }
    qint64 subPid = m_subPid.load();
    if (subPid > 0) {
        killProcessTree(subPid);
    }
}

/**
 * @brief Verifica che gli strumenti esterni yt-dlp ed ffmpeg siano installati ed eseguibili nel sistema.
 * @param errorDetails Dettagli dell'errore se uno strumento manca.
 * @return True se entrambi sono disponibili, altrimenti False.
 */
bool FFmpegManager::checkToolsAvailable(QString& errorDetails)
{
    QProcess testProc;
    testProc.start("yt-dlp", QStringList() << "--version");
    bool ytDlpOk = testProc.waitForFinished(3000) && (testProc.exitCode() == 0);

    QProcess ffmpegProc;
    ffmpegProc.start("ffmpeg", QStringList() << "-version");
    bool ffmpegOk = ffmpegProc.waitForFinished(3000) && (ffmpegProc.exitCode() == 0);

    if (!ytDlpOk) {
        errorDetails += "yt-dlp non trovato o non eseguibile nel sistema. ";
    }
    if (!ffmpegOk) {
        errorDetails += "ffmpeg non trovato o non eseguibile nel sistema. ";
    }

    return ytDlpOk && ffmpegOk;
}

/**
 * @brief Restituisce il nome completo per esteso della lingua (es. "Italiano", "Occitano", "Oriya", "Punjabi") convertendo le sigle brevi.
 * @param code Codice lingua estrapolato (es. "it", "oc", "or", "pa", "en-US").
 * @return Nome della lingua per esteso.
 */
QString FFmpegManager::getLanguageDisplayName(const QString& code)
{
    QString c = code.split('-').first().split('_').first().toLower();

    // Dizionario esteso delle sigle lingua ISO 639 tradotte per esteso in lingua italiana
    static const QMap<QString, QString> langMap = {
        {"it", "Italiano"}, {"en", "Inglese"}, {"es", "Spagnolo"}, {"fr", "Francese"},
        {"de", "Tedesco"}, {"ja", "Giapponese"}, {"zh", "Cinese"}, {"pt", "Portoghese"},
        {"ru", "Russo"}, {"ar", "Arabo"}, {"hi", "Hindi"}, {"ko", "Coreano"},
        {"nl", "Olandese"}, {"pl", "Polacco"}, {"tr", "Turco"}, {"uk", "Ucraino"},
        {"vi", "Vietnamita"}, {"cs", "Ceco"}, {"da", "Danese"}, {"el", "Greco"},
        {"fi", "Finlandese"}, {"hu", "Ungherese"}, {"id", "Indonesiano"}, {"no", "Norvegese"},
        {"ro", "Rumeno"}, {"sv", "Svedese"}, {"th", "Thailandese"}, {"he", "Ebraico"},
        {"bg", "Bulgaro"}, {"hr", "Croato"}, {"sk", "Slovacco"}, {"sl", "Sloveno"},
        {"sr", "Serbo"}, {"sq", "Albanese"}, {"ms", "Malese"}, {"bn", "Bengalese"},
        {"fa", "Persiano"}, {"ur", "Urdu"}, {"ta", "Tamil"}, {"te", "Telugu"},
        {"kn", "Kannada"}, {"ml", "Malayalam"}, {"mr", "Marathi"}, {"gu", "Gujarati"},
        {"pa", "Punjabi"}, {"or", "Oriya"}, {"oc", "Occitano"}, {"am", "Amharico"},
        {"aa", "Afar"}, {"ak", "Akan"}, {"as", "Assamese"}, {"ay", "Aymara"},
        {"az", "Azero"}, {"be", "Bielorusso"}, {"bi", "Bislama"}, {"bs", "Bosniaco"},
        {"ca", "Catalano"}, {"co", "Corso"}, {"cy", "Gallese"}, {"eo", "Esperanto"},
        {"et", "Estone"}, {"eu", "Basco"}, {"ga", "Irlandese"}, {"gl", "Galiziano"},
        {"is", "Islandese"}, {"ka", "Georgiano"}, {"kk", "Kazako"}, {"km", "Khmer"},
        {"ky", "Chirghiso"}, {"la", "Latino"}, {"lb", "Lussemburghese"}, {"lo", "Lao"},
        {"mg", "Malgascio"}, {"mi", "Maori"}, {"mk", "Macedone"}, {"mn", "Mongolo"},
        {"mt", "Maltese"}, {"my", "Birmano"}, {"ne", "Nepalese"}, {"ps", "Pashto"},
        {"qu", "Quechua"}, {"sd", "Sindhi"}, {"si", "Cingalese"}, {"so", "Somalo"},
        {"tg", "Tagico"}, {"tk", "Turkmeno"}, {"tl", "Tagalog"}, {"tt", "Tataro"},
        {"ug", "Uiguro"}, {"uz", "Uzbeco"}, {"yi", "Yiddish"}, {"zu", "Zulu"},
        {"orig", "Originale"}, {"auto", "Automatico"}, {"iw", "Ebraico"}
    };

    if (langMap.contains(c)) {
        return langMap[c];
    }

    QLocale loc(c);
    if (loc.language() != QLocale::C) {
        QString qName = QLocale::languageToString(loc.language());
        if (!qName.isEmpty() && qName != "C") {
            return qName;
        }
    }

    return code;
}

/**
 * @brief Converte i codici lingua in codici ISO 639 a 3 lettere riconosciuti dallo standard FFmpeg (es. "ita", "eng", "spa").
 * @param code Codice lingua in formato breve (es. "it", "en-US").
 * @return Codice ISO 639 a 3 lettere.
 */
QString FFmpegManager::toIso639Code(const QString& code)
{
    QString c = code.split('-').first().split('_').first().toLower().trimmed();
    static const QMap<QString, QString> isoMap = {
        {"it", "ita"}, {"en", "eng"}, {"es", "spa"}, {"fr", "fra"}, {"de", "deu"},
        {"ja", "jpn"}, {"zh", "zho"}, {"pt", "por"}, {"ru", "rus"}, {"ar", "ara"},
        {"hi", "hin"}, {"ko", "kor"}, {"nl", "nld"}, {"pl", "pol"}, {"tr", "tur"},
        {"uk", "ukr"}, {"vi", "vie"}, {"cs", "ces"}, {"da", "dan"}, {"el", "ell"},
        {"fi", "fin"}, {"hu", "hun"}, {"id", "ind"}, {"no", "nor"}, {"ro", "ron"},
        {"sv", "swe"}, {"th", "tha"}, {"he", "heb"}, {"iw", "heb"}, {"bg", "bul"},
        {"hr", "hrv"}, {"sk", "slk"}, {"sl", "slv"}, {"sr", "srp"}, {"sq", "sqi"},
        {"ms", "msa"}, {"bn", "ben"}, {"fa", "fas"}, {"ur", "urd"}, {"ta", "tam"},
        {"te", "tel"}, {"kn", "kan"}, {"ml", "mal"}, {"mr", "mar"}, {"gu", "guj"},
        {"pa", "pan"}, {"or", "ori"}, {"oc", "oci"}, {"am", "amh"}, {"ca", "cat"},
        {"eu", "eus"}, {"gl", "glg"}, {"is", "isl"}, {"ka", "kat"}, {"la", "lat"}
    };

    if (isoMap.contains(c)) {
        return isoMap[c];
    }
    if (c.length() >= 3) {
        return c.left(3);
    }
    return "und";
}

/**
 * @brief Avvia l'estrazione JSON asincrona dal link YouTube specificato.
 * @param url URL del video da analizzare.
 */
void FFmpegManager::analyzeUrl(const QString& url)
{
    cancelCurrentTask();
    m_isCancelled.store(false);
    m_mode = Analyzing;
    m_buffer.clear();
    m_lastErrorOutput.clear();
    m_currentMetadata = MediaMetadata();
    m_currentMetadata.url = url;

    QStringList args;
    args << "-J" << "--no-playlist" << url;

    emit progressUpdated(-1.0, "Fase 1 di 1: Analisi video in corso...", "Indeterminato");
    m_process->start("yt-dlp", args);
    if (m_process->waitForStarted(1000)) {
        m_currentPid.store(m_process->processId());
    }
}

/**
 * @brief Avvia il download video+audio+sottotitoli usando player_client=web_embedded.
 * @param options Struttura contenente i formati video/audio, informazioni di lingua, contenitore e sottotitoli scelti.
 *
 * Usa player_client=web_embedded invece di player_client=all:
 * - web_embedded fornisce tutti i format ID necessari (inclusi quelli localizzati come 140-16)
 *   e tutte le auto-captions, con solo 2 richieste API (config + player JSON)
 * - player_client=all genera 15+ richieste API a YouTube, causando HTTP 429 Too Many Requests
 *   che impedisce il download dei sottotitoli
 */
void FFmpegManager::startDownload(const DownloadOptions& options)
{
    cancelCurrentTask();
    m_isCancelled.store(false);
    m_mode = Downloading;
    m_buffer.clear();
    m_lastErrorOutput.clear();
    m_currentDownloadOptions = options;

    m_downloadDestinationCount = 0;
    m_currentStage = 1;
    m_totalStages = 3;
    if (!options.selectedSubtitleLangs.isEmpty()) {
        m_totalStages = 4;
    }

    // Genera la cartella temporanea .tmp_titolo
    QString safeTitle = FileManager::sanitizeFilename(m_currentMetadata.title);
    if (safeTitle.isEmpty()) safeTitle = "yt_video_download";
    m_tempWorkingDir = options.saveDirectory + "/.tmp_" + safeTitle;
    FileManager::ensureDirectoryExists(m_tempWorkingDir);

    // Parametri per yt-dlp: perfettamente allineati ed omogenei con la fase di analisi
    QStringList args;
    args << "--newline";
    args << "--no-playlist";
    args << "--ignore-errors";

    // Formattazione di download con supporto multi-audio e fallback resiliente
    QString videoFmt = options.selectedVideoFormatId;
    if (videoFmt.isEmpty()) videoFmt = "bestvideo";

    QString formatStr;
    if (!options.selectedAudioFormatIds.isEmpty()) {
        QString audioConcat = options.selectedAudioFormatIds.join("+");
        formatStr = QString("%1+%2/bestvideo+%2/%1+bestaudio/bestvideo+bestaudio/best").arg(videoFmt, audioConcat);
        args << "--audio-multistreams";
    } else {
        formatStr = QString("%1+bestaudio/bestvideo+bestaudio/best").arg(videoFmt);
    }
    args << "-f" << formatStr;

    // Impostazione del contenitore di destinazione (MP4, MKV, WebM)
    QString container = options.containerFormat.toLower();
    if (!container.isEmpty()) {
        args << "--merge-output-format" << container;
    }

    // Gestione ed embedding dei sottotitoli (yt-dlp li scarica e li incorpora nativamente come mov_text/subrip)
    if (!options.selectedSubtitleLangs.isEmpty()) {
        args << "--write-subs" << "--write-auto-subs";
        args << "--sub-langs" << options.selectedSubtitleLangs.join(",");
        args << "--embed-subs";
    }

    // Destinazione temporanea dei file scaricati ed elaborati
    args << "-o" << m_tempWorkingDir + "/%(title)s.%(ext)s";
    args << options.url;

    QString startStep = QString("Fase 1 di %1: Download flusso video e audio...").arg(m_totalStages);
    emit progressUpdated(-1.0, startStep, "Indeterminato");
    m_process->start("yt-dlp", args);
    if (m_process->waitForStarted(1000)) {
        m_currentPid.store(m_process->processId());
    }
}

/**
 * @brief Annulla l'operazione corrente, killa tutti i processi attivi e discendenti,
 * e rimuove tutti i file temporanei ed eventuali cache.
 */
void FFmpegManager::cancelCurrentTask()
{
    m_isCancelled.store(true);

    // Killa immediatamente tutti i processi attivi (yt-dlp, FFmpeg, ffprobe) e tutti i figli
    killActiveProcesses();

    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
    m_currentPid.store(0);

    if (m_subProcess && m_subProcess->state() != QProcess::NotRunning) {
        m_subProcess->kill();
        m_subProcess->waitForFinished(1000);
    }
    m_subPid.store(0);

    // Rimuove la cartella temporanea e tutti i file parzialmente scaricati o elaborati
    if (!m_tempWorkingDir.isEmpty() && QDir(m_tempWorkingDir).exists()) {
        QDir(m_tempWorkingDir).removeRecursively();
        m_tempWorkingDir.clear();
    }

    // Elimina eventuali file residui (.part, .ytdl, .tmp_*) rimasti nella cartella di salvataggio
    if (!m_currentDownloadOptions.saveDirectory.isEmpty()) {
        QDir saveDir(m_currentDownloadOptions.saveDirectory);
        if (saveDir.exists()) {
            QString safeTitle = FileManager::sanitizeFilename(m_currentMetadata.title);
            QStringList filters;
            filters << "*.part" << "*.ytdl" << ".tmp_*";
            QStringList leftover = saveDir.entryList(filters, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& item : leftover) {
                QString fullPath = saveDir.absoluteFilePath(item);
                if (safeTitle.isEmpty() || item.contains(safeTitle, Qt::CaseInsensitive) || item.startsWith(".tmp_")) {
                    QFileInfo fi(fullPath);
                    if (fi.isDir()) {
                        QDir(fullPath).removeRecursively();
                    } else {
                        QFile::remove(fullPath);
                    }
                }
            }
        }
    }

    // Pulisce la cache di yt-dlp per liberare lo stato
    QProcess::execute("yt-dlp", QStringList() << "--rm-cache-dir");

    m_buffer.clear();
    m_lastErrorOutput.clear();
    m_mode = Idle;
}

/**
 * @brief Slot per la lettura in tempo reale dell'output prodotto dal processo yt-dlp.
 */
void FFmpegManager::onProcessReadyRead()
{
    if (m_mode == Analyzing) {
        QByteArray stdoutData = m_process->readAllStandardOutput();
        m_buffer.append(QString::fromUtf8(stdoutData));
    } else if (m_mode == Downloading) {
        QByteArray stdoutData = m_process->readAllStandardOutput();
        QByteArray stderrData = m_process->readAllStandardError();

        if (!stderrData.isEmpty()) {
            m_lastErrorOutput.append(QString::fromUtf8(stderrData));
        }

        QByteArray data = stdoutData;
        data.append(stderrData);
        QString chunk = QString::fromUtf8(data);
        QStringList lines = chunk.split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            parseDownloadOutputLine(line);
        }
    }
}

/**
 * @brief Slot azionato al termine del processo QProcess. Verifica se il file video finale è stato generato prima di segnalare un errore.
 * @param exitCode Codice numerico restituito dal processo.
 * @param exitStatus Stato di uscita (NormalExit o CrashExit).
 */
void FFmpegManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_currentPid.store(0);
    if (m_isCancelled.load()) {
        m_mode = Idle;
        return;
    }

    if (m_mode == Analyzing) {
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            parseAnalysisJson(m_buffer.toUtf8());
        } else {
            emit analysisCompleted(false, MediaMetadata(), "Impossibile analizzare l'URL fornito. Verificare la connessione o l'URL.");
        }
    } else if (m_mode == Downloading) {
        QDir tempDir(m_tempWorkingDir);
        bool hasGeneratedFile = false;
        if (tempDir.exists()) {
            QStringList files = tempDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QString& f : files) {
                if (!f.endsWith(".part") && !f.endsWith(".ytdl") && !f.endsWith(".vtt") && !f.endsWith(".srt") && !f.endsWith("_playable.mp4")) {
                    hasGeneratedFile = true;
                    break;
                }
            }
        }

        if (hasGeneratedFile || (exitCode == 0 && exitStatus == QProcess::NormalExit)) {
            cleanupTempDirAndFinalize();
        } else {
            if (!m_tempWorkingDir.isEmpty() && tempDir.exists()) {
                tempDir.removeRecursively();
            }
            QString errStr = m_lastErrorOutput.trimmed();
            if (errStr.isEmpty()) {
                errStr = "Errore durante il download o il multiplexing con FFmpeg.";
            } else {
                errStr = errStr.left(300);
            }
            emit downloadCompleted(false, "", errStr);
        }
    }
    m_mode = Idle;
}

/**
 * @brief Sposta il file video finale dalla cartella temporanea a quella scelta dall'utente.
 * Se sono presenti file sottotitoli (.vtt/.srt) nella cartella temporanea, usa FFmpeg per
 * incorporarli nel file video insieme alla transcodifica audio Opus→AAC per compatibilità universale.
 */
void FFmpegManager::cleanupTempDirAndFinalize()
{
    if (m_isCancelled.load()) {
        if (!m_tempWorkingDir.isEmpty() && QDir(m_tempWorkingDir).exists()) {
            QDir(m_tempWorkingDir).removeRecursively();
            m_tempWorkingDir.clear();
        }
        return;
    }

    QString stageMsg = QString("Fase %1 di %2: Finalizzazione e muxing con FFmpeg...").arg(m_totalStages).arg(m_totalStages);
    emit progressUpdated(-1.0, stageMsg, "Indeterminato");

    QDir tempDir(m_tempWorkingDir);
    if (!tempDir.exists()) {
        emit downloadCompleted(false, "", "Cartella temporanea non trovata.");
        return;
    }

    QStringList files = tempDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    QString rawSourcePath;
    for (const QString& f : files) {
        if (!f.endsWith(".part") && !f.endsWith(".ytdl") && !f.endsWith(".vtt") && !f.endsWith(".srt") && !f.endsWith("_playable.mp4")) {
            rawSourcePath = tempDir.absoluteFilePath(f);
            break;
        }
    }

    if (rawSourcePath.isEmpty() && !files.isEmpty()) {
        rawSourcePath = tempDir.absoluteFilePath(files.first());
    }

    if (rawSourcePath.isEmpty()) {
        tempDir.removeRecursively();
        emit downloadCompleted(false, "", "Nessun file generato nella cartella temporanea.");
        return;
    }

    // Raccogli tutti i file sottotitoli (.vtt e .srt) presenti nella cartella temporanea
    QStringList subtitleFiles;
    for (const QString& f : files) {
        if (f.endsWith(".vtt") || f.endsWith(".srt")) {
            subtitleFiles.append(tempDir.absoluteFilePath(f));
        }
    }

    QString finalSourcePath = rawSourcePath;

    // Esegue il passaggio FFmpeg per:
    // 1. Transcodificare l'audio Opus in AAC per compatibilità universale (solo MP4)
    // 2. Taggare correttamente ogni traccia audio con lingua ISO 639-2 e titolo (es. "Italiano", "English")
    // 3. Taggare correttamente ogni traccia sottotitolo con lingua ISO 639-2 e titolo
    // 4. Incorporare i file sottotitoli esterni se non già presenti nel flusso
    if (rawSourcePath.endsWith(".mp4", Qt::CaseInsensitive) || rawSourcePath.endsWith(".mkv", Qt::CaseInsensitive)) {
        if (m_isCancelled.load()) {
            if (!m_tempWorkingDir.isEmpty() && tempDir.exists()) {
                tempDir.removeRecursively();
                m_tempWorkingDir.clear();
            }
            return;
        }

        QString playablePath = rawSourcePath.left(rawSourcePath.lastIndexOf('.')) + "_playable" + rawSourcePath.mid(rawSourcePath.lastIndexOf('.'));
        
        // Controlla tramite ffprobe se rawSourcePath contiene già tracce di sottotitoli incorporate
        bool hasEmbeddedSubs = false;
        int rawAudioStreamCount = 0;
        {
            QStringList probeArgs;
            probeArgs << "-v" << "error" << "-show_entries" << "stream=index,codec_type" << "-of" << "csv=p=0" << rawSourcePath;
            m_subProcess->start("ffprobe", probeArgs);
            if (m_subProcess->waitForStarted(1000)) {
                m_subPid.store(m_subProcess->processId());
            }
            if (m_subProcess->waitForFinished(10000) && m_subProcess->exitCode() == 0) {
                QString probeOut = QString::fromUtf8(m_subProcess->readAllStandardOutput());
                QStringList lines = probeOut.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
                for (const QString& line : lines) {
                    if (line.contains("subtitle", Qt::CaseInsensitive)) {
                        hasEmbeddedSubs = true;
                    } else if (line.contains("audio", Qt::CaseInsensitive)) {
                        rawAudioStreamCount++;
                    }
                }
            }
            m_subPid.store(0);
        }

        if (m_isCancelled.load()) {
            if (!m_tempWorkingDir.isEmpty() && tempDir.exists()) {
                tempDir.removeRecursively();
                m_tempWorkingDir.clear();
            }
            return;
        }

        QStringList ffArgs;
        ffArgs << "-y";
        ffArgs << "-i" << rawSourcePath;

        // Se yt-dlp non ha già incorporato i sottotitoli nel file sorgente, aggiungi i file esterni
        bool addExternalSubs = (!hasEmbeddedSubs && !subtitleFiles.isEmpty());
        if (addExternalSubs) {
            for (const QString& subFile : subtitleFiles) {
                ffArgs << "-i" << subFile;
            }
        }

        // Mappa flussi video e audio da input 0
        ffArgs << "-map" << "0:v?" << "-map" << "0:a?";

        // Mappa flussi sottotitoli
        if (hasEmbeddedSubs) {
            ffArgs << "-map" << "0:s?";
        } else if (addExternalSubs) {
            for (int i = 0; i < subtitleFiles.count(); ++i) {
                ffArgs << "-map" << QString("%1:0").arg(i + 1);
            }
        }

        // Codec: copia video invariato, transcodifica audio in AAC 192k solo per MP4 (per MKV mantiene copia lossless)
        ffArgs << "-c:v" << "copy";

        if (rawSourcePath.endsWith(".mp4", Qt::CaseInsensitive)) {
            ffArgs << "-c:a" << "aac" << "-b:a" << "192k";
            ffArgs << "-c:s" << "mov_text";
        } else {
            ffArgs << "-c:a" << "copy";
            ffArgs << "-c:s" << "srt";
        }

        // TAGGING METADATI AUDIO (Lingua ISO 639-2, Titolo e Handler Name)
        int totalAudioTracks = qMax(rawAudioStreamCount, m_currentDownloadOptions.selectedAudioTracks.count());
        if (totalAudioTracks == 0) totalAudioTracks = 1;

        for (int a = 0; a < totalAudioTracks; ++a) {
            QString langCode;
            QString langDisplay;

            if (a < m_currentDownloadOptions.selectedAudioTracks.count()) {
                langCode = m_currentDownloadOptions.selectedAudioTracks[a].langCode;
                langDisplay = m_currentDownloadOptions.selectedAudioTracks[a].langDisplay;
            }

            if (langCode.isEmpty() || langCode.toLower() == "default" || langCode.toLower() == "orig" || langCode.toLower() == "null") {
                langCode = m_currentMetadata.originalLanguage;
                if (!langCode.isEmpty()) {
                    langDisplay = getLanguageDisplayName(langCode);
                }
            }

            if (langCode.isEmpty()) {
                langCode = "und";
                langDisplay = "Audio";
            }

            QString isoCode = toIso639Code(langCode);
            QString cleanName = !langDisplay.isEmpty() ? langDisplay : getLanguageDisplayName(langCode);

            ffArgs << QString("-metadata:s:a:%1").arg(a) << QString("language=%1").arg(isoCode);
            ffArgs << QString("-metadata:s:a:%1").arg(a) << QString("title=%1").arg(cleanName);
            ffArgs << QString("-metadata:s:a:%1").arg(a) << QString("handler_name=%1").arg(cleanName);
        }

        // TAGGING METADATI SOTTOTITOLI (Lingua ISO 639-2, Titolo e Handler Name)
        int subCount = m_currentDownloadOptions.selectedSubtitleLangs.count();
        if (subCount == 0 && addExternalSubs) {
            subCount = subtitleFiles.count();
        }

        for (int s = 0; s < subCount; ++s) {
            QString langCode;
            if (s < m_currentDownloadOptions.selectedSubtitleLangs.count()) {
                langCode = m_currentDownloadOptions.selectedSubtitleLangs[s];
            } else if (s < subtitleFiles.count()) {
                QString subFileName = QFileInfo(subtitleFiles[s]).completeBaseName();
                int lastDot = subFileName.lastIndexOf('.');
                if (lastDot >= 0) langCode = subFileName.mid(lastDot + 1);
            }

            if (langCode.isEmpty() || langCode.toLower() == "default" || langCode.toLower() == "orig") {
                langCode = m_currentMetadata.originalLanguage;
            }

            if (langCode.isEmpty()) {
                langCode = "und";
            }

            QString isoCode = toIso639Code(langCode);
            QString langName = getLanguageDisplayName(langCode);

            ffArgs << QString("-metadata:s:s:%1").arg(s) << QString("language=%1").arg(isoCode);
            ffArgs << QString("-metadata:s:s:%1").arg(s) << QString("title=%1").arg(langName);
            ffArgs << QString("-metadata:s:s:%1").arg(s) << QString("handler_name=%1").arg(langName);
        }

        ffArgs << playablePath;

        m_subProcess->start("ffmpeg", ffArgs);
        if (m_subProcess->waitForStarted(1000)) {
            m_subPid.store(m_subProcess->processId());
        }
        // Timeout fino a 5 minuti per file lunghi con transcodifica audio
        if (m_subProcess->waitForFinished(300000) && m_subProcess->exitCode() == 0 && QFile::exists(playablePath)) {
            finalSourcePath = playablePath;
        }
        m_subPid.store(0);
    }

    if (m_isCancelled.load()) {
        if (!m_tempWorkingDir.isEmpty() && tempDir.exists()) {
            tempDir.removeRecursively();
            m_tempWorkingDir.clear();
        }
        return;
    }

    QFileInfo fileInfo(rawSourcePath);
    QString destPath = m_currentDownloadOptions.saveDirectory + "/" + fileInfo.fileName();

    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }

    bool moved = QFile::rename(finalSourcePath, destPath);
    if (!moved) {
        moved = QFile::copy(finalSourcePath, destPath);
        QFile::remove(finalSourcePath);
    }

    // Copia gli eventuali file di sottotitoli creati (.srt / .vtt) affinché siano presenti anche come file esterni nella cartella
    for (const QString& f : files) {
        if (f.endsWith(".srt") || f.endsWith(".vtt")) {
            QString srcSub = tempDir.absoluteFilePath(f);
            QString dstSub = m_currentDownloadOptions.saveDirectory + "/" + f;
            if (QFile::exists(dstSub)) QFile::remove(dstSub);
            QFile::copy(srcSub, dstSub);
        }
    }

    tempDir.removeRecursively();
    m_tempWorkingDir.clear();

    if (m_isCancelled.load()) {
        return;
    }

    if (moved) {
        emit progressUpdated(100.0, QString("Fase %1 di %1: Operazione Completata!").arg(m_totalStages), "00:00");
        emit downloadCompleted(true, destPath, "");
    } else {
        emit downloadCompleted(false, "", "Impossibile spostare il file dalla cartella temporanea a quella di destinazione.");
    }
}

/**
 * @brief Effettua il parsing della risposta JSON estraendo risoluzioni, tracce audio pulite e tutti i sottotitoli.
 * Esclude:
 * - I formati HLS muxati video+audio (protocol: m3u8_native) dalla lista video per evitare formati non scaricabili
 * - I formati -drc (Dynamic Range Compression) dalla lista audio che richiedono PO Token
 * - Le tracce audio con vcodec != "none" (flussi muxati) dalla lista audio
 * @param jsonOutput Output in byte ritornato da yt-dlp -J.
 */
void FFmpegManager::parseAnalysisJson(const QByteArray& jsonOutput)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonOutput, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[FFmpegManager] JSON parse error:" << err.errorString();
        emit analysisCompleted(false, MediaMetadata(), "Errore nel parsing dei metadata JSON: " + err.errorString());
        return;
    }

    QJsonObject obj = doc.object();
    m_currentMetadata.title = obj.value("title").toString("Video Sconosciuto");
    m_currentMetadata.durationSeconds = obj.value("duration").toInt(0);
    m_currentMetadata.thumbnailUrl = obj.value("thumbnail").toString();
    m_currentMetadata.originalLanguage = obj.value("language").toString();

    QMap<int, VideoFormatOption> videoMap;
    QList<AudioTrackOption> audioList;
    QList<SubtitleOption> subList;

    QJsonArray formats = obj.value("formats").toArray();
    QString origLangBase = m_currentMetadata.originalLanguage.left(2).toLower();

    for (const QJsonValue& val : formats) {
        QJsonObject fmt = val.toObject();
        QString formatId = fmt.value("format_id").toString();
        QString vcodec = fmt.value("vcodec").toString();
        QString acodec = fmt.value("acodec").toString();
        QString protocol = fmt.value("protocol").toString();
        int height = fmt.value("height").toInt(0);
        int fps = fmt.value("fps").toInt(0);
        qint64 filesize = fmt.value("filesize").toVariant().toLongLong();
        QString lang = fmt.value("language").toString();
        if (lang.isEmpty() || lang == "null") {
            lang = fmt.value("audio_ext").toString();
        }

        // Estrazione delle risoluzioni video - SOLO formati HTTPS scaricabili (esclusi HLS m3u8 e formati muxati video+audio)
        if (vcodec != "none" && height > 0 && acodec == "none" && protocol == "https") {
            VideoFormatOption vopt;
            vopt.formatId = formatId;
            vopt.height = height;
            vopt.fps = fps;
            vopt.resolution = QString("%1p").arg(height);
            vopt.vcodec = vcodec;
            vopt.ext = fmt.value("ext").toString();
            vopt.filesize = filesize;
            vopt.note = fmt.value("format_note").toString();

            if (!videoMap.contains(height) || filesize > videoMap[height].filesize) {
                videoMap[height] = vopt;
            }
        }

        // Estrazione delle tracce SOLO audio (vcodec == "none" e acodec valido)
        // Escludiamo:
        // - Flussi HLS video+audio muxati o frammentati m3u8_native (bitrate nullo o instabile)
        // - Formati -drc (Dynamic Range Compression) che richiedono PO Token
        if (acodec != "none" && !acodec.isEmpty() && vcodec == "none") {
            if (formatId.contains("-drc", Qt::CaseInsensitive) || fmt.value("format_note").toString().contains("drc", Qt::CaseInsensitive)) {
                continue;
            }

            if (protocol.contains("m3u8", Qt::CaseInsensitive)) {
                continue;
            }

            AudioTrackOption aopt;
            aopt.formatId = formatId;
            aopt.language = lang.isEmpty() ? "default" : lang;
            aopt.languageDisplay = getLanguageDisplayName(aopt.language);
            aopt.title = fmt.value("format_note").toString(fmt.value("ext").toString());
            aopt.acodec = acodec;

            // Usa 'abr' (audio bitrate), con fallback su 'tbr' se abr è assente
            double abr = fmt.value("abr").toDouble(0.0);
            if (abr <= 0.0) {
                abr = fmt.value("tbr").toDouble(0.0);
            }
            aopt.bitrate = qRound(abr);

            QString trackLangBase = aopt.language.left(2).toLower();
            bool titleHasOrig = aopt.title.contains("original", Qt::CaseInsensitive) || aopt.title.contains("default", Qt::CaseInsensitive);
            bool langMatchesOrig = (!origLangBase.isEmpty() && trackLangBase == origLangBase);
            bool isDefaultLang = (aopt.language.toLower() == "default");

            aopt.isOriginal = (titleHasOrig || langMatchesOrig || isDefaultLang);

            if (aopt.isOriginal && m_currentMetadata.originalLanguage.isEmpty()) {
                m_currentMetadata.originalLanguage = aopt.language;
            }

            // Inserisce tutte le tracce audio valide
            if (aopt.bitrate > 0 || filesize > 0) {
                audioList.append(aopt);
            }
        }
    }

    // Se un video non dispone di flussi diretti https per l'audio, fallback sui flussi alternativi
    if (audioList.isEmpty()) {
        for (const QJsonValue& val : formats) {
            QJsonObject fmt = val.toObject();
            QString formatId = fmt.value("format_id").toString();
            QString vcodec = fmt.value("vcodec").toString();
            QString acodec = fmt.value("acodec").toString();
            if (vcodec == "none" && acodec != "none") {
                AudioTrackOption aopt;
                aopt.formatId = formatId;
                aopt.language = fmt.value("language").toString("default");
                aopt.languageDisplay = getLanguageDisplayName(aopt.language);
                aopt.title = fmt.value("format_note").toString(fmt.value("ext").toString());
                aopt.acodec = acodec;
                aopt.bitrate = qRound(fmt.value("abr").toDouble(0.0));
                audioList.append(aopt);
            }
        }
    }

    // Ordina le tracce audio per bitrate decrescente per garantire che le tracce di massima qualità siano sempre in cima
    std::sort(audioList.begin(), audioList.end(), [](const AudioTrackOption& a, const AudioTrackOption& b) {
        return a.bitrate > b.bitrate;
    });

    // Ordina le risoluzioni video dalla più alta alla più bassa
    QList<int> heights = videoMap.keys();
    std::sort(heights.begin(), heights.end(), std::greater<int>());
    for (int h : heights) {
        m_currentMetadata.videoFormats.append(videoMap[h]);
    }

    m_currentMetadata.audioTracks = audioList;

    // Estrazione e filtraggio dei sottotitoli manuali ed automatici (escludendo le tracce vuote)
    QJsonObject subsObj = obj.value("subtitles").toObject();
    QJsonObject autoSubsObj = obj.value("automatic_captions").toObject();

    QStringList subLangs = subsObj.keys();
    for (const QString& lang : subLangs) {
        QJsonArray formatsArr = subsObj.value(lang).toArray();
        if (formatsArr.isEmpty()) continue;

        SubtitleOption sopt;
        sopt.langCode = lang;
        sopt.langName = getLanguageDisplayName(lang);
        sopt.ext = "vtt/srt";
        sopt.isAuto = false;
        sopt.isOriginal = (!origLangBase.isEmpty() && lang.toLower().startsWith(origLangBase));
        subList.append(sopt);
    }
    for (const QString& lang : autoSubsObj.keys()) {
        QJsonArray formatsArr = autoSubsObj.value(lang).toArray();
        if (formatsArr.isEmpty()) continue;

        if (!subLangs.contains(lang)) {
            SubtitleOption sopt;
            sopt.langCode = lang;
            sopt.langName = getLanguageDisplayName(lang) + " (Auto)";
            sopt.ext = "vtt/srt";
            sopt.isAuto = true;
            sopt.isOriginal = (!origLangBase.isEmpty() && lang.toLower().startsWith(origLangBase));
            subList.append(sopt);
        }
    }

    m_currentMetadata.subtitles = subList;

    emit analysisCompleted(true, m_currentMetadata, "");
}

/**
 * @brief Analizza le righe emesse da yt-dlp aggiornando la fase corrente (es. Fase 1 di 5, Fase 2 di 5) e la percentuale o lo stato indeterminato.
 * @param line Riga di testo dall'output del processo.
 */
void FFmpegManager::parseDownloadOutputLine(const QString& line)
{
    static QRegularExpression destRegex("\\[download\\] Destination:|Downloading subtitle");
    static QRegularExpression percentRegex("\\[download\\]\\s+(\\d+(?:\\.‏\\d+)?)%");
    static QRegularExpression etaRegex("ETA\\s+(\\d+:\\d+)");
    static QRegularExpression ffmpegRegex("\\[ffmpeg\\]|Merging|Embedding");

    // Monitora il passaggio alla destinazione successiva per avanzare di fase
    if (destRegex.match(line).hasMatch()) {
        m_downloadDestinationCount++;
        if (m_downloadDestinationCount == 1) {
            m_currentStage = 1;
        } else if (m_downloadDestinationCount == 2) {
            m_currentStage = 2;
        }
    }

    // Monitora il passaggio alla fase finale di multiplexing ed integrazione metadati con FFmpeg
    if (ffmpegRegex.match(line).hasMatch()) {
        m_currentStage = m_totalStages - 1;
        QString stageMsg = QString("Fase %1 di %2: Multiplexing ed unione metadati con FFmpeg...").arg(m_currentStage).arg(m_totalStages);
        // Passa avanzamento -1.0 per attivare l'animazione grafica a tempo indeterminato (busy indicator marquee)
        emit progressUpdated(-1.0, stageMsg, "Indeterminato");
        return;
    }

    // Estrazione della percentuale e dell'ETA dai log di download
    QRegularExpressionMatch pMatch = percentRegex.match(line);
    if (pMatch.hasMatch()) {
        double pct = pMatch.captured(1).toDouble();
        QString eta = "Calcolo...";
        QRegularExpressionMatch eMatch = etaRegex.match(line);
        if (eMatch.hasMatch()) {
            eta = eMatch.captured(1);
        }

        QString stageDesc;
        if (m_currentStage == 1) {
            stageDesc = QString("Fase 1 di %1: Download flusso video...").arg(m_totalStages);
        } else if (m_currentStage == 2) {
            stageDesc = QString("Fase 2 di %1: Download tracce audio selezionate...").arg(m_totalStages);
        } else {
            stageDesc = QString("Fase %1 di %2: Download flussi media in corso...").arg(m_currentStage).arg(m_totalStages);
        }

        emit progressUpdated(pct, stageDesc, eta);
        return;
    }
}
