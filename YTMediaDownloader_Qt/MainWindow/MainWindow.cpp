/**
 * @file MainWindow.cpp
 * @brief Implementazione della classe controller per la finestra principale MainWindow.
 * 
 * Gestisce l'interfaccia utente con barra dei menu principale (QMenuBar),
 * integrazione e visibilità a livello di sistema operativo (XDG Desktop Entry),
 * tracciamento a fasi della procedura, gestione della barra di avanzamento,
 * e supporto multilingua sincronizzato con bandiere e guida HTML.
 */

#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "MediaAnalyzerDialog/MediaAnalyzerDialog.h"
#include "SettingsDialog/SettingsDialog.h"
#include "HelpDialog/HelpDialog.h"
#include "AboutDialog/AboutDialog.h"
#include "CoreApp/Localization/LocalizationManager.h"
#include "CoreApp/Preferenze/SettingsManager.h"
#include "CoreApp/IO/FileManager.h"
#include "CoreApp/IO/DesktopIntegrationManager.h"
#include "CoreApp/DevLog/DevLogLogger.h"
#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QTimer>
#include <QIcon>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>

/**
 * @brief Costruttore della classe MainWindow.
 * @param parent Widget padre facoltativo.
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_workerThread(new QThread(this))
    , m_ffmpegManager(new FFmpegManager())
    , m_langMenu(new QMenu(this))
    , m_langActionGroup(new QActionGroup(this))
    , m_helpDialog(nullptr)
    , m_aboutDialog(nullptr)
    , m_menuFile(nullptr)
    , m_menuOptions(nullptr)
    , m_menuLanguage(nullptr)
    , m_menuHelp(nullptr)
    , m_actPasteAndAnalyze(nullptr)
    , m_actDownload(nullptr)
    , m_actCancelProcess(nullptr)
    , m_actOpenFile(nullptr)
    , m_actOpenFolder(nullptr)
    , m_actExit(nullptr)
    , m_actEditOptions(nullptr)
    , m_actToggleIntegration(nullptr)
    , m_actSettings(nullptr)
    , m_menuBarLangGroup(new QActionGroup(this))
    , m_actHelp(nullptr)
    , m_actAbout(nullptr)
    , m_isAnalyzed(false)
    , m_isProcessing(false)
{
    ui->setupUi(this);

    // Registrazione dei metatipi Qt per la comunicazione cross-thread
    qRegisterMetaType<MediaMetadata>("MediaMetadata");
    qRegisterMetaType<DownloadOptions>("DownloadOptions");

    // Sposta FFmpegManager sul thread worker dedicato
    m_ffmpegManager->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_ffmpegManager, &QObject::deleteLater);

    // Configurazione dimensioni della finestra
    setFixedHeight(sizeHint().height());
    setMinimumWidth(720);

    // Assegnazione dell'icona personalizzata dell'applicazione
    QIcon appIcon(":/icons/icon.png");
    if (appIcon.isNull()) {
        appIcon = QIcon(QCoreApplication::applicationDirPath() + "/Resources/icons/icon.png");
    }
    setWindowIcon(appIcon);
    QApplication::setWindowIcon(appIcon);

    // Configurazione del menu popup rapido di selezione lingua nella testata
    m_langActionGroup->setExclusive(true);
    struct LangItem { QString label; QString code; };
    const QList<LangItem> languages = {
        {"Italiano", "it_IT"},
        {"English", "en_EN"},
        {"Français", "fr_FR"},
        {"Deutsch", "de_DE"},
        {"Español", "es_ES"}
    };

    for (const auto& item : languages) {
        QAction *action = m_langMenu->addAction(LocalizationManager::languageIcon(item.code), item.label);
        action->setCheckable(true);
        action->setData(item.code);
        m_langActionGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, item]() {
            onLanguageActionTriggered(item.code);
        });
    }
    ui->btnLanguage->setMenu(m_langMenu);

    // Configurazione della barra dei menu principale dell'applicazione (QMenuBar)
    setupMenuBar();

    // Connessione dei segnali dei pulsanti della GUI
    connect(ui->btnToggleIntegration, &QPushButton::clicked, this, &MainWindow::onToggleDesktopIntegrationClicked);
    connect(ui->btnPasteAndAnalyze, &QPushButton::clicked, this, &MainWindow::onPasteAndAnalyzeClicked);
    connect(ui->btnCancelProcess, &QPushButton::clicked, this, &MainWindow::onCancelProcessClicked);
    connect(ui->btnEditOptions, &QPushButton::clicked, this, &MainWindow::onEditOptionsClicked);
    connect(ui->btnSettings, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(ui->btnHelp, &QPushButton::clicked, this, &MainWindow::onHelpClicked);
    connect(ui->btnDownload, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);
    connect(ui->btnOpenFile, &QPushButton::clicked, this, &MainWindow::onOpenFileClicked);
    connect(ui->btnOpenFolder, &QPushButton::clicked, this, &MainWindow::onOpenFolderClicked);

    // Monitora le modifiche nel campo di testo dell'URL per azzerare i parametri analizzati
    connect(ui->txtUrl, &QLineEdit::textChanged, this, &MainWindow::onUrlTextChanged);

    // Connessione delle richieste cross-thread da MainWindow a FFmpegManager
    connect(this, &MainWindow::requestAnalyzeUrl, m_ffmpegManager, &FFmpegManager::analyzeUrl, Qt::QueuedConnection);
    connect(this, &MainWindow::requestStartDownload, m_ffmpegManager, &FFmpegManager::startDownload, Qt::QueuedConnection);
    connect(this, &MainWindow::requestCancel, m_ffmpegManager, &FFmpegManager::cancelCurrentTask, Qt::QueuedConnection);

    // Connessione dei segnali del gestore asincrono FFmpegManager verso il thread UI
    connect(m_ffmpegManager, &FFmpegManager::analysisCompleted, this, &MainWindow::onAnalysisCompleted, Qt::QueuedConnection);
    connect(m_ffmpegManager, &FFmpegManager::progressUpdated, this, &MainWindow::onProgressUpdated, Qt::QueuedConnection);
    connect(m_ffmpegManager, &FFmpegManager::downloadCompleted, this, &MainWindow::onDownloadCompleted, Qt::QueuedConnection);

    // Avvia l'event loop del worker thread
    m_workerThread->start();

    // Carica l'ultimo file scaricato se presente e ancora esistente sul disco
    m_lastDownloadedFilePath = SettingsManager::instance().lastDownloadedFilePath();
    bool hasValidLastFile = !m_lastDownloadedFilePath.isEmpty() && QFileInfo::exists(m_lastDownloadedFilePath);
    ui->btnOpenFile->setEnabled(hasValidLastFile);
    if (m_actOpenFile) m_actOpenFile->setEnabled(hasValidLastFile);
    ui->btnOpenFolder->setEnabled(true);
    if (m_actOpenFolder) m_actOpenFolder->setEnabled(true);

    // Aggiorna stato visibilità integrazione sistema
    updateIntegrationButtonState();

    // Traduzione dei testi dell'interfaccia utente in base alle impostazioni correnti
    retranslateUi();

    // Posiziona immediatamente il cursore nel campo di immissione del link di YouTube
    ui->txtUrl->setFocus();

    DevLogLogger::instance().logAction("AVVIO", "MainWindow inizializzata con MenuBar, Desktop Integration e FFmpeg worker thread.");
}

/**
 * @brief Distruttore della classe MainWindow.
 */
