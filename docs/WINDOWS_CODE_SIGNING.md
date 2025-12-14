# Windows Code Signing

This document describes the Windows code signing implementation for Standard of Iron.

## Overview

The Windows build pipeline automatically signs the `.exe` file with Authenticode using an organization EV (Extended Validation) certificate. Code signing provides:

- **Verified publisher**: Windows displays the organization name as a verified publisher
- **Reduced SmartScreen friction**: Signed executables are less likely to trigger Windows Defender SmartScreen warnings
- **Trust and authenticity**: Users can verify the software comes from the legitimate publisher

## Implementation

### Build Pipeline

The code signing step is integrated into the Windows build workflow (`.github/workflows/windows.yml`) and runs automatically when tags are pushed (e.g., `v1.2.3`).

The signing process:
1. Runs after the build step completes
2. Only executes if the `WINDOWS_CERTIFICATE` secret is configured
3. Signs the main executable (`standard_of_iron.exe`)
4. Uses SHA-256 algorithm for both file digest and timestamp
5. Timestamps the signature using DigiCert's timestamp server
6. Verifies the signature after signing

### Signed Files

Currently, the pipeline signs:
- `standard_of_iron.exe` - The main game executable

**Note**: Qt DLLs and other third-party libraries are not signed by this pipeline as they should already be signed by their respective publishers (e.g., Qt Company).

## Setup Instructions

### Prerequisites

1. **EV Code Signing Certificate**: Obtain an Extended Validation code signing certificate from a trusted Certificate Authority (CA) such as:
   - DigiCert
   - Sectigo
   - GlobalSign
   - Entrust

2. **Certificate Format**: Export the certificate as a `.pfx` (PKCS#12) file with a strong password

### GitHub Secrets Configuration

To enable code signing in GitHub Actions, configure the following repository secrets:

1. **WINDOWS_CERTIFICATE** (Required)
   - Navigate to: Repository Settings → Secrets and variables → Actions → New repository secret
   - Name: `WINDOWS_CERTIFICATE`
   - Value: Base64-encoded content of your `.pfx` certificate file
   
   To encode your certificate on Linux/macOS:
   ```bash
   base64 -i certificate.pfx | tr -d '\n' > certificate.txt
   cat certificate.txt
   ```
   
   On Windows PowerShell:
   ```powershell
   $certBytes = [System.IO.File]::ReadAllBytes("certificate.pfx")
   $certBase64 = [System.Convert]::ToBase64String($certBytes)
   $certBase64 | Out-File -FilePath certificate.txt -NoNewline
   Get-Content certificate.txt
   ```
   
   Copy the entire base64 string and paste it as the secret value.

2. **WINDOWS_CERTIFICATE_PASSWORD** (Required)
   - Name: `WINDOWS_CERTIFICATE_PASSWORD`
   - Value: The password used to protect the `.pfx` certificate file

### Verification

After pushing a tag and completing the build:

1. Download the Windows artifact from GitHub Actions
2. Extract the `.exe` file
3. Right-click the `.exe` → Properties → Digital Signatures tab
4. Verify that:
   - The signature shows your organization name
   - The signature is valid
   - The timestamp is present and valid

Alternatively, use `signtool` to verify from the command line:
```cmd
signtool verify /pa /v standard_of_iron.exe
```

## Security Considerations

1. **Certificate Protection**: 
   - Store the certificate securely
   - Use a strong password
   - Limit access to the certificate and password
   - Use GitHub's encrypted secrets feature (never commit certificates to the repository)

2. **Certificate Expiration**:
   - Monitor certificate expiration dates
   - Plan for renewal well in advance
   - Update the GitHub secret when renewing

3. **Timestamping**:
   - The signature includes a trusted timestamp
   - Signatures remain valid even after the certificate expires (as long as signed before expiration)
   - If the timestamp server is unavailable, signing will fail (this is expected behavior)

## Troubleshooting

### Signing Step Skipped

If the signing step is skipped, verify:
- The `WINDOWS_CERTIFICATE` secret is configured
- The secret name matches exactly (case-sensitive)
- The workflow has permission to access secrets

### Signing Fails

If signing fails, check:
1. Certificate is valid and not expired
2. Password is correct
3. Certificate is in `.pfx` format
4. Base64 encoding is correct (no line breaks or extra characters)
5. Timestamp server (http://timestamp.digicert.com) is accessible

### SmartScreen Still Shows Warnings

Even with a valid signature, SmartScreen may show warnings if:
- The certificate is new and hasn't built reputation yet
- The executable hasn't been downloaded by many users yet
- The signature is from a standard (non-EV) certificate

EV certificates typically bypass these warnings immediately, but standard certificates may require time to build reputation.

## Future Enhancements

Potential improvements for the code signing implementation:

1. **Sign Custom DLLs**: If the project adds custom DLL libraries, include them in the signing process
2. **Dual Signing**: Add SHA-1 signatures for older Windows versions (Windows 7, 8)
3. **Azure SignTool**: Migrate to Azure Key Vault for certificate storage if using Azure DevOps
4. **Signature Verification**: Add automated signature verification to CI/CD pipeline

## References

- [Microsoft SignTool Documentation](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)
- [Windows Authenticode Documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/authenticode)
- [GitHub Actions Encrypted Secrets](https://docs.github.com/en/actions/security-guides/encrypted-secrets)
