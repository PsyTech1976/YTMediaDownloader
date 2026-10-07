/**
 * @file SettingsManager.h
 * @brief Gestore centralizzato e persistente delle impostazioni dell'utente.
 * 
 * Utilizza QSettings per memorizzare e recuperare la cartella di download predefinita,
 * la lingua preferita dell'interfaccia e il percorso dell'ultimo file scaricato.
 */

#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QString>
#include <QSettings>

/**
 * @class SettingsManager
 * @brief Singleton per la persistenza delle preferenze applicative su disco.
 */
class SettingsManager {
public:
    /**
     * @brief Restituisce l'istanza singleton di SettingsManager.
     * @return Riferimento all'istanza unica condivisa.
     */
    static SettingsManager& instance();

    /**
     * @brief Restituisce il percorso della cartella predefinita di salvataggio.
     * @return Percorso assoluto della cartella.
     */
    QString defaultSavePath() const;

    /**
     * @brief Imposta la cartella di salvataggio predefinita.
     * @param path Percorso assoluto della directory.
     */
    void setDefaultSavePath(const QString& path);

    /**
     * @brief Restituisce il codice della lingua preferita dall'utente.
     * @return Codice lingua (es. "it_IT").
     */
    QString preferredLanguage() const;

    /**
     * @brief Imposta e memorizza il codice della lingua preferita.
     * @param langCode Codice lingua da impostare.
     */
    void setPreferredLanguage(const QString& langCode);

    /**
     * @brief Restituisce il percorso dell'ultimo file multimediale scaricato con successo.
     * @return Percorso assoluto del file.
     */
    QString lastDownloadedFilePath() const;

    /**
     * @brief Memorizza il percorso dell'ultimo file multimediale scaricato.
     * @param path Percorso assoluto del file.
     */
    void setLastDownloadedFilePath(const QString& path);

private:
    SettingsManager();
    ~SettingsManager() = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    QSettings m_settings;
};

#endif // SETTINGSMANAGER_H
