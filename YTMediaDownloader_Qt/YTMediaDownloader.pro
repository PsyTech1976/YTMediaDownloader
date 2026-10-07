QT += core gui widgets xml

CONFIG += c++17

TARGET = YTMediaDownloader
TEMPLATE = app

# Include paths
INCLUDEPATH += $$PWD

# Documentazione ed altri file tracciati nel progetto Qt
OTHER_FILES += \
    DevLog.txt \
    MainWindow/MainWindow_ref.txt \
    MediaAnalyzerDialog/MediaAnalyzerDialog_ref.txt \
    SettingsDialog/SettingsDialog_ref.txt \
    CoreApp/IO/IO_ref.txt \
    CoreApp/FFmpeg/FFmpeg_ref.txt \
    CoreApp/Localization/Localization_ref.txt \
    CoreApp/Preferenze/Preferenze_ref.txt \
    CoreApp/DevLog/DevLog_ref.txt \
    localizzazione/it_IT.rsc \
    localizzazione/en_EN.rsc \
    localizzazione/es_ES.rsc \
    localizzazione/fr_FR.rsc \
    localizzazione/de_DE.rsc \
    docs/user-guide/index.html \
    docs/user-guide/guide_it.html \
    docs/user-guide/guide_en.html \
    docs/user-guide/guide_fr.html \
    docs/user-guide/guide_de.html \
    docs/user-guide/guide_es.html \
    docs/user-guide/style.css

SOURCES += \
    main.cpp \
    MainWindow/MainWindow.cpp \
    MediaAnalyzerDialog/MediaAnalyzerDialog.cpp \
    SettingsDialog/SettingsDialog.cpp \
    HelpDialog/HelpDialog.cpp \
    AboutDialog/AboutDialog.cpp \
    CoreApp/IO/FileManager.cpp \
    CoreApp/IO/DesktopIntegrationManager.cpp \
    CoreApp/FFmpeg/FFmpegManager.cpp \
    CoreApp/Localization/LocalizationManager.cpp \
    CoreApp/Preferenze/SettingsManager.cpp \
    CoreApp/DevLog/DevLogLogger.cpp

HEADERS += \
    MainWindow/MainWindow.h \
    MediaAnalyzerDialog/MediaAnalyzerDialog.h \
    SettingsDialog/SettingsDialog.h \
    HelpDialog/HelpDialog.h \
    AboutDialog/AboutDialog.h \
    CoreApp/IO/FileManager.h \
    CoreApp/IO/DesktopIntegrationManager.h \
    CoreApp/FFmpeg/FFmpegManager.h \
    CoreApp/Localization/LocalizationManager.h \
    CoreApp/Preferenze/SettingsManager.h \
    CoreApp/DevLog/DevLogLogger.h

FORMS += \
    MainWindow/MainWindow.ui \
    MediaAnalyzerDialog/MediaAnalyzerDialog.ui \
    SettingsDialog/SettingsDialog.ui

RESOURCES += \
    Resources/resources.qrc
