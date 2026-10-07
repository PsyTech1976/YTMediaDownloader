/**
 * @file MainWindow.h
 * @brief Controller principale della finestra grafica di YT Media Downloader Pro.
 * 
 * Gestisce l'interfaccia utente principale, la barra dei menu di sistema (QMenuBar),
 * i pulsanti di azione rapida, l'integrazione a livello di sistema operativo (XDG Desktop Entry),
 * il worker thread asincrono per l'elaborazione FFmpeg/yt-dlp e la sincronizzazione multilingua.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QCloseEvent>
#include <QShowEvent>
#include "CoreApp/FFmpeg/FFmpegManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class HelpDialog;
class AboutDialog;
class QMenu;
class QAction;
class QActionGroup;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Costruttore della classe MainWindow.
     * @param parent Widget genitore facoltativo.
     */
    explicit MainWindow(QWidget *parent = nullptr);

    /**
     * @brief Distruttore della classe MainWindow.
     */
    ~MainWindow() override;

    /**
     * @brief Aggiorna tutti i testi tradotti dell'interfaccia (finestra, controlli e barra dei menu).
     */
    void retranslateUi();

signals:
    /**
     * @brief Segnale emesso per richiedere l'analisi di un URL al worker thread FFmpegManager.
     */
    void requestAnalyzeUrl(const QString& url);

    /**
     * @brief Segnale emesso per avviare il download con le opzioni selezionate al worker thread.
     */
    void requestStartDownload(const DownloadOptions& options);

    /**
     * @brief Segnale emesso per richiedere l'interruzione immediata del processo attivo.
     */
    void requestCancel();

protected:
    /**
     * @brief Intercetta la chiusura della finestra per terminare processi e thread in esecuzione.
     */
    void closeEvent(QCloseEvent *event) override;

    /**
     * @brief Apre la posizione di download o il file scaricato nel file manager di sistema.
     * Metodo dedicato e richiamabile per semplificare la manutenzione futura:
     * - Se un file è stato scaricato ed esiste, lo evidenzia/seleziona nella cartella (Modalità 2).
     * - Se nessun file è stato scaricato, apre la cartella di destinazione (Modalità 1).
     */
    void openDownloadLocation();

    /**
     * @brief Intercetta l'evento di visualizzazione della finestra per impostare il focus sul campo URL.
     */
    void showEvent(QShowEvent *event) override;

private slots:
    void onPasteAndAnalyzeClicked();
    void onCancelProcessClicked();
    void onEditOptionsClicked();
    void onSettingsClicked();
    void onDownloadClicked();
    void onOpenFileClicked();
    void onOpenFolderClicked();
    void onHelpClicked();
    void onAboutClicked();
    void onToggleDesktopIntegrationClicked();
    void onUrlTextChanged(const QString& text);
    void onLanguageActionTriggered(const QString& langCode);
    void onLanguageChangedFromHelp(const QString& langCode);

    void onAnalysisCompleted(bool success, const MediaMetadata& metadata, const QString& errorMessage);
    void onProgressUpdated(double percentage, const QString& currentStep, const QString& timeRemaining);
    void onDownloadCompleted(bool success, const QString& finalFilePath, const QString& errorMessage);

private:
    void setupMenuBar();
    void updateSummaryText();
    void setProcessingState(bool processing);
    void resetAnalysisState();
    void updateIntegrationButtonState();

    Ui::MainWindow *ui;
    QThread *m_workerThread;
    FFmpegManager *m_ffmpegManager;
    QMenu *m_langMenu;
    QActionGroup *m_langActionGroup;
    HelpDialog *m_helpDialog;
    AboutDialog *m_aboutDialog;

    // Elementi della barra dei menu (QMenuBar)
    QMenu *m_menuFile;
    QMenu *m_menuOptions;
    QMenu *m_menuLanguage;
    QMenu *m_menuHelp;

    QAction *m_actPasteAndAnalyze;
    QAction *m_actDownload;
    QAction *m_actCancelProcess;
    QAction *m_actOpenFile;
    QAction *m_actOpenFolder;
    QAction *m_actExit;

    QAction *m_actEditOptions;
    QAction *m_actToggleIntegration;
    QAction *m_actSettings;

    QActionGroup *m_menuBarLangGroup;

    QAction *m_actHelp;
    QAction *m_actAbout;

    MediaMetadata m_currentMetadata;
    DownloadOptions m_selectedOptions;
    QString m_lastDownloadedFilePath;
    bool m_isAnalyzed;
    bool m_isProcessing;
};

#endif // MAINWINDOW_H
