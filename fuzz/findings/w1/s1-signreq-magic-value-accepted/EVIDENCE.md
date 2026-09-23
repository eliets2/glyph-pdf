# EVIDENCE — S1: signing-request sidecar accepts any magic KEY VALUE

- **Type:** ACCEPTS-INVALID (logic; not a crash)
- **Surface:** `SigningRequestModel::fromJson` — src/core/SigningRequestModel.cpp:134-138
- **Harness:** `fuzz/harnesses/harness_signreq.cpp` (deterministic driver), corpus
  `fuzz/corpus/signreq/` (36 seeds x 32 mutants = 1120 execs; 17 finding execs,
  6 distinct seeds, single bucket)
- **Property violated:** P1 of the harness — the class contract (SigningRequestModel.h:29-32)
  says the file MUST carry the magic pair `"glyphpdf-signrequest": 1`. The decoder
  only tests the key's PRESENCE (`magic.isUndefined()`); any other value —
  999, 1.5, true, false, null, "one" — still yields a clean accept.
- **Impact:** a file that announces a different magic VALUE is parsed as a
  valid signing request. Forward-compat gatekeep ("magic: 2 means a newer
  dialect") silently degrades to key-presence-only; today the version check
  still runs, so the practical severity is LOW, but the handshake does not
  implement its own documented contract.
- **Also caught by duplicate-key mutation:** `{"glyphpdf-signrequest": 1, ...,
  "glyphpdf-signrequest": 2}` (Qt keeps the LAST duplicate) is accepted with
  magic value 2 — seed `fuzz/corpus/signreq/magic_dup_last_bad.json`.
- **Repro (deterministic, in-process):**
  ```
  PATH="$PWD/build-fz:$PATH" ./build-fz/fuzz/fuzz_signreq.exe one \
      fuzz/findings/w1/s1-signreq-magic-value-accepted/seed-magic-int-999.json
  # → VERDICT FINDING P1_ACCEPTS_INVALID_MAGIC_VALUE  (rc=42)
  ```
- **Suggested fix direction (hand-off — not applied):** after the presence
  test, require `magic.isDouble() && magic.toInt() == 1` (mirror the
  schemaVersion check), or test `magic.toInt(-1) != kSchemaVersion`.
- **Environment:** g++ 16.1.0 ucrt64, Qt 6, build-fz Debug, branch
  feat/sweep-w1-fuzz @ 8f62a17 base. No crashes, no hangs, no round-trip
  failures, no shape violations observed in the campaign.
