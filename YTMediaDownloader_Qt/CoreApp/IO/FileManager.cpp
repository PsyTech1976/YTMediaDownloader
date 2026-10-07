/**
 * @file FileManager.cpp
 * @brief Implementazione dei metodi statici ausiliari per le operazioni su file system.
 * 
 * Fornisce funzioni di utilità per il recupero delle cartelle di sistema, verifica di scrittura,
 * igienizzazione dei nomi file e formattazione delle dimensioni in byte.
 */

#include "FileManager.h"
#include <QRegularExpression>

/**
 * @brief Restituisce il percorso della cartella Video predefinita dell'utente o la Home directory.
 * @return Stringa con il percorso della directory.
 */
QString FileManager::defaultDownloadDirectory()
{
    QString moviesPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (moviesPath.isEmpty() || !QDir(moviesPath).exists()) {
        moviesPath = QDir::homePath();
    }
    return moviesPath;
}

/**
 * @brief Verifica che una directory esista, altrimenti la crea ricorsivamente.
 * @param path Percorso della directory da verificare o creare.
 * @return True se la directory esiste ed è pronta, altrimenti False.
 */
bool FileManager::ensureDirectoryExists(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

/**
 * @brief Verifica se un percorso inserito è una directory valida ed accessibile in scrittura.
 * @param path Percorso da verificare.
 * @return True se valido e scrivibile, altrimenti False.
 */
bool FileManager::isValidDirectory(const QString& path)
{
    QFileInfo info(path);
    return info.exists() && info.isDir() && info.isWritable();
}

/**
 * @brief Converte una dimensione espressa in byte in una stringa leggibile (B, KB, MB, GB).
 * @param bytes Dimensione espressa in byte.
 * @return Stringa formattata con l'unità di misura idonea.
 */
QString FileManager::formatFileSize(qint64 bytes)
{
    if (bytes < 1024)
        return QString("%1 B").arg(bytes);
    double kb = bytes / 1024.0;
    if (kb < 1024)
        return QString("%1 KB").arg(kb, 0, 'f', 1);
    double mb = kb / 1024.0;
    if (mb < 1024)
        return QString("%1 MB").arg(mb, 0, 'f', 1);
    double gb = mb / 1024.0;
    return QString("%1 GB").arg(gb, 0, 'f', 2);
}

/**
 * @brief Rimuove o sostituisce i caratteri non validi nei nomi file per il file system di sistema.
 * @param name Nome file originale da sanificare.
 * @return Nome file sanificato privo di caratteri illegali.
 */
QString FileManager::sanitizeFilename(const QString& name)
{
    QString clean = name;
    clean.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    return clean.trimmed();
}

#include <QProcess>
#include <QProcessEnvironment>
#include <QDesktopServices>
#include <QUrl>

/**
 * @brief Esegue un comando desktop di sistema in modo staccato (asincrono), sanificando
 * l'ambiente da percorsi interni AppImage per garantire la compatibilità con i programmi host.
 * @param program Nome del programma o percorso eseguibile.
 * @param args Argomenti della riga di comando.
 * @return True se il processo è stato avviato con successo.
 */
bool FileManager::runDetachedCommand(const QString& program, const QStringList& args)
{
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // Se l'applicazione è in esecuzione all'interno di un AppImage, sanifica le variabili d'ambiente
    // per evitare conflitti di librerie o plugin Qt con i file manager di sistema (es. Dolphin, Nautilus).
    if (qEnvironmentVariableIsSet("APPDIR")) {
        QString appDir = qEnvironmentVariable("APPDIR");

        if (qEnvironmentVariableIsSet("LD_LIBRARY_PATH_ORIG")) {
            env.insert("LD_LIBRARY_PATH", qEnvironmentVariable("LD_LIBRARY_PATH_ORIG"));
        } else {
            QString ldPath = env.value("LD_LIBRARY_PATH");
            QStringList parts = ldPath.split(':', Qt::SkipEmptyParts);
            QStringList cleaned;
            for (const QString& p : parts) {
                if (!p.startsWith(appDir)) {
                    cleaned.append(p);
                }
            }
            if (cleaned.isEmpty()) {
                env.remove("LD_LIBRARY_PATH");
            } else {
                env.insert("LD_LIBRARY_PATH", cleaned.join(':'));
            }
        }

        if (env.value("QT_PLUGIN_PATH").contains(appDir)) {
            env.remove("QT_PLUGIN_PATH");
        }
        if (env.value("QML2_IMPORT_PATH").contains(appDir)) {
            env.remove("QML2_IMPORT_PATH");
        }
    }

    process.setProcessEnvironment(env);
    process.setProgram(program);
    process.setArguments(args);
    return process.startDetached();
}

/**
 * @brief Mostra il file scaricato nel file manager del desktop evidenziandolo se supportato.
 * @param filePath Percorso assoluto del file.
 * @return True se l'operazione ha avuto successo, altrimenti False.
 */
bool FileManager::showInFileManager(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return false;
    }

#if defined(Q_OS_WIN)
    QStringList args;
    args << "/select," << QDir::toNativeSeparators(filePath);
    return QProcess::startDetached("explorer.exe", args);
#elif defined(Q_OS_MAC)
    QStringList args;
    args << "-e" << QString("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(filePath);
    args << "-e" << "tell application \"Finder\" to activate";
    return QProcess::startDetached("osascript", args);
#else
    // Linux / X11 / Wayland:
    QString absPath = fileInfo.absoluteFilePath();
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();

    // 1. KDE Plasma: usa dolphin nativo con parametro --select (supportato pienamente su X11 e Wayland)
    if (desktop.contains("kde") || !QStandardPaths::findExecutable("dolphin").isEmpty()) {
        if (!QStandardPaths::findExecutable("dolphin").isEmpty()) {
            if (runDetachedCommand("dolphin", QStringList() << "--select" << absPath)) {
                return true;
            }
        }
    }

    // 2. GNOME: usa nautilus nativo con parametro --select
    if (desktop.contains("gnome") || !QStandardPaths::findExecutable("nautilus").isEmpty()) {
        if (!QStandardPaths::findExecutable("nautilus").isEmpty()) {
            if (runDetachedCommand("nautilus", QStringList() << "--select" << absPath)) {
                return true;
            }
        }
    }

    // 3. Cinnamon: usa nemo --no-desktop
    if (!QStandardPaths::findExecutable("nemo").isEmpty()) {
        if (runDetachedCommand("nemo", QStringList() << "--no-desktop" << absPath)) {
            return true;
        }
    }

    // 4. MATE: usa caja --select
    if (!QStandardPaths::findExecutable("caja").isEmpty()) {
        if (runDetachedCommand("caja", QStringList() << "--select" << absPath)) {
            return true;
        }
    }

    // 5. DBus ShowItems (standard Freedesktop FileManager1) in modo asincrono / non bloccante
    if (!QStandardPaths::findExecutable("dbus-send").isEmpty()) {
        QString fileUri = QUrl::fromLocalFile(absPath).toString();
        QStringList dbusArgs;
        dbusArgs << "--session" << "--dest=org.freedesktop.FileManager1"
                 << "--type=method_call" << "/org/freedesktop/FileManager1"
                 << "org.freedesktop.FileManager1.ShowItems"
                 << QString("array:string:%1").arg(fileUri)
                 << "string:";
        if (runDetachedCommand("dbus-send", dbusArgs)) {
            return true;
        }
    }

    // 6. Fallback finale: apri la cartella padre
    return openDirectory(fileInfo.absolutePath());
#endif
}

/**
 * @brief Apre una cartella nel file manager predefinito del desktop mostrandone il contenuto.
 * @param dirPath Percorso assoluto della directory.
 * @return True se l'operazione ha avuto successo, altrimenti False.
 */
bool FileManager::openDirectory(const QString& dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        ensureDirectoryExists(dirPath);
    }

#if defined(Q_OS_WIN)
    return QProcess::startDetached("explorer.exe", QStringList() << QDir::toNativeSeparators(dirPath));
#elif defined(Q_OS_MAC)
    return QProcess::startDetached("open", QStringList() << dirPath);
#else
    QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").toLower();
    if (desktop.contains("kde") && !QStandardPaths::findExecutable("dolphin").isEmpty()) {
        if (runDetachedCommand("dolphin", QStringList() << dirPath)) {
            return true;
        }
    } else if (desktop.contains("gnome") && !QStandardPaths::findExecutable("nautilus").isEmpty()) {
        if (runDetachedCommand("nautilus", QStringList() << dirPath)) {
            return true;
        }
    }

    if (!QStandardPaths::findExecutable("xdg-open").isEmpty()) {
        if (runDetachedCommand("xdg-open", QStringList() << dirPath)) {
            return true;
        }
    }

    return QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
#endif
}


