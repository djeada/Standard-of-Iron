#!/bin/bash
# Developer ID signing, notarization and stapling for the macOS release.
#
# build-macos.yml runs the two modes in this order:
#
#   app <path/to/standard_of_iron.app>
#       Sign nested code from the inside out (dylibs, frameworks, plugins),
#       then the main executable and the bundle, using the hardened runtime
#       and dist/macos/standard_of_iron.entitlements. Submit a ZIP of that
#       exact bundle to Apple and require "Accepted". Staple the ticket to the
#       bundle and verify it with codesign, stapler and spctl. This app is what
#       the Steam depot ships, and what the DMG is built from.
#
#   dmg <path/to/image.dmg>
#       Sign, notarize and staple the disk image built from the stapled app,
#       then verify it.
#
# The DMG used to be created *before* the app was Developer-ID-signed, so the
# app inside the published image was the earlier ad-hoc copy. Running the two
# modes separately, with DMG creation between them, prevents that.
#
# Credentials (environment):
#   MACOS_CERTIFICATE           base64 .p12 holding a Developer ID Application identity
#   MACOS_CERTIFICATE_PASSWORD  password for the .p12
#   MACOS_KEYCHAIN_PASSWORD     optional; a random one is generated otherwise
#   APPLE_ID, APPLE_ID_PASSWORD, APPLE_TEAM_ID   notarytool credentials
#
# Policy (environment):
#   SOI_REQUIRE_SIGNING=true    missing or partial credentials are an error.
#                               Steam release candidates set this; a Steam Mac
#                               build must never fall back to ad-hoc signing.
#   otherwise                   missing credentials skip with a notice, and the
#                               ad-hoc signature applied earlier stays.
#
# Writes "signed=true|false" to $GITHUB_OUTPUT when that is set.
#
# usage: scripts/sign-and-notarize-macos.sh app|dmg <path>

set -euo pipefail

MODE="${1:-}"
TARGET="${2:-}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENTITLEMENTS="${REPO_ROOT}/dist/macos/standard_of_iron.entitlements"
REQUIRE="${SOI_REQUIRE_SIGNING:-false}"
WORK="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"

usage() {
  echo "usage: $0 app|dmg <path>" >&2
  exit 2
}

emit_output() {
  if [ -n "${GITHUB_OUTPUT:-}" ]; then
    echo "signed=$1" >>"${GITHUB_OUTPUT}"
  fi
}

case "${MODE}" in
  app) [ -d "${TARGET}" ] || {
    echo "error: app bundle not found: ${TARGET}" >&2
    exit 1
  } ;;
  dmg) [ -f "${TARGET}" ] || {
    echo "error: disk image not found: ${TARGET}" >&2
    exit 1
  } ;;
  *) usage ;;
esac

missing=()
for name in MACOS_CERTIFICATE MACOS_CERTIFICATE_PASSWORD APPLE_ID APPLE_ID_PASSWORD APPLE_TEAM_ID; do
  if [ -z "${!name:-}" ]; then
    missing+=("${name}")
  fi
done

if [ "${#missing[@]}" -gt 0 ]; then
  if [ "${REQUIRE}" = "true" ]; then
    echo "error: SOI_REQUIRE_SIGNING is set but these are missing: ${missing[*]}" >&2
    echo "       A production Mac build must be Developer-ID-signed and notarized." >&2
    exit 1
  fi
  # Signing without notarization produces an app that Gatekeeper still
  # blocks. That is worse than an honest ad-hoc build: it looks finished and
  # is not.
  echo "notice: macOS signing/notarization credentials incomplete (${missing[*]});"
  echo "        leaving the ad-hoc signature in place."
  emit_output false
  exit 0
fi

KEYCHAIN_PATH="${WORK}/soi-signing-$$.keychain-db"
CERT_PATH="${WORK}/soi-signing-$$.p12"
KEYCHAIN_PASSWORD="${MACOS_KEYCHAIN_PASSWORD:-$(uuidgen)}"
ORIGINAL_KEYCHAINS="$(security list-keychains -d user | sed 's/"//g')"

cleanup() {
  rm -f "${CERT_PATH}" "${WORK}/soi-notarize-$$.zip"
  # shellcheck disable=SC2086
  security list-keychains -d user -s ${ORIGINAL_KEYCHAINS} >/dev/null 2>&1 || true
  security delete-keychain "${KEYCHAIN_PATH}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

security create-keychain -p "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}"
security set-keychain-settings -lut 21600 "${KEYCHAIN_PATH}"
security unlock-keychain -p "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}"
echo "${MACOS_CERTIFICATE}" | base64 --decode >"${CERT_PATH}"
security import "${CERT_PATH}" -k "${KEYCHAIN_PATH}" \
  -P "${MACOS_CERTIFICATE_PASSWORD}" -T /usr/bin/codesign
rm -f "${CERT_PATH}"
security set-key-partition-list -S apple-tool:,apple:,codesign: -s \
  -k "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_PATH}" >/dev/null
# shellcheck disable=SC2086
security list-keychains -d user -s "${KEYCHAIN_PATH}" ${ORIGINAL_KEYCHAINS}

IDENTITY="$(security find-identity -v -p codesigning "${KEYCHAIN_PATH}" |
  sed -n 's/.*"\(Developer ID Application:.*\)"/\1/p' | head -1)"
