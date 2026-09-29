PROGRAM-CONSOLIDATION-2026-09-25 §1.3 — XMP drop on document-property edits

Pin: TestExpiryInterface::titleEditAfterExpiryKeepsTheCustomXMPMarker
(set expiry via the engine → PoDoFoBackend::setMetadata edits title+author →
save → readExpiryDate must still return the expiry).

Files:
- pass-after-TestExpiryInterface.txt  full transcript with the fix applied
- pass-after-x3-totals.txt            3 consecutive runs, 8P/0F each
- nc-reverted-TestExpiryInterface.txt the fix scoped back to the reset variant
  (SyncXMPMetadata(true)) — the pin fails exactly as the program item
  describes: readExpiryDate(out) = Invalid QDate after the title edit, while
  the other 7 slots of the suite still pass (the revert is scoped).
- fail-before: identical code state to nc-reverted (the pin is new; the
  pre-fix behavior IS the reverted state), so nc-reverted doubles as the
  fail-before transcript.

The fix: PoDoFoBackend::setMetadata syncs with SyncXMPMetadata() (no packet
reset); the PDF/A export path (convertToPdfA) keeps its deliberate reset.
