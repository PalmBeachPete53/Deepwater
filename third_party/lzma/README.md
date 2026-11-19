# LZMA decoder (LZMA SDK 24.09)

Files taken unchanged from <https://github.com/ip7z/7zip/tree/24.09/C>: `LzmaDec.c`, `LzmaDec.h`, `7zTypes.h`,
`Precomp.h`, `Compiler.h`, `7zWindows.h`. Author: Igor Pavlov. License: public domain (see each file's header and
<https://www.7-zip.org/sdk.html>).

Deepwater uses it to read Dukascopy's `.bi5` candle files (LZMA "alone" format: 5 bytes of properties, 8 bytes of
size, then the stream) in Data › Technical data (`src/technicaldata/Dukascopy.cpp`).
