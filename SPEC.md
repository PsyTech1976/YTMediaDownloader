# SPEC.md - Specifiche Tecniche e Architetturali di YTMediaDownloader Pro

## 1. Panoramica del Progetto (Project Overview)

**YTMediaDownloader Pro** è un'applicazione desktop moderna basata su **C++17** e framework **Qt 5** (Widgets) progettata per l'analisi avanzata, la selezione dettagliata dei flussi e il download multiplexato ad alta fedeltà di video, tracce audio multiple e sottotitoli da YouTube e piattaforme connesse.
Il software è un **software realizzato con IA su un'idea di PsyTech6**.

L'applicazione supporta:
- **Paternità ed Identità**: Realizzato con intelligenza artificiale su concept originale di PsyTech6, con finestra "Informazioni sul Software" dedicata (`AboutDialog`).
- **Analisi approfondita e resilient downloading**: Estrazione JSON con bypass delle restrizioni DRM, formati non supportati (-drc) e download con preservazione fedele delle tracce audio selezionate (priorità ai flussi diretti HTTPS ad alta qualità e ordinamento per bitrate).
- **Selezione personalizzata avanzata**: Formati video fino a 4K/8K a 60fps, contenitori MP4/MKV/WebM, audio multitraccia prioritario per lingua originale e localizzata, e sottotitoli multipli.
- **Worker Thread asincrono (`QThread`)**: Elaborazione FFmpegManager disaccoppiata dall'interfaccia grafica per garantire reattività fluida, con terminazione ad albero dei processi (`killProcessTree` via SIGKILL/pkill) e rimozione pulita dei file temporanei in caso di interruzione.
- **Barra dei menu (`QMenuBar`) e scorciatoie**: Menu File, Opzioni, Lingua e Aiuto con scorciatoie complete (`Ctrl+V`, `Ctrl+D`, `Esc`, `Ctrl+O`, `Ctrl+F`, `Ctrl+E`, `Ctrl+Q`, `F1`, `Ctrl+I`).
- **Integrazione di sistema (`DesktopIntegrationManager`)**: Pulsante toggle rapido per abilitare o disabilitare la visibilità dell'applicazione nei menu di avvio e nella ricerca del sistema operativo desktop (XDG Freedesktop `.desktop`).
- **Focus automatico all'avvio**: Il cursore di testo si posiziona automaticamente nel campo link URL (`txtUrl`) pronto per l'incolla immediato.
- **Localizzazione con bandiere nazionali**: Selezione delle 5 lingue supportate (🇮🇹 Italiano, 🇬🇧 English, 🇫🇷 Français, 🇩🇪 Deutsch, 🇪🇸 Español) con bandiere visibili in tutti i menu di scelta (popup, barra dei menu, impostazioni, guida).
- **Guida Utente HTML multilingua**: Finestra separata (`HelpDialog`) sincronizzata con la lingua del programma, con selettore popup e possibilità di apertura nel browser.
- **AppImage Standalone con FFmpeg integrato**: Pacchetto portabile completamente autosufficiente con bundling dei binari e librerie `ffmpeg`, `ffprobe`, `yt-dlp`.
- **Copia di distribuzione su Scrivania**: Generazione automatica di `YTMediaDownloader.AppImage` direttamente sulla cartella Scrivania dell'utente.

---

## 2. Struttura del Repository e Architettura del Codice

