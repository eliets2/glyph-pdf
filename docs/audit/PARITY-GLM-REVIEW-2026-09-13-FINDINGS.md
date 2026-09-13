# GlyphPDF `feat/parity-glm` — Consolidated Review Findings

- **Snapshot:** base `main` 703fa34 → branch tip `9ba3cea`
- **Date:** 2026-09-13
- **Sources:** exhaustive 14-lane multi-agent Workflow (3-vote adversarial verification, harvested from journal), `/code-review high` deep pass, and manual audit + inline code confirmation.
- **Status legend:** ✅verified (adversarial 2/2 or inline-confirmed) · ⚠️cross-confirmed (two independent methods) · 🔎lead (single finder, not yet adversarially verified)

---

## Confirmed / high-confidence

### 1. ✅ HIGH — CID `/W` array out-of-bounds heap write + DoS
`src/engines/podofo/PoDoFoBackend.cpp` ~2528, `ensureCompleteCidSets()`
The `/W` array of a Type0 font's descendant CIDFont is parsed with **no bounds validation** (`GetNumber() -> int64_t`, no clamp to 0..65535).
- Negative CID, e.g. `/W [-5 [500 500 500]]`: collects cids `{-5,-4,-3}`, `maxCid=-3` → `bits` sized `(-3/8)+1 = 1`; the write loop then evaluates `bits[static_cast<size_t>(-5)/8]` (~2.3e18) → **wild OOB heap write**; `0x80u >> (cid % 8)` with negative shift is additionally UB. Heap corruption escapes `exportPdfA`'s `catch(std::exception)`.
- Huge range, e.g. `/W [0 4000000000 500]`: inserts ~4e9 entries into `std::set<int64_t>` + allocates a ~500 MB vector → **OOM / hang (DoS)**.
- **Reachable** via **Export → PDF/A** (`exportPdfA` calls `ensureCompleteCidSets` unconditionally) on any opened crafted PDF.
- **Fix:** reject/clamp `cid` to `0 <= cid <= 65535` before insert and before indexing; cap the range span.

### 2. ✅ HIGH — `#else` branch compile error (config-dependent build break)
`src/core/Capability.cpp` ~382, `probeOcrRapidModels()`
`Capability c;` is declared only inside the `#if` (RapidOCR) branch, but the `#else` branch does `c.status = …; c.whyNot = …; c.alternative = …;` and `return c;` — **`c` is undeclared on the `#else` path → compile error on any build without the RapidOCR/ONNX engine.** (The current CI/build config compiles the `#if` branch, so this is latent.)
- **Fix:** hoist `Capability c;` above the `#if`/`#else`.

### 3. ✅ HIGH — Excel export emits duplicate cell references → invalid XLSX
`src/engines/ConversionManager.cpp` ~848, `exportToExcelInHouse()`
The per-row cell loop writes `r="<col><row>"` from `el.column` with no dedup/merge. **Two non-empty runs sharing a geometry-derived column emit duplicate `<c r="B2">` refs in one `<row>` → invalid OOXML**; Excel flags the file corrupt / repairs it.
- **Fix:** merge or skip runs that resolve to the same column (last-write-wins or concatenate).

### 4. ⚠️ MEDIUM — Post-sign re-validation is fail-open *(found by both `/code-review high` and the Workflow)*
`src/engines/SignatureManager.cpp` ~1677
The D6 post-condition re-validation only fails when a returned `SignatureInfo` has `integrityIntact == false`. An **empty** `validateSignatures()` result (signature not detected/parsed on the candidate) makes the loop body never run → `lastOutcome` stays `Success` → the document is committed as successfully signed though its signatures could not be confirmed intact.
- **Fix:** treat an empty/again-unparseable result as failure, not vacuous pass.

---

## Surfaced leads (single finder — not yet adversarially verified)

- 🔎 MEDIUM — `SignatureManager.cpp` ~1352: a B-T signature-timestamp failure is silently downgraded to B-B but still reported as full `Success` (user believes they got a timestamped signature).
- 🔎 MEDIUM — `ConversionManager.cpp` ~358, `exportToHtml`: the PDF font name (`el.fontName`) is concatenated **raw** into an HTML `style` attribute (attribute-injection / breakage on names with quotes/`;`).
- 🔎 MEDIUM — `ConversionManager.cpp` ~312, `exportToExcel` (OpenXLSX path): silently overwrites a cell when two runs in a row resolve to the same coordinate (data loss).
- 🔎 MEDIUM — `Capability.cpp` ~191, `applyToWidget`: the Degraded branch never reverses a registry-owned disable, so a widget disabled once can stay permanently disabled across transitions.
- 🔎 LOW — `SignatureManager.cpp` ~1273 / ~1522: `leafCert` / `issuerCert` (raw `X509*`, not RAII) leak on several signing error/exception paths.
- 🔎 LOW — PPTX writer path (`ConversionManager.cpp` ~1199): text/fontName via `QXmlStreamWriter` — likely a non-issue (the writer escapes), flagged for completeness.

---

## From `/code-review high` — redaction-proof lane (Workflow finder never completed under the cap)

