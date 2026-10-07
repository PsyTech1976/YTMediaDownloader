#ifndef DEVLOGLOGGER_H
#define DEVLOGLOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>

class DevLogLogger {
public:
    static DevLogLogger& instance();

    void logAction(const QString& category, const QString& details);
    QString readLogContent();

private:
    DevLogLogger();
    ~DevLogLogger() = default;
    DevLogLogger(const DevLogLogger&) = delete;
    DevLogLogger& operator=(const DevLogLogger&) = delete;

    QString m_logFilePath;
};

#endif // DEVLOGLOGGER_H
