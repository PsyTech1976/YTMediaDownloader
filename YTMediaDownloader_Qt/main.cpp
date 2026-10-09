/**
 * @file main.cpp
 * @brief Punto di ingresso principale dell'applicazione YTMediaDownloader.
 * 
 * Questo file contiene la funzione main() che inizializza l'istanza di QApplication,
 * applica le preferenze di localizzazione salvate dall'utente ed avvia la finestra
 * principale MainWindow.
 */

#include <QApplication>
#include <QLockFile>
#include <QDir>
#include <QMessageBox>
#include "MainWindow/MainWindow.h"
#include "CoreApp/Preferenze/SettingsManager.h"
#include "CoreApp/Localization/LocalizationManager.h"
#include <QStyleFactory>
#include <QFile>
#include <QPalette>

/**
 * @brief Funzione di ingresso principale dell'applicazione.
 * @param argc Numero di argomenti da riga di comando.
 * @param argv Vettore delle stringhe di argomento.
 * @return Codice di uscita dell'applicazione Qt.
 */
int main(int argc, char *argv[])
{
    // Inizializza l'applicazione GUI Qt
    QApplication a(argc, argv);
    a.setApplicationName("YTMediaDownloader");
    a.setOrganizationName("YTMediaDownloaderOrg");

    // Imposta lo stile multipiattaforma Fusion per garantire coerenza visiva su tutti i sistemi
    a.setStyle(QStyleFactory::create("Fusion"));

    // Configura una palette moderna e luminosa coerente con il design di sistema
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(248, 249, 250));
    pal.setColor(QPalette::WindowText, QColor(33, 37, 41));
    pal.setColor(QPalette::Base, QColor(255, 255, 255));
    pal.setColor(QPalette::AlternateBase, QColor(241, 243, 245));
    pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    pal.setColor(QPalette::ToolTipText, QColor(33, 37, 41));
    pal.setColor(QPalette::Text, QColor(33, 37, 41));
    pal.setColor(QPalette::Button, QColor(248, 249, 250));
    pal.setColor(QPalette::ButtonText, QColor(33, 37, 41));
    pal.setColor(QPalette::Highlight, QColor(13, 110, 253));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::Link, QColor(13, 110, 253));
    a.setPalette(pal);

    // Applica il foglio di stile unificato incorporato nelle risorse
    QFile themeFile(":/theme.qss");
    if (themeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        a.setStyleSheet(QString::fromUtf8(themeFile.readAll()));
        themeFile.close();
    }

    // Inizializza ed applica la lingua preferita memorizzata nelle impostazioni
    QString prefLang = SettingsManager::instance().preferredLanguage();
    LocalizationManager::instance().loadLanguage(prefLang);

    // Controllo istanza singola: impedisce l'avvio simultaneo di più istanze
    QString lockPath = QDir::tempPath() + "/YTMediaDownloader_app_instance.lock";
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(30000); // Riconosce come stale i lock rimasti da processi terminati in modo anomalo
    if (!lockFile.tryLock(100)) {
        QMessageBox::warning(nullptr,
            LOC("MainWindow", "already_running_title", "Applicazione già attiva"),
            LOC("MainWindow", "already_running_msg", "Un'altra istanza di YTMediaDownloader è già in esecuzione nel sistema. Non è consentito eseguire più istanze contemporaneamente."));
        return 0;
    }

    // Istanzia e mostra la finestra principale
    MainWindow w;
    w.show();

    // Avvia il ciclo di eventi Qt
    return a.exec();
}