- 🔎 HIGH — `src/core/RedactionProof.cpp` ~693: the proof maps marks with `pageHeight` only, ignoring page `/Rotate` and non-zero MediaBox origin → on rotated/offset pages no source run is attributed, `removedStrings` stays empty, and the entry is certified **`verified-no-text-in-region` (PASS)** — a **false certification** of a redaction it never checked.
- 🔎 MEDIUM-HIGH — `src/core/RedactionProof.cpp` ~905: proof recall depends entirely on source-side PDFium extraction; text PDFium cannot extract (subset font, no `/ToUnicode`) yields empty `removedStrings` → passing verdict, contradicting the pack's claim that non-standard encodings are covered.
- 🔎 LOW — `src/engines/RedactOperation.cpp` ~222: burn-in overlay-label Y uses `pageHeight` without the MediaBox lower-left origin → label mispositioned on offset pages (excision itself unaffected).

---

## Round-2 (Sonnet workflow, NEW findings — excluding all of the above)

### Newly CONFIRMED (3/3 adversarial)

- **5. ✅ HIGH — AEAD not bound to entry identity** `src/core/EncryptedFileSecretStore.cpp:~250`
  AES-256-GCM ciphertexts are never bound (no AAD) to the JSON `service` key they're stored under, and `resolveKey()` is service-independent. A local actor with **write access to `secrets.enc.json` (no AES key / DPAPI access needed)** can swap the base64 blobs of two entries; each still decrypts + authenticates under its own nonce/tag, so `readSecret("api_key_prod")` silently returns the swapped secret — the exact cross-entry substitution an AEAD tag is supposed to prevent. The Windows DPAPI path shares the flaw (constant description string, `pOptionalEntropy=nullptr`). **Fix:** feed the `service` name as GCM AAD (and as DPAPI optional-entropy).
- **6. ✅ HIGH — Unbounded response buffering (DoS)** `src/engines/ai/OllamaProvider.cpp:~290`
  `reply->readAll()` buffers the entire HTTP response body with no size cap → a malicious/compromised endpoint returns a huge body and exhausts memory. **Fix:** enforce a max response size and abort when exceeded.

### High-severity LEADS (finder-surfaced; verifiers capped — need confirmation)

- 🔎 **CRITICAL-if-real** `RedactionProof.cpp:~819`: proof attribution derives `removedStrings` only from PDFium page-content runs; text living only in annotation/form-field values not present in the page content stream could go unattributed → unswept. Companion to #11/#12. (Needs confirmation of whether `extractPageTextRuns` covers appearance-stream text; the sweep *does* scan annotations/form-fields for the derived targets.)
- 🔎 HIGH `PoDoFoBackend.cpp:~236` `restoreResidentFromSource`: reloads the source without the stored `encryptionPassword` → rolling back an encrypted document fails/misbehaves.
- 🔎 HIGH `BatchMode.cpp:~1439`: async merge worker emits a phantom extra `BatchFileResult` beyond one-per-input-file → over-counting.
- 🔎 HIGH `OCRMode.cpp:~707` / `~617`: `onReOcrRegion` / `onRejectResults` lack the `ReviewState` guard that `onAcceptResults` has → invalid/re-entrant OCR actions.
- 🔎 HIGH `CompareMode.cpp:~392`: `applyChangeTypeFilters` recomputes the anchor-index role per row on every filter toggle (perf / possible correctness).
- 🔎 HIGH `ConversionManager.cpp:~238` `deriveColumns`: nearest-existing-anchor assignment within `tol` can misassign a run to the wrong column.

### Medium / low LEADS

- 🔎 MED `PoDoFoBackend.cpp:~1362` `releaseResidentFile`: never clears `d->encryptionPassword` → password lingers in memory.
- 🔎 MED `BatchMode.cpp:~1408`: a cancelled merge reports earlier input files as successes pointing at the incomplete merge output.
- 🔎 MED `SignatureManager.cpp:~1681`: N06 checked-replacement path never deletes the reserved signing candidate on two failure paths (temp leak).
- 🔎 MED `EncryptedFileSecretStore.cpp:~204`: `cipherLen` = `blob.size()` (64-bit) truncated into a 32-bit int → mis-length on a very large blob.
- 🔎 MED `OCRMode.cpp:~909`: `applyWordCorrection`/`markWordDeleted` mutate review words but never refresh the canvas (`setWords`) → stale display.
- 🔎 MED `CompareMode.cpp:~236` / `~267`: `setDiffResult` call ordering / CHANGES-tree row ordering.
- 🔎 MED `ConversionManager.cpp:~193` `clusterIntoRows`: line-join tolerance from `max(fontSize,…)` can merge distinct rows.
- 🔎 MED `RedactMode.cpp:~539`: each `runRedactOperation` allocates a `RedactOperation` parented to a long-lived object, never freed → accumulating leak.
- 🔎 LOW `EncryptedFileSecretStore.cpp:~198`: derived AES key + decrypted plaintext not zeroized after use.

### Round-2 coverage
- Completed finders (9 lanes): secret-crypto, ai-provider, ocr, compare, conversion, podofo-engine, batch, redaction-proof, signatures.
- **Still no workflow finder coverage (capped in both rounds): `pages`, `shell-arch`, `ui`.**

---

## Verification hygiene & coverage

- The adversarial pass **refuted ~7** other candidate findings (plausible-but-wrong) — not listed here.
- **Coverage gaps** (no completed Workflow finder, due to the account cap): `pages`, `shell-arch`, `ui`. `save-forms` (R01 SafeSave) and `batch` (G12 accounting) were verified in the earlier manual audit; `redaction-proof` is covered above via `/code-review high`.
- **Branch is a moving target** (tip advanced during review); the review session is fixing some findings in parallel. Re-confirm against the current tip before landing fixes.
