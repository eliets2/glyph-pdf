## Summary
<!-- What does this PR do? Link to the issue it closes: "Closes #N" -->

## Changes
<!-- Brief bullet list of what changed -->

## Testing
- [ ] `cmake --build build --parallel 8` succeeds with no new warnings
- [ ] `QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j4` — all tests pass
- [ ] No new `RapidOCR_Mock` / hardcoded / placeholder strings introduced
- [ ] Pattern-5 check: `grep -rniE "Fake|Mock|hardcoded|placeholder" src/ui src/modes src/shell` returns nothing unintentional

## Security checklist (if this PR touches redaction / signing / encryption / sanitization)
- [ ] No new GPL/AGPL dependencies introduced
- [ ] No JBIG2 pattern-matching mode used
- [ ] Redaction: content-stream excision confirmed (not black-rect overlay)
- [ ] New ONNX models have confirmed Apache/BSD license + provenance in PROVENANCE.md

## DCO sign-off
By submitting this PR I certify that my contributions are made under the
[Developer Certificate of Origin](https://developercertificate.org/), version 1.1.
