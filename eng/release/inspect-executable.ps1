# VERSIONINFOと署名状態を読むだけ。製品は起動しない。
param([Parameter(Mandatory)][string]$Executable)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$item = Get-Item -LiteralPath $Executable
[ordered]@{
    fileVersion = $item.VersionInfo.FileVersion
    productVersion = $item.VersionInfo.ProductVersion
    signature = (Get-AuthenticodeSignature -LiteralPath $item.FullName).Status.ToString()
} | ConvertTo-Json -Compress
