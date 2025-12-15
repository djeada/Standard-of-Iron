# Quick Start: macOS Signing Setup

This is a quick reference for maintainers. For complete details, see [MACOS_SIGNING.md](MACOS_SIGNING.md).

## Required GitHub Secrets

Add these 6 secrets to enable macOS code signing and notarization:

| Secret Name | Value | How to Get |
|------------|-------|------------|
| `MACOS_CERTIFICATE` | Base64-encoded .p12 file | `base64 -i cert.p12 \| pbcopy` |
| `MACOS_CERTIFICATE_PASSWORD` | Password for .p12 | Password you set when exporting cert |
| `MACOS_KEYCHAIN_PASSWORD` | Random password | `openssl rand -base64 32` |
| `APPLE_ID` | Apple ID email | Your Apple Developer account email |
| `APPLE_ID_PASSWORD` | App-specific password | Generate at appleid.apple.com |
| `APPLE_TEAM_ID` | 10-character team ID | Find in Apple Developer Portal |

## How It Works

### With Secrets Configured ✅
1. App is signed with Developer ID
2. App is notarized by Apple
3. Ticket is stapled to DMG
4. No Gatekeeper warnings for users

### Without Secrets ℹ️
1. Workflow prints info message
2. Signing is skipped
3. Build completes successfully
4. Users see "unidentified developer" warning

## Testing

```bash
# Local test with credentials
export MACOS_CERTIFICATE="$(base64 -i cert.p12)"
export MACOS_CERTIFICATE_PASSWORD="your-password"
export MACOS_KEYCHAIN_PASSWORD="temp-password"
export APPLE_ID="your@email.com"
export APPLE_ID_PASSWORD="xxxx-xxxx-xxxx-xxxx"
export APPLE_TEAM_ID="A1B2C3D4E5"

./scripts/sign-and-notarize-macos.sh \
    build/bin/standard_of_iron.app \
    standard_of_iron-macos.dmg
```

## Verify Signed App

```bash
# Check signature
codesign --verify --deep --strict --verbose=2 standard_of_iron.app

# Check Gatekeeper
spctl --assess --type execute --verbose standard_of_iron.app

# Verify stapled ticket
stapler validate standard_of_iron-macos.dmg
```

## Prerequisites

- Apple Developer Account ($99/year)
- Developer ID Application certificate
- App-specific password for notarization

See [MACOS_SIGNING.md](MACOS_SIGNING.md) for detailed setup instructions.
