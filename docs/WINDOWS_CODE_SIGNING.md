# Windows Code Signing

Windows packaging is implemented in `.github/workflows/build-windows.yml`. The workflow is reusable and is called by both the weekly packaging job and the release workflow. When the signing secrets are available, it signs `standard_of_iron.exe` with Authenticode and verifies the result before packaging continues.

Signing is optional at the workflow level: if either required secret is absent, the signing step prints an informational message and exits successfully. The Windows build and package are still produced.

## What the workflow signs

The repository signs one first-party Windows binary:

- `build\bin\standard_of_iron.exe`

The signing step runs after the game executable is built and before Qt deployment. Third-party Qt DLLs and the bundled Mesa fallback are not signed by this repository.

## Required repository secrets

The workflow reads two secrets:

| Secret | Purpose |
| --- | --- |
| `WINDOWS_CERTIFICATE` | Base64-encoded `.pfx` / PKCS #12 signing certificate and private key |
| `WINDOWS_CERTIFICATE_PASSWORD` | Password protecting that `.pfx` export |

Both values must be present for signing to run.

### Encoding the certificate

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

Store the resulting single-line value as `WINDOWS_CERTIFICATE`. Store the export password separately as `WINDOWS_CERTIFICATE_PASSWORD`.

The `.pfx`, decoded certificate bytes, private key, and password are release credentials and must not be committed or written to build logs.

## SignTool invocation

The workflow locates an x64 `signtool.exe` under the installed Windows Kits directories and signs with these effective arguments:

```text
sign
/f <temporary cert.pfx>
/p <certificate password>
/tr http://timestamp.digicert.com
/td SHA256
/fd SHA256
/v
<standard_of_iron.exe>
```

The file digest and RFC 3161 timestamp digest are both SHA-256.

The certificate is decoded into the runner's temporary directory only for the signing step. A `finally` block removes the temporary `.pfx` whether signing succeeds or fails.

## Verification

Immediately after signing, the workflow runs:

```text
signtool verify /pa /v standard_of_iron.exe
```

A non-zero result fails the job. The package is therefore not allowed to continue from a signing attempt whose Authenticode signature cannot be verified by SignTool.

A downloaded package can be checked with the same command from a Windows Developer Command Prompt:

```cmd
signtool verify /pa /v standard_of_iron.exe
```

Explorer also exposes the signature under **Properties → Digital Signatures**.

## When signing runs

`.github/workflows/build-windows.yml` is a `workflow_call` workflow rather than a tag-only workflow.

The repository currently invokes it from:

- `.github/workflows/weekly.yml` for the weekly Windows packaging dry run; and
- `.github/workflows/release.yml` for release candidates created from release tags or manual release dispatches.

Both callers use `secrets: inherit`, so either path can sign when the repository secrets are configured.

The build workflow itself never publishes a release. Publishing happens later in `release.yml` after all three platform packages and their checksums have passed verification.

## Failure modes

### Signing is skipped

The step is skipped when `WINDOWS_CERTIFICATE` or `WINDOWS_CERTIFICATE_PASSWORD` is empty. This is expected for environments that do not receive release credentials.

### The executable is missing

The workflow fails if `build\bin\standard_of_iron.exe` does not exist when the signing step begins.

### SignTool is unavailable

The workflow searches the Windows Kits installation for an x64 `signtool.exe`. If none is found, the signing step fails.

### Certificate decoding or signing fails

Invalid base64, an incorrect `.pfx` password, an unusable code-signing certificate, or a signing failure causes the step to fail.

### Timestamping fails

The workflow uses `http://timestamp.digicert.com` as its RFC 3161 timestamp service. If SignTool cannot obtain a valid timestamp, signing fails rather than silently creating a different signing mode.

## SmartScreen

Authenticode verification and SmartScreen reputation are separate checks. This repository's workflow proves that the executable was signed by the configured certificate and that SignTool accepts the signature. It does not contain a separate SmartScreen-reputation gate.

A SmartScreen warning should therefore be investigated separately from a failed Authenticode verification.

## Source of truth

The behavior described here is defined by `.github/workflows/build-windows.yml`. The release orchestration that calls it is in `.github/workflows/release.yml`, and the weekly packaging call is in `.github/workflows/weekly.yml`.
