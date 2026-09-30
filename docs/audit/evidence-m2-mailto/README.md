# M-2 evidence — mailto header injection encoded away (feat/sec-consume-ocsp)

Base: 2ccfd5ba. Suite: TestControllers (14 slots after this lane).
Finding: AUDIT-SECURITY-2026-09-25 M-2 (MEDIUM, CWE-93) — the non-MAPI share
fallback interpolated the attacker-influenceable document filename into the
mailto query unencoded (HomeController.cpp:467-468), letting
"q1 report&bcc=attacker@evil.example.pdf" or a raw CRLF payload inject a
hidden BCC header into the user's mail client.

Fix: HomeController::shareEmailUrl(subject, body) — pure static (same
testability status as planForExport) percent-encoding BOTH interpolations with
QUrl::toPercentEncoding; shareViaEmail now composes through it.

Pin: testShareEmailUrlEncodesHeaderSignificantChars — three shapes:
CRLF BCC injection, raw "&" query smuggling, and a benign readability control.
Assertions: no raw CR/LF, exactly one raw query separator ("&"), no case-
insensitive "bcc=" surviving, mailto:?subject= prefix intact, unreserved
characters stay readable.

- fail-before (old unencoded construction scoped back in): RED — 2 passed,
  1 failed (injected header survives).
- NC (same scoped revert, distinct run): 13 passed, 1 failed.
- pass-after x3 serial (full suite): 14 passed, 0 failed each.

M-3 note (checked per task instructions before touching this lane):
SendForSigningController.cpp still has ZERO OcspConsent references on this
branch — the M-3 consent-gate gap is NOT fixed here; owner item for its lane.
