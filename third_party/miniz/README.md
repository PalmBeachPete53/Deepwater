# miniz 3.0.2

`miniz.c`, `miniz.h` and `LICENSE` taken unchanged from the amalgamated release
<https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip> (sha256 of the zip:
ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5). Authors: Rich Geldreich, RAD Game Tools, Valve
Software. License: MIT (see `LICENSE`).

Deepwater uses its streaming inflater (`tinfl_decompress`) to read Binance's public trade archives (a zip holding one
CSV) while they download, without unpacking them in memory: Data › Technical data, footprint history
(`src/technicaldata/FootprintArchive.cpp`).
