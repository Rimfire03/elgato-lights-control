<#
.SYNOPSIS
    Signe (Authenticode + horodatage) des executables/DLL avec le certificat global TomLine prod&co.

.DESCRIPTION
    Le certificat est cherche dans Cert:\CurrentUser\My : par empreinte (-Thumbprint ou variable
    TOMLINE_SIGN_THUMBPRINT), sinon le certificat de signature de code valide le plus recent dont le
    sujet contient "TomLine prod&co". Aucun mot de passe ni .pfx dans le depot.

.EXAMPLE
    .\tools\Sign-File.ps1 -Path dist\Install-Elgato-Lights.exe, build_x64\RelWithDebInfo\obs-elgato-lights.dll
#>
param(
    [Parameter(Mandatory = $true)][string[]]$Path,
    [string]$Thumbprint = $env:TOMLINE_SIGN_THUMBPRINT,
    [string[]]$TimestampServers = @("http://timestamp.digicert.com", "http://timestamp.sectigo.com")
)

$ErrorActionPreference = "Stop"

$candidates = Get-ChildItem Cert:\CurrentUser\My | Where-Object {
    $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) -and
    ($_.EnhancedKeyUsageList.ObjectId -contains "1.3.6.1.5.5.7.3.3")
}
if ($Thumbprint) {
    $cert = $candidates | Where-Object { $_.Thumbprint -eq $Thumbprint } | Select-Object -First 1
} else {
    $cert = $candidates | Where-Object { $_.Subject -like "*TomLine prod&co*" } |
        Sort-Object NotAfter -Descending | Select-Object -First 1
}
if (-not $cert) {
    throw "Aucun certificat de signature de code valide dans Cert:\CurrentUser\My. Importez la sauvegarde .pfx (Import-PfxCertificate)."
}
Write-Host "Certificat : $($cert.Subject) (empreinte $($cert.Thumbprint), expire le $($cert.NotAfter.ToString('dd/MM/yyyy')))"

foreach ($file in $Path) {
    if (-not (Test-Path $file)) { throw "Fichier introuvable : $file" }
    $signed = $false
    foreach ($server in $TimestampServers) {
        try {
            $r = Set-AuthenticodeSignature -FilePath $file -Certificate $cert -HashAlgorithm SHA256 -TimestampServer $server
            if ($r.SignerCertificate) { $signed = $true; break }
        } catch {
            Write-Warning "Horodatage via $server impossible : $($_.Exception.Message)"
        }
    }
    if (-not $signed) {
        Write-Warning "Signature SANS horodatage pour $file"
        Set-AuthenticodeSignature -FilePath $file -Certificate $cert -HashAlgorithm SHA256 | Out-Null
    }
    $check = Get-AuthenticodeSignature -FilePath $file
    if (-not $check.SignerCertificate) { throw "Echec de signature : $file ($($check.StatusMessage))" }
    Write-Host "Signe : $file ($($check.Status))"
}