MainWindow::~MainWindow()
{
    if (m_ffmpegManager) {
        m_ffmpegManager->killActiveProcesses();
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    delete ui;
}

/**
 * @brief Inizializza la barra dei menu con comandi di accesso rapido e scorciatoie da tastiera.
 */
void MainWindow::setupMenuBar()
{
    QMenuBar *bar = menuBar();

    // Menu File
    m_menuFile = bar->addMenu("");
    m_actPasteAndAnalyze = m_menuFile->addAction("", this, &MainWindow::onPasteAndAnalyzeClicked, QKeySequence("Ctrl+V"));
    m_actDownload = m_menuFile->addAction("", this, &MainWindow::onDownloadClicked, QKeySequence("Ctrl+D"));
    m_actDownload->setEnabled(false);
    m_actCancelProcess = m_menuFile->addAction("", this, &MainWindow::onCancelProcessClicked, QKeySequence("Escape"));
    m_actCancelProcess->setEnabled(false);
    m_menuFile->addSeparator();
    m_actOpenFile = m_menuFile->addAction("", this, &MainWindow::onOpenFileClicked, QKeySequence("Ctrl+O"));
    m_actOpenFile->setEnabled(false);
    m_actOpenFolder = m_menuFile->addAction("", this, &MainWindow::onOpenFolderClicked, QKeySequence("Ctrl+F"));
    m_menuFile->addSeparator();
    m_actExit = m_menuFile->addAction("", this, &QWidget::close, QKeySequence("Ctrl+Q"));

    // Menu Opzioni
    m_menuOptions = bar->addMenu("");
    m_actEditOptions = m_menuOptions->addAction("", this, &MainWindow::onEditOptionsClicked, QKeySequence("Ctrl+E"));
    m_actEditOptions->setEnabled(false);
    m_menuOptions->addSeparator();
    m_actToggleIntegration = m_menuOptions->addAction("", this, &MainWindow::onToggleDesktopIntegrationClicked);
    m_actToggleIntegration->setCheckable(true);
    m_menuOptions->addSeparator();
    m_actSettings = m_menuOptions->addAction("", this, &MainWindow::onSettingsClicked, QKeySequence("Ctrl+,"));

    // Menu Lingua (con bandiere e selezione esclusiva)
    m_menuLanguage = bar->addMenu("");
    m_menuBarLangGroup->setExclusive(true);
    struct LangItem { QString label; QString code; };
    const QList<LangItem> languages = {
        {"Italiano", "it_IT"},
        {"English", "en_EN"},
        {"Français", "fr_FR"},
        {"Deutsch", "de_DE"},
        {"Español", "es_ES"}
    };

    for (const auto& item : languages) {
        QAction *action = m_menuLanguage->addAction(LocalizationManager::languageIcon(item.code), item.label);
        action->setCheckable(true);
        action->setData(item.code);
        m_menuBarLangGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, item]() {
            onLanguageActionTriggered(item.code);
        });
    }

    // Menu Aiuto
    m_menuHelp = bar->addMenu("");
    m_actHelp = m_menuHelp->addAction("", this, &MainWindow::onHelpClicked, QKeySequence("F1"));
    m_menuHelp->addSeparator();
    m_actAbout = m_menuHelp->addAction("", this, &MainWindow::onAboutClicked, QKeySequence("Ctrl+I"));
}

