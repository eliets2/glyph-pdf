# GlyphPDF v1.0.1 — Maintenance Release

A privacy-first PDF workstation for Windows. No telemetry. No cloud. No subscription.
Licensed under [Apache-2.0](../../LICENSE).

This release fixes installability on clean machines and revives two optional features
that were silently broken in v1.0.0. **If you installed v1.0.0, upgrade to this build.**

---

## Fixed

- **"The procedure entry point could not be found in the dynamic link library" on a
  fresh Windows install.** `onnxruntime.dll` requires the Visual C++ 2015–2022 runtime
  (`VCRUNTIME140.dll`, `VCRUNTIME140_1.dll`, `MSVCP140.dll`), which wasn't bundled. The
  installer and portable ZIP now include it — GlyphPDF launches with **zero**
  prerequisites on a clean Windows 10/11 machine.
- **Office → PDF import did nothing in v1.0.0.** The LibreOffice path was resolved at
  *build* time from the build machine, so on the release builder (which had no
  LibreOffice) the feature was compiled out. It now detects a converter at **runtime**
  (a bundled copy, the PATH, Program Files, or the registry) and works with whatever the
  user has installed — LibreOffice **or** Microsoft Office.
- **PDF/A validation was permanently "unavailable" in v1.0.0**, for the same
  build-time-path reason.

## Added

- **PDF/A validation works out of the box.** veraPDF 1.30.2 and a minimal OpenJDK 21
  runtime are bundled, so conformance validation runs with **no Java install required**.
  veraPDF is invoked as a subprocess only (GPLv3+/MPLv2+ — license-safe aggregation;
  see `third_party/verapdf/LICENSE.txt` in the source tree).
- **Friendly one-click "Download…" prompts** for LibreOffice when an optional converter
  is missing, replacing the old developer-oriented "build with -D…" message.
- The **portable edition** (`GlyphPDF-1.0.1-x64-portable.zip`) — unzip and run, no
  installation, nothing written to the registry.

Everything else is unchanged from v1.0.0 — see
[release-notes-v1.0.0.md](release-notes-v1.0.0.md) for the full feature list.

---

## Installation

**System requirements:** Windows 10 (1607+) / 11, x64. ~600 MB disk space. Nothing else.

- **Installer:** download `GlyphPDF-1.0.1-x64.msi`, verify the SHA-256 below, run it.
- **Portable:** download `GlyphPDF-1.0.1-x64-portable.zip`, verify, unzip anywhere, run `GlyphPDF.exe`.

Upgrading from v1.0.0 is automatic — the MSI replaces the previous install.

---

## SHA-256 Verification

```
SHA-256: 86EB190A196EF56256F75E8998241628C8752BCCDE43A441566293934AA52486
File:    GlyphPDF-1.0.1-x64.msi

SHA-256: A4D9C681E485940C92ED63AF16E4620D7DE5F53BB2F6CE4B3A8DCC43DD9784E7
File:    GlyphPDF-1.0.1-x64-portable.zip
```

```powershell
(Get-FileHash "GlyphPDF-1.0.1-x64.msi" -Algorithm SHA256).Hash
```
