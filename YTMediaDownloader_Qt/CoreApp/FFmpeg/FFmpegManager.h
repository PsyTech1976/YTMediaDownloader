/**
 * @file FFmpegManager.h
 * @brief Gestore asincrono per l'analisi media, download e muxing FFmpeg con yt-dlp.
 * 
 * Esegue le chiamate a yt-dlp, ffprobe e ffmpeg su un thread separato, fornendo
 * avanzamento continuo, gestione multi-audio, sottotitoli e terminazione dei processi.
 */

#ifndef FFMPEGMANAGER_H
#define FFMPEGMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>
#include <QMetaType>
#include <atomic>

/**
 * @brief Struttura dati per le opzioni dei formati video disponibili.
 */
struct VideoFormatOption {
    QString formatId;       ///< ID del formato yt-dlp (es. 137, 248)
    QString resolution;     ///< Risoluzione leggibile (es. "1080p", "720p")
    int height = 0;         ///< Altezza in pixel (es. 1080)
    int fps = 0;            ///< Frame rate al secondo
    QString vcodec;         ///< Codec video (es. "avc1", "vp9", "av01")
    QString ext;            ///< Estensione contenitore video (es. "mp4", "webm")
    qint64 filesize = 0;    ///< Dimensione stimata o reale in byte
    QString note;           ///< Nota informativa o bitrate
};

/**
 * @brief Struttura dati per le tracce audio disponibili.
 */
struct AudioTrackOption {
    QString formatId;       ///< ID del formato yt-dlp (es. 140, 251, 140-16)
    QString language;       ///< Codice lingua della traccia (es. "it", "en-US")
    QString languageDisplay;///< Nome visualizzato della lingua (es. "Italiano")
    QString title;          ///< Titolo o descrizione (es. "dubbed", "original")
    QString acodec;         ///< Codec audio (es. "mp4a.40.2", "opus")
    int bitrate = 0;        ///< Bitrate medio stimato in kbps
    bool isOriginal = false;///< True se è la traccia originale del video
};

/**
 * @brief Struttura dati per le tracce di sottotitoli disponibili.
 */
struct SubtitleOption {
    QString langCode;       ///< Codice lingua del sottotitolo (es. "it", "en")
    QString langName;       ///< Nome visualizzato della lingua
    QString ext;            ///< Formato file sottotitoli (es. "vtt", "srt")
    bool isOriginal = false;///< True se coincide con la lingua del video
    bool isAuto = false;    ///< True se sottotitolo automatico (ASR)
};

/**
 * @brief Informazioni sulla singola traccia audio selezionata per il tagging.
 */
struct AudioSelectionInfo {
    QString formatId;       ///< ID del formato
    QString langCode;       ///< Codice ISO della lingua
    QString langDisplay;    ///< Nome leggibile esteso per il tag del flusso
};

/**
 * @brief Metadati estratti dalla pagina o dal manifesto del video YouTube.
 */
struct MediaMetadata {
    QString url;                                ///< URL sorgente del video
    QString title;                              ///< Titolo del video sanificato
    int durationSeconds = 0;                    ///< Durata complessiva in secondi
    QString thumbnailUrl;                       ///< URL dell'immagine di copertina
    QString originalLanguage;                   ///< Lingua originale identificata
    QList<VideoFormatOption> videoFormats;      ///< Lista delle risoluzioni video disponibili
    QList<AudioTrackOption> audioTracks;        ///< Lista delle tracce audio disponibili
    QList<SubtitleOption> subtitles;            ///< Lista dei sottotitoli disponibili
};

/**
 * @brief Opzioni scelte dall'utente per il download e la lavorazione finale.
 */
struct DownloadOptions {
    QString url;                                ///< URL del video
    QString saveDirectory;                      ///< Cartella finale di salvataggio
    QString selectedVideoFormatId;              ///< ID formato video selezionato
    QString containerFormat;                    ///< Contenitore finale ("mp4", "mkv", "webm")
    QStringList selectedAudioFormatIds;         ///< Elenco ID delle tracce audio selezionate
    QList<AudioSelectionInfo> selectedAudioTracks; ///< Dati completi per il tagging audio
    QStringList selectedSubtitleLangs;          ///< Elenco codici lingua sottotitoli scelti
};

Q_DECLARE_METATYPE(VideoFormatOption)
Q_DECLARE_METATYPE(AudioTrackOption)
Q_DECLARE_METATYPE(SubtitleOption)
Q_DECLARE_METATYPE(AudioSelectionInfo)
Q_DECLARE_METATYPE(MediaMetadata)
Q_DECLARE_METATYPE(DownloadOptions)

/**
 * @class FFmpegManager
 * @brief Gestore asincrono dell'elaborazione media, estrazione flussi e muxing FFmpeg.
 */
class FFmpegManager : public QObject {
    Q_OBJECT
public:
    explicit FFmpegManager(QObject* parent = nullptr);
    ~FFmpegManager();

    void killActiveProcesses();
    static bool checkToolsAvailable(QString& errorDetails);
    static QString getLanguageDisplayName(const QString& code);
    static QString toIso639Code(const QString& code);
    static void killProcessTree(qint64 pid);

public slots:
    void analyzeUrl(const QString& url);
    void startDownload(const DownloadOptions& options);
    void cancelCurrentTask();

signals:
    void analysisCompleted(bool success, const MediaMetadata& metadata, const QString& errorMessage);
    void progressUpdated(double percentage, const QString& currentStep, const QString& timeRemaining);
    void downloadCompleted(bool success, const QString& finalFilePath, const QString& errorMessage);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void parseAnalysisJson(const QByteArray& jsonOutput);
    void parseDownloadOutputLine(const QString& line);
    void cleanupTempDirAndFinalize();

    QProcess* m_process;
    QProcess* m_subProcess;
    std::atomic<qint64> m_currentPid{0};
    std::atomic<qint64> m_subPid{0};
    std::atomic<bool> m_isCancelled{false};

    enum ProcessMode { Idle, Analyzing, Downloading } m_mode;
    MediaMetadata m_currentMetadata;
    DownloadOptions m_currentDownloadOptions;
    QString m_buffer;
    QString m_lastErrorOutput;
    QString m_tempWorkingDir;

    int m_totalStages;
    int m_currentStage;
    int m_downloadDestinationCount;
};

#endif // FFMPEGMANAGER_H