/**
 * @brief Intercetta la chiusura della finestra per terminare processi e thread pulitamente.
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isProcessing && m_ffmpegManager) {
        m_ffmpegManager->killActiveProcesses();
        emit requestCancel();
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    event->accept();
}

/**
 * @brief Intercetta l'evento di visualizzazione della finestra per posizionare il cursore nel campo URL.
 */
void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    activateWindow();
    ui->txtUrl->setFocus(Qt::OtherFocusReason);
    QTimer::singleShot(0, this, [this]() {
        activateWindow();
        ui->txtUrl->setFocus(Qt::OtherFocusReason);
        ui->txtUrl->selectAll();
    });
}

/**
 * @brief Slot azionato dal clic sul pulsante o menu di integrazione nel sistema operativo.
 */
void MainWindow::onToggleDesktopIntegrationClicked()
{
    bool newState = DesktopIntegrationManager::toggleIntegration();
    updateIntegrationButtonState();

    if (newState) {
        ui->lblStatus->setText(LOC("MainWindow", "status_sys_integrated", "Applicazione integrata nel menu di sistema con successo!"));
        QMessageBox::information(this,
            LOC("MainWindow", "btn_sys_integration_on", "Nel Menu Sistema"),
            LOC("MainWindow", "status_sys_integrated", "Applicazione integrata nel menu di sistema con successo! Ora è visibile nella ricerca delle applicazioni del desktop."));
    } else {
        ui->lblStatus->setText(LOC("MainWindow", "status_sys_unintegrated", "Applicazione rimossa dal menu di sistema."));
        QMessageBox::information(this,
            LOC("MainWindow", "btn_sys_integration_off", "Integra nel Sistema"),
            LOC("MainWindow", "status_sys_unintegrated", "Applicazione rimossa dal menu di sistema."));
    }
}

/**
 * @brief Aggiorna l'aspetto visivo del pulsante e dell'azione di integrazione desktop.
 */
void MainWindow::updateIntegrationButtonState()
{
    bool integrated = DesktopIntegrationManager::isIntegrated();
    if (m_actToggleIntegration) {
        m_actToggleIntegration->setChecked(integrated);
    }

    if (integrated) {
        ui->btnToggleIntegration->setText("✔ " + LOC("MainWindow", "btn_sys_integration_on", "Nel Menu Sistema"));
        ui->btnToggleIntegration->setToolTip(LOC("MainWindow", "tip_sys_integration_on", "L'applicazione è visibile nei menu e nella ricerca del sistema operativo. Clicca per disabilitarla."));
        ui->btnToggleIntegration->setStyleSheet("QPushButton { font-weight: bold; color: #198754; background-color: #e8f5e9; border: 1px solid #a3cfbb; border-radius: 4px; padding: 4px 8px; }"
                                                "QPushButton:hover { background-color: #d1e7dd; }");
    } else {
        ui->btnToggleIntegration->setText("➕ " + LOC("MainWindow", "btn_sys_integration_off", "Integra nel Sistema"));
        ui->btnToggleIntegration->setToolTip(LOC("MainWindow", "tip_sys_integration_off", "Rende l'applicazione visibile nel menu applicazioni e nella ricerca di sistema del computer."));
        ui->btnToggleIntegration->setStyleSheet("QPushButton { font-weight: normal; color: #495057; background-color: #f8f9fa; border: 1px solid #ced4da; border-radius: 4px; padding: 4px 8px; }"
                                                "QPushButton:hover { background-color: #e9ecef; }");
    }
}

/**
 * @brief Slot azionato dal menu Aiuto per visualizzare la finestra Informazioni sul Software.
 */
void MainWindow::onAboutClicked()
{
    AboutDialog dialog(this);
    dialog.exec();
}

/**
 * @brief Slot attivato al cambio lingua dal menu popup della finestra principale.
 * @param langCode Codice completo della lingua (es. "it_IT", "en_EN", ecc.).
 */
void MainWindow::onLanguageActionTriggered(const QString& langCode)
{
    SettingsManager::instance().setPreferredLanguage(langCode);
    LocalizationManager::instance().loadLanguage(langCode);
    retranslateUi();
    DevLogLogger::instance().logAction("LINGUA", QString("Lingua modificata dall'utente in: %1").arg(langCode));
}

/**
 * @brief Slot per sincronizzare la lingua del programma quando viene modificata dalla finestra Guida.
 * @param langCode Codice completo della lingua.
 */
void MainWindow::onLanguageChangedFromHelp(const QString& langCode)
{
    QString curLang = LocalizationManager::instance().currentLanguage();
    if (curLang.startsWith(langCode.left(2), Qt::CaseInsensitive)) {
        return;
    }
    onLanguageActionTriggered(langCode);
}

/**
 * @brief Aggiorna dinamicamente tutti i testi dell'interfaccia utente in base alla lingua attiva.
 */
