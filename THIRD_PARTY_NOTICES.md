# Third-party notices

Thermavip is distributed under the BSD 3-Clause licence recorded in `LICENSE`.
It also incorporates the third-party code listed below, which keeps its own
licence. Redistributing Thermavip means redistributing these notices with it.

## LdrDllNotificationHook

| | |
|---|---|
| Upstream | https://github.com/m417z/LdrDllNotificationHook |
| Licence | MIT |
| Copyright | Copyright 2023 Michael Maltsev |
| Files | `src/Logging/LdrDllNotificationHook.h`, `src/Logging/LdrDllNotificationHook.cpp`, `src/Logging/LdrInit.cpp` |
| Revision taken | not recorded |

These three units are compiled into `VipLogging`, so the notice travels with
every binary distribution.

The upstream revision is unknown: the copy carries no version marker and none
was recorded when it was taken. Until it is identified there is no way to tell
whether the local copy has diverged, or whether an upstream fix applies to it.

### MIT licence text

```
Copyright 2023 Michael Maltsev

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

## qwt

| | |
|---|---|
| Upstream | https://qwt.sourceforge.io/ |
| Licence | to be established, see below |
| Files | `src/Plotting/**` |
| Revision taken | not recorded |

The README states that the Plotting library is "a heavily modified version of
qwt". Nothing else in the tree records that: every file under `src/Plotting`
opens with the project BSD 3-Clause header naming CEA/IRFM only, and no qwt
licence text exists anywhere in the repository.

Eight files still carry qwt identifiers in comments and documentation blocks
(`QwtClipper`, `QwtCurveFitter`, `QwtRasterData`, `QWT_HIGH_DPI`, ...). Those
are leftovers of the original code, not attribution: they confirm the
derivation without discharging any obligation.

**Open question, for the project owners.** The upstream licence and the qwt
version derived from both need to be identified before the obligations can be
stated here, and the derived files marked accordingly. This notice records the
gap; it does not close it.

## Binaries shipped in the repository

Two directories hold third-party binaries that are committed and redistributed
with the project. Neither carries a licence file today.

### FFmpeg, prebuilt for MSVC

| | |
|---|---|
| Location | `3rd_64/ffmpeg-7.1-msvc/lib/` |
| Content | 16 committed binaries: avcodec, avdevice, avfilter, avformat, avutil, postproc, swresample, swscale |
| Version | 7.1, per the directory name |
| Licence | LGPL or GPL depending on how the build was configured, not recorded |

FFmpeg is LGPL by default and becomes GPL as soon as GPL components are
enabled. The build script in this repository, used on the platforms that
compile FFmpeg from source, passes `--enable-gpl` and `--enable-libx264`, so
what it produces is GPL. The configuration of the committed MSVC binaries is
not recorded anywhere, so it cannot be stated here.

### Xpdf command line tools

| | |
|---|---|
| Location | `tools/win32/` |
| Content | 10 executables: pdfdetach, pdffonts, pdfimages, pdfinfo, pdftohtml, pdftopbm, pdftopng, pdftoppm, pdftops, pdftotext |
| Upstream | Xpdf, Glyph & Cog |
| Copyright | Copyright 1996-2019 Glyph & Cog, LLC, read from the binaries themselves |
| Licence | GPL, per the upstream project |

**Open question, for the project owners.** Redistributing GPL executables
alongside a BSD 3-Clause project is possible, but it has conditions, and they
are met by nothing in the repository today: no licence text accompanies either
set of binaries, and no build configuration is recorded for FFmpeg. Deciding
how to satisfy them, or whether to stop shipping the binaries, is not something
a code change settles. This notice makes the situation visible.
