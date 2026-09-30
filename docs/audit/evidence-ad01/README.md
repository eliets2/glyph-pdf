# AD-01 evidence — DSS OCSP responder-signature verification (feat/sec-consume-ocsp)

Base: `2ccfd5ba` (main). Suite: `TestSignatureRealCrypto` (33 slots after this lane).

## What the pins prove

- `testForgedDssOcspResponderSignatureRejected` — signs a B_B document (no DSS),
  appends the ISA-allowlisted DSS revision (catalog + `/DSS` + `/Certs` + `/OCSPs`
  subtree, written through the production `buildDssDictionary` via the new
  `appendDssOcspRevisionForTesting` seam) carrying a certID-matched, fresh
  GOOD response signed by a throwaway self-signed key. The verdict must not
  stay Valid/ValidWithDSS, `isValid` must be false, and the honest reason
  (`ocspStatus == "UnverifiedResponder"`) must be reported.
- `testGenuineResponderSignedDssOcspStaysGood` — same path with the response
  signed by the issuing test CA (the authorized responder). Must keep
  Valid/ValidWithDSS + `isValid` (over-block guard).
- `testRevokedCertReportsRevoked` (repaired) — genuine CA-signed REVOKED fixture
  response injected via the same seam; verdict must be exactly "Revoked".

## Fail-before (RED on base)

`01-fail-before-TestSignatureRealCrypto.log`: 31 passed, 2 failed.
- Forged pin RED: the forged GOOD response WAS consumed (certID match + status
  read from unauthenticated data). On base + OpenSSL 3.5.7 the downgrade
  happened only incidentally, as `ocspStatus="Expired"` — see the freshness
  defect below; the pin fails because the refusal reason is not honest and the
  revocation verdict is not attacker-resistant.
- Genuine pin RED: a genuine CA-signed GOOD response degraded to UntrustedChain.

## Second defect found in the same consume block (probe-proven)

The freshness call `OCSP_check_validity(thisUpdate, nextUpdate, 0, 0)` rejects
EVERY response on OpenSSL 3.5.7 (vendored UCRT64): the 3.5 implementation
guards the max-age check with `maxsec >= 0`, so `maxsec=0` enforces
"thisUpdate not older than 0 seconds" — impossible for any real response
(thisUpdate is always in the past). Errors observed: `status too old` /
`status expired` / `error in thisupdate field` (probe:
`openssl357-ocsp-check-validity-probe.c`). The base suite only stayed green
because the REVOKED branch short-circuits before the freshness check, so the
DSS GOOD path was structurally dead. Fix: `(300, -1)` — 300 s clock skew,
max-age policy disabled (expired nextUpdate windows still fail).

## Fix

`verifyDssOcspResponder` (SignatureManager.cpp, Private) re-applies the D3
sign-side invariant at the consume site: `OCSP_basic_verify(basic, certs,
trustStore, 0)` with DSS `/Certs` as the UNTRUSTED pool only and the same
trust store the CMS chain verification used. Any failure refuses the response
entirely (both GOOD and REVOKED directions) and maps to
`UntrustedChain`/`UnverifiedResponder`/`isValid=false` (ER-1 precedent).

## Pass-after ×3 (serial)

`01-pass-after-{1,2,3}-TestSignatureRealCrypto.log`: 33 passed, 0 failed each.

## NC (scoped revert, recorded once)

`01-nc-reverted-TestSignatureRealCrypto.log`: gate + freshness args reverted
(seam kept) → 31 passed, 2 failed (same failure modes as fail-before).

## Repair note: testRevokedCertReportsRevoked

On base this pin was already red (not caused by this lane): the sign-side
offline-OCSP fixture seam (`fetchOcspResponse` under `GLYPHPDF_TESTING`) is
dead code — SignatureManager.cpp compiles into the `pdfws_engines` library,
which never receives the per-test-target `GLYPH_TESTING` define, so the
`revoked_ocsp_response.der` fixture was never loaded at sign time and no DSS
`/OCSPs` entry existed. The pin now injects the same genuine fixture response
via `appendDssOcspRevisionForTesting` (the production DSS append path); the
final assertions are unchanged (exactly "Revoked", `isValid == false`).
Owner item: the `GLYPH_TESTING`/`GLYPHPDF_TESTING` ifdef-vs-define mismatch and
its library-compilation dead zone are still in the tree (also affects the
embed-side bypass block); production builds are unaffected because
`GLYPHPDF_ENABLE_TEST_FIXTURES` stays OFF and production never defines either
macro.
