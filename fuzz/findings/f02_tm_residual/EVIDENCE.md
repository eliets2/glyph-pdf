# EVIDENCE f02-tm-residual  (HANDOFF to native-adversary)

Type:            property-violation (redaction residual) + content-leak
Harness/target:  fuzz/bin/redaction_driver.exe (mode `tm`) -> PdfEditorEngine::applyRedactions -> PoDoFoBackend (real engine)
Property:        Edact-Ray / redaction-completeness — after redaction over a rect that
                 covers a glyph run, NO text-showing operator may re-emit the redacted glyphs.
Source root cause (per native-adversary lead, confirmed by reading code):
                 PoDoFoBackend.cpp:1324-1328 — the `Tm` handler reads ONLY the translation
                 (e,f) of "a b c d e f Tm" and discards the linear part (a,b,c,d). The span
                 intersection isIntersectingSpan() (line 1244, used at 1461) therefore models
                 rotated/scaled/skewed text as a horizontal span at (textX..textX+adv, textY),
                 so a redaction rect that covers the TRUE glyph path does not intersect the
                 MODELED span and the glyphs are never scrubbed.

Minimized input: source_input.pdf  (well-formed per `qpdf --check`; single text run:
                 `0 1 -1 0 100 700 Tm (VERTLEAK) Tj`, i.e. 90deg rotation)
Redacted output: redacted_output.pdf  (the leak: `(VERTLEAK) Tj` survives; black rect drawn at 90 720 40 65)
Oracle report:   oracle_report.json   (verdict: LEAK, residual Tj operand "VERTLEAK")
Engine self-report contradiction: the engine's own redaction log claimed
                 `"operations": ["excised_text_operators"]` for that region while the operator
                 was NOT excised — i.e. a FALSE-POSITIVE success in the audit trail.

Repro command:
  cd /c/Users/User/Projects/pdf
  bash fuzz/build_clang/build_redaction_driver.sh
  export PATH="$PWD/build:/c/msys64/ucrt64/bin:$PATH"
  ./fuzz/bin/redaction_driver.exe tm fuzz/scratch/r.pdf "VERTLEAK" 0 1 -1 0 100 700 90 7 40 65
  python fuzz/oracle/tm_residual_oracle.py --pdf fuzz/scratch/r.pdf --secret VERTLEAK

Controls proving it is the Tm linear part (not geometry error):
  - identity Tm (1 0 0 1 100 700), same-style covering rect -> CLEAN (scrubbed to `[ -7056 ] TJ`)
  - rotation Tm (0 1 -1 0 / 0.707 ... 45deg)               -> LEAK
  Matrix in fuzz/run_oracles.sh; reports in fuzz/scratch/case_*.json.

Triage notes (OBSERVATIONS only — native-adversary rules exploitability):
  - Class: information disclosure (visually-redacted text remains machine-extractable).
  - Attacker-controlled? The Tm matrix is authored in the source document; rotated text is
    common (vertical labels, stamps, CJK). Trigger does NOT require a malicious actor — any
    user redacting rotated/scaled text is exposed.
  - First user-code frame: PoDoFoBackend.cpp:1324 (Tm parse) / 1244 (span model).
  - Suggested fix direction (NOT applied): fold the full Tm matrix (and CTM) into the glyph-run
    bounding polygon before intersecting redaction rects; intersect polygon-vs-rect, not
    horizontal-span-vs-rect.

Environment: g++ 16.1.0 (ucrt64), Qt6 (ucrt64), PoDoFo vendored 1.1, qpdf 12.3.2,
             GlyphPDF v1.3.2.2, MINGW64_NT-10.0-26200, uid 197609.