```
YTMediaDownloader/
├── SPEC.md                               # Specifiche tecniche del progetto (questo file)
├── DevLog.txt                            # Registro cronologico delle modifiche e degli eventi
├── YTMediaDownloader/                    # Cartella di distribuzione standalone
│   ├── YTMediaDownloader                 # Binario ELF compilato
│   ├── YTMediaDownloader.AppImage        # Pacchetto portabile completo
│   ├── docs/user-guide/                  # Documentazione HTML multilingua
│   ├── localizzazione/                   # File di traduzione XML/RSC (IT, EN, FR, DE, ES)
│   └── Resources/icons/                  # Icone applicazione
└── YTMediaDownloader_Qt/                 # Progetto sorgente Qt 5 / C++17
    ├── YTMediaDownloader.pro             # File di configurazione qmake
    ├── main.cpp                          # Inizializzazione, lingua e QLockFile istanza singola
    ├── MainWindow/                       # Controller UI principale, MenuBar, Focus
    │   ├── MainWindow.h
    │   ├── MainWindow.cpp
    │   └── MainWindow.ui
    ├── AboutDialog/                      # Finestra Info con attribuzione PsyTech6 & IA
    │   ├── AboutDialog.h
    │   └── AboutDialog.cpp
    ├── MediaAnalyzerDialog/              # Dialog modale per selezione risoluzioni/tracce/sottotitoli
    │   ├── MediaAnalyzerDialog.h
    │   ├── MediaAnalyzerDialog.cpp
    │   └── MediaAnalyzerDialog.ui
    ├── SettingsDialog/                   # Dialog preferenze generali e cartelle
    │   ├── SettingsDialog.h
    │   ├── SettingsDialog.cpp
    │   └── SettingsDialog.ui
    ├── HelpDialog/                       # Finestra Guida Utente HTML multilingua
    │   ├── HelpDialog.h
    │   └── HelpDialog.cpp
    ├── docs/user-guide/                  # Guide HTML in 5 lingue e foglio di stile CSS
    │   ├── index.html
    │   ├── guide_it.html
    │   ├── guide_en.html
    │   ├── guide_fr.html
    │   ├── guide_de.html
    │   ├── guide_es.html
    │   └── style.css
    ├── CoreApp/
    │   ├── FFmpeg/                       # Motore asincrono FFmpeg, download e muxing
    │   │   ├── FFmpegManager.h
    │   │   └── FFmpegManager.cpp
    │   ├── IO/                           # Gestione filesystem e integrazione desktop OS
    │   │   ├── FileManager.h
    │   │   ├── FileManager.cpp
    │   │   ├── DesktopIntegrationManager.h
    │   │   └── DesktopIntegrationManager.cpp
    │   ├── Localization/                 # Gestore internazionalizzazione e bandiere
    │   │   ├── LocalizationManager.h
    │   │   └── LocalizationManager.cpp
    │   ├── Preferenze/                   # Persistenza preferenze utente (QSettings)
    │   │   ├── SettingsManager.h
    │   │   └── SettingsManager.cpp
    │   └── DevLog/                       # Logger eventi di sessione
    │       ├── DevLogLogger.h
    │       └── DevLogLogger.cpp
    ├── Resources/                        # Risorse incorporate Qt
    │   ├── resources.qrc
    │   └── icons/icon.png
    └── localizzazione/                   # File XML di localizzazione .rsc
        ├── it_IT.rsc
        ├── en_EN.rsc
        ├── fr_FR.rsc
        ├── de_DE.rsc
        └── es_ES.rsc
```

---

## 3. Componenti Chiave

