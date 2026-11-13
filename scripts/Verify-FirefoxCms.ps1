param(
    [Parameter(Mandatory=$true)][string]$Package,
    [Parameter(Mandatory=$true)][string]$ExpectedId
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Security

# Pinned Mozilla production add-on signing root, from Firefox source:
# https://raw.githubusercontent.com/mozilla-firefox/firefox/main/security/manager/ssl/addons-public.pem
# The chain is evaluated in memory; the user's certificate stores are untouched.
$pem = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'mozilla-addons-public.pem'))
$base64 = $pem -replace '-----BEGIN CERTIFICATE-----', '' -replace '-----END CERTIFICATE-----', '' -replace '\s', ''
$root = [Security.Cryptography.X509Certificates.X509Certificate2]::new([Convert]::FromBase64String($base64))
$archive = [IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($Package))
try {
    function Read-Entry([string]$name) {
        $entry = $archive.GetEntry($name)
        if (-not $entry -or $entry.Length -gt 16MB) { throw "Missing or oversized signing entry: $name" }
        $input = $entry.Open()
        $output = [IO.MemoryStream]::new()
        try { $input.CopyTo($output); return ,$output.ToArray() }
        finally { $input.Dispose(); $output.Dispose() }
    }
    $content = [Security.Cryptography.Pkcs.ContentInfo]::new((Read-Entry 'META-INF/mozilla.sf'))
    $cms = [Security.Cryptography.Pkcs.SignedCms]::new($content, $true)
    $cms.Decode((Read-Entry 'META-INF/mozilla.rsa'))
    if ($cms.SignerInfos.Count -ne 1) { throw 'Expected exactly one Mozilla signer' }
    $cms.CheckSignature($true)
    $signer = $cms.SignerInfos[0].Certificate
    if (-not $signer -or $signer.GetNameInfo([Security.Cryptography.X509Certificates.X509NameType]::SimpleName, $false) -cne $ExpectedId) {
        throw 'Signing certificate does not identify this add-on'
    }
    $chain = [Security.Cryptography.X509Certificates.X509Chain]::new()
    try {
        $chain.ChainPolicy.RevocationMode = [Security.Cryptography.X509Certificates.X509RevocationMode]::NoCheck
        $chain.ChainPolicy.VerificationFlags = [Security.Cryptography.X509Certificates.X509VerificationFlags]::AllowUnknownCertificateAuthority
        $chain.ChainPolicy.UrlRetrievalTimeout = [TimeSpan]::FromSeconds(3)
        if ($chain.ChainPolicy.PSObject.Properties['DisableCertificateDownloads']) { $chain.ChainPolicy.DisableCertificateDownloads = $true }
        [void]$chain.ChainPolicy.ExtraStore.Add($root)
        foreach ($certificate in $cms.Certificates) { [void]$chain.ChainPolicy.ExtraStore.Add($certificate) }
        if (-not $chain.Build($signer) -or $chain.ChainElements.Count -lt 2) { throw 'Mozilla signing certificate chain is invalid' }
        $last = $chain.ChainElements[$chain.ChainElements.Count - 1].Certificate
        if ([Convert]::ToBase64String($last.RawData) -cne [Convert]::ToBase64String($root.RawData)) {
            throw 'Signing certificate does not chain to the pinned Mozilla production root'
        }
    } finally { $chain.Dispose() }
} finally { $archive.Dispose(); $root.Dispose() }
Write-Output 'Mozilla add-on signing certificate and PKCS#7 signature verified.'
