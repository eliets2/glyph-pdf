# ONNX Runtime Provenance

ONNX Runtime powers RapidOcrEngine (PP-OCRv5, `HAS_RAPIDOCR`), the secondary
OCR engine of the ROVER ensemble. Like the PDFium runtime and unlike the
committed 7-Zip bundle, the multi-megabyte runtime binaries are NOT committed
to git — they are staged by `scripts/bootstrap-vendor-deps.sh` from the
official Microsoft releases, and the staged bytes are enforced by a
configure-time SHA-256 pin (CMakeLists.txt — the same discipline as the
quickjs-ng / 7-Zip / PDFium pins). The staged trees are gitignored
(`onnxruntime-win-x64-*/`, `onnxruntime-linux-*/` in `.gitignore`).

## Windows artifact (existing, recorded here 2026-10-04)

- **Artifact**: `onnxruntime-win-x64-1.17.3.zip`
- **Source URL**: https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-win-x64-1.17.3.zip
- **Version**: 1.17.3 (MIT)
- **SHA-256 (archive)**: `356a33d024f2709786bebd5d4ca06cd5392875da95daa0455aae72edc8993256`
- **Tree**: repo-root `onnxruntime-win-x64-1.17.3/` (`lib/onnxruntime.dll`
  + `lib/onnxruntime.lib` + `include/`); imported by CMakeLists.txt on WIN32.

## Native-Linux artifact (L03, 2026-10-04)

- **Artifact**: `onnxruntime-linux-x64-1.17.3.tgz` (CPU execution provider)
- **Source URL**: https://github.com/microsoft/onnxruntime/releases/download/v1.17.3/onnxruntime-linux-x64-1.17.3.tgz
- **Version**: 1.17.3 (artifact `VERSION_NUMBER`: `1.17.3`; `GIT_COMMIT_ID`:
  `4beca149a321180e95e3a0ee8bfa62d32d46ea2b`; MIT — `LICENSE` travels inside
  the staged tree). The SAME version as the Windows pin, so the ROVER
  ensemble runs against one runtime version on both platforms.
- **Upstream SHA publication status (honest record)**: no published SHA-256
  exists for this asset — the GitHub release API carries no asset `digest`
  for v1.17.3 assets and the release notes publish no checksum table. Per the
  lane honesty rules the OBSERVED hash of the 2026-10-04 download is recorded
  below and enforced from now on (any future re-download that does not match
  this pin fails the bootstrap and the configure).
- **SHA-256 (archive, observed 2026-10-04)**:
  `f2f11f9da1e3e19b22a8b378b9af57a58433f40e3db6a803e75c0ec0eba97a20`
- **SHA-256 (`lib/libonnxruntime.so` → real file `libonnxruntime.so.1.17.3`,
  SONAME `libonnxruntime.so.1.17.3`)**:
  `8bdcd79ab25e38d1d7646948e07a7d87ed95c2164a04e43dd685147fd5c86b4c`
- **Tree**: repo-root `onnxruntime-linux-x64-1.17.3/` (the tgz's own
  top-level directory); imported by CMakeLists.txt on UNIX with the
  configure-time pin check above.
- **Staging**: `scripts/bootstrap-vendor-deps.sh` (Linux branch) — archive
  hash verified at download time (download-once cache under
  `GLYPHPDF_ARTIFACT_CACHE`, default `/opt/glyphpdf-artifact-cache`),
  extracted-payload hash verified before the tree lands. Runtime resolution
  is via build RPATH (L05: Linux needs no Windows-style DLL staging).

## License

MIT License, Copyright (c) Microsoft Corporation. License text ships inside
the staged tree (`LICENSE`, `ThirdPartyNotices.txt`) and the repo-side record
lives in `LICENSE-3RD-PARTY.md`. GlyphPDF links onnxruntime in-process
(C API/C++ API headers + shared library) — MIT permits this use; the staged
license files must travel with any distribution that ships the library
(packaging follow-up for Linux install rules).
