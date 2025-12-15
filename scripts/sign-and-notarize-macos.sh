#!/bin/bash
set -e

# macOS Code Signing and Notarization Script
# This script signs a .app bundle with Developer ID, notarizes with Apple, and staples the ticket to a DMG
#
# Required Environment Variables:
#   MACOS_CERTIFICATE          - Base64-encoded .p12 certificate
#   MACOS_CERTIFICATE_PASSWORD - Password for the .p12 certificate
#   MACOS_KEYCHAIN_PASSWORD    - Password for temporary keychain
#   APPLE_ID                   - Apple ID email for notarization
#   APPLE_ID_PASSWORD          - App-specific password for Apple ID
#   APPLE_TEAM_ID              - 10-character Apple Developer Team ID
#
# Optional Environment Variables:
#   SKIP_SIGNING               - If set to "true", skip all signing/notarization

# Usage: ./sign-and-notarize-macos.sh <app_path> <dmg_path>
# Example: ./sign-and-notarize-macos.sh build/bin/standard_of_iron.app standard_of_iron-macos.dmg

APP_PATH="$1"
DMG_PATH="$2"

if [ -z "$APP_PATH" ] || [ -z "$DMG_PATH" ]; then
    echo "Usage: $0 <app_path> <dmg_path>"
    echo "Example: $0 build/bin/standard_of_iron.app standard_of_iron-macos.dmg"
    exit 1
fi

# Check if signing should be skipped
if [ "$SKIP_SIGNING" = "true" ]; then
    echo "ℹ️  SKIP_SIGNING is set to true, skipping signing and notarization"
    exit 0
fi

# Check if required credentials are present
if [ -z "$MACOS_CERTIFICATE" ] || [ -z "$MACOS_CERTIFICATE_PASSWORD" ]; then
    echo "ℹ️  macOS signing credentials not found, skipping signing and notarization"
    echo "   To enable signing, set MACOS_CERTIFICATE and MACOS_CERTIFICATE_PASSWORD secrets"
    exit 0
fi

if [ -z "$APPLE_ID" ] || [ -z "$APPLE_ID_PASSWORD" ] || [ -z "$APPLE_TEAM_ID" ]; then
    echo "⚠️  Warning: Signing credentials present but notarization credentials missing"
    echo "   Will sign the app but skip notarization"
    echo "   To enable notarization, set APPLE_ID, APPLE_ID_PASSWORD, and APPLE_TEAM_ID secrets"
    SKIP_NOTARIZATION=true
fi

# Verify paths exist
if [ ! -d "$APP_PATH" ]; then
    echo "❌ Error: App bundle not found at $APP_PATH"
    exit 1
fi

if [ ! -f "$DMG_PATH" ]; then
    echo "❌ Error: DMG not found at $DMG_PATH"
    exit 1
fi

echo "🔐 Starting macOS code signing and notarization process..."
echo "   App: $APP_PATH"
echo "   DMG: $DMG_PATH"

# Create temporary keychain
KEYCHAIN_PATH="$RUNNER_TEMP/app-signing.keychain-db"
KEYCHAIN_PASSWORD="${MACOS_KEYCHAIN_PASSWORD:-$(uuidgen)}"

echo "🔑 Creating temporary keychain..."
security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"
security set-keychain-settings -lut 21600 "$KEYCHAIN_PATH"
security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"

# Decode and import certificate
echo "📜 Importing signing certificate..."
CERT_PATH="$RUNNER_TEMP/certificate.p12"
echo "$MACOS_CERTIFICATE" | base64 --decode > "$CERT_PATH"
security import "$CERT_PATH" -k "$KEYCHAIN_PATH" -P "$MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN_PATH"

# Set the keychain as the default for codesign operations
# shellcheck disable=SC2046
security list-keychains -d user -s "$KEYCHAIN_PATH" $(security list-keychains -d user | sed 's/"//g')

# Find the Developer ID Application certificate
CERTIFICATE_NAME=$(security find-identity -v -p codesigning "$KEYCHAIN_PATH" | grep "Developer ID Application" | head -1 | sed 's/.*"\(.*\)"/\1/')

if [ -z "$CERTIFICATE_NAME" ]; then
    echo "❌ Error: Developer ID Application certificate not found in keychain"
    security delete-keychain "$KEYCHAIN_PATH" || true
    rm -f "$CERT_PATH"
    exit 1
fi

echo "✅ Found certificate: $CERTIFICATE_NAME"

# Sign all frameworks and dylibs in the app bundle first (deep signing)
echo "🔏 Signing frameworks and libraries..."
find "$APP_PATH/Contents/Frameworks" \( -name "*.framework" -o -name "*.dylib" \) 2>/dev/null | while read -r framework; do
    echo "  Signing: $framework"
    if ! codesign --force --verify --verbose --timestamp \
        --options runtime \
        --sign "$CERTIFICATE_NAME" \
        "$framework" 2>&1; then
        echo "  ⚠️  Warning: Failed to sign $framework, continuing..."
    fi
done

