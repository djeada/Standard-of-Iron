# macOS Code Signing and Notarization Guide

This document explains how to set up macOS code signing and notarization for the Standard of Iron project.

## Overview

The macOS build workflow includes automatic code signing and notarization to ensure the application can be opened on macOS without Gatekeeper warnings. This process:

1. **Signs** the `.app` bundle with a Developer ID Application certificate
2. **Notarizes** the app with Apple's notary service
3. **Staples** the notarization ticket to the DMG for offline verification

## Prerequisites

To enable signing and notarization, you need:

1. **Apple Developer Account** (paid membership, $99/year)
2. **Developer ID Application Certificate** from Apple
3. **App-specific password** for notarization

## Setup Instructions

### Step 1: Create Developer ID Application Certificate

1. Log in to [Apple Developer Portal](https://developer.apple.com/account)
2. Navigate to **Certificates, Identifiers & Profiles**
3. Click **+** to create a new certificate
4. Select **Developer ID Application** (for distribution outside Mac App Store)
5. Follow the instructions to create a Certificate Signing Request (CSR)
6. Download the certificate and install it in Keychain Access

### Step 2: Export Certificate as .p12

1. Open **Keychain Access** on your Mac
2. Find your **Developer ID Application** certificate
3. Right-click and select **Export**
4. Choose `.p12` format
5. Set a strong password (you'll need this for GitHub Secrets)
6. Save the file

### Step 3: Create App-Specific Password

1. Go to [appleid.apple.com](https://appleid.apple.com)
2. Sign in with your Apple ID
3. Navigate to **Security** → **App-Specific Passwords**
4. Click **Generate an app-specific password**
5. Label it (e.g., "Standard of Iron Notarization")
6. Copy the generated password (format: `xxxx-xxxx-xxxx-xxxx`)

### Step 4: Find Your Team ID

1. Go to [Apple Developer Portal](https://developer.apple.com/account)
2. Click on your account name in the top right
3. Your **Team ID** is a 10-character alphanumeric code (e.g., `A1B2C3D4E5`)

### Step 5: Configure GitHub Secrets

Add the following secrets to your GitHub repository:

1. Go to repository **Settings** → **Secrets and variables** → **Actions**
2. Click **New repository secret** and add each of these:

| Secret Name | Description | How to Get Value |
|------------|-------------|------------------|
| `MACOS_CERTIFICATE` | Base64-encoded .p12 certificate | Run: `base64 -i certificate.p12 | pbcopy` (copies to clipboard) |
| `MACOS_CERTIFICATE_PASSWORD` | Password for the .p12 file | The password you set when exporting the certificate |
| `MACOS_KEYCHAIN_PASSWORD` | Password for temporary keychain | Any strong random password (e.g., `openssl rand -base64 32`) |
| `APPLE_ID` | Your Apple ID email | Your Apple Developer account email |
| `APPLE_ID_PASSWORD` | App-specific password | The password from Step 3 (with dashes) |
| `APPLE_TEAM_ID` | Your Apple Developer Team ID | The 10-character code from Step 4 |

### Example: Encoding Certificate

```bash
# On macOS, encode and copy to clipboard
base64 -i /path/to/certificate.p12 | pbcopy

# On Linux
base64 -w 0 /path/to/certificate.p12
```

## Behavior

### With Secrets Configured

When all required secrets are present:

1. ✅ The `.app` is signed with your Developer ID
2. ✅ The app is submitted to Apple for notarization
3. ✅ The notarization ticket is stapled to the DMG
4. ✅ Gatekeeper opens the app without warnings

### Without Secrets

If secrets are not configured:

1. ℹ️  The workflow prints an informational message
2. ℹ️  Signing and notarization are skipped
3. ✅ The build completes successfully
4. ⚠️  Users will see "unidentified developer" warning

This graceful fallback allows:
- Testing builds without certificates
- Community contributors to build without Apple Developer accounts
- Quick iterations during development

### Partial Configuration

If only signing credentials are present (no notarization credentials):

1. ✅ The app is signed
2. ⚠️  Notarization is skipped with a warning
3. ⚠️  Users may still see Gatekeeper warnings

## Troubleshooting

### Notarization Failed

If notarization fails, the script will:
1. Show the error message from Apple
2. Attempt to retrieve detailed logs
3. Exit with an error code

Common issues:
- **Invalid code signature**: Ensure frameworks are signed before the main app
- **Hardened runtime violations**: Add appropriate entitlements if needed
- **Invalid credentials**: Verify Apple ID, password, and Team ID

### Certificate Not Found

Error: "Developer ID Application certificate not found in keychain"

Solutions:
- Verify the certificate is a **Developer ID Application** certificate
- Ensure the certificate is not expired
- Check that the .p12 file contains the certificate and private key

### Keychain Access Denied

Error: "User interaction is not allowed"

Solutions:
- Ensure `MACOS_KEYCHAIN_PASSWORD` secret is set
- Check that the keychain is properly unlocked in the script

## Security Considerations

### Secret Protection

GitHub Secrets are:
- ✅ Encrypted at rest
- ✅ Only exposed to GitHub Actions runners
- ✅ Never logged or exposed in outputs
- ✅ Redacted in build logs

The signing script:
- Creates a temporary keychain for signing
- Immediately deletes certificates after use
- Cleans up keychains on completion or failure

### Certificate Expiration

Developer ID Application certificates:
- Valid for 5 years
- Must be renewed before expiration
- Apple will email reminders before expiration

Remember to:
- Update the certificate in GitHub Secrets before expiration
- Test the new certificate before the old one expires

## Testing

### Local Testing

You can test the signing script locally:

```bash
# Set environment variables
export MACOS_CERTIFICATE="$(base64 -i certificate.p12)"
export MACOS_CERTIFICATE_PASSWORD="your-password"
export MACOS_KEYCHAIN_PASSWORD="temp-password"
export APPLE_ID="your@email.com"
export APPLE_ID_PASSWORD="xxxx-xxxx-xxxx-xxxx"
export APPLE_TEAM_ID="A1B2C3D4E5"

# Run the script
./scripts/sign-and-notarize-macos.sh build/bin/standard_of_iron.app standard_of_iron-macos.dmg
```

### Verify Signed App

```bash
# Check code signature
codesign --verify --deep --strict --verbose=2 standard_of_iron.app

# Check Gatekeeper assessment
spctl --assess --type execute --verbose standard_of_iron.app

# Verify stapled ticket
stapler validate standard_of_iron-macos.dmg
```

### Test Without Secrets

To test the fallback behavior:

```bash
# Run without environment variables
./scripts/sign-and-notarize-macos.sh build/bin/standard_of_iron.app standard_of_iron-macos.dmg
```

Expected output: "macOS signing credentials not found, skipping signing and notarization"

## References

- [Apple Code Signing Guide](https://developer.apple.com/support/code-signing/)
- [Apple Notarization Guide](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution)
- [notarytool Documentation](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/customizing_the_notarization_workflow)
- [GitHub Actions Secrets](https://docs.github.com/en/actions/security-guides/encrypted-secrets)

## Support

If you encounter issues:

1. Check the [Troubleshooting](#troubleshooting) section above
2. Review the build logs in GitHub Actions
3. Open an issue with the error message and relevant logs
4. For sensitive credential issues, reach out privately to maintainers
