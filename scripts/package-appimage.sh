#!/bin/bash
set -euo pipefail

# AppImage packaging script for Linux
# Usage: ./scripts/package-appimage.sh [APP_NAME] [APP_DIR] [QML_SOURCES_PATHS]

APP_NAME="${1:-standard_of_iron}"
APP_DIR="${2:-build/bin}"
QML_SOURCES_PATHS="${3:-ui/qml}"

# linuxdeploy is pinned to a tagged release rather than the `continuous`
# channel. A release must build the same way twice, and `continuous` moves
# under us: an upstream regression would otherwise fail a release for reasons
# that have nothing to do with this repository. Checksums are verified because
# a pinned tag alone still trusts whatever the URL happens to serve.
#
# To bump: change the tag, run the download by hand, and record the new digest.
LINUXDEPLOY_TAG="1-alpha-20251107-1"
LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
LINUXDEPLOY_QT_TAG="1-alpha-20250213-1"
LINUXDEPLOY_QT_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"

# The tools are AppImages themselves. Keeping them out of the working
# directory is what lets the caller glob for `*.AppImage` and get exactly the
# package that was just built -- with them alongside, a release job globbing
# for its own output picks up linuxdeploy instead.
TOOL_DIR=".linuxdeploy-tools"
mkdir -p "${TOOL_DIR}"

fetch_tool() {
  local url="$1" out="$2" want="$3"

  echo "--- ${out}"
  curl -fsSL --retry 3 --retry-delay 5 -o "${out}" "${url}"

  local got
  got="$(sha256sum "${out}" | cut -d' ' -f1)"
  if [ "${got}" != "${want}" ]; then
    echo "error: ${out} failed checksum verification" >&2
    echo "  expected ${want}" >&2
    echo "  actual   ${got}" >&2
    exit 1
  fi
  chmod +x "${out}"
}

echo "=== Downloading pinned linuxdeploy tools ==="
fetch_tool \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_TAG}/linuxdeploy-x86_64.AppImage" \
  "${TOOL_DIR}/linuxdeploy-x86_64.AppImage" \
  "${LINUXDEPLOY_SHA256}"
fetch_tool \
  "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${LINUXDEPLOY_QT_TAG}/linuxdeploy-plugin-qt-x86_64.AppImage" \
  "${TOOL_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage" \
  "${LINUXDEPLOY_QT_SHA256}"

# linuxdeploy finds its plugins by scanning PATH for `linuxdeploy-plugin-*`.
export PATH="${PWD}/${TOOL_DIR}:${PATH}"

# The Qt SDK can ship SQL drivers for optional databases whose proprietary
# client libraries are not present on the runner. The game only uses SQLite,
# so give linuxdeploy-plugin-qt a sanitized plugin tree containing that driver
# while leaving the rest of the Qt installation untouched.
QMAKE_BIN="$(command -v qmake || true)"
if [ -z "${QMAKE_BIN}" ]; then
  echo "error: qmake is required for Qt deployment" >&2
  exit 1
fi

QT_PLUGIN_SOURCE="$("${QMAKE_BIN}" -query QT_INSTALL_PLUGINS)"
QT_PLUGIN_ROOT="$(mktemp -d)"
trap 'rm -rf "${QT_PLUGIN_ROOT}"' EXIT
cp -a "${QT_PLUGIN_SOURCE}/." "${QT_PLUGIN_ROOT}/"

if [ ! -s "${QT_PLUGIN_ROOT}/sqldrivers/libqsqlite.so" ]; then
  echo "error: Qt SQLite SQL driver is missing from ${QT_PLUGIN_SOURCE}" >&2
  exit 1
fi
find "${QT_PLUGIN_ROOT}/sqldrivers" \
  -maxdepth 1 \
  \( -type f -o -type l \) \
  -name 'libqsql*.so' \
  ! -name 'libqsqlite.so' \
  -delete

QMAKE_WRAPPER="${QT_PLUGIN_ROOT}/qmake-sqlite-only"
cat >"${QMAKE_WRAPPER}" <<EOF
#!/bin/sh
if [ "\${1-}" = "-query" ]; then
  "${QMAKE_BIN}" "\$@" | sed 's|^QT_INSTALL_PLUGINS:.*|QT_INSTALL_PLUGINS:${QT_PLUGIN_ROOT}|'
else
  exec "${QMAKE_BIN}" "\$@"
fi
EOF
chmod +x "${QMAKE_WRAPPER}"
export QMAKE="${QMAKE_WRAPPER}"

echo "=== Preparing AppDir ==="
mkdir -p AppDir/usr/bin
cp "${APP_DIR}/${APP_NAME}" AppDir/usr/bin/

mkdir -p AppDir/usr/share/applications
cp dist/linux/standard_of_iron.desktop AppDir/usr/share/applications/

mkdir -p AppDir/usr/share/icons/hicolor/1024x1024/apps
cp dist/linux/standard_of_iron.png AppDir/usr/share/icons/hicolor/1024x1024/apps/

echo "=== Copying assets ==="
rsync -a assets/ AppDir/usr/bin/assets/

# Qt is LGPL v3, so its notice has to travel with the binary rather than living
# only in the source repository. The music additionally carries a
# non-commercial restriction, which a player can only discover if the file that
# records it is in the package they downloaded.
echo "=== Copying licences ==="
mkdir -p AppDir/usr/share/doc/standard_of_iron
cp LICENSE THIRD_PARTY_LICENSES.md AppDir/usr/share/doc/standard_of_iron/
cp LICENSE THIRD_PARTY_LICENSES.md AppDir/usr/bin/
for licence in LICENSE THIRD_PARTY_LICENSES.md; do
  test -s "AppDir/usr/bin/${licence}"
  test -s "AppDir/usr/share/doc/standard_of_iron/${licence}"
done

echo "=== Building AppImage ==="
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS}"
"${TOOL_DIR}/linuxdeploy-x86_64.AppImage" --appdir AppDir --plugin qt --output appimage

echo "=== AppImage creation complete ==="
ls -lh ./*.AppImage