void MainWindow::retranslateUi()
{
    setWindowTitle(LOC("MainWindow", "window_title", "YT Media Downloader Pro"));
    ui->btnSettings->setText(LOC("MainWindow", "btn_settings", "Impostazioni"));
    ui->btnHelp->setText(LOC("MainWindow", "btn_help", "Guida Utente"));
    ui->grpUrl->setTitle(LOC("MainWindow", "grp_url", "Link Video YouTube"));
    ui->lblUrl->setText(LOC("MainWindow", "lbl_url", "Link Video YouTube:"));
    ui->txtUrl->setPlaceholderText(LOC("MainWindow", "txt_url_placeholder", "Incolla qui il link di YouTube (es. https://www.youtube.com/watch?v=...)"));
    ui->btnPasteAndAnalyze->setText(LOC("MainWindow", "btn_paste_analyze", "Incolla e Analizza Link"));
    ui->btnCancelProcess->setText(LOC("MainWindow", "btn_cancel_process", "Blocca Processo"));
    ui->grpSummary->setTitle(LOC("MainWindow", "grp_summary", "Opzioni Selezionate"));
    ui->btnEditOptions->setText(LOC("MainWindow", "btn_edit_options", "Modifica Opzioni..."));
    ui->grpProgress->setTitle(LOC("MainWindow", "grp_progress", "Stato Avanzamento Processo"));
    ui->lblStepTag->setText(LOC("MainWindow", "lbl_step", "Passaggio:"));
    ui->lblEtaTag->setText(LOC("MainWindow", "lbl_eta", "Tempo Rimanente:"));
    ui->btnOpenFile->setText(LOC("MainWindow", "btn_open_file", "Apri File Scaricato"));
    ui->btnOpenFolder->setText(LOC("MainWindow", "btn_open_folder", "Mostra nella Cartella"));
    ui->btnDownload->setText(m_isProcessing ? LOC("MainWindow", "btn_stop_download", "Blocca Download") : LOC("MainWindow", "btn_download", "Avvia Download"));
    ui->lblStatus->setText(m_isProcessing ? LOC("MainWindow", "status_downloading", "Download e lavorazione in corso...") : LOC("MainWindow", "status_idle", "Pronto. Inserisci un URL di YouTube per iniziare."));

    // Aggiorna testi dei menu principali
    if (m_menuFile) m_menuFile->setTitle(LOC("MainWindow", "menu_file", "&File"));
    if (m_menuOptions) m_menuOptions->setTitle(LOC("MainWindow", "menu_options", "&Opzioni"));
    if (m_menuLanguage) m_menuLanguage->setTitle(LOC("MainWindow", "menu_language", "&Lingua"));
    if (m_menuHelp) m_menuHelp->setTitle(LOC("MainWindow", "menu_help", "&Aiuto"));

    // Aggiorna azioni del menu File
    if (m_actPasteAndAnalyze) m_actPasteAndAnalyze->setText(LOC("MainWindow", "act_paste_analyze", "Incolla e Analizza Link"));
    if (m_actDownload) m_actDownload->setText(m_isProcessing ? LOC("MainWindow", "btn_stop_download", "Blocca Download") : LOC("MainWindow", "act_download", "Avvia Download"));
    if (m_actCancelProcess) m_actCancelProcess->setText(LOC("MainWindow", "act_cancel_process", "Blocca Processo"));
    if (m_actOpenFile) m_actOpenFile->setText(LOC("MainWindow", "act_open_file", "Apri File Scaricato"));
    if (m_actOpenFolder) m_actOpenFolder->setText(LOC("MainWindow", "act_open_folder", "Mostra nella Cartella"));
    if (m_actExit) m_actExit->setText(LOC("MainWindow", "act_exit", "Esci"));

    // Aggiorna azioni del menu Opzioni
    if (m_actEditOptions) m_actEditOptions->setText(LOC("MainWindow", "act_edit_options", "Modifica Opzioni Video/Audio..."));
    if (m_actToggleIntegration) m_actToggleIntegration->setText(LOC("MainWindow", "act_toggle_integration", "Visibilità nei Menu di Sistema"));
    if (m_actSettings) m_actSettings->setText(LOC("MainWindow", "act_settings", "Impostazioni..."));

    // Aggiorna azioni del menu Aiuto
    if (m_actHelp) m_actHelp->setText(LOC("MainWindow", "act_help", "Guida Utente"));
    if (m_actAbout) m_actAbout->setText(LOC("MainWindow", "act_about", "Informazioni sul Software..."));

    // Aggiorna pulsante e stato integrazione desktop
    updateIntegrationButtonState();

    // Aggiorna etichetta, icona bandiera e spunta del pulsante menu lingua
    QString curLang = LocalizationManager::instance().currentLanguage();
    ui->btnLanguage->setIcon(LocalizationManager::languageIcon(curLang));
    ui->btnLanguage->setIconSize(QSize(22, 15));
    ui->btnLanguage->setText(LocalizationManager::languageCleanName(curLang) + " ▼");
    ui->btnLanguage->setToolTip(LOC("MainWindow", "tip_language", "Seleziona la lingua dell'interfaccia"));

    // Sincronizza spunta nei menu lingua (popup e barra dei menu)
    for (QAction *action : m_langMenu->actions()) {
        if (action->data().toString().startsWith(curLang.left(2), Qt::CaseInsensitive)) {
            action->setChecked(true);
        }
    }
    for (QAction *action : m_menuBarLangGroup->actions()) {
        if (action->data().toString().startsWith(curLang.left(2), Qt::CaseInsensitive)) {
            action->setChecked(true);
        }
    }

    if (m_isAnalyzed) {
        updateSummaryText();
    } else {
        ui->txtOptionsSummary->setPlainText(LOC("MainWindow", "status_idle_summary", "Nessun video analizzato. Inserisci un link e premi \"Incolla e Analizza Link\"."));
    }

    // Se la guida utente è aperta, sincronizza istantaneamente anche la sua lingua
    if (m_helpDialog) {
        m_helpDialog->loadLanguage(curLang.left(2).toLower());
        m_helpDialog->retranslateUi();
    }
}

