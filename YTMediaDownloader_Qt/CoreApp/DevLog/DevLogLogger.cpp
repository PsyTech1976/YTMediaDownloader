/**
 * @file DevLogLogger.cpp
 * @brief Implementazione del gestore di tracciamento e registrazione nel file DevLog.txt.
 * 
 * Permette l'aggiunta di eventi con marcatura oraria ed il recupero del contenuto del log.
 */

#include "DevLogLogger.h"

/**
 * @brief Costruttore privato della classe Singleton DevLogLogger.
 */
DevLogLogger::DevLogLogger()
{
    m_logFilePath = QCoreApplication::applicationDirPath() + "/DevLog.txt";
    if (!QFile::exists(m_logFilePath)) {
        m_logFilePath = "DevLog.txt";
    }
}

/**
 * @brief Restituisce l'istanza singleton condivisa di DevLogLogger.
 * @return Riferimento statico all'istanza.
 */
DevLogLogger& DevLogLogger::instance()
{
    static DevLogLogger inst;
    return inst;
}

/**
 * @brief Registra un'azione nel file DevLog.txt includendo la data ed ora correnti.
 * @param category Categoria dell'evento (es. "AVVIO", "ANALISI", "DOWNLOAD", "ERRORE").
 * @param details Dettagli e descrizione dell'operazione.
 */
void DevLogLogger::logAction(const QString& category, const QString& details)
{
    QFile file(m_logFilePath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        stream << QString("[%1] %2: %3\n").arg(timestamp, category, details);
        file.close();
    }
}

/**
 * @brief Legge e restituisce l'intero contenuto del file di log DevLog.txt.
 * @return Stringa contenente il testo del registro di log.
 */
QString DevLogLogger::readLogContent()
{
    QFile file(m_logFilePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        QString content = stream.readAll();
        file.close();
        return content;
    }
    return "Nessun registro di log trovato.";
}