if [ -z "${IDENTITY}" ]; then
  echo "error: no Developer ID Application identity in the certificate" >&2
  exit 1
fi
echo "Signing identity: ${IDENTITY}"

sign() {
  codesign --force --timestamp --options runtime --sign "${IDENTITY}" "$@"
}

notarize_and_staple() {
  local path="$1" submission="$1"
  if [ -d "${path}" ]; then
    # notarytool accepts a ZIP, DMG or PKG. The ZIP is made from this exact
    # bundle, and the ticket is stapled back onto the same bundle.
    submission="${WORK}/soi-notarize-$$.zip"
    ditto -c -k --keepParent "${path}" "${submission}"
  fi

  echo "Submitting $(basename "${path}") for notarization..."
  local output status=0
  output="$(xcrun notarytool submit "${submission}" \
    --apple-id "${APPLE_ID}" --password "${APPLE_ID_PASSWORD}" \
    --team-id "${APPLE_TEAM_ID}" --wait --timeout 30m 2>&1)" || status=$?
  echo "${output}"

  local id
  id="$(echo "${output}" | awk '/^ *id:/ {print $2; exit}')"
  if [ "${status}" -ne 0 ] || ! echo "${output}" | grep -q "status: Accepted"; then
    echo "error: notarization of $(basename "${path}") was not accepted" >&2
    if [ -n "${id}" ]; then
      xcrun notarytool log "${id}" --apple-id "${APPLE_ID}" \
        --password "${APPLE_ID_PASSWORD}" --team-id "${APPLE_TEAM_ID}" || true
    fi
    exit 1
  fi
  echo "Notarization accepted (submission ${id})"

  xcrun stapler staple "${path}"
  xcrun stapler validate "${path}"
}

if [ "${MODE}" = "app" ]; then
  APP="${TARGET}"
  [ -f "${ENTITLEMENTS}" ] || {
    echo "error: ${ENTITLEMENTS} is missing" >&2
    exit 1
  }

  # Sign from the inside out: a container's signature seals the signatures of
  # the code it holds, so nested code is signed before its container.
  # Standalone dylibs first, then frameworks as bundles, then plugins.
  while IFS= read -r -d '' dylib; do
    sign "${dylib}"
  done < <(find "${APP}/Contents/Frameworks" -type f -name '*.dylib' -print0 2>/dev/null)
  while IFS= read -r -d '' framework; do
    sign "${framework}"
  done < <(find "${APP}/Contents/Frameworks" -maxdepth 1 -type d -name '*.framework' -print0 2>/dev/null)
  while IFS= read -r -d '' plugin; do
    sign "${plugin}"
  done < <(find "${APP}/Contents/PlugIns" -type f -name '*.dylib' -print0 2>/dev/null)
  # After macdeployqt, the QML plugins live under Resources/qml.
  while IFS= read -r -d '' plugin; do
    sign "${plugin}"
  done < <(find "${APP}/Contents/Resources" -type f -name '*.dylib' -print0 2>/dev/null)

  # The entitlements go on the executable and the bundle, the code the Steam
  # overlay is injected into, and nowhere else.
  executable="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "${APP}/Contents/Info.plist")"
  sign --entitlements "${ENTITLEMENTS}" "${APP}/Contents/MacOS/${executable}"
  sign --entitlements "${ENTITLEMENTS}" "${APP}"

  codesign --verify --deep --strict --verbose=2 "${APP}"
  codesign --display --entitlements - "${APP}" >"${WORK}/soi-entitlements.txt" 2>&1
  cat "${WORK}/soi-entitlements.txt"
  for entitlement in com.apple.security.cs.disable-library-validation \
    com.apple.security.cs.allow-dyld-environment-variables; do
    grep -q "${entitlement}" "${WORK}/soi-entitlements.txt" || {
      echo "error: ${entitlement} did not reach the signed app" >&2
      exit 1
    }
  done
  if grep -q "com.apple.security.app-sandbox" "${WORK}/soi-entitlements.txt"; then
    echo "error: the Steam build must not be sandboxed" >&2
    exit 1
  fi

  notarize_and_staple "${APP}"

  # The final word: what a player's Gatekeeper will say about this bundle.
  codesign --verify --deep --strict --verbose=2 "${APP}"
  spctl --assess --type execute --verbose=4 "${APP}"
else
  DMG="${TARGET}"
  sign "${DMG}"
  codesign --verify --verbose=2 "${DMG}"
  notarize_and_staple "${DMG}"
  spctl --assess --type open --context context:primary-signature --verbose=4 "${DMG}"
fi

emit_output true
echo "Developer ID signing, notarization and stapling complete: ${TARGET}"
