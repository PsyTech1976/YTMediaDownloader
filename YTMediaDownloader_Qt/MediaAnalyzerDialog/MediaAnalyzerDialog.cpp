/**
 * @file MediaAnalyzerDialog.cpp
 * @brief Implementazione del dialogo di selezione opzioni media MediaAnalyzerDialog.
 * 
 * Organizza le tracce audio ed i sottotitoli in 3 sezioni ben distinte tramite separatori visuali:
 * 1. Tracce Originali del video (in cima, selezionando automaticamente la traccia standard a qualità/bitrate più alto).
 * 2. Tracce Localizzate nella lingua dell'applicazione.
 * 3. Tutte le altre tracce (altre lingue).
 * Privilegia le tracce audio standard non-DRC in cima alla lista e salva i dati di lingua espliciti per ciascun flusso audio.
 */

#include "MediaAnalyzerDialog.h"
#include "ui_MediaAnalyzerDialog.h"
#include "CoreApp/Localization/LocalizationManager.h"
#include "CoreApp/IO/FileManager.h"
#include <QListWidgetItem>
#include <QFont>
#include <QBrush>
#include <algorithm>

/**
 * @brief Costruttore della classe MediaAnalyzerDialog.
 * @param metadata Struttura contenente tutti i flussi estratti dall'analisi.
 * @param parent Widget padre facoltativo.
 */
MediaAnalyzerDialog::MediaAnalyzerDialog(const MediaMetadata& metadata, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MediaAnalyzerDialog)
    , m_metadata(metadata)
{
    ui->setupUi(this);

    // Connessione dei pulsanti di conferma ed annullamento
    connect(ui->btnConfirm, &QPushButton::clicked, this, &MediaAnalyzerDialog::onConfirmClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &MediaAnalyzerDialog::onCancelClicked);

    // Connessione dei pulsanti di selezione/deselezione massiva per l'audio
    connect(ui->btnSelectAllAudio, &QPushButton::clicked, this, &MediaAnalyzerDialog::onSelectAllAudioClicked);
    connect(ui->btnDeselectAllAudio, &QPushButton::clicked, this, &MediaAnalyzerDialog::onDeselectAllAudioClicked);

    // Connessione dei pulsanti di selezione/deselezione massiva per i sottotitoli
    connect(ui->btnSelectAllSubs, &QPushButton::clicked, this, &MediaAnalyzerDialog::onSelectAllSubsClicked);
    connect(ui->btnDeselectAllSubs, &QPushButton::clicked, this, &MediaAnalyzerDialog::onDeselectAllSubsClicked);

    // Popola tutti i widget di elenco ed applica la localizzazione
    populateFields();
    retranslateUi();
}

/**
 * @brief Distruttore della classe MediaAnalyzerDialog.
 */
MediaAnalyzerDialog::~MediaAnalyzerDialog()
{
    delete ui;
}

/**
 * @brief Aggiorna i testi dell'interfaccia utente in base alla lingua attiva.
 */
void MediaAnalyzerDialog::retranslateUi()
{
    setWindowTitle(LOC("MediaAnalyzerDialog", "window_title", "Analisi e Selezione Tracce Media"));
    ui->grpVideo->setTitle(LOC("MediaAnalyzerDialog", "grp_video", "Formato e Risoluzione Video"));
    ui->lblResolution->setText(LOC("MediaAnalyzerDialog", "lbl_resolution", "Risoluzione Video:"));
    ui->lblContainer->setText(LOC("MediaAnalyzerDialog", "lbl_container", "Contenitore Output:"));
    ui->grpAudio->setTitle(LOC("MediaAnalyzerDialog", "grp_audio", "Tracce Audio da Allegare (Selezione Multipla)"));
    ui->grpSubs->setTitle(LOC("MediaAnalyzerDialog", "grp_subs", "Sottotitoli da Allegare (Selezione Multipla)"));

    ui->btnSelectAllAudio->setText(LOC("MediaAnalyzerDialog", "btn_select_all", "Seleziona Tutti"));
    ui->btnDeselectAllAudio->setText(LOC("MediaAnalyzerDialog", "btn_deselect_all", "Deseleziona Tutti"));
    ui->btnSelectAllSubs->setText(LOC("MediaAnalyzerDialog", "btn_select_all", "Seleziona Tutti"));
    ui->btnDeselectAllSubs->setText(LOC("MediaAnalyzerDialog", "btn_deselect_all", "Deseleziona Tutti"));

    ui->btnConfirm->setText(LOC("MediaAnalyzerDialog", "btn_confirm", "Conferma Selezione"));
    ui->btnCancel->setText(LOC("MediaAnalyzerDialog", "btn_cancel", "Annulla"));
}

