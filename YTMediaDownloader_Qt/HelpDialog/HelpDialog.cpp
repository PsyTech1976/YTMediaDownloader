/**
 * @file HelpDialog.cpp
 * @brief Implementazione della finestra separata per la visualizzazione della guida utente HTML.
 * 
 * Supporta la consultazione multilingua in 5 lingue (IT, EN, FR, DE, ES)
 * con menu popup dedicato per il cambio istantaneo della lingua della guida,
 * navigazione interattiva interna e apertura facoltativa nel browser web esterno.
 */

#include "HelpDialog.h"
#include "CoreApp/Localization/LocalizationManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollBar>
#include <QDebug>

/**
 * @brief Costruttore della finestra HelpDialog.
 * @param parent Widget genitore.
 */
HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
    , m_textBrowser(nullptr)
    , m_btnLangMenu(nullptr)
    , m_btnOpenBrowser(nullptr)
    , m_btnClose(nullptr)
    , m_langMenu(nullptr)
    , m_currentLangCode("it")
{
    setupUi();

    // Inizializza con la lingua attualmente impostata nell'applicazione
    QString appLang = LocalizationManager::instance().currentLanguage().left(2).toLower();
    if (appLang != "it" && appLang != "en" && appLang != "fr" && appLang != "de" && appLang != "es") {
        appLang = "it";
    }
    loadLanguage(appLang);
}

/**
 * @brief Distruttore della classe HelpDialog.
 */
HelpDialog::~HelpDialog()
{
}

/**
 * @brief Inizializza l'interfaccia grafica della finestra Guida.
 */
