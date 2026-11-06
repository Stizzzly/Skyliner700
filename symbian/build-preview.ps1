param([string]$Compiler="g++")
$ErrorActionPreference="Stop"
Push-Location $PSScriptRoot
try {
    $command=Get-Command $Compiler -ErrorAction Stop
    $env:PATH=(Split-Path $command.Source)+";"+$env:PATH
    $common=@("-std=c++98","-O2","-Wall","-Wextra","-Wno-missing-field-initializers","-Wno-class-memaccess","-Isrc","-Iupstream","src/soft3d.cpp","src/physics.cpp","src/terrain.cpp","src/fixed.cpp","src/hud.cpp","-static")
    & $Compiler @common tests/resolution_tests.cpp -o tests/resolution-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Resolution test build failed'}
    & ./tests/resolution-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Resolution switch regression failed'}
    & $Compiler @common tests/hud_tests.cpp -o tests/hud-tests.exe
    if($LASTEXITCODE -ne 0){throw 'HUD test build failed'}
    & ./tests/hud-tests.exe
    if($LASTEXITCODE -ne 0){throw 'HUD cache/clipping tests failed'}
    & $Compiler @common tests/framebuffer_tests.cpp -o tests/framebuffer-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Framebuffer test build failed'}
    & ./tests/framebuffer-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Framebuffer bounds checks failed'}
    & $Compiler @common tests/smoke.cpp -o tests/smoke.exe
    if($LASTEXITCODE -ne 0){throw "Smoke build failed"}
    Push-Location tests
    try { $normal=(& ./smoke.exe | Out-String); Write-Output $normal; if($LASTEXITCODE -ne 0){throw "Smoke checks failed"} } finally { Pop-Location }
    foreach($variant in @('SKY_SCALAR_SPANS','SKY_REFERENCE_RASTER')){
        & $Compiler @common "-D$variant" tests/smoke.cpp -o tests/smoke-check.exe
        if($LASTEXITCODE -ne 0){throw "Raster comparison build failed"}
        Push-Location tests
        try {
            $comparison=(& ./smoke-check.exe | Out-String)
            if($LASTEXITCODE -ne 0 -or $comparison -ne $normal){throw "Raster output differs for $variant"}
        } finally { Pop-Location }
    }
    Write-Output 'PASS: paired spans, scalar spans and original coverage produce identical 80-frame checksums.'
    $baseline=@($common | Where-Object { $_ -ne 'src/soft3d.cpp' })
    & $Compiler -Itests/reference_renderer @baseline tests/reference_renderer/soft3d.cpp tests/smoke.cpp -o tests/smoke-baseline.exe
    if($LASTEXITCODE -ne 0){throw 'Baseline renderer build failed'}
    Push-Location tests
    try {
        $original=(& ./smoke-baseline.exe | Out-String)
        if($LASTEXITCODE -ne 0 -or $original -ne $normal){throw 'Optimized renderer differs from 0.1.7 reference adjusted to native resolution'}
    } finally { Pop-Location }
    Write-Output 'PASS: 0.1.7 reference (native dimensions/focal length) and optimized renderer match across all 80 frames.'
    & $Compiler @baseline tests/clipping_tests.cpp -o tests/clipping-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Clipping test build failed'}
    & ./tests/clipping-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Clipping differential checks failed'}
    & $Compiler @baseline tests/raster_bounds_tests.cpp -o tests/raster-bounds-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Raster bound test build failed'}
    & ./tests/raster-bounds-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Raster bound color/Z oracle failed'}
    & $Compiler @baseline tests/projection_tests.cpp -o tests/projection-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Projection test build failed'}
    & ./tests/projection-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Projection checks failed'}
    & $Compiler -std=c++98 -O2 -Isrc tests/runtime_tests.cpp -static -o tests/runtime-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Runtime test build failed'}
    & ./tests/runtime-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Runtime policy checks failed'}
    & $Compiler @common src/menu.cpp src/preview_win32.cpp -mwindows -lgdi32 -luser32 -o Skyliner700-preview.exe
    if($LASTEXITCODE -ne 0){throw "Preview build failed"}
    & $Compiler -std=c++98 -O2 -Isrc src/menu.cpp tests/menu_tests.cpp -static -o tests/menu-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Menu test build failed'}
    & ./tests/menu-tests.exe
    if($LASTEXITCODE -ne 0){throw 'Menu navigation checks failed'}
    & $Compiler @common src/menu.cpp tests/ui_preview.cpp -o tests/ui-preview.exe
    if($LASTEXITCODE -ne 0){throw 'UI preview build failed'}
    & $Compiler -std=c++98 -O2 -Isrc src/physics.cpp src/terrain.cpp src/fixed.cpp tests/fixed_tests.cpp -static -o tests/upstream-physics.exe
    if($LASTEXITCODE -ne 0){throw "Physics regression build failed"}
    & ./tests/upstream-physics.exe
    if($LASTEXITCODE -ne 0){throw "Physics regression checks failed"}
} finally { Pop-Location }
