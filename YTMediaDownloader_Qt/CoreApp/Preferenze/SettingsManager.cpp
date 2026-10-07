/**
 * @file SettingsManager.cpp
 * @brief Implementazione del gestore singleton delle preferenze utente persistenti.
 * 
 * Utilizza QSettings per memorizzare e recuperare le impostazioni quali la cartella di salvataggio
 * predefinita ed il codice della lingua preferita dall'utente.
 */

#include "SettingsManager.h"
#include "CoreApp/IO/FileManager.h"

/**
 * @brief Costruttore privato della classe SettingsManager.
 */
SettingsManager::SettingsManager()
    : m_settings("YTMediaDownloaderOrg", "YTMediaDownloader")
{
}

/**
 * @brief Restituisce l'istanza singleton condivisa di SettingsManager.
 * @return Riferimento statico all'istanza.
 */
SettingsManager& SettingsManager::instance()
{
    static SettingsManager inst;
    return inst;
}

/**
 * @brief Recupera il percorso di salvataggio predefinito registrato nelle preferenze.
 * @return Stringa del percorso assoluto della directory.
 */
QString SettingsManager::defaultSavePath() const
{
    QString path = m_settings.value("defaultSavePath", "").toString();
    if (path.isEmpty() || !FileManager::isValidDirectory(path)) {
        return FileManager::defaultDownloadDirectory();
    }
    return path;
}

/**
 * @brief Imposta e memorizza in modo permanente il nuovo percorso di salvataggio predefinito.
 * @param path Nuovo percorso di salvataggio da memorizzare.
 */
void SettingsManager::setDefaultSavePath(const QString& path)
{
    m_settings.setValue("defaultSavePath", path);
}

/**
 * @brief Recupera il codice lingua preferito dall'utente.
 * @return Codice lingua (es. "it_IT").
 */
QString SettingsManager::preferredLanguage() const
{
    return m_settings.value("preferredLanguage", "it_IT").toString();
}

/**
 * @brief Imposta e memorizza in modo permanente il codice lingua preferito.
 * @param langCode Codice lingua da memorizzare.
 */
void SettingsManager::setPreferredLanguage(const QString& langCode)
{
    m_settings.setValue("preferredLanguage", langCode);
}

/**
 * @brief Recupera il percorso dell'ultimo file scaricato con successo.
 * @return Percorso assoluto del file o stringa vuota se non presente.
 */
QString SettingsManager::lastDownloadedFilePath() const
{
    return m_settings.value("lastDownloadedFilePath", "").toString();
}

/**
 * @brief Memorizza il percorso dell'ultimo file scaricato con successo.
 * @param path Percorso assoluto del file scaricato.
 */
void SettingsManager::setLastDownloadedFilePath(const QString& path)
{
    m_settings.setValue("lastDownloadedFilePath", path);
}
