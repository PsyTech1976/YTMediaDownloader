/**
 * @file LocalizationManager.cpp
 * @brief Implementazione del gestore di localizzazione multilingua per l'applicazione.
 * 
 * Legge ed interpreta dinamicamente file XML con estensione .rsc per tradurre le finestre UI.
 * Mappa le sigle dei codici lingua nei rispettivi nomi leggibili completi (es. "it_IT" -> "Italiano", "de_DE" -> "Deutsch")
 * e si sincronizza all'avvio con le preferenze memorizzate in SettingsManager.
 */

#include "LocalizationManager.h"
#include "CoreApp/Preferenze/SettingsManager.h"

/**
 * @brief Costruttore privato della classe Singleton LocalizationManager.
 * Inizializza la lingua attiva caricando la preferenza memorizzata dall'utente.
 */
LocalizationManager::LocalizationManager()
    : m_currentLang("it_IT")
{
    loadLanguage(SettingsManager::instance().preferredLanguage());
}

/**
 * @brief Restituisce l'istanza singleton condivisa di LocalizationManager.
 * @return Riferimento statico all'istanza.
 */
LocalizationManager& LocalizationManager::instance()
{
    static LocalizationManager inst;
    return inst;
}

/**
 * @brief Converte la sigla del codice lingua nell'etichetta leggibile completa.
 * @param langCode Sigla del codice lingua (es. "it_IT", "en_EN", "es_ES", "fr_FR", "de_DE").
 * @return Nome della lingua leggibile (es. "Italiano", "English", "Español", "Français", "Deutsch").
 */
QString LocalizationManager::languageCleanName(const QString& langCode)
{
    QString c = langCode.toLower();
    if (c.startsWith("it")) return "Italiano";
    if (c.startsWith("en")) return "English";
    if (c.startsWith("es")) return "Español";
    if (c.startsWith("fr")) return "Français";
    if (c.startsWith("de")) return "Deutsch";
    if (c.startsWith("ja")) return "日本語";
    if (c.startsWith("zh")) return "中文";
    if (c.startsWith("pt")) return "Português";
    if (c.startsWith("ru")) return "Русский";
    return langCode;
}

/**
 * @brief Restituisce l'icona della bandiera associata al codice lingua specificato.
 * @param langCode Codice lingua (es. "it_IT", "it", "en_EN", ecc.).
 * @return Oggetto QIcon con la grafica della bandiera nazionale.
 */
QIcon LocalizationManager::languageIcon(const QString& langCode)
{
    QString c = langCode.toLower();
    if (c.startsWith("it")) return QIcon(":/icons/flags/it.png");
    if (c.startsWith("en")) return QIcon(":/icons/flags/gb.png");
    if (c.startsWith("es")) return QIcon(":/icons/flags/es.png");
    if (c.startsWith("fr")) return QIcon(":/icons/flags/fr.png");
    if (c.startsWith("de")) return QIcon(":/icons/flags/de.png");
    return QIcon();
}

/**
 * @brief Converte la sigla del codice lingua nell'etichetta leggibile completa.
 * @param langCode Sigla del codice lingua (es. "it_IT", "en_EN", "es_ES", "fr_FR", "de_DE").
 * @return Nome della lingua leggibile.
 */
QString LocalizationManager::languageNativeName(const QString& langCode)
{
    return languageCleanName(langCode);
}

/**
 * @brief Carica ed interpreta il file XML di localizzazione corrispettivo al codice lingua specificato.
 * @param langCode Codice lingua da caricare (es. "it_IT", "en_EN", "es_ES", "fr_FR", "de_DE").
 * @return True se il file è stato caricato ed interpretato con successo, altrimenti False.
 */
bool LocalizationManager::loadLanguage(const QString& langCode)
{
    m_translations.clear();
    m_currentLang = langCode;

    // Determina il percorso del file di localizzazione (.rsc)
    QString fileRelativePath = QString("localizzazione/%1.rsc").arg(langCode);
    QString filePath = QCoreApplication::applicationDirPath() + "/" + fileRelativePath;

    if (!QFile::exists(filePath)) {
        filePath = fileRelativePath;
    }

    if (!QFile::exists(filePath)) {
        // Cerca nelle risorse incorporate Qt
        QString qrcPath = QString(":/localizzazione/%1.rsc").arg(langCode);
        if (QFile::exists(qrcPath)) {
            filePath = qrcPath;
        }
    }

    if (!QFile::exists(filePath)) {
        qWarning() << "[LocalizationManager] File non trovato:" << filePath;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    // Parsing della struttura del documento XML
    QDomDocument doc;
    if (!doc.setContent(&file)) {
        file.close();
        return false;
    }
    file.close();

    QDomElement root = doc.documentElement();
    if (root.tagName() != "localization") {
        return false;
    }

    // Legge tutte le definizioni di finestra ed i relativi valori di traduzione
    QDomNode windowNode = root.firstChild();
    while (!windowNode.isNull()) {
        QDomElement windowElem = windowNode.toElement();
        if (!windowElem.isNull() && windowElem.tagName() == "window") {
            QString windowName = windowElem.attribute("name");
            QDomNode entryNode = windowElem.firstChild();
            while (!entryNode.isNull()) {
                QDomElement entryElem = entryNode.toElement();
                if (!entryElem.isNull() && entryElem.tagName() == "entry") {
                    QString key = entryElem.attribute("key");
                    QString localized = entryElem.attribute("localized");
                    if (localized.isEmpty()) {
                        localized = entryElem.attribute("default");
                    }
                    m_translations[windowName][key] = localized;
                }
                entryNode = entryNode.nextSibling();
            }
        }
        windowNode = windowNode.nextSibling();
    }

    return true;
}

/**
 * @brief Restituisce il codice della lingua attualmente in uso.
 * @return Stringa del codice lingua (es. "it_IT").
 */
QString LocalizationManager::currentLanguage() const
{
    return m_currentLang;
}

/**
 * @brief Restituisce l'elenco dei codici lingua disponibili scansionando la cartella localizzazione/.
 * @return Lista di stringhe con i codici lingua.
 */
QStringList LocalizationManager::availableLanguages() const
{
    QStringList langs;
    QString dirPath = QCoreApplication::applicationDirPath() + "/localizzazione";
    QDir dir(dirPath);

    if (!dir.exists()) {
        dir = QDir("localizzazione");
    }

    QStringList files = dir.entryList(QStringList() << "*.rsc", QDir::Files);
    for (const QString& file : files) {
        langs << file.left(file.lastIndexOf('.'));
    }

    if (langs.isEmpty()) {
        langs << "it_IT" << "en_EN" << "es_ES" << "fr_FR" << "de_DE";
    }

    return langs;
}

/**
 * @brief Restituisce la stringa tradotta per una determinata chiave e finestra UI.
 * @param uiWindow Nome della finestra/classe UI che la richiede.
 * @param key Chiave identificativa della stringa.
 * @param defaultValue Valore di fallback da mostrare in caso di assenza della traduzione.
 * @return Stringa tradotta o valore predefinito di fallback.
 */
QString LocalizationManager::trXml(const QString& uiWindow, const QString& key, const QString& defaultValue)
{
    if (m_translations.contains(uiWindow) && m_translations[uiWindow].contains(key)) {
        QString localizedValue = m_translations[uiWindow][key];
        if (!localizedValue.isEmpty()) {
            return localizedValue;
        }
    }
    return defaultValue;
}
