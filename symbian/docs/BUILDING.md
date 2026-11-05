# Native build notes

Built on Windows with the original S60 SDK 2.0, target ARMI UREL.

## Tool sources

- Nokia SDK archive: https://archive.org/download/nokia_sdks_n_dev_tools/s60_sdk_v2_0.zip
  SHA256: `e69e7cb278d805b6c9da0c7fc7588d3157296f5ccfb64e744bf57f446e0254bc`
- Windows Perl: https://archive.org/download/nokia_sdks_n_dev_tools/ActivePerl-5.6.1.635-MSWin32-x86.msi
  SHA256: `c107b8025fb7f94d3e23d7460f33fe4a036adbd643ff6f24d1b27cf759ddb8a2`
- Cygwin runtime: https://mirrors.kernel.org/sourceware/cygwin-archive/20221123/x86/release/cygwin/cygwin-3.3.6-1.tar.xz
  SHA256: `cb39b20f4bffa59efd7df2f718d53b76c811bd83f52038592c81c1538a845798`
- CAB extraction: https://github.com/twogood/unshield (tested 1.6.2).

The SDK installer CABs were extracted with unshield rather than installed.
The extracted `Epoc32` directory contains the headers, libraries and tools.
ActivePerl was also extracted locally, mapping MSI File/Component/Directory
tables to their original filenames; no machine-wide installation was used.

## Modern Windows compatibility fixes

1. The original `epoc32/gcc/bin/cygwin1.dll` crashed with access violation.
   Save that file, then replace it **inside this SDK copy only** with the
   x86 `usr/bin/cygwin1.dll` from the package linked above.
2. With the newer Cygwin runtime, normalize CRLF to LF in SDK `include`
   files (`.h`, `.rh`, `.hrh`, `.inl`) and the two GCC `specs` files beneath
   `epoc32/gcc/lib/gcc-lib`. Otherwise old CPP fails to join continued macros,
   or the assembler tries to open a carriage-return filename.
3. Use Windows Perl, not MSYS Perl. Put its `bin` in PATH before the SDK tools.
4. From the `symbian/` directory, run `build-s60.ps1` with the SDK root. It sets EPOCROOT and tool paths,
   invokes `bldmake bldfiles`, `abld build armi urel`, and `makesis`.
5. Keep the generated `.pkg` in CRLF. The old makesis silently created a
   100-byte empty package when the comment and records were LF-only.

## Build and checks

Run from `symbian/` in PowerShell, with Windows Perl on PATH:

```powershell
.\build-s60.ps1 -SdkRoot C:\S60SDK
.\build-preview.ps1 -Compiler C:\msys64\ucrt64\bin\g++.exe
python tests/verify_sis.py Skyliner700.sis
python tests/audit_arm.py C:\S60SDK PATH_TO_ARMI_UREL_OBJECTS
```

The native build produces `symbian/Skyliner700.sis`; generated files are ignored.
The preview script builds the Windows harness and runs renderer, fixed-point,
physics, input, menu and framebuffer checks. Its binaries/images are ignored.
The original floating-point physics and renderer in `upstream/` and
`tests/reference_renderer/` are test references, excluded from the phone build.

Optional native instruction checks require Python's `unicorn` package:

```powershell
python tests/arm_span_tests.py C:\S60SDK PATH_TO_SOFT3D.o
python tests/arm_scale_tests.py C:\S60SDK PATH_TO_SOFT3D.o
python tests/arm_raw_tests.py C:\S60SDK PATH_TO_SOFT3D.o
```

Unicorn checks results and ABI, not ARM920T cycle timing. The object directory
is printed in SDK build output and depends on the checkout's absolute path.
Latest validation: [VALIDATION.txt](VALIDATION.txt). Historical implementation
notes: [DEVELOPMENT_NOTES.md](DEVELOPMENT_NOTES.md).

Development UID `0x0F700660` is intended for private testing.
