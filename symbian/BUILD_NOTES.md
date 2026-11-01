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
4. Run `build-s60.ps1` with the SDK root. It sets EPOCROOT and tool paths,
   invokes `bldmake bldfiles`, `abld build armi urel`, and `makesis`.
5. Keep the generated `.pkg` in CRLF. The old makesis silently created a
   100-byte empty package when the comment and records were LF-only.

## Validation

Petran identifies the output as an ARM EPOC DLL with UIDs
`10000079 100039ce 0f700660`, one export, and zero writable data/BSS.
The current package has three compressed payloads: `.app`, `.rsc` and `.aif`. All were
decompressed and compared byte for byte with the SDK build outputs.

The portable renderer/core compiled under C++98 with MinGW. Tests cover
framebuffer stride/guard integrity across 80 camera poses and takeoff.
The upstream physics regression suite also passes against the adapted core.
These checks validate the build and desktop behavior. Separately, the author reported approximately 30 FPS in flight on a Nokia 6600 after the 0.1.8 optimizations.

Development UID `0x0F700660` is for private testing; obtain an appropriate UID
before a public release.

## Version 0.1.4 integer rewrite

Native sources now include fixed.cpp and use Q16.16 throughout. estlib is
removed from the MMP libraries. Long long integer division is expected,
floating-point helpers are not. tests/audit_arm.py checks all five ARM objects.
The desktop regression target now runs fixed_tests.cpp against the original
float reference (tests/reference.inc); it is excluded from the native build.

## Version 0.1.5 DSA

CDirectScreenAccess and MDirectScreenAccess use the original SDK 2.0 API.
bitgdi.lib supplies CFbsScreenDevice::Update. DSA Gc() already configures
window-relative origin and the server-approved clipping region; Paint does
not reset these. AbortNow touches only local state. Restart traps StartL,
and no GC/device pointers are retained across callbacks. DSA performance and
notification lifecycle remain on-device checks. Key 6 compares DSA with WS.

## Version 0.1.6

Phone output is DSA only. Window Server presentation and key 6 are removed.
Depth buffering and reciprocal-depth interpolation are removed. Visible
clipped polygons are stored in a bounded queue and stable-sorted far to near.
Runway strips and ground footprint cutouts reduce ordering errors; general
intersections/cyclic overlap are not solved by average-depth sorting.

## Version 0.1.7

Z buffering is restored; painter queues are removed. DSA remains the only
phone presentation backend. A single 360-entry sin table is indexed without
interpolation. Camera normalization uses precomputed constants. Physics sqrt
uses a normalized [1,4] lookup table with linear interpolation, not the old
bitwise square-root algorithm. DrawSpan unrolls eight depth-tested pixels.
Desktop checks compare checksums of all 80 frames from unrolled spans, scalar
spans and the original coverage loop; all match (86c26771).

## Version 0.1.8

Native GCC/ARM builds use inline LDRH for Pixel reads and aligned STR for
packed 2x output. Portable builds use standard loads and memcpy stores to
avoid strict-aliasing violations. Source pointers retain Pixel alignment;
packed output is selected only for a 4-byte-aligned base and even pixel stride.

Vec is POD raw Q16.16 storage; saturating arithmetic order is preserved.
Project shares one Q40 reciprocal and corrects quotient remainders twice.
For full signed Q16.16 x/y and z >= 2m the unsigned product fits 64 bits,
and the reciprocal error is below two quotient units. Tests compare one
million random vectors and boundary cases against exact signed division.
Visible terrain vertex projections are reused; clipped triangles retain
full clipping/projection. Archived 0.1.7 renderer sources are desktop-only
regression oracles. All 80 rendered frame checksums match, including the
portable scalar and original coverage paths.

## Version 0.1.9 UI and AIF

menu.cpp supplies integer-only 176x208 main/pause screens and icon geometry.
MenuState transitions are shared by the phone and Windows preview. Cached
menu CFbsBitmap adds 73216 bytes of pixel storage; it is rebuilt only on menu
changes/restart, outside the flight render loop. The HUD no longer exposes
DSA/Z-buffer or R/OUT timing labels.

The SDK MMP AIF directive compiles two bitmap/mask pairs (44x44, 42x29),
with captions and the same app UID. Black mask pixels are opaque, white
pixels are transparent. Build scripts include AIF in the legacy SIS and
verify_sis.py now expects three payloads. Source bitmaps are in aif/.

## Version 0.1.10 steering correction

Positive rendered roll lowers the right wing; forwardX=sin(yaw) makes positive
yaw a right turn. The bank yaw rate and lateral lift now use matching positive
signs. Key 7/9 yaw mapping is adjusted accordingly. A new regression rejected
the original implementation, then passed both turn directions at four headings.
The float oracle is explicitly patched with the same intentional physical fix.

## ARM instruction mode audit

The existing 0.1.10 phone build already uses 32-bit ARM instructions, not Thumb.
SDK target ARMI selects `-march=armv4t -mthumb-interwork` using `gcc`;
THUMB selects the separate `thumb-epoc-pe-gcc` compiler. Interworking enables
calls between instruction sets; it does not select Thumb instruction emission.
The existing object disassembly includes ARM SMULL and conditional instructions.

`fixed.h`, included by all six application modules, now rejects Symbian Thumb
compilation. `tests/audit_arm.py` also checks that all decoded application
instructions are 32 bits and reports integer division helper dependencies.
This does not assert that linked SDK libraries contain no Thumb code.
No mode switch or FPS improvement is claimed: the renderer was already ARM.
Software integer division remains present; ARM920T has no SDIV/UDIV instruction.
Reference: https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/divide-and-conquer

## Version 0.1.11: exact division-free perspective

DepthInverse uses a read-only 256-entry/1024-byte table, binary normalization
(no CLZ requirement), and two unsigned integer Newton steps. Seeds are lower
bounds for 2^63/n with normalized n in [2^31,2^32). Initial relative error is
below 1/256. Each step squares this error with less than two units lost to
integer rounding; after two steps the normalized absolute error is below six
units. The Q40 conversion shifts at least nine bits for z >= 131072, so one
bounded remainder correction restores floor(2^40/z) exactly. x stays below
2^32, permitting a 32-bit variable shift instead of __lshrdi3.
Existing bounded quotient corrections preserve truncation of x/y and Z depth.
All 80 image checksums equal 0.1.7; a million vectors and every table boundary
at each normalization exponent match exact reference division. Native Project
has no division/modulo/wide-shift helper relocations. Other renderer paths and
physics still use software integer division. No on-device FPS claim for 0.1.11.
Regenerate the table with `python tests/generate_reciprocal.py`.

## Version 0.1.12: division-free scanline bounds

NormalizedInverse shares the existing 1 KiB LUT between depth and edge setup.
Each non-horizontal edge caches floor(2^32/abs(dx)) once per triangle, with
bounded correction after the two Newton steps; no runtime division is needed
for this setup. Zero steps are handled separately. Reciprocal 2^32 for divisor
1 is held in UWide. For unsigned numerator n < 2^32, multiplying by the rounded
down reciprocal underestimates floor(n/d) by at most one; a remainder comparison
recovers the exact quotient. ceil(n/d) is floor((n-1)/d)+1, avoiding n+d overflow.
Span endpoint rounding therefore matches the former integer division exactly.
Tests cover all divisors 1..65536 with boundary numerators, plus a million random
32-bit cases. All 80 baseline frame hashes are unchanged (86c26771). Native
SOFT3D.o no longer imports __divsi3; __divdi3 (clipping/gradient setup) and
__modsi3 (polygon indexing) remain. No device FPS claim for 0.1.12.
