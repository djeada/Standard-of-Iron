# Windows Code Signing

The Windows release pipeline signs `standard_of_iron.exe` with Authenticode when release credentials are available. A valid signature gives users a verifiable publisher identity, protects the integrity of the executable after signing, and allows Windows reputation systems to associate releases with a consistent publisher.

Signing and SmartScreen reputation are related but separate. A correctly signed new build can still be reported as unrecognized while reputation develops, so code signing should be treated as an authenticity requirement rather than a guarantee that every warning disappears immediately.

## Release workflow

The signing step is part of `.github/workflows/windows.yml` and runs for tagged release builds when `WINDOWS_CERTIFICATE` is configured.

The pipeline:

1. builds the Windows release artifact;
2. imports the configured signing certificate;
3. signs `standard_of_iron.exe`;
4. uses SHA-256 for the file digest;
5. requests an RFC 3161 timestamp with a SHA-256 timestamp digest;
6. verifies the resulting Authenticode signature; and
7. fails the release-signing step if signing or verification fails.

### Files signed by this repository

The repository currently signs only:

- `standard_of_iron.exe` — the main game executable.

Bundled third-party libraries are not re-signed by this workflow. If the project begins producing its own DLLs or additional executable helpers, add those first-party binaries to the signing step as well.

## Certificate requirements

The workflow expects a trusted Authenticode code-signing certificate exported as a password-protected `.pfx` / PKCS #12 file.

An existing organization-validation (OV) or extended-validation (EV) certificate can be used when it is compatible with the CI import path. EV is not required for SmartScreen reputation: current Microsoft guidance states that OV and EV signatures both participate in the same reputation-building process, and EV no longer provides an automatic first-download SmartScreen bypass.

For future signing infrastructure, a managed signing service such as Microsoft's Artifact Signing can also be considered. That would be a workflow change rather than a drop-in replacement for the current `.pfx`-secret path.

## Configure GitHub Actions secrets

Two repository secrets enable the current implementation.

### `WINDOWS_CERTIFICATE`

Store the complete `.pfx` file as one base64 string with no embedded line breaks.

On GNU/Linux:

```sh
base64 -w 0 certificate.pfx
```

On macOS:

```sh
base64 -i certificate.pfx | tr -d '\n'
```

On Windows PowerShell:

```powershell
$certBytes = [System.IO.File]::ReadAllBytes("certificate.pfx")
$certBase64 = [System.Convert]::ToBase64String($certBytes)
$certBase64 | Out-File -FilePath certificate.txt -NoNewline
Get-Content certificate.txt
```

Copy the resulting base64 text into a repository secret named `WINDOWS_CERTIFICATE` under **Settings → Secrets and variables → Actions**.

### `WINDOWS_CERTIFICATE_PASSWORD`

Store the password used to protect the `.pfx` export in a second repository secret named `WINDOWS_CERTIFICATE_PASSWORD`.

The certificate file, private key, decoded export, and password must never be committed to the repository.

## Digest and timestamp policy

Use SHA-256 for both the Authenticode file digest and RFC 3161 timestamp digest.

Conceptually, the signing command should carry the equivalent of:

```text
/fd SHA256 /tr <RFC-3161 timestamp URL> /td SHA256
```

Modern SignTool versions require an explicit file digest and timestamp digest, and Microsoft recommends SHA-256 rather than SHA-1.

A trusted timestamp proves that the executable was signed while the certificate was valid. That allows a correctly timestamped signature to remain valid after the signing certificate itself later expires, provided the certificate was valid and trusted at signing time and has not been invalidated for another reason.

Do not add SHA-1 dual-signing for current Windows targets. SHA-1 is deprecated for new code-signing signatures; SHA-256-only signing is appropriate for supported modern Windows systems.

## Verifying a release

After a tagged build completes, download the Windows artifact and verify the executable before publishing it.

From Explorer, open **Properties → Digital Signatures** and confirm that:

- the expected publisher is shown;
- Windows reports the signature as valid; and
- a trusted timestamp is present.

From a Developer Command Prompt, use SignTool:

```cmd
signtool verify /pa /v standard_of_iron.exe
```

The command should succeed without signature or trust-chain errors.

## SmartScreen expectations

A trusted Authenticode signature identifies the publisher, but SmartScreen also evaluates reputation.

For a newly released executable or a publisher identity without established reputation, users can still see an “unrecognized app” warning even when the signature is valid. Reputation can improve across releases when builds are consistently signed with the same trusted publisher identity.

Do not diagnose a SmartScreen warning as a signing failure until the Authenticode signature itself has been verified. They are different signals.

For non-Store distribution, Microsoft currently recommends signing every release and maintaining a consistent signing identity. Microsoft Store distribution uses Microsoft's signing path and has different SmartScreen behavior.

## Security considerations

### Protect the certificate and private key

The signing identity is a release credential. Protect it accordingly:

- keep the `.pfx` out of source control and artifacts;
- use a strong export password;
- restrict access to the repository secrets;
- do not echo decoded certificate data or passwords in workflow logs; and
- rotate or revoke the credential immediately if the private key may have been exposed.

### Monitor expiration and revocation

Track the certificate's validity period and replace it before a planned release would be blocked. Update both the encoded certificate and password secret when rotating to a new export.

Where possible, retain a consistent publisher identity across renewals so Windows reputation signals are not needlessly fragmented.

### Treat timestamp availability as part of release infrastructure

The timestamp request is part of a complete signature. If the configured RFC 3161 service is unavailable, the release-signing step should fail rather than silently publishing an untimestamped executable.

## Troubleshooting

### Signing step is skipped

Confirm that:

- `WINDOWS_CERTIFICATE` exists in the repository or environment from which the workflow runs;
- the secret name matches exactly;
- the workflow event is allowed to access repository secrets; and
- the job is actually running the tagged-release path that contains signing.

### Certificate import or signing fails

Check that:

1. the certificate is valid and includes an accessible private key;
2. `WINDOWS_CERTIFICATE_PASSWORD` matches the `.pfx` export;
3. the base64 value decodes to the original file without extra characters or line wrapping;
4. the certificate is suitable for Authenticode code signing; and
5. the signing identity chains to a root trusted by the target Windows systems.

### Timestamping fails

Check the configured RFC 3161 endpoint and the runner's network access. Also confirm that the signing command specifies both the timestamp URL and `/td SHA256`.

### SmartScreen still warns

First verify the Authenticode signature with `signtool verify /pa /v`.

If the signature is valid, a SmartScreen warning can still be expected for a new file or publisher identity until reputation develops. EV certificates no longer bypass that reputation process automatically, so replacing a valid OV certificate with EV solely to eliminate first-download warnings is not a reliable fix.

## Future improvements

Potential improvements should strengthen key handling and coverage rather than reintroduce legacy signing algorithms:

- sign any first-party DLLs or helper executables added to the release package;
- evaluate managed key storage or Artifact Signing so the long-lived private key does not need to be exported into a repository secret; and
- keep signature verification as an explicit release gate for every file the project signs.

## References

- [Microsoft SignTool documentation](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)
- [Microsoft SmartScreen reputation for Windows app developers](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)
- [Windows code-signing options](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options)
- [Authenticode documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/authenticode)
- [GitHub Actions encrypted secrets](https://docs.github.com/en/actions/security-guides/encrypted-secrets)

The release rule is straightforward: sign every published first-party executable with a trusted, consistent identity, timestamp it with SHA-256, verify the result, and treat reputation warnings as a separate concern from signature validity.
