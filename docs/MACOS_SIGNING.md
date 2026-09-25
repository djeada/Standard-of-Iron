# macOS Code Signing and Notarization

macOS packaging is implemented in `.github/workflows/build-macos.yml`. The
workflow is reusable: the weekly packaging job and the release workflow both
call it. Developer ID signing and notarization are done by
`scripts/sign-and-notarize-macos.sh`, which has two modes, `app` and `dmg`.

## Packaging order

1. Build `standard_of_iron.app`. With `debug-symbols`, extract the dSYM and
   strip the binary's debug map.
2. Run `macdeployqt`.
3. Copy assets and licences into the bundle.
4. Ad-hoc sign the complete bundle (`codesign --force --deep --sign -`).
5. Run the packaged release self-test and the OpenGL 3.3 fallback self-test.
6. **`sign-and-notarize-macos.sh app`**:
    1. Developer-ID-sign the nested code, then the executable and the bundle,
       with the hardened runtime and the entitlements.
    2. Notarize a ZIP of that exact bundle.
    3. Staple the ticket to the bundle.
    4. Verify with `codesign`, `stapler` and `spctl`.
7. Verify the final bundle: `codesign --verify --deep --strict`, a renderer
   self-test, and `--print-data-paths`.
8. Create the DMG from that bundle with `hdiutil create`.
9. **`sign-and-notarize-macos.sh dmg`**: sign, notarize and staple the DMG,
   then run `spctl` on it.
10. Pack the bundle as `…-macos-<arch>.app.tar.gz`. This is the Steam depot
    payload.
11. Mount the DMG, and require its app to match the build-directory bundle file
    for file, including symlinks and modes. When signed, run `stapler` and
    `spctl` on the mounted copy.
12. Stage and verify the Steam depot from the tarball, then run `hdiutil verify`
    and write the checksums.

Steps 6 and 9 run only when every credential is present. Without them, the
ad-hoc bundle from step 4 is packaged, unless `require-signing` is set; then
step 6 fails the build.

The DMG therefore always contains the final app. It used to be created before
Developer ID signing, and the ticket was stapled to that earlier image, so the
signed app never reached the DMG. `ReleaseContract.MacDmgIsBuiltFromTheNotarizedApp`
keeps the order.

## Entitlements

`dist/macos/standard_of_iron.entitlements` is applied to the main executable
and the bundle, and to nothing else:

| Entitlement                                              | Why                                                                       |
| -------------------------------------------------------- | ------------------------------------------------------------------------- |
| `com.apple.security.cs.disable-library-validation`       | lets the hardened runtime load the Steam overlay dylib, which Valve signs |
| `com.apple.security.cs.allow-dyld-environment-variables` | Steam injects the overlay through `DYLD_INSERT_LIBRARIES`                 |

`com.apple.security.app-sandbox` is deliberately absent, because Steam cannot
launch or overlay a sandboxed app. The script fails if the signed app is
missing either entitlement or has the sandbox one.

## Credentials

| Secret                       | Used for                                                                                   |
| ---------------------------- | ------------------------------------------------------------------------------------------ |
| `MACOS_CERTIFICATE`          | base64 `.p12` containing the Developer ID Application certificate and private key          |
| `MACOS_CERTIFICATE_PASSWORD` | password for the `.p12` export                                                             |
| `MACOS_KEYCHAIN_PASSWORD`    | password for the temporary CI keychain; the script generates one with `uuidgen` when empty |
| `APPLE_ID`                   | Apple ID used for notarization                                                             |
| `APPLE_ID_PASSWORD`          | app-specific password for that Apple ID                                                    |
| `APPLE_TEAM_ID`              | 10-character team ID                                                                       |

Signing is all or nothing. If any of the five required values is missing, the
script signs nothing. A signed but unnotarized app is still blocked by
Gatekeeper, and looks more finished than it is.

With `SOI_REQUIRE_SIGNING=true`, a missing credential is an error. The release
workflow passes this from the repository variable of the same name. Steam
uploads always demand a Developer-ID-signed, notarized bundle
(`steam-upload.yml`, `verify-macos`).

Encode the certificate on macOS with `base64 -i certificate.p12 | pbcopy`, or on
GNU/Linux with `base64 -w 0 certificate.p12`.

## Bundle identity

`CMakeLists.txt` sets the Info.plist values that Qt's template would otherwise
fill with placeholders:

- `CFBundleIdentifier`: `io.github.djeada.standardofiron`
- `CFBundleName`: `Standard of Iron`
- the version strings: `PROJECT_VERSION`

Notarization, Gatekeeper and Steam key on the identifier, so never change it.
The save directory does not follow `CFBundleName`, because
`App::Core::apply_application_identity` pins the name `QStandardPaths` uses.

## Verification commands

```sh
codesign --verify --deep --strict --verbose=2 standard_of_iron.app
codesign --display --entitlements - standard_of_iron.app
xcrun stapler validate standard_of_iron.app
spctl --assess --type execute --verbose standard_of_iron.app
spctl --assess --type open --context context:primary-signature --verbose standard_of_iron-<version>-macos-<arch>.dmg
```

## Temporary credential handling

The script decodes the certificate into `$RUNNER_TEMP` and imports it into a
temporary keychain. It deletes the file right after the import. An `EXIT` trap
removes the keychain and restores the keychain search list on every path,
including failures. The `.p12`, its password and the Apple credentials must
never be committed or printed.

## Source of truth

`.github/workflows/build-macos.yml` defines the package order.
`scripts/sign-and-notarize-macos.sh` defines the signing and notarization
behaviour.
