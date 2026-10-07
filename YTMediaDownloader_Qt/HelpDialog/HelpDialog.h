#ifndef HELPDIALOG_H
#define HELPDIALOG_H

#include <QDialog>
#include <QString>

class QTextBrowser;
class QPushButton;
class QMenu;
class QLabel;

class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr);
    ~HelpDialog();

    void loadLanguage(const QString& langCode);
    void retranslateUi();

signals:
    void languageChanged(const QString& langCode);

private slots:
    void onSelectLanguage(const QString& langCode);
    void onOpenInBrowserClicked();

private:
    void setupUi();
    QString resolveHtmlFilePath(const QString& langCode) const;
    void updateLangButtonText();

    QTextBrowser *m_textBrowser;
    QPushButton *m_btnLangMenu;
    QPushButton *m_btnOpenBrowser;
    QPushButton *m_btnClose;
    QMenu *m_langMenu;

    QString m_currentLangCode; // "it", "en", "fr", "de", "es"
};

#endif // HELPDIALOG_H