# Sign Qt plugins
if [ -d "$APP_PATH/Contents/PlugIns" ]; then
    echo "🔏 Signing Qt plugins..."
    find "$APP_PATH/Contents/PlugIns" -name "*.dylib" 2>/dev/null | while read -r plugin; do
        echo "  Signing: $plugin"
        if ! codesign --force --verify --verbose --timestamp \
            --options runtime \
            --sign "$CERTIFICATE_NAME" \
            "$plugin" 2>&1; then
            echo "  ⚠️  Warning: Failed to sign $plugin, continuing..."
        fi
    done
fi

# Sign the main executable
if [ -d "$APP_PATH/Contents/MacOS" ]; then
    EXECUTABLE=$(find "$APP_PATH/Contents/MacOS" -type f -executable | head -1)
    if [ -n "$EXECUTABLE" ]; then
        echo "🔏 Signing main executable: $EXECUTABLE"
        codesign --force --verify --verbose --timestamp \
            --options runtime \
            --sign "$CERTIFICATE_NAME" \
            --entitlements /dev/null \
            "$EXECUTABLE"
    fi
fi

# Sign the .app bundle itself
echo "🔏 Signing app bundle..."
codesign --force --deep --verify --verbose --timestamp \
    --options runtime \
    --sign "$CERTIFICATE_NAME" \
    "$APP_PATH"

# Verify the signature
echo "✅ Verifying app signature..."
codesign --verify --deep --strict --verbose=2 "$APP_PATH"
spctl --assess --type execute --verbose "$APP_PATH" || echo "⚠️  Gatekeeper assessment failed (expected before notarization)"

# Clean up certificate file immediately
rm -f "$CERT_PATH"

# Notarization
if [ "$SKIP_NOTARIZATION" != "true" ]; then
    echo "📤 Starting notarization process..."
    
    # Create a zip of the app for notarization (Apple recommends zip for apps)
    ZIP_PATH="$RUNNER_TEMP/app-for-notarization.zip"
    echo "📦 Creating zip for notarization..."
    ditto -c -k --keepParent "$APP_PATH" "$ZIP_PATH"
    
    # Submit for notarization using notarytool (modern approach)
    echo "📤 Submitting to Apple for notarization..."
    NOTARIZATION_OUTPUT=$(xcrun notarytool submit "$ZIP_PATH" \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_ID_PASSWORD" \
        --team-id "$APPLE_TEAM_ID" \
        --wait \
        --timeout 30m 2>&1)
    
    echo "$NOTARIZATION_OUTPUT"
    
    # Check if notarization succeeded
    if echo "$NOTARIZATION_OUTPUT" | grep -q "status: Accepted"; then
        echo "✅ Notarization successful!"
        
        # Extract submission ID for potential log retrieval
        SUBMISSION_ID=$(echo "$NOTARIZATION_OUTPUT" | grep "id:" | head -1 | awk '{print $2}')
        if [ -n "$SUBMISSION_ID" ]; then
            echo "   Submission ID: $SUBMISSION_ID"
        fi
    else
        echo "❌ Notarization failed!"
        echo "   Attempting to retrieve logs..."
        
        # Try to extract submission ID from output
        SUBMISSION_ID=$(echo "$NOTARIZATION_OUTPUT" | grep "id:" | head -1 | awk '{print $2}')
        if [ -n "$SUBMISSION_ID" ]; then
            echo "   Retrieving detailed logs for submission: $SUBMISSION_ID"
            xcrun notarytool log "$SUBMISSION_ID" \
                --apple-id "$APPLE_ID" \
                --password "$APPLE_ID_PASSWORD" \
                --team-id "$APPLE_TEAM_ID" || true
        fi
        
        # Clean up
        security delete-keychain "$KEYCHAIN_PATH" || true
        rm -f "$ZIP_PATH"
        exit 1
    fi
    
    # Clean up zip file
    rm -f "$ZIP_PATH"
    
    # Staple the notarization ticket to the DMG
    echo "📎 Stapling notarization ticket to DMG..."
    xcrun stapler staple "$DMG_PATH"
    
    # Verify stapling
    echo "✅ Verifying stapled ticket..."
    xcrun stapler validate "$DMG_PATH"
    
    echo "✅ Notarization and stapling complete!"
else
    echo "ℹ️  Skipping notarization (credentials not provided)"
fi

# Clean up keychain
echo "🧹 Cleaning up temporary keychain..."
security delete-keychain "$KEYCHAIN_PATH" || true

echo ""
echo "✅ =========================================="
echo "✅  macOS Signing Complete!"
echo "✅ =========================================="
echo ""
echo "The app bundle has been signed with:"
echo "  Certificate: $CERTIFICATE_NAME"
if [ "$SKIP_NOTARIZATION" != "true" ]; then
    echo "  Notarization: ✅ Accepted by Apple"
    echo "  Ticket: ✅ Stapled to DMG"
    echo ""
    echo "Gatekeeper will open the app without warnings."
else
    echo "  Notarization: ⏭️  Skipped"
    echo ""
    echo "Note: The app is signed but not notarized."
    echo "Users may still see warnings on first launch."
fi
echo ""