/**
 * @brief Gestisce lo stato abilitato/disabilitato dei pulsanti di controllo durante o fuori dalle elaborazioni.
 * @param processing Se True, disabilita i pulsanti dell'interfaccia ed abilita 'Blocca Processo'.
 */
void MainWindow::setProcessingState(bool processing)
{
    m_isProcessing = processing;

    ui->btnPasteAndAnalyze->setEnabled(!processing);
    ui->btnSettings->setEnabled(!processing);
    ui->btnHelp->setEnabled(!processing);
    ui->btnLanguage->setEnabled(!processing);
    ui->btnCancelProcess->setEnabled(processing);

    if (m_actPasteAndAnalyze) m_actPasteAndAnalyze->setEnabled(!processing);
    if (m_actCancelProcess) m_actCancelProcess->setEnabled(processing);
    if (m_actSettings) m_actSettings->setEnabled(!processing);

    if (processing) {
        ui->btnEditOptions->setEnabled(false);
        if (m_actEditOptions) m_actEditOptions->setEnabled(false);

        // Il pulsante Download resta abilitato e diventa "Blocca Download"
        ui->btnDownload->setEnabled(true);
        ui->btnDownload->setText(LOC("MainWindow", "btn_stop_download", "Blocca Download"));
        if (m_actDownload) {
            m_actDownload->setEnabled(true);
            m_actDownload->setText(LOC("MainWindow", "btn_stop_download", "Blocca Download"));
        }

        ui->btnOpenFile->setEnabled(false);
        if (m_actOpenFile) m_actOpenFile->setEnabled(false);

        // Il pulsante Mostra nella Cartella resta sempre attivo
        ui->btnOpenFolder->setEnabled(true);
        if (m_actOpenFolder) m_actOpenFolder->setEnabled(true);
    } else {
        ui->progressBar->setRange(0, 100);
        ui->btnEditOptions->setEnabled(m_isAnalyzed);
        if (m_actEditOptions) m_actEditOptions->setEnabled(m_isAnalyzed);

        ui->btnDownload->setEnabled(m_isAnalyzed);
        if (m_actDownload) {
            m_actDownload->setEnabled(m_isAnalyzed);
            m_actDownload->setText(LOC("MainWindow", "btn_download", "Avvia Download"));
        }

        // Ripristina il testo originale del pulsante Download
        ui->btnDownload->setText(LOC("MainWindow", "btn_download", "Avvia Download"));
        bool hasDownloadedFile = !m_lastDownloadedFilePath.isEmpty() && QFileInfo::exists(m_lastDownloadedFilePath);
        ui->btnOpenFile->setEnabled(hasDownloadedFile);
        if (m_actOpenFile) m_actOpenFile->setEnabled(hasDownloadedFile);

        ui->btnOpenFolder->setEnabled(true); // Sempre attivo
        if (m_actOpenFolder) m_actOpenFolder->setEnabled(true);
    }
}

/**
 * @brief Azzera le informazioni analizzate ed i parametri in memoria se l'utente modifica l'URL del video.
 */
void MainWindow::resetAnalysisState()
{
    m_isAnalyzed = false;
    m_currentMetadata = MediaMetadata();
    m_selectedOptions = DownloadOptions();

    ui->btnEditOptions->setEnabled(false);
    ui->btnDownload->setEnabled(false);
    if (m_actEditOptions) m_actEditOptions->setEnabled(false);
    if (m_actDownload) m_actDownload->setEnabled(false);

    ui->txtOptionsSummary->setPlainText(LOC("MainWindow", "status_idle_summary", "Nessun video analizzato. Inserisci un link e premi \"Incolla e Analizza Link\"."));
    ui->lblStatus->setText(LOC("MainWindow", "status_idle", "Pronto. Inserisci un URL di YouTube per iniziare."));
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->lblStep->setText("Pronto.");
    ui->lblEta->setText("--:--");

    DevLogLogger::instance().logAction("RESET", "Resettate le opzioni ed i metadata a seguito della modifica del link video.");
}

/**
 * @brief Slot attivato quando il testo nel campo URL viene digitato o modificato.
 * @param text Nuovo contenuto dell'URL.
 */
void MainWindow::onUrlTextChanged(const QString& text)
{
    Q_UNUSED(text);
    if (m_isAnalyzed && !m_isProcessing) {
        resetAnalysisState();
    }
}

