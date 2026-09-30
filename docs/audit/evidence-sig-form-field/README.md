# Scorecard §4 #6 evidence — real digital-signature form field (feat/sig-form-consent)

Base: 02d1a898 (main; branch tip at delivery: see the lane report). Suites:
TestFormBuilder (14 slots), TestSignatureRealCrypto (34 pass / 1 pre-existing skip).

Finding: PARITY-SCORECARD-2026-09-30.md §3 row 37 / §4 row 6 (M) — the Form
Builder SIGNATURE tool hard-mapped to a plain text box
(`FormBuilderMode::toolModeToFieldType`: `case ToolMode::FormAddSignature:
return FieldType::Text; // Sig uses text box`), so "placed signature fields"
were /FT /Tx and the signing path could never sign into them.

Fix (modeled on SignatureFieldCreator::createSignatureFields):
- `AddFormFieldCommand::FieldType` gains `Signature` (appended at the end;
  the values travel only as transient ints, never persisted);
- `IFormManager`/`FormManager::addSignatureField` writes a REAL /FT /Sig field
  through the shared R01 form-save transaction: `CreateField<PdfSignature>` +
  raw user /Rect stored verbatim (W2B-1 law), placeholder /V dropped on the
  field AND widget dicts (honest UNSIGNED prepared state), and a spec-basic
  `/Lock <</Type/SigFieldLock/Action/All>>` (ISO 32000-1 §12.7.4.5 — the form
  locks when the field is signed; the engine does not consult it, so signing
  is unaffected);
- signing-path wiring is by construction: `SignatureManager::signDocumentImpl`
  signs the FIRST unsigned signature field in document order, so the placed
  field is the one the next sign fills, and `validateSignatures` (the existing
  SignatureInfo path) reads it back — unsigned until signed;
- FormBuilder UI: `toolModeToFieldType(FormAddSignature) -> FieldType::Signature`
  (Button KEEPS its text-box mapping — a different parity row), the SIGNATURE
  tool button carries a tooltip naming what it really places, and the
  placement rubber band is DISTINCT (dashed deep-green,
  `PdfViewerWidget::formRubberBandStyleFor` — pure static so the distinctness
  is pinnable offscreen);
- TestFormBuilder now links pdfws_ui (+ Qt6::Pdf/Qt6::PdfWidgets) so the UI
  placement contract is pinnable; the now-redundant explicit
  DocumentSession.cpp source was removed (duplicate moc symbols).

Pins (TestFormBuilder unless noted):
1. `signatureToolMapsToRealSignatureFieldType` — mapping pin + regression
   guards (Text/Button keep the text-box mapping).
2. `signaturePlacementWritesRealSigField` — placement through the EXACT UI
   mapping; reopened bytes are /FT /Sig, carry NO /V, carry
   /SigFieldLock/Action/All; a TEXT control field in the same document
   proves the pin distinguishes the types.
3. `signatureFieldRoundTripsAsSignature` — fresh reload sees a signature
   field (never a text field); listFields reports it.
4. `signaturePlacementBandIsDistinct` — the signature band style is non-empty,
   `dashed`, and different from the default band.
5. TestSignatureRealCrypto `placedSigFieldIsSignedIntoAndValidates` —
   extraction/validation + sign-into: validateSignatures lists the placed
   field as an UNSIGNED signature field before signing; after
   `signDocument` (B-B, real P12) exactly ONE signature exists, it lives on
   `sig_placed` (no engine-created "Signature N" field), integrity intact.

- fail-before (the old "Sig uses text box" world scoped back in: mapping ->
  Text, mutator -> text-box stand-in + TextBox validator, band style ->
  default): RED — TestFormBuilder 10 passed / 4 failed (all four pins; the
  placement pin fails with field type 4=TextBox vs 7=Signature, i.e. the old
  /FT /Tx outcome) and TestSignatureRealCrypto 33 passed / 1 failed.
- NC (same scoped revert, distinct run): TestFormBuilder 10 passed / 4 failed
  — only the new pins fail, the 10 pre-existing slots are untouched.
- pass-after ×3 serial (fix restored; full suites): see
  sigform-pass-after-{1,2,3}-*.log — 14/14 and 34 pass/1 skip every run
  (the skip is the pre-existing revoked-fixture slot).

M-3 note (checked per task instructions): the M-3 consent-gate gap is fixed on
this same branch by the signing commit — see docs/audit/evidence-m3-consent/.