/**
 * @brief Popola i controlli GUI inserendo le tracce ed i 3 livelli di separazione sia per Audio che per Sottotitoli.
 * Spunta di default SOLO la traccia originale a qualità/bitrate più alto e memorizza i dati di lingua per i tag.
 */
void MediaAnalyzerDialog::populateFields()
{
    // Popola le risoluzioni video disponibili
    ui->comboResolution->clear();
    for (const VideoFormatOption& vfmt : m_metadata.videoFormats) {
        QString sizeStr = (vfmt.filesize > 0) ? QString(" (~%1)").arg(FileManager::formatFileSize(vfmt.filesize)) : "";
        QString text = QString("%1 (%2, %3 fps)%4").arg(vfmt.resolution, vfmt.vcodec, QString::number(vfmt.fps), sizeStr);
        ui->comboResolution->addItem(text, vfmt.formatId);
    }

    // Popola i contenitori di output
    ui->comboContainer->clear();
    ui->comboContainer->addItem("MP4 (.mp4)", "mp4");
    ui->comboContainer->addItem("MKV (.mkv)", "mkv");
    ui->comboContainer->addItem("WebM (.webm)", "webm");

    // Determina il codice lingua ed il nome leggibile della lingua dell'applicazione
    QString appLangCode = LocalizationManager::instance().currentLanguage();
    QString appLangBase = appLangCode.left(2).toLower();
    QString appLangName = FFmpegManager::getLanguageDisplayName(appLangBase).toUpper();

    // =========================================================================
    // 1. POPOLAMENTO TRACCE AUDIO (3 SEZIONI: ORIGINALI, LINGUA APP, ALTRE)
    // =========================================================================
    ui->listAudioTracks->clear();

    if (m_metadata.audioTracks.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem("Traccia Audio Predefinita (Best Audio)", ui->listAudioTracks);
        item->setCheckState(Qt::Checked);
        item->setData(Qt::UserRole, "bestaudio");
        item->setData(Qt::UserRole + 1, "default");
        item->setData(Qt::UserRole + 2, "Default Audio");
    } else {
        QList<AudioTrackOption> origAudio;
        QList<AudioTrackOption> appAudio;
        QList<AudioTrackOption> otherAudio;

        for (const AudioTrackOption& aopt : m_metadata.audioTracks) {
            if (aopt.isOriginal) {
                origAudio.append(aopt);
            } else if (aopt.language.toLower().startsWith(appLangBase)) {
                appAudio.append(aopt);
            } else {
                otherAudio.append(aopt);
            }
        }

        // Se la sezione originale è vuota, assegna le tracce predefinite/in lingua predefinita come originali
        if (origAudio.isEmpty() && !m_metadata.audioTracks.isEmpty()) {
            origAudio.append(m_metadata.audioTracks.first());
        }

        // Ordina le tracce audio: prima le tracce standard non-DRC, poi per bitrate decrescente
        auto sortAudioTracks = [](const AudioTrackOption& a, const AudioTrackOption& b) {
            bool aDrc = a.formatId.contains("-drc", Qt::CaseInsensitive) || a.title.contains("drc", Qt::CaseInsensitive);
            bool bDrc = b.formatId.contains("-drc", Qt::CaseInsensitive) || b.title.contains("drc", Qt::CaseInsensitive);
            if (aDrc != bDrc) {
                return !aDrc; // Le tracce non-DRC hanno sempre la precedenza in cima alla lista
            }
            return a.bitrate > b.bitrate;
        };

        std::sort(origAudio.begin(), origAudio.end(), sortAudioTracks);
        std::sort(appAudio.begin(), appAudio.end(), sortAudioTracks);
        std::sort(otherAudio.begin(), otherAudio.end(), sortAudioTracks);

        // Sezione 1: Tracce Audio Originali del video (selezionando SOLO la prima con bitrate/qualità più alta)
        if (!origAudio.isEmpty()) {
            QListWidgetItem* header = new QListWidgetItem(LOC("MediaAnalyzerDialog", "header_audio_orig", "--- TRACCE AUDIO ORIGINALI ---"), ui->listAudioTracks);
            header->setFlags(Qt::NoItemFlags);
            header->setForeground(QBrush(QColor("#0d6efd")));
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);

            bool isFirstOrig = true;
            for (const AudioTrackOption& aopt : origAudio) {
                QString text = QString("[%1] %2 (%3, %4 kbps)")
                                   .arg(aopt.languageDisplay, aopt.title, aopt.acodec, QString::number(aopt.bitrate));
                QListWidgetItem* item = new QListWidgetItem(text, ui->listAudioTracks);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

                if (isFirstOrig) {
                    item->setCheckState(Qt::Checked);
                    isFirstOrig = false;
                } else {
                    item->setCheckState(Qt::Unchecked);
                }
                item->setData(Qt::UserRole, aopt.formatId);
                item->setData(Qt::UserRole + 1, aopt.language);
                item->setData(Qt::UserRole + 2, aopt.languageDisplay);
            }
        }

        // Sezione 2: Tracce Audio Localizzate nella lingua dell'applicazione
        if (!appAudio.isEmpty()) {
            QString appHeaderTpl = LOC("MediaAnalyzerDialog", "header_audio_app", "--- TRACCE AUDIO LOCALIZZATE (%1) ---");
            QListWidgetItem* header = new QListWidgetItem(appHeaderTpl.arg(appLangName), ui->listAudioTracks);
            header->setFlags(Qt::NoItemFlags);
            header->setForeground(QBrush(QColor("#198754")));
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);

            bool isFirstApp = true;
            for (const AudioTrackOption& aopt : appAudio) {
                QString text = QString("[%1] %2 (%3, %4 kbps)")
                                   .arg(aopt.languageDisplay, aopt.title, aopt.acodec, QString::number(aopt.bitrate));
                QListWidgetItem* item = new QListWidgetItem(text, ui->listAudioTracks);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

                if (isFirstApp) {
                    item->setCheckState(Qt::Checked);
                    isFirstApp = false;
                } else {
                    item->setCheckState(Qt::Unchecked);
                }
                item->setData(Qt::UserRole, aopt.formatId);
                item->setData(Qt::UserRole + 1, aopt.language);
                item->setData(Qt::UserRole + 2, aopt.languageDisplay);
            }
        }

        // Sezione 3: Altre Tracce Audio (Altre lingue)
        if (!otherAudio.isEmpty()) {
            QListWidgetItem* header = new QListWidgetItem(LOC("MediaAnalyzerDialog", "header_audio_other", "--- ALTRE TRACCE AUDIO ---"), ui->listAudioTracks);
            header->setFlags(Qt::NoItemFlags);
            header->setForeground(QBrush(QColor("#6c757d")));
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);

            for (const AudioTrackOption& aopt : otherAudio) {
                QString text = QString("[%1] %2 (%3, %4 kbps)")
                                   .arg(aopt.languageDisplay, aopt.title, aopt.acodec, QString::number(aopt.bitrate));
                QListWidgetItem* item = new QListWidgetItem(text, ui->listAudioTracks);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                item->setData(Qt::UserRole, aopt.formatId);
                item->setData(Qt::UserRole + 1, aopt.language);
                item->setData(Qt::UserRole + 2, aopt.languageDisplay);
            }
        }
    }

    // =========================================================================
    // 2. POPOLAMENTO SOTTOTITOLI (3 SEZIONI: ORIGINALI, LINGUA APP, ALTRI)
    // =========================================================================
    ui->listSubtitles->clear();

    if (m_metadata.subtitles.isEmpty()) {
        QListWidgetItem* item = new QListWidgetItem("Nessun sottotitolo disponibile", ui->listSubtitles);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    } else {
        QList<SubtitleOption> origSubs;
        QList<SubtitleOption> appSubs;
        QList<SubtitleOption> otherSubs;

        QString origLangBase = m_metadata.originalLanguage.left(2).toLower();

        for (const SubtitleOption& sopt : m_metadata.subtitles) {
            if (!origLangBase.isEmpty() && sopt.langCode.toLower().startsWith(origLangBase)) {
                origSubs.append(sopt);
            } else if (sopt.langCode.toLower().startsWith(appLangBase)) {
                appSubs.append(sopt);
            } else {
                otherSubs.append(sopt);
            }
        }

        auto sortSubs = [](const SubtitleOption& a, const SubtitleOption& b) {
            if (a.isAuto != b.isAuto) {
                return a.isAuto < b.isAuto;
            }
            return a.langName < b.langName;
        };
        std::sort(origSubs.begin(), origSubs.end(), sortSubs);
        std::sort(appSubs.begin(), appSubs.end(), sortSubs);
        std::sort(otherSubs.begin(), otherSubs.end(), sortSubs);

        // Sezione 1: Sottotitoli Originali del video
        if (!origSubs.isEmpty()) {
            QListWidgetItem* header = new QListWidgetItem(LOC("MediaAnalyzerDialog", "header_subs_orig", "--- SOTTOTITOLI ORIGINALI ---"), ui->listSubtitles);
            header->setFlags(Qt::NoItemFlags);
            header->setForeground(QBrush(QColor("#0d6efd")));
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);

            for (const SubtitleOption& sopt : origSubs) {
                QString text = QString("%1 [%2]").arg(sopt.langName, sopt.langCode);
                QListWidgetItem* item = new QListWidgetItem(text, ui->listSubtitles);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                item->setData(Qt::UserRole, sopt.langCode);
            }
        }

        // Sezione 2: Sottotitoli Localizzati nella lingua dell'applicazione
        if (!appSubs.isEmpty()) {
            QString appSubTpl = LOC("MediaAnalyzerDialog", "header_subs_app", "--- SOTTOTITOLI LOCALIZZATI (%1) ---");
            QListWidgetItem* header = new QListWidgetItem(appSubTpl.arg(appLangName), ui->listSubtitles);
            header->setFlags(Qt::NoItemFlags);
            header->setForeground(QBrush(QColor("#198754")));
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);

            for (const SubtitleOption& sopt : appSubs) {
                QString text = QString("%1 [%2]").arg(sopt.langName, sopt.langCode);
                QListWidgetItem* item = new QListWidgetItem(text, ui->listSubtitles);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
                item->setData(Qt::UserRole, sopt.langCode);
            }
        }

        // Sezione 3: Altri Sottotitoli (Altre lingue)
        if (!otherSubs.isEmpty()) {
            QListWidgetItem* header = new QListWidgetItem(LOC("MediaAnalyzerDialog", "header_subs_other", "--- ALTRI SOTTOTITOLI ---"), ui->listSubtitles);
            header->setFlags(Qt::NoItemFlags);
            header->setForeground(QBrush(QColor("#6c757d")));
            QFont font = header->font();
            font.setBold(true);
            header->setFont(font);

            for (const SubtitleOption& sopt : otherSubs) {
                QString text = QString("%1 [%2]").arg(sopt.langName, sopt.langCode);
                QListWidgetItem* item = new QListWidgetItem(text, ui->listSubtitles);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
                item->setData(Qt::UserRole, sopt.langCode);
            }
        }
    }
}

