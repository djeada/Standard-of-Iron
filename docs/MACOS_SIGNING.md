# macOS Code Signing and Notarization

macOS packaging is implemented in `.github/workflows/build-macos.yml`. The workflow is reusable and is called by both the weekly packaging job and the release workflow.

The packaging path uses two different kinds of signing:

1. an **ad-hoc signature** that is always applied after `macdeployqt` and asset copying so the modified application bundle can run on macOS; and
2. an optional **Developer ID** signing and notarization script when release credentials are available.

These are separate stages with different purposes.

## Packaging order

The current workflow performs the relevant steps in this order:

1. build `standard_of_iron.app`;
2. run `macdeployqt`;
3. copy assets and licences into the bundle;
4. ad-hoc sign the complete bundle with `codesign --sign -`;
5. run packaged renderer and OpenGL self-tests;
6. create the DMG with `hdiutil create`;
7. run `scripts/sign-and-notarize-macos.sh` with the app path and the already-created DMG;
8. verify the DMG and write its SHA-256 checksum.

The ad-hoc signature is therefore part of every package produced by `build-macos.yml`, regardless of whether Developer ID credentials are configured.

## Ad-hoc signing

After deployment and asset copying, the workflow runs:

```sh
codesign --force --deep --sign - "${APP_DIR}/${APP_NAME}.app"
codesign --verify --deep --strict --verbose=2 "${APP_DIR}/${APP_NAME}.app"
```

This reseals the bundle after `macdeployqt` and the asset-copy steps have changed its contents. The packaged self-tests run against that ad-hoc-signed bundle.

An ad-hoc signature is not a Developer ID signature and does not provide notarization or a verified external publisher identity.

## Optional Developer ID signing

The workflow calls:

```sh
./scripts/sign-and-notarize-macos.sh \
  "${APP_DIR}/${APP_NAME}.app" \
  "${DMG_NAME}"
```

The script exits successfully without Developer ID signing when either of these values is missing:

- `MACOS_CERTIFICATE`; or
- `MACOS_CERTIFICATE_PASSWORD`.

If they are present, the script decodes the `.p12`, imports it into a temporary keychain, locates a **Developer ID Application** identity, signs nested frameworks and plugins, signs the main executable, signs the `.app` bundle, and verifies the resulting application signature.

### Signing credentials

| Secret                       | Used for                                                                                                 |
| ---------------------------- | -------------------------------------------------------------------------------------------------------- |
| `MACOS_CERTIFICATE`          | Base64-encoded `.p12` containing the Developer ID certificate and private key                            |
| `MACOS_CERTIFICATE_PASSWORD` | Password for the `.p12` export                                                                           |
| `MACOS_KEYCHAIN_PASSWORD`    | Password for the temporary CI keychain; the script generates one with `uuidgen` when this value is empty |

Only the certificate and certificate password determine whether Developer ID signing runs.

## Optional notarization

Notarization requires all three of these values:

- `APPLE_ID`;
- `APPLE_ID_PASSWORD`; and
- `APPLE_TEAM_ID`.

When any of them is absent, `sign-and-notarize-macos.sh` still performs Developer ID signing if the certificate credentials are present, but sets `SKIP_NOTARIZATION=true` and skips the notary submission.

When all notarization credentials are present, the script:

1. creates a ZIP of the signed `.app` with `ditto`;
2. submits the ZIP through `xcrun notarytool submit --wait`;
3. fails if Apple does not return `status: Accepted`;
4. retrieves the notary log on failure when it can recover a submission ID;
5. runs `xcrun stapler staple` on the DMG path; and
6. validates the stapled target with `xcrun stapler validate`.

## Current DMG ordering constraint

`build-macos.yml` creates the DMG **before** `sign-and-notarize-macos.sh` performs Developer ID signing on the `.app` in the build directory.

A DMG created by `hdiutil create -srcfolder` contains the state of the source bundle at DMG creation time. Later changes to `build/bin/standard_of_iron.app` do not rewrite the copy already stored in the DMG.

The repository therefore establishes these facts:

- the app used for packaged self-tests is ad-hoc signed;
- the build-directory app can receive a Developer ID signature after the DMG exists; and
- the script attempts to staple and validate the existing DMG after notarizing a ZIP of the subsequently signed app.

The workflow does **not** currently establish that the Developer-ID-signed copy of the app is the copy contained in the already-created DMG. Documentation and release checks should not claim that property unless the packaging order or verification path is changed to prove it.

## Encoding the certificate

On macOS:

```sh
base64 -i /path/to/certificate.p12 | pbcopy
```

On GNU/Linux:

```sh
base64 -w 0 /path/to/certificate.p12
```

Store the result as `MACOS_CERTIFICATE` and the export password as `MACOS_CERTIFICATE_PASSWORD`.

## Verification commands

For an app bundle:

```sh
codesign --verify --deep --strict --verbose=2 standard_of_iron.app
```

For Gatekeeper assessment:

```sh
spctl --assess --type execute --verbose standard_of_iron.app
```

For a stapled target:

```sh
xcrun stapler validate standard_of_iron-macos.dmg
```

The CI workflow also runs `hdiutil verify` on the DMG and writes a SHA-256 checksum after the optional signing/notarization step.

## Temporary credential handling

`sign-and-notarize-macos.sh` writes the decoded certificate to `$RUNNER_TEMP/certificate.p12` and imports it into a temporary keychain. The certificate file is removed after application signing, and the temporary keychain is deleted at the end of the successful path or explicitly on notarization failure.

The `.p12`, its password, Apple authentication values, and private key material are release credentials and must not be committed or printed in logs.

## When this workflow runs

`.github/workflows/build-macos.yml` is called by:

- `.github/workflows/weekly.yml` for weekly package validation; and
- `.github/workflows/release.yml` for release candidates.

Both callers use `secrets: inherit`, so either path can exercise the optional Developer ID and notarization stages when repository credentials are configured.

## Source of truth

The package order is defined by `.github/workflows/build-macos.yml`. Developer ID signing and notarization behavior is defined by `scripts/sign-and-notarize-macos.sh`.