/**
 * @brief Slot per il pulsante unico 'Incolla e Analizza Link': incolla il link dagli appunti ed avvia l'analisi in un solo clic.
 */
void MainWindow::onPasteAndAnalyzeClicked()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    QString clipText = clipboard->text().trimmed();

    if (!clipText.isEmpty() && (clipText.contains("youtube.com") || clipText.contains("youtu.be") || clipText.startsWith("http"))) {
        ui->txtUrl->setText(clipText);
    }

    QString url = ui->txtUrl->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "Attenzione", "Inserisci o incolla un URL valido di YouTube prima di analizzare.");
        return;
    }

    QString toolError;
    if (!FFmpegManager::checkToolsAvailable(toolError)) {
        QMessageBox::critical(this, "Errore Strumenti", toolError);
        return;
    }

    setProcessingState(true);
    ui->progressBar->setRange(0, 0); // Animazione continua per tempo indeterminato durante l'analisi
    ui->lblStep->setText(LOC("MainWindow", "status_analyzing", "Fase 1 di 1: Analisi video in corso..."));
    ui->lblEta->setText("Indeterminato");
    ui->lblStatus->setText(LOC("MainWindow", "status_analyzing", "Analisi video in corso..."));

    emit requestAnalyzeUrl(url);
    DevLogLogger::instance().logAction("ANALISI", QString("Incollato ed avviata analisi per URL: %1").arg(url));
}

/**
 * @brief Slot per il pulsante 'Blocca Processo': interrompe la lavorazione e ripristina anticipatamente i pulsanti.
 */
void MainWindow::onCancelProcessClicked()
{
    if (!m_isProcessing) return;

    // Termina immediatamente tutti i processi figli e subprocessi attivi
    m_ffmpegManager->killActiveProcesses();
    // Invia segnale di cancellazione al worker thread per la rimozione file e reset stato
    emit requestCancel();

    setProcessingState(false);
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->lblStep->setText(LOC("MainWindow", "status_cancelled", "Processo annullato dall'utente."));
    ui->lblEta->setText("--:--");
    ui->lblStatus->setText(LOC("MainWindow", "status_cancelled", "Processo annullato dall'utente."));

    DevLogLogger::instance().logAction("ANNULLA", "Processo interrotto dall'utente tramite pulsante 'Blocca Processo'.");
}

/**
 * @brief Slot per il pulsante 'Modifica Opzioni...': riapre la finestra MediaAnalyzerDialog utilizzando i metadati memorizzati senza ri-analizzare.
 */
void MainWindow::onEditOptionsClicked()
{
    if (!m_isAnalyzed) return;

    MediaAnalyzerDialog dialog(m_currentMetadata, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_selectedOptions = dialog.getSelectedOptions();
        updateSummaryText();
        DevLogLogger::instance().logAction("OPZIONI", "Modificate le opzioni selezionate utilizzando i metadati in memoria.");
    }
}

/**
 * @brief Slot azionato al completamento dell'analisi media.
 * @param success Esito dell'analisi.
 * @param metadata Dettagli estratti da yt-dlp.
 * @param errorMessage Descrizione dell'eventuale errore.
 */
void MainWindow::onAnalysisCompleted(bool success, const MediaMetadata& metadata, const QString& errorMessage)
{
    setProcessingState(false);

    if (!success) {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(0);
        ui->lblStatus->setText(LOC("MainWindow", "status_error", "Errore:") + " " + errorMessage);
        QMessageBox::critical(this, "Errore Analisi", errorMessage);
        DevLogLogger::instance().logAction("ERRORE", QString("Analisi fallita: %1").arg(errorMessage));
        return;
    }

    m_currentMetadata = metadata;
    DevLogLogger::instance().logAction("ANALISI", QString("Analisi completata con successo: %1").arg(metadata.title));

    MediaAnalyzerDialog dialog(metadata, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_selectedOptions = dialog.getSelectedOptions();
        m_isAnalyzed = true;
        ui->btnEditOptions->setEnabled(true);
        ui->btnDownload->setEnabled(true);
        if (m_actEditOptions) m_actEditOptions->setEnabled(true);
        if (m_actDownload) m_actDownload->setEnabled(true);
        updateSummaryText();
        ui->lblStatus->setText(LOC("MainWindow", "status_ready", "Analisi completata. Scegli le opzioni e avvia il download."));
    } else {
        ui->lblStatus->setText(LOC("MainWindow", "status_idle", "Pronto. Inserisci un URL di YouTube per iniziare."));
    }
}

/**
 * @brief Aggiorna il testo del riepilogo delle opzioni scelte formattando in HTML con supporto per la barra di scorrimento.
 */
