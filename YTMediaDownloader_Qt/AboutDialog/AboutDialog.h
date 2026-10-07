/**
 * @file AboutDialog.h
 * @brief Finestra modale di informazioni e crediti sul software YT Media Downloader Pro.
 * 
 * Riporta la descrizione delle funzionalità, la versione dell'applicazione, le tecnologie impiegate
 * e l'attribuzione di ideazione: "Software realizzato con IA su un'idea di PsyTech6".
 */

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Costruttore della classe AboutDialog.
     * @param parent Widget genitore facoltativo.
     */
    explicit AboutDialog(QWidget *parent = nullptr);

    /**
     * @brief Distruttore della classe AboutDialog.
     */
    ~AboutDialog() override = default;

    /**
     * @brief Aggiorna dinamicamente i testi della finestra in base alla lingua attiva.
     */
    void retranslateUi();

private:
    void setupUi();

    QLabel *m_lblIcon;
    QLabel *m_lblTitle;
    QLabel *m_lblIdeaCredit;
    QLabel *m_lblDescription;
    QLabel *m_lblTechInfo;
    QPushButton *m_btnClose;
};

#endif // ABOUTDIALOG_H