### 3.1 DesktopIntegrationManager (`CoreApp/IO/DesktopIntegrationManager.h`)
Gestisce la registrazione dell'applicazione nei menu del desktop Linux in conformità alle specifiche freedesktop.org XDG:
- `isIntegrated()`: Verifica se `~/.local/share/applications/YTMediaDownloader.desktop` è presente.
- `setIntegrated(bool enable)`: Crea o rimuove il file `.desktop` configurando il percorso di `Exec` (con supporto dinamico della variabile d'ambiente `$APPIMAGE` se eseguito come AppImage) e l'icona applicativa `ytmediadownloader.png`.
- Esegue in background `update-desktop-database ~/.local/share/applications/` per rendere immediatamente visibile o rimuovere l'applicazione dai launcher di sistema (GNOME, KDE Plasma, XFCE).

### 3.2 AboutDialog (`AboutDialog/AboutDialog.h`)
Finestra modale informativa che visualizza:
- Logo ad alta risoluzione e titolo dell'applicazione: "YT Media Downloader Pro".
- Versione software (1.5.0) e stack tecnologico (Qt 5, C++17, FFmpeg, yt-dlp).
- Paternità e crediti formali: **"Software realizzato con IA su un'idea di PsyTech6."**

### 3.3 Barra dei Menu (`QMenuBar`)
Implementata in `MainWindow::setupMenuBar()` con collegamenti completi a:
- **File**: Incolla e Analizza Link (`Ctrl+V`), Avvia Download (`Ctrl+D`), Blocca Processo (`Esc`), Apri File Scaricato (`Ctrl+O`), Mostra nella Cartella (`Ctrl+F`), Esci (`Ctrl+Q`).
- **Opzioni**: Modifica Opzioni Video/Audio... (`Ctrl+E`), Visibilità nei Menu di Sistema (azione con spunta sincronizzata con il pulsante header), Impostazioni... (`Ctrl+,`).
- **Lingua**: Gruppo di azioni esclusive con icone grafiche PNG delle bandiere (IT, EN, FR, DE, ES) incorporate nelle risorse Qt (`:/icons/flags/`).
- **Aiuto**: Guida Utente (`F1`), Informazioni sul Software... (`Ctrl+I`).

### 3.4 Focus del Cursore all'Avvio
In `MainWindow::MainWindow` e `MainWindow::showEvent(QShowEvent *event)` viene eseguita la chiamata esplicita `ui->txtUrl->setFocus();`, garantendo che alla prima apparizione a schermo il cursore sia immediatamente posizionato nel campo di input dell'URL.

### 3.5 Privacy e Sicurezza dei Percorsi
- Nessun dato sensibile, username di sistema, token o password salvati nel codice sorgente.
- Tutte le cartelle di default e cache utilizzano `QStandardPaths` e `QDir::homePath()`.

### 3.6 Gestione e Integrazione Desktop: "Mostra nella Cartella"
- **Metodo Dedicato `MainWindow::openDownloadLocation()`**:
  - Centralizza la logica di visualizzazione della destinazione o del file multimediale scaricato.
  - **Modalità 1 (Nessun file scaricato)**: apre la cartella di download predefinita o selezionata nel file manager del desktop mostrandone il contenuto.
  - **Modalità 2 (File scaricato)**: se il file è presente su disco, apre il gestore file di sistema ed evidenzia/seleziona direttamente il file.
  - Mantiene il pulsante `btnOpenFolder` sempre attivo in tutti gli stati dell'applicazione.
  - Persistenza della cronologia dell'ultimo download (`lastDownloadedFilePath`) tramite `SettingsManager`, preservando la selezione del file anche al riavvio dell'applicazione.
  - Feedback visivo istantaneo sulla barra di stato (`lblStatus`).
- **Esecuzione Asincrona e Sanificazione AppImage (`FileManager::runDetachedCommand`)**:
  - Esegue i comandi desktop in modo non bloccante (`QProcess::startDetached`).
  - Sanifica `LD_LIBRARY_PATH` e percorsi interni AppImage (`$APPDIR`), ripristinando `LD_LIBRARY_PATH_ORIG` per evitare conflitti di librerie con i file manager di sistema (Dolphin su KDE Plasma, Nautilus su GNOME, Nemo, Caja).
  - Utilizza `--select` nativo su Dolphin e Nautilus con fallback non bloccante all'interfaccia D-Bus `ShowItems`.

---

## 4. Packaging Standalone e Rilascio AppImage

L'AppImage `YTMediaDownloader.AppImage` include nativamente:
1. Il binario `YTMediaDownloader` compilato con C++17 e Qt 5.
2. I binari `yt-dlp`, `ffmpeg`, `ffprobe` inseriti in `usr/bin/`.
3. Le librerie condivise audio/video (`libavdevice`, `libavfilter`, `libavformat`, `libavcodec`, `libavutil`, `libswresample`, `libswscale`) in `usr/lib/`.
4. Script di bootstrap `AppRun` che imposta dinamicamente e preserva l'ambiente originale:
   ```sh
   HERE="$(dirname "$(readlink -f "${0}")")"
   export PATH="${HERE}/usr/bin:${PATH}"
   export LD_LIBRARY_PATH_ORIG="${LD_LIBRARY_PATH}"
   export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
   exec "${HERE}/usr/bin/YTMediaDownloader" "$@"
   ```
5. Una copia eseguibile dell'AppImage viene posizionata sulla cartella Scrivania dell'utente (`/home/tonibu/Scrivania/YTMediaDownloader.AppImage`).