void MainWindow::updateSummaryText()
{
    QStringList audioDescs;
    for (const AudioSelectionInfo& info : m_selectedOptions.selectedAudioTracks) {
        audioDescs.append(info.langDisplay);
    }
    if (audioDescs.isEmpty()) audioDescs.append(LOC("MainWindow", "summary_default_audio", "Audio Predefinito"));

    QStringList subDescs;
    for (const QString& code : m_selectedOptions.selectedSubtitleLangs) {
        subDescs.append(FFmpegManager::getLanguageDisplayName(code));
    }
    if (subDescs.isEmpty()) subDescs.append(LOC("MainWindow", "summary_no_subtitles", "Nessun sottotitolo"));

    QString summaryHtml = QString("<b>%1:</b> %2<br/>"
                                  "<b>%3:</b> %4 &nbsp;|&nbsp; <b>%5:</b> %6<br/>"
                                  "<b>%7 (%8):</b> %9<br/>"
                                  "<b>%10 (%11):</b> %12")
                              .arg(LOC("MainWindow", "summary_title", "Titolo Video"))
                              .arg(m_currentMetadata.title.toHtmlEscaped())
                              .arg(LOC("MainWindow", "summary_resolution", "Risoluzione Selezionata"))
                              .arg(m_selectedOptions.selectedVideoFormatId.toHtmlEscaped())
                              .arg(LOC("MainWindow", "summary_container", "Contenitore"))
                              .arg(m_selectedOptions.containerFormat.toUpper().toHtmlEscaped())
                              .arg(LOC("MainWindow", "summary_audio_tracks", "Tracce Audio Selezionate"))
                              .arg(m_selectedOptions.selectedAudioTracks.count())
                              .arg(audioDescs.join(", "))
                              .arg(LOC("MainWindow", "summary_subtitles", "Sottotitoli Selezionati"))
                              .arg(m_selectedOptions.selectedSubtitleLangs.count())
                              .arg(subDescs.join(", "));

    ui->txtOptionsSummary->setHtml(summaryHtml);
}

/**
 * @brief Slot azionato dal pulsante 'Impostazioni': apre la finestra delle preferenze per configurare percorso e lingua.
 */
void MainWindow::onSettingsClicked()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        retranslateUi();
        DevLogLogger::instance().logAction("SETTINGS", "Aggiornate preferenze da finestra Impostazioni.");
    }
}

/**
 * @brief Slot azionato dal pulsante 'Avvia Download': avvia il download utilizzando il percorso di salvataggio dalle preferenze.
 */
void MainWindow::onDownloadClicked()
{
    // Se il download è già in corso, il pulsante funziona come "Blocca Download"
    if (m_isProcessing) {
        onCancelProcessClicked();
        return;
    }

    if (!m_isAnalyzed) {
        QMessageBox::warning(this, "Attenzione", "Effettua prima l'analisi del video.");
        return;
    }

    m_selectedOptions.saveDirectory = SettingsManager::instance().defaultSavePath();
    if (!FileManager::ensureDirectoryExists(m_selectedOptions.saveDirectory)) {
        QMessageBox::critical(this, "Errore Cartella", "Impossibile accedere o creare la cartella di destinazione impostata nelle preferenze.");
        return;
    }

    setProcessingState(true);
    ui->progressBar->setRange(0, 0);
    ui->lblStatus->setText(LOC("MainWindow", "status_downloading", "Download e lavorazione in corso..."));

    emit requestStartDownload(m_selectedOptions);
    DevLogLogger::instance().logAction("DOWNLOAD", QString("Avviato download nella cartella predefinita: %1").arg(m_selectedOptions.saveDirectory));
}

/**
 * @brief Slot per l'apertura diretta del file scaricato tramite il player predefinito del sistema operativo.
 */
void MainWindow::onOpenFileClicked()
{
    if (!m_lastDownloadedFilePath.isEmpty() && QFileInfo::exists(m_lastDownloadedFilePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastDownloadedFilePath));
        DevLogLogger::instance().logAction("APERTURA", QString("Aperto file scaricato: %1").arg(m_lastDownloadedFilePath));
    } else {
        QMessageBox::warning(this, "File non trovato", "Il file scaricato non è stato trovato o è stato rimosso.");
    }
}

/**
 * @brief Apre la posizione di download o il file scaricato nel file manager di sistema.
 * Metodo dedicato e richiamabile per semplificare la manutenzione futura:
 * - Se un file è stato scaricato ed esiste, lo evidenzia/seleziona nella cartella (Modalità 2).
 * - Se nessun file è stato scaricato, apre la cartella di destinazione mostrandone il contenuto (Modalità 1).
 */
void MainWindow::openDownloadLocation()
{
    // Modalità 2: Se è stato scaricato un file ed esiste sul filesystem, mostralo selezionandolo nella cartella
    if (!m_lastDownloadedFilePath.isEmpty() && QFileInfo::exists(m_lastDownloadedFilePath)) {
        bool ok = FileManager::showInFileManager(m_lastDownloadedFilePath);
        if (ok) {
            ui->lblStatus->setText(LOC("MainWindow", "status_file_selected", "File evidenziato nel gestore file.") + QString(" (%1)").arg(QFileInfo(m_lastDownloadedFilePath).fileName()));
            DevLogLogger::instance().logAction("FILE_MANAGER", QString("Selezionato file scaricato nella cartella: %1").arg(m_lastDownloadedFilePath));
        } else {
            ui->lblStatus->setText(LOC("MainWindow", "status_error", "Errore:") + " Impossibile aprire il file manager.");
            DevLogLogger::instance().logAction("ERRORE", QString("Impossibile evidenziare file: %1").arg(m_lastDownloadedFilePath));
        }
        return;
    }

    // Modalità 1: Se non è stato scaricato nessun file, apri la cartella di salvataggio mostrandone solo il contenuto
    QString targetFolder;
    if (!m_selectedOptions.saveDirectory.isEmpty() && QDir(m_selectedOptions.saveDirectory).exists()) {
        targetFolder = m_selectedOptions.saveDirectory;
    } else {
        targetFolder = SettingsManager::instance().defaultSavePath();
        if (targetFolder.isEmpty() || !QDir(targetFolder).exists()) {
            targetFolder = FileManager::defaultDownloadDirectory();
        }
    }

    FileManager::ensureDirectoryExists(targetFolder);
    bool ok = FileManager::openDirectory(targetFolder);
    if (ok) {
        ui->lblStatus->setText(LOC("MainWindow", "status_folder_opened", "Cartella aperta con successo.") + QString(" (%1)").arg(targetFolder));
        DevLogLogger::instance().logAction("FILE_MANAGER", QString("Aperta cartella di download: %1").arg(targetFolder));
    } else {
        ui->lblStatus->setText(LOC("MainWindow", "status_error", "Errore:") + " Impossibile aprire la cartella.");
        DevLogLogger::instance().logAction("ERRORE", QString("Impossibile aprire cartella: %1").arg(targetFolder));
    }
}

