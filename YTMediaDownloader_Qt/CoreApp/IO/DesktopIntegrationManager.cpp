/**
 * @file DesktopIntegrationManager.cpp
 * @brief Implementazione del gestore dell'integrazione nel sistema operativo.
 * 
 * Gestisce la persistenza del launcher .desktop secondo le specifiche freedesktop.org XDG Desktop Entry,
 * l'estrazione dell'icona dell'applicazione e l'aggiornamento automatico del database delle applicazioni desktop.
 */

#include "DesktopIntegrationManager.h"
#include "CoreApp/DevLog/DevLogLogger.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QProcess>
#include <QPixmap>
#include <QTextStream>

/**
 * @brief Restituisce il percorso assoluto del file .desktop dell'applicazione.
 * Risolve la directory ~/.local/share/applications/ tramite le API standard di Qt.
 */
QString DesktopIntegrationManager::desktopFilePath()
{
    QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (appsDir.isEmpty()) {
        appsDir = QDir::homePath() + "/.local/share/applications";
    }
    return appsDir + "/YTMediaDownloader.desktop";
}

/**
 * @brief Restituisce il percorso assoluto dell'icona utilizzata per il menu di sistema.
 */
QString DesktopIntegrationManager::iconFilePath()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + "/.local/share";
    }
    return dataDir + "/icons/hicolor/256x256/apps/ytmediadownloader.png";
}

/**
 * @brief Verifica se il file .desktop è presente nel percorso delle applicazioni utente.
 * @return True se il file esiste, altrimenti False.
 */
bool DesktopIntegrationManager::isIntegrated()
{
    return QFile::exists(desktopFilePath());
}

/**
 * @brief Abilita o disabilita la visibilità del programma nel menu del sistema operativo.
 * @param enable Se True crea file desktop e icona; se False rimuove il file desktop.
 * @return True se l'operazione ha avuto successo, altrimenti False.
 */
bool DesktopIntegrationManager::setIntegrated(bool enable)
{
    QString dPath = desktopFilePath();

    if (!enable) {
        bool removed = true;
        if (QFile::exists(dPath)) {
            removed = QFile::remove(dPath);
        }
        // Rimuove anche eventuali varianti lowercase
        QString dPathLower = QFileInfo(dPath).absolutePath() + "/ytmediadownloader.desktop";
        if (QFile::exists(dPathLower)) {
            QFile::remove(dPathLower);
        }

        refreshDesktopDatabase();
        DevLogLogger::instance().logAction("INTEGRAZIONE_SISTEMA", "Integrazione nel menu di sistema disabilitata e file .desktop rimosso.");
        return removed;
    }

    // Creazione directory ~/.local/share/applications/ se non esistente
    QFileInfo dInfo(dPath);
    QDir appsDir = dInfo.absoluteDir();
    if (!appsDir.exists()) {
        appsDir.mkpath(".");
    }

    // Garanzia presenza icona
    QString iconDest = iconFilePath();
    QFileInfo iconInfo(iconDest);
    QDir iconDir = iconInfo.absoluteDir();
    if (!iconDir.exists()) {
        iconDir.mkpath(".");
    }

    // Esporta icona dalle risorse Qt o copia quella locale
    if (!QFile::exists(iconDest)) {
        QPixmap iconPix(":/icons/icon.png");
        if (!iconPix.isNull()) {
            iconPix.save(iconDest, "PNG");
        } else {
            // Fallback su icona applicazione in esecuzione
            QString localIcon = QCoreApplication::applicationDirPath() + "/Resources/icons/icon.png";
            if (QFile::exists(localIcon)) {
                QFile::copy(localIcon, iconDest);
            }
        }
    }

    // Se l'icona nel percorso hicolor non è creabile, usa la cartella applications
    if (!QFile::exists(iconDest)) {
        iconDest = appsDir.absolutePath() + "/ytmediadownloader.png";
        QPixmap iconPix(":/icons/icon.png");
        if (!iconPix.isNull()) {
            iconPix.save(iconDest, "PNG");
        }
    }

    // Determina l'eseguibile corretto (supporto AppImage se presente)
    QString execPath;
    if (qEnvironmentVariableIsSet("APPIMAGE")) {
        execPath = QString::fromUtf8(qgetenv("APPIMAGE"));
    } else {
        execPath = QCoreApplication::applicationFilePath();
    }

    // Scrittura del file .desktop con formato conforme a XDG Desktop Entry Specification
    QFile file(dPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        DevLogLogger::instance().logAction("ERRORE", QString("Impossibile creare il file desktop: %1").arg(dPath));
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "[Desktop Entry]\n";
    out << "Type=Application\n";
    out << "Version=1.0\n";
    out << "Name=YT Media Downloader Pro\n";
    out << "GenericName=YouTube Media Downloader\n";
    out << "Comment=Scarica audio e video multitraccia da YouTube con IA\n";
    out << "Exec=\"" << execPath << "\" %u\n";
    out << "Icon=" << (QFile::exists(iconDest) ? iconDest : "video-display") << "\n";
    out << "Terminal=false\n";
    out << "Categories=AudioVideo;Video;Audio;Network;Qt;\n";
    out << "StartupNotify=true\n";
    out << "Keywords=youtube;downloader;video;audio;media;yt-dlp;ffmpeg;\n";
    file.close();

    // Rende il file eseguibile
    QFile::setPermissions(dPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                 QFile::ReadGroup | QFile::ExeGroup |
                                 QFile::ReadOther | QFile::ExeOther);

    refreshDesktopDatabase();
    DevLogLogger::instance().logAction("INTEGRAZIONE_SISTEMA", QString("File .desktop creato con successo in: %1 (Exec: %2)").arg(dPath, execPath));
    return true;
}

/**
 * @brief Inverte lo stato di visibilità nel sistema.
 * @return True se ora è integrata, False se rimossa.
 */
bool DesktopIntegrationManager::toggleIntegration()
{
    bool currentlyIntegrated = isIntegrated();
    setIntegrated(!currentlyIntegrated);
    return !currentlyIntegrated;
}

/**
 * @brief Esegue update-desktop-database per aggiornare la cache delle applicazioni utente.
 */
void DesktopIntegrationManager::refreshDesktopDatabase()
{
    QString appsDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (appsDir.isEmpty()) {
        appsDir = QDir::homePath() + "/.local/share/applications";
    }

    // Esegue update-desktop-database in background se presente nel sistema
    QProcess::startDetached("update-desktop-database", QStringList() << appsDir);
}
