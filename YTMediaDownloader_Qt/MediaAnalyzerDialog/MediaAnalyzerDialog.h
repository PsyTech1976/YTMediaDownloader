#ifndef MEDIAANALYZERDIALOG_H
#define MEDIAANALYZERDIALOG_H

#include <QDialog>
#include "CoreApp/FFmpeg/FFmpegManager.h"

namespace Ui {
class MediaAnalyzerDialog;
}

class MediaAnalyzerDialog : public QDialog {
    Q_OBJECT

public:
    explicit MediaAnalyzerDialog(const MediaMetadata& metadata, QWidget *parent = nullptr);
    ~MediaAnalyzerDialog();

    DownloadOptions getSelectedOptions() const;
    void retranslateUi();

private slots:
    void onConfirmClicked();
    void onCancelClicked();
    void onSelectAllAudioClicked();
    void onDeselectAllAudioClicked();
    void onSelectAllSubsClicked();
    void onDeselectAllSubsClicked();

private:
    void populateFields();

    Ui::MediaAnalyzerDialog *ui;
    MediaMetadata m_metadata;
    DownloadOptions m_resultOptions;
};

#endif // MEDIAANALYZERDIALOG_H
