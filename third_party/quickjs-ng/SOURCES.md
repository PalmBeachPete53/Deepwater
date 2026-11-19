# QuickJS-ng — origin

JavaScript engine of the Charts tab's custom indicators (src/charts/core/CustomJs.cpp), built as the static C
library `quickjs` (CMakeLists.txt and tests/charts/CMakeLists.txt).

- Project: QuickJS-ng, https://github.com/quickjs-ng/quickjs (fork of Fabrice Bellard's QuickJS)
- Licence: MIT, see LICENSE in this folder
- Release: tag `v0.18.0` (published 2026-11-01)
- Downloaded: 2026-11-16 from https://github.com/quickjs-ng/quickjs/archive/refs/tags/v0.18.0.tar.gz
- SHA-256 of that tarball: `28bcd89aa3683fd2a946537e5ce3f11cc46b5cbf3383c7561aaef3df5ff9f1dd`

Kept, unchanged (byte for byte from the tarball's `quickjs-0.18.0/` folder): the library sources `quickjs.c`,
`libregexp.c`, `libunicode.c`, `dtoa.c`, the headers they include (`quickjs.h`, `quickjs-atom.h`, `quickjs-opcode.h`,
`quickjs-c-atomics.h`, `cutils.h`, `list.h`, `dtoa.h`, `libregexp.h`, `libregexp-opcode.h`, `libunicode.h`,
`libunicode-table.h`, `builtin-array-fromasync.h`, `builtin-iterator-zip.h`, `builtin-iterator-zip-keyed.h`) and
`LICENSE`. Left out: quickjs-libc (std/os modules: files, processes — custom indicators get none of it), the qjs/qjsc
tools, tests, examples, docs, the upstream build files. `CMakeLists.txt` and this file are Deepwater's own.

Update: download the new release tarball into an empty scratch folder, check its SHA-256, copy the same files, run
the Charts core tests (custom_tests.cpp) and update this file.
