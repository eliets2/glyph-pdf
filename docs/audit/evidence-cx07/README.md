# CX-07 evidence — MCIDs inside Form XObjects now use /MCR with /Stm

Defect: the tree writer emitted a bare integer `/K` with the element's `/Pg`
pointing at the PAGE for every MCID — including MCIDs whose marked-content
sequence lives inside a Form XObject. A conforming consumer resolving a bare
integer /K searches the /Pg content stream and finds nothing there, so every
tagged Form XObject content line was structurally unresolvable.

Fix: elements now track the stream each MCID was emitted in
(`ElementRec::mcidStreams`, filled by `assignGroups`). For an MCID in a Form
XObject the writer emits a marked-content reference instead of a bare int:

    <</Type/MCR /Pg <page> /Stm <form> /MCID n>>

(`/Pg` stays the page; `/Stm` names the form). Elements spanning page and
form streams get a mixed /K array of ints and /MCR dicts. The form's
`/StructParents` and its ParentTree entries already existed (the group/
rewrite machinery numbers and marks the form's own stream); they are now
load-bearing and verified. The validator (`validateTaggedStructureTree`)
checks /MCR shape — /Type/MCR, /Pg present, /Stm resolves to a Form XObject
with /StructParents — and resolves the MCID through the form's own
ParentTree entry, never through the page-owned streams.

Files:
- `fail-before-bare-int-K.txt` — with the old bare-integer emission
  temporarily re-introduced (writer-only revert; validator stays new), both
  CX-07 tests fail their independent structure walk: no /MCR anywhere. Note
  the old writer still PASSED the validator's backward-compatible page-owned
  scan — precisely why the independent walk, not the old validator, is the
  honest detector of this defect.
- `pass-after-full.txt` — the fixed code: TestAccessibilityTagger
  `Totals: 20 passed, 0 failed, 1 skipped` (the skip is the pre-existing,
  recorded veraPDF CLI dependence). Related lanes: TestAccessibilityPanel
  16 passed, TestSweepW3UxFlows 14 passed, TestViewingModes 10 passed.

Tests (independent PoDoFo structure walks, not the engine's own verifier):
- `formXObjectMcidsUseMcrReferences` — page + Form fixture (page-stream
  text plus /Fm0 Do): the tagged tree validates; the walk resolves every
  /MCR (/Pg is the page, /Stm is the form, the form carries /StructParents
  and ParentTree[key][mcid] points back at the element), and the page text
  keeps its bare-integer /K.
- `formOnlyMcidsAlsoUseMcrReferences` — Form-only fixture: every MCID is a
  /MCR reference and NO bare-integer /K appears anywhere.
