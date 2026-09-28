# INV-1 evidence — SignatureManager unsigned-incremental catalog allowlist

Status: **REPRODUCED and fixed** (signature-validity finding, handled as HIGH).

## The concern (Codex, CODEX-REVIEW-2026-09-25.md, "Skimmed" section)

`SignatureManager::isLegitimateIncrementalAppend` (the ISA / shadow-attack scan)
allowlisted EVERY object found in the trailing bytes whose object number already
exists in the base and whose `/Type` is `/Catalog` — "Allowed to update Catalog
for B-LT DSS inclusion" — without checking WHAT changed in the catalog.

An attacker can append one unsigned incremental revision that redefines only the
Catalog object and adds `/OpenAction` (JavaScript), `/AA`, `/Names` or a `/Pages`
re-point using direct objects only. No new indirect object appears, so the scan's
"new object" rules never fire; the catalog rewrite sailed through the allowlist,
the ISA downgrade did not trigger, and the signature kept reporting
`trustStatus = "Valid"`, `isValid = true`.

## Fixture (validated end-to-end, offline, real crypto)

Suite `TestSignatureRealCrypto`, both tests use the real signing fixtures under
`tests/fixtures/signing/` (`kInputPdf`, `kP12Path` + password, test CA store):

1. `testUnsignedCatalogOnlyRevisionDowngraded` — sign the input with the real
   test key at B-B; assert the baseline validates clean
   (`trustStatus == "Valid"`, `isValid == true`); then append ONE unsigned
   incremental revision with raw PoDoFo `SaveUpdate` (exactly what an external
   attacker tool does, bypassing every app guard) whose only change is the
   Catalog amended with `/OpenAction << /S /JavaScript /JS (...) >>` as a direct
   dictionary; assert the status is downgraded away from Valid/ValidWithDSS and
   `isValid == false`.
2. `testOwnBltDssRevisionNotDowngraded` — companion guard: the app's OWN PAdES
   B-LT revision (`buildDssDictionary` — `/DSS` added to the Catalog plus DSS
   subtree objects appended) must keep validating clean, so the tightened
   allowlist does not over-block legitimate long-term-validation updates.

Before the fix, run 1 fails with the baseline assertions PASSING (so the fixture
is validated end-to-end, not self-deceiving):

```
FAIL!  : TestSignatureRealCrypto::testUnsignedCatalogOnlyRevisionDowngraded()
'info.trustStatus != QLatin1String("Valid") && info.trustStatus != QLatin1String("ValidWithDSS")'
returned FALSE. (INV-1: unsigned catalog-only revision (OpenAction) must not
validate clean, got: Valid)
```

## The fix

`src/engines/SignatureManager.cpp`: the catalog allowlist is now DSS-ONLY.
A trailing `/Type /Catalog` redefinition is legitimate only if the updated
catalog equals the base catalog except for `/DSS` (`inv1CatalogUpdateAddsDssOnly`,
comparing values with `inv1ValueEquals`, which resolves references within each
own document — PoDoFo's SaveUpdate direct/indirect normalization is not a content
change — and handles the PDF page ↔ annotation `/P` reference CYCLE with a
path-scoped pair set, so identical cycles terminate instead of exhausting the
depth cap; a real difference inside a cycle is still found because every
reachable node pair is compared exactly once). `/OpenAction`, `/AA`, `/Names`,
`/Pages` etc. rewrites are direct-object expressible and now fail the scan.

## Transcripts (this directory)

| File | Content |
|---|---|
| `INV1-fail-before.txt` | Full `TestSignatureRealCrypto` run at HEAD `SignatureManager` + the new tests: 27 passed, **1 failed** (the attack test, `got: Valid`), 1 skipped (pre-existing GLYPH_TESTING skip). |
| `INV1-pass-after-run1.txt` .. `run3.txt` | Three consecutive green runs with the fix: 28 passed, 0 failed, 1 skipped each. |

## Reproduce

```
export PATH=/c/msys64/ucrt64/bin:$PATH
cmake --build build-final --parallel 3 --target TestSignatureRealCrypto
cd build-final && QT_QPA_PLATFORM=offscreen ./TestSignatureRealCrypto.exe
```

## Suites touched (R16 — sequential, no full ctest)

| Suite | Totals |
|---|---|
| TestSignatureRealCrypto | 28 passed, 0 failed, 1 skipped (×3 runs) |
| TestSignatureValidation | 9 passed, 0 failed, 0 skipped |
| TestSignatureValidationMock | 23 passed, 0 failed, 0 skipped |
| TestSignatureAppearance | 14 passed, 0 failed, 0 skipped |
| TestCertifySelector | 14 passed, 0 failed, 0 skipped |
| TestEraseSignedGuard | 3 passed, 0 failed, 0 skipped |
| TestOptimizeSignedGuard | 4 passed, 0 failed, 0 skipped |
| TestSep13LeadBtDowngrade | 4 passed, 0 failed, 0 skipped |
| TestSignatureBadges | 25 passed, 0 failed, 0 skipped |
