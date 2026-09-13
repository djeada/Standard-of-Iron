# macOS Code Signing and Notarization

The macOS release workflow can sign and notarize Standard of Iron so distributed builds pass Gatekeeper without requiring users to override the normal macOS security flow.

The release path has three stages:

1. **Code signing** — sign the `.app` bundle with a Developer ID Application certificate.
2. **Notarization** — submit the signed application to Apple's notary service.
3. **Stapling** — attach the notarization ticket to the distributed DMG so verification can succeed without another network lookup.

Signing is optional for development builds. The workflow degrades gracefully when credentials are unavailable, which keeps local and community builds accessible.

## Prerequisites

To enable the complete signing and notarization path, you need:

- an Apple Developer account with Developer ID distribution access;
- a **Developer ID Application** certificate and its private key; and
- credentials that the notarization service can use, including an app-specific password.

## 1. Create a Developer ID Application certificate

1. Sign in to the [Apple Developer account](https://developer.apple.com/account).
2. Open **Certificates, Identifiers & Profiles**.
3. Create a new certificate.
4. Choose **Developer ID Application**, which is the certificate type used for applications distributed outside the Mac App Store.
5. Follow Apple's instructions to create the Certificate Signing Request (CSR).
6. Download the issued certificate and install it in Keychain Access.

## 2. Export the certificate as PKCS #12

The CI workflow needs both the certificate and its private key.

1. Open **Keychain Access**.
2. Locate the **Developer ID Application** certificate.
3. Export it in `.p12` format.
4. Protect the exported file with a strong password.
5. Keep both the `.p12` file and password private; CI will receive them through repository secrets.

## 3. Create an app-specific password

1. Sign in at [appleid.apple.com](https://appleid.apple.com).
2. Open the account security controls.
3. Create an app-specific password for notarization.
4. Give it a recognizable label such as `Standard of Iron Notarization`.
5. Copy the generated password and store it securely.

## 4. Find the Apple Developer Team ID

Open the Apple Developer account page and locate the team's 10-character alphanumeric **Team ID**. The notarization workflow uses this together with the Apple ID credentials.

## 5. Configure GitHub Actions secrets

In the repository, open **Settings → Secrets and variables → Actions** and add the required repository secrets.

| Secret | Purpose | Value |
| ------ | ------- | ----- |
| `MACOS_CERTIFICATE` | Certificate and private key | Base64-encoded `.p12` file |
| `MACOS_CERTIFICATE_PASSWORD` | Unlocks the `.p12` export | Password chosen during export |
| `MACOS_KEYCHAIN_PASSWORD` | Protects the temporary CI keychain | Strong random value |
| `APPLE_ID` | Notarization account | Apple ID email address |
| `APPLE_ID_PASSWORD` | Notarization authentication | App-specific password |
| `APPLE_TEAM_ID` | Identifies the developer team | 10-character Team ID |

Encode the certificate without introducing line breaks. On macOS:

```sh
base64 -i /path/to/certificate.p12 | pbcopy
```

On GNU/Linux:

```sh
base64 -w 0 /path/to/certificate.p12
```

Paste the resulting text into `MACOS_CERTIFICATE`.

## Workflow behavior

### Complete configuration

When all signing and notarization secrets are available, the workflow:

1. imports the certificate into a temporary keychain;
2. signs the application with the Developer ID identity;
3. submits the signed build to Apple's notarization service;
4. staples the accepted ticket to the DMG; and
5. fails the release job if a required signing or notarization step fails.

### No signing credentials

When the signing credentials are absent, the script reports that signing is unavailable and skips the release-signing path. The build itself still succeeds.

This fallback is intentional. It allows:

- contributors without Apple Developer credentials to build the project;
- local development and testing without importing release certificates; and
- CI jobs that only need to validate compilation or packaging structure.

The resulting application is unsigned and should not be treated as a normal end-user release artifact.

### Signing without notarization credentials

If the certificate is configured but notarization credentials are incomplete, the application can still be signed while notarization is skipped with a warning. Such a package does not provide the same Gatekeeper experience as the fully notarized release path.

## Security model

Release credentials belong in GitHub Actions secrets rather than repository files, workflow output, or checked-in shell configuration.

The signing script limits credential lifetime on the runner by:

- creating a temporary keychain;
- importing the certificate only for the signing operation; and
- deleting temporary keychain material during cleanup on success or failure.

Treat the `.p12` password, keychain password, Apple ID app-specific password, and certificate export as release credentials. Rotate or replace them when compromised, revoked, or expired.

## Local signing test

The same script used by CI can be exercised locally by supplying the required environment variables:

```sh
export MACOS_CERTIFICATE="$(base64 -i certificate.p12)"
export MACOS_CERTIFICATE_PASSWORD="your-password"
export MACOS_KEYCHAIN_PASSWORD="temp-password"
export APPLE_ID="your@email.com"
export APPLE_ID_PASSWORD="xxxx-xxxx-xxxx-xxxx"
export APPLE_TEAM_ID="A1B2C3D4E5"

./scripts/sign-and-notarize-macos.sh \
  build/bin/standard_of_iron.app \
  standard_of_iron-macos.dmg
```

Use disposable shell history or another secure method when working with real release credentials locally.

## Verifying a signed release

Verify the application signature:

```sh
codesign --verify --deep --strict --verbose=2 standard_of_iron.app
```

Ask Gatekeeper to assess the application:

```sh
spctl --assess --type execute --verbose standard_of_iron.app
```

Verify the stapled notarization ticket on the DMG:

```sh
stapler validate standard_of_iron-macos.dmg
```

All three checks should succeed for a fully signed and notarized release artifact.

## Verifying the unsigned fallback

Run the signing script without the required environment variables:

```sh
./scripts/sign-and-notarize-macos.sh \
  build/bin/standard_of_iron.app \
  standard_of_iron-macos.dmg
```

The expected behavior is an informational message that macOS signing credentials were not found, followed by a successful return to the ordinary build workflow.

## Troubleshooting

### Notarization fails

The script reports the error from Apple's service and attempts to retrieve the detailed notarization log before exiting with failure.

Common causes include:

- an invalid or incomplete code signature;
- nested frameworks or binaries that were not signed correctly;
- hardened-runtime or entitlement problems; and
- invalid Apple ID, app-specific password, or Team ID credentials.

Start with the notarization log because it identifies the rejected component more precisely than the top-level workflow error.

### Developer ID certificate is not found

If the temporary keychain does not contain a usable signing identity:

- confirm that the exported certificate is specifically **Developer ID Application**;
- confirm that the certificate remains valid;
- confirm that the `.p12` contains both the certificate and its private key; and
- confirm that `MACOS_CERTIFICATE_PASSWORD` matches the export password.

### Keychain reports `User interaction is not allowed`

CI must be able to unlock and authorize the temporary keychain non-interactively. Confirm that `MACOS_KEYCHAIN_PASSWORD` is configured and that the signing script successfully creates, unlocks, and configures the temporary keychain before invoking `codesign`.

## Credential maintenance

Developer ID certificates and Apple authentication credentials have independent lifetimes. Replace expiring or revoked credentials in repository secrets before producing the next release and validate the replacement with the same signing, Gatekeeper, and stapling checks above.

Do not wait for a release job to discover that a signing identity has expired.

## References

- [Apple code signing](https://developer.apple.com/support/code-signing/)
- [Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution)
- [Customizing the notarization workflow](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution/customizing_the_notarization_workflow)
- [GitHub Actions encrypted secrets](https://docs.github.com/en/actions/security-guides/encrypted-secrets)

When troubleshooting a release, keep sensitive values out of issues and build logs. Share the failing command, non-secret diagnostic output, and notarization log details needed to reproduce the problem without exposing credentials.
