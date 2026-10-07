/**
 * @file LocalizationManager.h
 * @brief Gestore centralizzato per la localizzazione multilingua dinamica dell'interfaccia utente.
 * 
 * Legge e memorizza le traduzioni estratte dai file XML/RSC e fornisce macro comode
 * per il recupero immediato dei testi localizzati.
 */

#ifndef LOCALIZATIONMANAGER_H
#define LOCALIZATIONMANAGER_H

#include <QString>
#include <QMap>
#include <QXmlStreamReader>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QtXml/QDomDocument>

class LocalizationManager {
public:
    /**
     * @brief Restituisce l'istanza singleton di LocalizationManager.
     * @return Riferimento all'istanza unica.
     */
    static LocalizationManager& instance();

    /**
     * @brief Carica il file di traduzione corrispondente al codice lingua specificato.
     * @param langCode Codice della lingua (es. "it_IT", "en_EN", "fr_FR", "de_DE", "es_ES").
     * @return True se caricato correttamente, altrimenti False.
     */
    bool loadLanguage(const QString& langCode);

    /**
     * @brief Restituisce il codice della lingua correntemente attiva.
     * @return Codice lingua (es. "it_IT").
     */
    QString currentLanguage() const;

    /**
     * @brief Restituisce la lista di tutte le lingue disponibili scansionando la cartella localizzazione/.
     * @return Elenco dei codici lingua disponibili.
     */
    QStringList availableLanguages() const;

    /**
     * @brief Recupera la stringa tradotta per una finestra e chiave specifiche.
     * @param uiWindow Nome della finestra/componente.
     * @param key Chiave identificativa della stringa.
     * @param defaultValue Testo predefinito di fallback.
     * @return Testo tradotto o testo predefinito.
     */
    QString trXml(const QString& uiWindow, const QString& key, const QString& defaultValue);

    /**
     * @brief Converte un codice lingua nel rispettivo nome nativo.
     * @param langCode Codice lingua (es. "it_IT").
     * @return Stringa descrittiva.
     */
    static QString languageNativeName(const QString& langCode);

    /**
     * @brief Restituisce l'icona grafica della bandiera nazionale associata al codice lingua.
     * @param langCode Codice lingua (es. "it_IT", "it", "en_EN", ecc.).
     * @return Oggetto QIcon con la bandiera a colori.
     */
    static QIcon languageIcon(const QString& langCode);

    /**
     * @brief Restituisce il nome leggibile nativo pulito privo di caratteri emoji.
     * @param langCode Codice lingua (es. "it_IT").
     * @return Nome testuale (es. "Italiano", "English", ecc.).
     */
    static QString languageCleanName(const QString& langCode);

private:
    LocalizationManager();
    ~LocalizationManager() = default;

    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    QString m_currentLang;
    QMap<QString, QMap<QString, QString>> m_translations;
};

/**
 * @def LOC(window, key, defaultVal)
 * @brief Macro rapida per ottenere la stringa localizzata tramite LocalizationManager.
 */
#define LOC(window, key, defaultVal) LocalizationManager::instance().trXml(window, key, defaultVal)

#endif // LOCALIZATIONMANAGER_H
