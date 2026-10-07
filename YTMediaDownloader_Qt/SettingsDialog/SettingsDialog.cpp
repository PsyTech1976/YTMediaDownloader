/**
 * @file SettingsDialog.cpp
 * @brief Implementazione della finestra di dialogo per la gestione delle preferenze dell'applicazione.
 * 
 * Permette l'allargamento orizzontale della finestra mantenendo l'altezza bloccata ed ottimizzata
 * senza spazi vuoti verticali.
 */

#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"
#include "CoreApp/Preferenze/SettingsManager.h"
#include "CoreApp/Localization/LocalizationManager.h"
#include <QFileDialog>

/**
 * @brief Costruttore della classe SettingsDialog.
 * @param parent Widget padre facoltativo.
 */
SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);

    // Consente l'allargamento in larghezza bloccando l'altezza allo stato compatto attuale
    setFixedHeight(sizeHint().height());
    setMinimumWidth(450);

    // Connessione dei segnali dei pulsanti Sfoglia, Salva ed Annulla
    connect(ui->btnBrowsePath, &QPushButton::clicked, this, &SettingsDialog::onBrowsePathClicked);
    connect(ui->btnSave, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);

    // Inizializza i valori correnti dai gestori delle impostazioni
    ui->txtDefaultPath->setText(SettingsManager::instance().defaultSavePath());

    ui->comboLanguage->clear();
    ui->comboLanguage->setIconSize(QSize(20, 14));
    QStringList langs = LocalizationManager::instance().availableLanguages();
    for (const QString& lang : langs) {
        QString displayName = LocalizationManager::languageNativeName(lang);
        ui->comboLanguage->addItem(LocalizationManager::languageIcon(lang), displayName, lang);
    }
    int curIndex = ui->comboLanguage->findData(SettingsManager::instance().preferredLanguage());
    if (curIndex >= 0) {
        ui->comboLanguage->setCurrentIndex(curIndex);
    }

    retranslateUi();
}

/**
 * @brief Distruttore della classe SettingsDialog.
 */
SettingsDialog::~SettingsDialog()
{
    delete ui;
}

/**
 * @brief Traduce i testi dell'interfaccia utente in base alla lingua attiva.
 */
void SettingsDialog::retranslateUi()
{
    setWindowTitle(LOC("SettingsDialog", "window_title", "Preferenze Applicazione"));
    ui->grpGeneral->setTitle(LOC("SettingsDialog", "grp_general", "Impostazioni Generali"));
    ui->lblDefaultPath->setText(LOC("SettingsDialog", "lbl_default_path", "Percorso di Salvataggio Predefinito:"));
    ui->lblDefaultLang->setText(LOC("SettingsDialog", "lbl_default_lang", "Lingua Predefinita:"));
    ui->btnSave->setText(LOC("SettingsDialog", "btn_save", "Salva Preferenze"));
    ui->btnCancel->setText(LOC("SettingsDialog", "btn_cancel", "Annulla"));
}

/**
 * @brief Slot azionato dal pulsante 'Sfoglia...': permette di selezionare la nuova cartella predefinita.
 */
void SettingsDialog::onBrowsePathClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Seleziona Cartella Predefinita", ui->txtDefaultPath->text());
    if (!dir.isEmpty()) {
        ui->txtDefaultPath->setText(dir);
    }
}

/**
 * @brief Slot azionato dal pulsante 'Salva Preferenze': salva i nuovi valori in modo persistente.
 */
void SettingsDialog::onSaveClicked()
{
    SettingsManager::instance().setDefaultSavePath(ui->txtDefaultPath->text());
    QString selectedLang = ui->comboLanguage->currentData().toString();
    SettingsManager::instance().setPreferredLanguage(selectedLang);
    LocalizationManager::instance().loadLanguage(selectedLang);
    accept();
}

/**
 * @brief Slot azionato dal pulsante 'Annulla'.
 */
void SettingsDialog::onCancelClicked()
{
    reject();
}
