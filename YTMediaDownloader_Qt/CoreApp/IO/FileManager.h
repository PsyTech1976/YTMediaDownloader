/**
 * @file FileManager.h
 * @brief Gestore delle operazioni di file system, cartelle e integrazione desktop.
 * 
 * Fornisce utilità per determinare percorsi predefiniti, convalidare directory,
 * sanificare i nomi file ed interagire con i file manager di sistema (Nautilus, Dolphin, Explorer, Finder).
 */

#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QString>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

class FileManager {
public:
    /**
     * @brief Restituisce la cartella di download predefinita (es. Videos o Home dell'utente).
     * @return Percorso assoluto della cartella.
     */
    static QString defaultDownloadDirectory();

    /**
     * @brief Assicura che una directory esista creandola se necessario.
     * @param path Percorso della directory.
     * @return True se la cartella esiste o è stata creata, altrimenti False.
     */
    static bool ensureDirectoryExists(const QString& path);

    /**
     * @brief Verifica se un percorso corrisponde a una directory esistente e scrivibile.
     * @param path Percorso da verificare.
     * @return True se valida e accessibile in scrittura, altrimenti False.
     */
    static bool isValidDirectory(const QString& path);

    /**
     * @brief Formatta una dimensione in byte in formato leggibile (B, KB, MB, GB).
     * @param bytes Quantità di byte.
     * @return Stringa leggibile formattata.
     */
    static QString formatFileSize(qint64 bytes);

    /**
     * @brief Rimuove caratteri illegali o riservati dai nomi dei file multimediali.
     * @param name Nome file grezzo.
     * @return Nome file sanificato sicuro per il filesystem.
     */
    static QString sanitizeFilename(const QString& name);

    /**
     * @brief Mostra il file scaricato nel file manager evidenziandolo/selezionandolo.
     * @param filePath Percorso assoluto del file.
     * @return True se il comando di sistema è stato inviato con successo.
     */
    static bool showInFileManager(const QString& filePath);

    /**
     * @brief Apre una cartella nel file manager predefinito del sistema operativo.
     * @param dirPath Percorso assoluto della directory.
     * @return True se aperta con successo.
     */
    static bool openDirectory(const QString& dirPath);

    /**
     * @brief Esegue un comando desktop di sistema in modo staccato (asincrono), sanificando
     * l'ambiente da percorsi interni AppImage per garantire la compatibilità con i programmi host.
     * @param program Nome del programma o percorso eseguibile.
     * @param args Argomenti della riga di comando.
     * @return True se il processo è stato avviato con successo.
     */
    static bool runDetachedCommand(const QString& program, const QStringList& args);
};

#endif // FILEMANAGER_H