/**
 * @brief Slot per visualizzare la posizione del file o della cartella nel desktop.
 * Invoca il metodo dedicato openDownloadLocation().
 */
void MainWindow::onOpenFolderClicked()
{
    openDownloadLocation();
}

/**
 * @brief Slot per l'apertura della finestra separata della Guida Utente multilingua.
 */
void MainWindow::onHelpClicked()
{
    if (!m_helpDialog) {
        m_helpDialog = new HelpDialog(this);
        m_helpDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_helpDialog, &QObject::destroyed, this, [this]() {
            m_helpDialog = nullptr;
        });
        connect(m_helpDialog, &HelpDialog::languageChanged, this, &MainWindow::onLanguageChangedFromHelp);
    }

    // Assicura che la guida sia sincronizzata con la lingua corrente del programma
    QString appLang = LocalizationManager::instance().currentLanguage().left(2).toLower();
    m_helpDialog->loadLanguage(appLang);
    m_helpDialog->retranslateUi();
    m_helpDialog->show();
    m_helpDialog->raise();
    m_helpDialog->activateWindow();
    DevLogLogger::instance().logAction("GUIDA", "Aperta finestra Guida Utente agganciata alla lingua del programma.");
}

/**
 * @brief Slot per l'aggiornamento della barra di avanzamento e del tempo stimato (ETA).
 * Gestisce sia il valore percentuale che l'animazione grafica continua a tempo indeterminato (busy indicator).
 * @param percentage Percentuale completata (se negativa, attiva l'animazione continua a tempo indeterminato).
 * @param currentStep Descrizione della fase corrente (es. Fase 1 di 4).
 * @param timeRemaining Tempo rimanente o stringa 'Indeterminato'.
 */
void MainWindow::onProgressUpdated(double percentage, const QString& currentStep, const QString& timeRemaining)
{
    if (percentage < 0.0) {
        // Quando la percentuale non è calcolabile, attiva l'animazione grafica continua (marquee)
        ui->progressBar->setRange(0, 0);
        ui->lblEta->setText(LOC("MainWindow", "status_indeterminate", "Indeterminato"));
    } else {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(static_cast<int>(percentage));
        ui->lblEta->setText(timeRemaining);
    }
    ui->lblStep->setText(currentStep);
}

/**
 * @brief Slot azionato al termine dell'operazione di download e lavorazione.
 * @param success Esito del download.
 * @param finalFilePath Percorso assoluto del file salvato.
 * @param errorMessage Messaggio di errore in caso di fallimento.
 */
void MainWindow::onDownloadCompleted(bool success, const QString& finalFilePath, const QString& errorMessage)
{
    if (success) {
        m_lastDownloadedFilePath = finalFilePath;
        SettingsManager::instance().setLastDownloadedFilePath(finalFilePath);
    }

    setProcessingState(false);
    ui->progressBar->setRange(0, 100);

    if (success) {
        ui->btnOpenFile->setEnabled(true);
        if (m_actOpenFile) m_actOpenFile->setEnabled(true);

        ui->btnOpenFolder->setEnabled(true);
        if (m_actOpenFolder) m_actOpenFolder->setEnabled(true);

        ui->progressBar->setValue(100);
        ui->lblStep->setText(LOC("MainWindow", "status_completed", "Operazione completata con successo!"));
        ui->lblStatus->setText(LOC("MainWindow", "status_completed", "Operazione completata con successo!"));

        DevLogLogger::instance().logAction("DOWNLOAD", QString("Download completato con successo: %1").arg(finalFilePath));
    } else {
        ui->btnOpenFile->setEnabled(false);
        if (m_actOpenFile) m_actOpenFile->setEnabled(false);

        ui->btnOpenFolder->setEnabled(true);
        if (m_actOpenFolder) m_actOpenFolder->setEnabled(true);

        ui->progressBar->setValue(0);
        ui->lblStatus->setText(LOC("MainWindow", "status_error", "Errore:") + " " + errorMessage);
        QMessageBox::critical(this, "Errore Download", errorMessage);
        DevLogLogger::instance().logAction("ERRORE", QString("Download fallito: %1").arg(errorMessage));
    }
}
