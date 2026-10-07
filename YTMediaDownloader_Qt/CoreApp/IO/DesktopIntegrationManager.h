/**
 * @file DesktopIntegrationManager.h
 * @brief Gestore dell'integrazione del programma a livello di sistema operativo (XDG Desktop Entry).
 * 
 * Consente di abilitare o disabilitare dinamicamente la visibilità dell'applicazione nei menu di sistema,
 * nella ricerca delle applicazioni (GNOME Shell, KDE Plasma, XFCE, App Launcher) e nell'ambiente desktop,
 * gestendo la creazione e rimozione del file .desktop in ~/.local/share/applications/.
 */

#ifndef DESKTOPINTEGRATIONMANAGER_H
#define DESKTOPINTEGRATIONMANAGER_H

#include <QString>

class DesktopIntegrationManager {
public:
    /**
     * @brief Verifica se l'integrazione a livello di sistema operativo è attualmente attiva.
     * @return True se il file .desktop esiste in ~/.local/share/applications/, altrimenti False.
     */
    static bool isIntegrated();

    /**
     * @brief Abilita o disabilita la visibilità dell'applicazione nel menu di sistema.
     * @param enable True per installare il file .desktop e l'icona, False per rimuoverli.
     * @return True se l'operazione ha avuto successo, altrimenti False.
     */
    static bool setIntegrated(bool enable);

    /**
     * @brief Inverte lo stato di integrazione (attiva se disattivata, rimuove se attiva).
     * @return Il nuovo stato dell'integrazione (True = integrata, False = non integrata).
     */
    static bool toggleIntegration();

    /**
     * @brief Restituisce il percorso completo del file .desktop dell'applicazione.
     * @return Percorso assoluto del file desktop.
     */
    static QString desktopFilePath();

    /**
     * @brief Restituisce il percorso completo dell'icona installata per l'integrazione di sistema.
     * @return Percorso assoluto dell'icona.
     */
    static QString iconFilePath();

private:
    static void refreshDesktopDatabase();
};

#endif // DESKTOPINTEGRATIONMANAGER_H
