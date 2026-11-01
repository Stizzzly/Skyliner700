param([Parameter(Mandatory=$true)][string]$SdkRoot)
$ErrorActionPreference="Stop"
$sdk=(Resolve-Path -LiteralPath $SdkRoot).Path.TrimEnd('\')
if($sdk -match ' '){throw "Legacy SDK requires a path without spaces"}
if(-not(Test-Path "$sdk\epoc32\tools\bldmake.bat")){throw "Expected S60 SDK root containing epoc32/tools/bldmake.bat"}
if($PSScriptRoot -match ' '){throw "Copy this project to a path without spaces, e.g. C:\S60Work\KeyHookProbe"}
if([IO.Path]::GetPathRoot($sdk) -ne [IO.Path]::GetPathRoot($PSScriptRoot)){throw "Project and SDK must be on the same drive"}
$env:EPOCROOT=$sdk.Substring(2)+'\'
$env:PATH="$sdk\epoc32\tools;$sdk\epoc32\gcc\bin;"+$env:PATH
Push-Location "$PSScriptRoot\group"
try {
    & bldmake.bat bldfiles 2>&1 | Tee-Object -Variable bldOutput
    if($LASTEXITCODE -ne 0 -or -not(Test-Path ./abld.bat) -or (($bldOutput | Out-String) -match 'BLDMAKE ERROR')){throw "bldmake failed"}
    & ./abld.bat build armi urel 2>&1 | Tee-Object -Variable buildOutput
    # Some legacy batch wrappers return zero even after make/RCOMP fails.
    if($LASTEXITCODE -ne 0 -or (($buildOutput | Out-String) -match '(?im)(\*\*\*|RCOMP failed|^\s*ERROR:)')){throw "abld failed"}
    $app="$sdk\epoc32\release\armi\urel\KeyHookProbe.app"
    $rsc="$sdk\epoc32\data\z\system\apps\KeyHookProbe\KeyHookProbe.rsc"
    if(-not(Test-Path $app)){throw "Missing $app"}
    if(-not(Test-Path $rsc)){throw "Missing $rsc; inspect SDK resource build output"}
    $pkg=(Get-Content ./keyhook.pkg -Raw).Replace('@APP@',$app).Replace('@RSC@',$rsc)
    # The 2003 makesis parser requires CRLF, including after comment lines.
    $pkg=[regex]::Replace($pkg,"\r?\n","`r`n")
    [IO.File]::WriteAllText((Join-Path (Get-Location) 'keyhook.generated.pkg'),$pkg,[Text.Encoding]::ASCII)
    & makesis.exe ./keyhook.generated.pkg "$PSScriptRoot\KeyHookProbe.sis"
    if($LASTEXITCODE -ne 0){throw "makesis failed"}
    if((Get-Item "$PSScriptRoot\KeyHookProbe.sis").Length -lt 1000){throw "Unexpectedly small SIS; inspect package contents"}
    Write-Output "Built $PSScriptRoot\KeyHookProbe.sis"
} finally { Pop-Location }