/**
 * @brief Restituisce le opzioni selezionate dall'utente.
 * @return Struttura DownloadOptions.
 */
DownloadOptions MediaAnalyzerDialog::getSelectedOptions() const
{
    return m_resultOptions;
}

/**
 * @brief Slot per selezionare tutte le tracce audio reali (ignorando i separatori).
 */
void MediaAnalyzerDialog::onSelectAllAudioClicked()
{
    for (int i = 0; i < ui->listAudioTracks->count(); ++i) {
        QListWidgetItem* item = ui->listAudioTracks->item(i);
        QString fmtId = item->data(Qt::UserRole).toString();
        if (!fmtId.isEmpty() && (item->flags() & Qt::ItemIsUserCheckable)) {
            item->setCheckState(Qt::Checked);
        }
    }
}

/**
 * @brief Slot per deselezionare tutte le tracce audio reali (ignorando i separatori).
 */
void MediaAnalyzerDialog::onDeselectAllAudioClicked()
{
    for (int i = 0; i < ui->listAudioTracks->count(); ++i) {
        QListWidgetItem* item = ui->listAudioTracks->item(i);
        QString fmtId = item->data(Qt::UserRole).toString();
        if (!fmtId.isEmpty() && (item->flags() & Qt::ItemIsUserCheckable)) {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

/**
 * @brief Slot per selezionare tutti i sottotitoli reali (ignorando i separatori).
 */
void MediaAnalyzerDialog::onSelectAllSubsClicked()
{
    for (int i = 0; i < ui->listSubtitles->count(); ++i) {
        QListWidgetItem* item = ui->listSubtitles->item(i);
        QString langCode = item->data(Qt::UserRole).toString();
        if (!langCode.isEmpty() && (item->flags() & Qt::ItemIsUserCheckable)) {
            item->setCheckState(Qt::Checked);
        }
    }
}

/**
 * @brief Slot per deselezionare tutti i sottotitoli reali (ignorando i separatori).
 */
void MediaAnalyzerDialog::onDeselectAllSubsClicked()
{
    for (int i = 0; i < ui->listSubtitles->count(); ++i) {
        QListWidgetItem* item = ui->listSubtitles->item(i);
        QString langCode = item->data(Qt::UserRole).toString();
        if (!langCode.isEmpty() && (item->flags() & Qt::ItemIsUserCheckable)) {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

/**
 * @brief Slot azionato al pulsante 'Conferma Selezione': salva le scelte con le informazioni di lingua per il tagging in FFmpeg.
 */
void MediaAnalyzerDialog::onConfirmClicked()
{
    m_resultOptions.url = m_metadata.url;
    m_resultOptions.selectedVideoFormatId = ui->comboResolution->currentData().toString();
    m_resultOptions.containerFormat = ui->comboContainer->currentData().toString();

    // Raccoglie le traccia audio spuntate includendo i dati di lingua per i tag dei metadati
    m_resultOptions.selectedAudioFormatIds.clear();
    m_resultOptions.selectedAudioTracks.clear();

    for (int i = 0; i < ui->listAudioTracks->count(); ++i) {
        QListWidgetItem* item = ui->listAudioTracks->item(i);
        QString fmtId = item->data(Qt::UserRole).toString();
        if (!fmtId.isEmpty() && item->checkState() == Qt::Checked) {
            m_resultOptions.selectedAudioFormatIds.append(fmtId);

            AudioSelectionInfo info;
            info.formatId = fmtId;
            info.langCode = item->data(Qt::UserRole + 1).toString();
            info.langDisplay = item->data(Qt::UserRole + 2).toString();
            m_resultOptions.selectedAudioTracks.append(info);
        }
    }

    // Raccoglie i sottotitoli spuntati
    m_resultOptions.selectedSubtitleLangs.clear();
    for (int i = 0; i < ui->listSubtitles->count(); ++i) {
        QListWidgetItem* item = ui->listSubtitles->item(i);
        QString langCode = item->data(Qt::UserRole).toString();
        if (!langCode.isEmpty() && item->checkState() == Qt::Checked) {
            m_resultOptions.selectedSubtitleLangs.append(langCode);
        }
    }

    accept();
}

/**
 * @brief Slot azionato al pulsante 'Annulla'.
 */
void MediaAnalyzerDialog::onCancelClicked()
{
    reject();
}
