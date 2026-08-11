# Windows XP x86 build and test

Use the dedicated 32-bit preset from a PowerShell opened in the repository:

```powershell
C:\msys64\mingw32\bin\cmake.exe --build --preset gcc-x86-xp-release
C:\msys64\mingw32\bin\ctest.exe --preset gcc-x86-xp-release --output-on-failure
```

The release folder is `cmake-build-gcc-x86-xp-release`.  Copy the complete
folder, including its `assets` subdirectory, to the XP machine.

Before the first run, install VMware Tools in the XP guest and enable VMware
3D acceleration.  Use `dxdiag` to confirm Direct3D acceleration.  The game
uses native fixed-function D3D9 and no D3DX runtime.

The VM validates 32-bit XP compatibility.  The final acceptance run is on the
physical Windows XP SP2 laptop with the Radeon Xpress 200 driver: navigate the
menus, pause with Esc, take off, make banked turns, run F5 to `TST PASS`, and
exit cleanly.
