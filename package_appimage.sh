#!/bin/bash
set -e

BASE_DIR="/home/tonibu/.gemini/antigravity/scratch/YTMediaDownloader"
QT_DIR="${BASE_DIR}/YTMediaDownloader_Qt"
STAGING_DIR="/tmp/YTMediaDownloader_AppDir"
OUT_DIR="${BASE_DIR}/YTMediaDownloader"

echo "[1/6] Preparazione cartella AppDir in ${STAGING_DIR}..."
rm -rf "${STAGING_DIR}"
mkdir -p "${STAGING_DIR}/usr/bin"
mkdir -p "${STAGING_DIR}/usr/lib"
mkdir -p "${STAGING_DIR}/usr/bin/localizzazione"
mkdir -p "${STAGING_DIR}/usr/bin/docs/user-guide"
mkdir -p "${STAGING_DIR}/usr/bin/Resources/icons"

echo "[2/6] Copia binari ed eseguibili..."
cp "${QT_DIR}/YTMediaDownloader" "${STAGING_DIR}/usr/bin/"
chmod +x "${STAGING_DIR}/usr/bin/YTMediaDownloader"

# Bundling di ffmpeg, ffprobe, yt-dlp
cp /usr/bin/ffmpeg "${STAGING_DIR}/usr/bin/"
cp /usr/bin/ffprobe "${STAGING_DIR}/usr/bin/"
cp /usr/bin/yt-dlp "${STAGING_DIR}/usr/bin/"
chmod +x "${STAGING_DIR}/usr/bin/ffmpeg" "${STAGING_DIR}/usr/bin/ffprobe" "${STAGING_DIR}/usr/bin/yt-dlp"

echo "[3/6] Copia librerie FFmpeg..."
cp -a /usr/lib/libavcodec.so* "${STAGING_DIR}/usr/lib/" 2>/dev/null || true
cp -a /usr/lib/libavformat.so* "${STAGING_DIR}/usr/lib/" 2>/dev/null || true
cp -a /usr/lib/libavutil.so* "${STAGING_DIR}/usr/lib/" 2>/dev/null || true
cp -a /usr/lib/libavfilter.so* "${STAGING_DIR}/usr/lib/" 2>/dev/null || true
cp -a /usr/lib/libavdevice.so* "${STAGING_DIR}/usr/lib/" 2>/dev/null || true
cp -a /usr/lib/libswresample.so* "${STAGING_DIR}/usr/lib/" 2>/dev/null || true
cp -a /usr/lib/libswscale.so* "${STAGING_DIR}/usr/lib/" 2>/dev/null || true

echo "[4/6] Copia risorse, guide e localizzazione..."
cp "${QT_DIR}/localizzazione/"*.rsc "${STAGING_DIR}/usr/bin/localizzazione/"
cp -r "${QT_DIR}/docs/user-guide/"* "${STAGING_DIR}/usr/bin/docs/user-guide/"
cp -r "${QT_DIR}/Resources/icons" "${STAGING_DIR}/usr/bin/Resources/"
cp "${QT_DIR}/Resources/icons/icon.png" "${STAGING_DIR}/YTMediaDownloader.png"

echo "[5/6] Generazione AppRun e YTMediaDownloader.desktop..."
cat << 'EOF' > "${STAGING_DIR}/AppRun"
#!/bin/sh
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH_ORIG="${LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
exec "${HERE}/usr/bin/YTMediaDownloader" "$@"
EOF
chmod +x "${STAGING_DIR}/AppRun"

cat << 'EOF' > "${STAGING_DIR}/YTMediaDownloader.desktop"
[Desktop Entry]
Type=Application
Name=YT Media Downloader Pro
GenericName=YouTube Media Downloader
Comment=Scarica audio e video multitraccia da YouTube con IA
Exec=YTMediaDownloader %u
Icon=YTMediaDownloader
Terminal=false
Categories=AudioVideo;Video;Audio;Network;Qt;
StartupNotify=true
Keywords=youtube;downloader;video;audio;media;
EOF

echo "[6/6] Creazione pacchetto AppImage con appimagetool..."
ARCH=x86_64 appimagetool "${STAGING_DIR}" "${OUT_DIR}/YTMediaDownloader.AppImage"
chmod +x "${OUT_DIR}/YTMediaDownloader.AppImage"

echo "=== BUILD COMPLETATA CON SUCCESSO: ${OUT_DIR}/YTMediaDownloader.AppImage ==="
