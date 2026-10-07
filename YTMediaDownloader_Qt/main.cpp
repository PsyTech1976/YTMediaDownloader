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
