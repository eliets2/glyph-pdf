# EVIDENCE — S3: scanAccessibility leaks PdfError past its "never throws" contract

- **Type:** CRASH-class (uncaught exception escaping a documented-no-throw API)
- **Surface:** `gp::scanAccessibility` — src/engines/AccessibilityChecker.cpp:182-322.
  The header pins: "Never throws: load failures come back as loadOk=false with
  `loadError` set" (AccessibilityChecker.h:71). Only the initial `doc.Load()` is
  wrapped; every later lazy object parse (`GetCatalog()`, `GetTrailer()`,
  page/resource and AcroForm traversal) can throw `PoDoFo::PdfError`, which
  escapes the function.
- **Harness:** `fuzz/harnesses/harness_a11y.cpp`, corpus `fuzz/corpus/a11y/`
  (8 seeds × 32 mutants = 256 execs completed; 6 finding execs across 4 distinct
  base fixtures, all one bucket).
- **Observed escapes** (verbatim from campaign.log):
  - `PdfErrorCode::InvalidObject, A object was expected but not found.` (broken_xref mut8)
  - `PdfErrorCode::BrokenFile, The file content is broken.` (doc_title_ok mut14, mut21; tagged_markinfo_false_img mut27)
  - `PdfErrorCode::InvalidDataType` (doc_title_ok mut16; fields_tu_gaps mut4)
- **Impact:** in the application, an exception from scanAccessibility propagates
  through callers that do not expect one (the checker's own contract says they
  may rely on that) → terminate/crash of the panel flow.
- **Repro (deterministic from each single preserved seed):**
  ```
  PATH="$PWD/build-fz:$PATH" ./build-fz/fuzz/fuzz_a11y.exe one \
      fuzz/findings/w1/s3-a11y-scan-throws/seed-broken_xref-mut8.bin
  # → VERDICT FINDING S3_SCAN_THREW_PODOFO: ...  (rc=42; 6/6 seeds reproduce)
  ```
- **Suggested fix direction (hand-off — not applied):** wrap the whole body of
  scanAccessibility after Load in try/catch (PdfError + std::exception) and
  downgrade to a partial report, OR wrap each section (catalog/trailer/pages/
  AcroForm) individually. Mirrors what the fix surface already does.
- **Clean surfaces in this campaign:** applyAccessibilityFix never threw and
  never produced an unreadable "fixed" doc (S3_FIX_THREW / S3_OK_FIX_LEFT_
  UNREADABLE_DOC: 0 hits); the Form-XObject depth-8 cap held without crash
  (formxobject_cycle.pdf + mutants, 32/32 execs OK/LOAD_ERROR); bounded-sample
  caps never exceeded (S3_UNBOUNDED_FINDING_SAMPLE: 0 hits).
- **Environment:** g++ 16.1.0 ucrt64, Qt 6, vendored PoDoFo 1.1.0, build-fz
  Debug, feat/sweep-w1-fuzz.
