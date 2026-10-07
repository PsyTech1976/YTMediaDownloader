/**
 * @file AboutDialog.cpp
 * @brief Implementazione della finestra di dialogo Informazioni sul Software.
 * 
 * Presenta all'utente l'icona ad alta risoluzione, la versione corrente e la dichiarazione di paternità
 * dell'idea: "Software realizzato con IA su un'idea di PsyTech6."
 */

#include "AboutDialog.h"
#include "CoreApp/Localization/LocalizationManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QIcon>
#include <QFrame>

/**
 * @brief Costruttore della finestra AboutDialog.
 * @param parent Widget genitore facoltativo.
 */
AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , m_lblIcon(nullptr)
    , m_lblTitle(nullptr)
    , m_lblIdeaCredit(nullptr)
    , m_lblDescription(nullptr)
    , m_lblTechInfo(nullptr)
    , m_btnClose(nullptr)
{
    setupUi();
    retranslateUi();
}

/**
 * @brief Inizializza il layout grafico e i componenti visivi della finestra.
 */
void AboutDialog::setupUi()
{
    setWindowIcon(QIcon(":/icons/icon.png"));
    setModal(true);
    setFixedSize(540, 420);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 20);
    mainLayout->setSpacing(14);

    // Intestazione con icona grande e titolo
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(16);

    m_lblIcon = new QLabel(this);
    QPixmap iconPix(":/icons/icon.png");
    if (!iconPix.isNull()) {
        m_lblIcon->setPixmap(iconPix.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    m_lblIcon->setFixedSize(72, 72);
    headerLayout->addWidget(m_lblIcon);

    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    m_lblTitle = new QLabel(this);
    m_lblTitle->setStyleSheet("font-size: 17pt; font-weight: bold; color: #1a1a1a;");
    titleLayout->addWidget(m_lblTitle);

    QLabel *lblVersion = new QLabel("Versione 1.5.0 - Professional Edition", this);
    lblVersion->setStyleSheet("font-size: 9.5pt; color: #6c757d; font-weight: 500;");
    titleLayout->addWidget(lblVersion);

    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Separatore orizzontale elegante
    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setStyleSheet("color: #dee2e6;");
    mainLayout->addWidget(line);

    // Riquadro evidenziato con l'attribuzione di ideazione (PsyTech6 & IA)
    QFrame *creditBox = new QFrame(this);
    creditBox->setStyleSheet("QFrame { background-color: #f0f7ff; border: 1.5px solid #b6d4fe; border-radius: 8px; padding: 12px; }");
    QVBoxLayout *creditBoxLayout = new QVBoxLayout(creditBox);
    creditBoxLayout->setContentsMargins(12, 10, 12, 10);
    creditBoxLayout->setSpacing(4);

    m_lblIdeaCredit = new QLabel(this);
    m_lblIdeaCredit->setWordWrap(true);
    m_lblIdeaCredit->setAlignment(Qt::AlignCenter);
    m_lblIdeaCredit->setStyleSheet("font-size: 11pt; font-weight: bold; color: #0b5ed7;");
    creditBoxLayout->addWidget(m_lblIdeaCredit);
    mainLayout->addWidget(creditBox);

    // Descrizione dell'applicazione
    m_lblDescription = new QLabel(this);
    m_lblDescription->setWordWrap(true);
    m_lblDescription->setStyleSheet("font-size: 9.5pt; color: #333333; line-height: 140%;");
    mainLayout->addWidget(m_lblDescription);

    // Informazioni tecniche su librerie e dipendenze
    m_lblTechInfo = new QLabel(this);
    m_lblTechInfo->setWordWrap(true);
    m_lblTechInfo->setStyleSheet("font-size: 8.5pt; color: #6c757d;");
    mainLayout->addWidget(m_lblTechInfo);

    mainLayout->addStretch();

    // Barra inferiore con pulsante Chiudi
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();

    m_btnClose = new QPushButton(this);
    m_btnClose->setCursor(Qt::PointingHandCursor);
    m_btnClose->setStyleSheet("QPushButton { padding: 7px 24px; font-size: 9.5pt; font-weight: bold; background-color: #0d6efd; color: white; border: none; border-radius: 5px; }"
                              "QPushButton:hover { background-color: #0b5ed7; }"
                              "QPushButton:pressed { background-color: #0a58ca; }");
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_btnClose);

    mainLayout->addLayout(bottomLayout);
}

/**
 * @brief Aggiorna tutti i testi tradotti della finestra About.
 */
void AboutDialog::retranslateUi()
{
    setWindowTitle(LOC("AboutDialog", "window_title", "Informazioni su YT Media Downloader Pro"));
    m_lblTitle->setText(LOC("AboutDialog", "app_name", "YT Media Downloader Pro"));

    // Nota di ideazione obbligatoria specificata dall'utente
    m_lblIdeaCredit->setText(LOC("AboutDialog", "idea_credit", "Software realizzato con IA su un'idea di PsyTech6."));

    m_lblDescription->setText(LOC("AboutDialog", "app_description",
        "Strumento avanzato e ad alte prestazioni per l'analisi, il download, l'estrazione e il muxing "
        "di flussi multimediali da YouTube. Supporta risoluzioni video fino a 4K/8K, download multitraccia "
        "con canali audio localizzati e integrazione dei sottotitoli conformi agli standard ISO 639."));

    m_lblTechInfo->setText(LOC("AboutDialog", "tech_info",
        "Tecnologie integrate: Qt 5 (C++17), FFmpeg / ffprobe multithread, yt-dlp con protocollo anti-rate limit. "
        "Rilasciato come pacchetto portabile standalone universale."));

    m_btnClose->setText(LOC("AboutDialog", "btn_close", "Chiudi"));
}