void HelpDialog::setupUi()
{
    setWindowTitle(LOC("HelpDialog", "window_title", "Guida Utente - YTMediaDownloader"));
    setWindowIcon(QIcon(":/icons/icon.png"));
    resize(860, 680);
    setMinimumSize(650, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    // Barra superiore con menu lingua e pulsanti azione
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    topBarLayout->setSpacing(8);

    // Pulsante con menu popup per la selezione rapida della lingua
    m_btnLangMenu = new QPushButton(this);
    m_btnLangMenu->setCursor(Qt::PointingHandCursor);
    m_btnLangMenu->setStyleSheet("QPushButton { padding: 6px 12px; font-weight: bold; background-color: #f1f3f5; border: 1px solid #ced4da; border-radius: 4px; }"
                                 "QPushButton:hover { background-color: #e9ecef; }");

    m_langMenu = new QMenu(this);
    
    auto addLangAction = [this](const QString& label, const QString& code) {
        QAction *action = m_langMenu->addAction(LocalizationManager::languageIcon(code), label);
        connect(action, &QAction::triggered, this, [this, code]() {
            onSelectLanguage(code);
        });
    };

    addLangAction("Italiano", "it");
    addLangAction("English", "en");
    addLangAction("Français", "fr");
    addLangAction("Deutsch", "de");
    addLangAction("Español", "es");

    m_btnLangMenu->setMenu(m_langMenu);
    topBarLayout->addWidget(m_btnLangMenu);

    topBarLayout->addStretch();

    // Pulsante "Apri nel Browser"
    m_btnOpenBrowser = new QPushButton(LOC("HelpDialog", "btn_open_browser", "Apri nel Browser"), this);
    m_btnOpenBrowser->setCursor(Qt::PointingHandCursor);
    m_btnOpenBrowser->setStyleSheet("QPushButton { padding: 6px 14px; background-color: #e7f1ff; color: #0d6efd; border: 1px solid #b6d4fe; border-radius: 4px; font-weight: 500; }"
                                   "QPushButton:hover { background-color: #cfe2ff; }");
    connect(m_btnOpenBrowser, &QPushButton::clicked, this, &HelpDialog::onOpenInBrowserClicked);
    topBarLayout->addWidget(m_btnOpenBrowser);

    // Pulsante "Chiudi"
    m_btnClose = new QPushButton(LOC("HelpDialog", "btn_close", "Chiudi"), this);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet("QPushButton { padding: 6px 16px; border: 1px solid #ced4da; border-radius: 4px; }"
                             "QPushButton:hover { background-color: #e9ecef; }");
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    topBarLayout->addWidget(m_btnClose);

    mainLayout->addLayout(topBarLayout);

    // Visualizzatore HTML principale
    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setOpenExternalLinks(false); // Intercettiamo i link per gestire le ancore e il cambio lingua
    m_textBrowser->setStyleSheet("QTextBrowser { background-color: #ffffff; border: 1px solid #dee2e6; border-radius: 6px; padding: 10px; }");
    
    // Gestione clic sui link interni all'HTML (es. cambio lingua da link o ancore)
    connect(m_textBrowser, &QTextBrowser::anchorClicked, this, [this](const QUrl& url) {
        QString urlStr = url.toString();
        if (urlStr.startsWith("#")) {
            m_textBrowser->scrollToAnchor(urlStr.mid(1));
        } else if (urlStr.contains("guide_it.html")) {
            onSelectLanguage("it");
        } else if (urlStr.contains("guide_en.html")) {
            onSelectLanguage("en");
        } else if (urlStr.contains("guide_fr.html")) {
            onSelectLanguage("fr");
        } else if (urlStr.contains("guide_de.html")) {
            onSelectLanguage("de");
        } else if (urlStr.contains("guide_es.html")) {
            onSelectLanguage("es");
        } else if (urlStr.startsWith("http://") || urlStr.startsWith("https://")) {
            QDesktopServices::openUrl(url);
        } else {
            m_textBrowser->setSource(url);
        }
    });

    mainLayout->addWidget(m_textBrowser);
}

/**
 * @brief Risolve il percorso del file HTML per la lingua specificata.
 * @param langCode Codice a 2 lettere della lingua (it, en, fr, de, es).
 * @return Percorso su file system del documento HTML corrispondente.
 */
QString HelpDialog::resolveHtmlFilePath(const QString& langCode) const
{
    QString fileName = QString("guide_%1.html").arg(langCode);

    // 1. Cerca nella cartella docs/user-guide/ all'interno della directory dell'applicazione
    QString appDir = QCoreApplication::applicationDirPath();
    QString pathInApp = appDir + "/docs/user-guide/" + fileName;
    if (QFile::exists(pathInApp)) {
        return pathInApp;
    }

    // 2. Cerca nella directory docs/user-guide/ relativa alla cartella corrente
    QString pathRel = "docs/user-guide/" + fileName;
    if (QFile::exists(pathRel)) {
        return QFileInfo(pathRel).absoluteFilePath();
    }

    // 3. Cerca nella directory docs/user-guide/ nel path di sviluppo
    QString devPath = appDir + "/../docs/user-guide/" + fileName;
    if (QFile::exists(devPath)) {
        return QFileInfo(devPath).absoluteFilePath();
    }

    // 4. Cerca nella sottocartella docs di livello superiore
    QString parentDocs = QDir::cleanPath(appDir + "/../../docs/user-guide/" + fileName);
    if (QFile::exists(parentDocs)) {
        return parentDocs;
    }

    // 4. Fallback incorporato nelle risorse Qt
    QString qrcPath = ":/docs/user-guide/" + fileName;
    if (QFile::exists(qrcPath)) {
        return qrcPath;
    }

    return QString();
}

/**
 * @brief Carica e visualizza la guida per la lingua specificata.
 * @param langCode Codice a 2 lettere della lingua (it, en, fr, de, es).
 */
void HelpDialog::loadLanguage(const QString& langCode)
{
    m_currentLangCode = langCode.toLower();
    if (m_currentLangCode != "it" && m_currentLangCode != "en" && 
        m_currentLangCode != "fr" && m_currentLangCode != "de" && m_currentLangCode != "es") {
        m_currentLangCode = "it";
    }

    updateLangButtonText();

    QString htmlPath = resolveHtmlFilePath(m_currentLangCode);
    if (!htmlPath.isEmpty() && QFile::exists(htmlPath)) {
        QFile file(htmlPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString htmlContent = QString::fromUtf8(file.readAll());
            file.close();

            // Imposta il percorso base per consentire al QTextBrowser di caricare style.css
            QUrl baseUrl = QUrl::fromLocalFile(QFileInfo(htmlPath).absolutePath() + "/");
            m_textBrowser->document()->setBaseUrl(baseUrl);
            m_textBrowser->setHtml(htmlContent);
            m_textBrowser->verticalScrollBar()->setValue(0);
            return;
        }
    }

    // Fallback qualora il file non fosse momentaneamente leggibile
    m_textBrowser->setHtml(QString("<h2>Guida Utente non trovata</h2><p>Impossibile caricare il file per la lingua: %1</p>").arg(m_currentLangCode));
}

/**
 * @brief Aggiorna l'etichetta del pulsante del menu lingua in base alla selezione attuale.
 */
void HelpDialog::updateLangButtonText()
{
    QString label = LOC("HelpDialog", "lbl_guide_lang", "Lingua Guida:") + " " + LocalizationManager::languageCleanName(m_currentLangCode) + " ▼";

    if (m_btnLangMenu) {
        m_btnLangMenu->setIcon(LocalizationManager::languageIcon(m_currentLangCode));
        m_btnLangMenu->setIconSize(QSize(22, 15));
        m_btnLangMenu->setText(label);
    }
}

/**
 * @brief Slot azionato quando l'utente seleziona una lingua dal menu popup.
 * @param langCode Codice lingua selezionato.
 */
void HelpDialog::onSelectLanguage(const QString& langCode)
{
    loadLanguage(langCode);
    QString fullCode = langCode + "_" + langCode.toUpper();
    if (langCode == "en") fullCode = "en_EN";
    emit languageChanged(fullCode);
}

/**
 * @brief Slot azionato al clic su 'Apri nel Browser'.
 */
void HelpDialog::onOpenInBrowserClicked()
{
    QString htmlPath = resolveHtmlFilePath(m_currentLangCode);
    if (!htmlPath.isEmpty() && QFile::exists(htmlPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(htmlPath));
    } else {
        // Apri l'index se la guida specifica non è trovata
        QString indexPath = resolveHtmlFilePath("it");
        if (!indexPath.isEmpty()) {
            QFileInfo info(indexPath);
            QString mainIndex = info.absolutePath() + "/index.html";
            if (QFile::exists(mainIndex)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(mainIndex));
            }
        }
    }
}

/**
 * @brief Aggiorna i testi dell'interfaccia quando la lingua dell'applicazione viene modificata.
 */
void HelpDialog::retranslateUi()
{
    setWindowTitle(LOC("HelpDialog", "window_title", "Guida Utente - YTMediaDownloader"));
    if (m_btnOpenBrowser) {
        m_btnOpenBrowser->setText(LOC("HelpDialog", "btn_open_browser", "Apri nel Browser"));
    }
    if (m_btnClose) {
        m_btnClose->setText(LOC("HelpDialog", "btn_close", "Chiudi"));
    }
    updateLangButtonText();
}
