// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace gp::formjs {

// The AF host-object shim: the Acrobat-dialect layer a bare ECMAScript engine
// does not provide (design doc §1.3/§2.3). This is a port of the Calculate/
// Format subset of Mozilla pdf.js' `src/scripting_api/aform.js` plus the util
// date/print layer (`src/scripting_api/util.js`, `src/shared/scripting_utils.js`),
// Apache-2.0, Copyright 2020 Mozilla Foundation — ported with attribution per
// LICENSE-3RD-PARTY.md. Golden tests (tests/TestFormJsCalc.cpp) pin the exact
// formatted outputs so drift from the reference implementation is detected.
//
// Phase 1 surface (per docs/research/form-js-implementation-plan.md §4):
//   AFSimple, AFSimple_Calculate (+ SUM/PRD/AVG/MIN/MAX with the Acrobat
//   "PRODUCT"/"AVERAGE" aliases), AFNumber_Format, AFNumber_Parse (authored
//   from the Acrobat JS API reference — pdf.js has no reference port),
//   AFPercent_Format, AFDate_Format/FormatEx, AFTime_Format/FormatEx,
//   AFParseDateEx, AFMergeChange, AFMakeNumber, AFMakeArrayFromList,
//   AFExtractNums. The util subset (printf/printd/scand) backs them.
//
// Deliberately NOT in the Phase 1 shim (P2/P3 hooks, never stubbed — the
// scripts see them as absent, per design doc §3.1 "absent, not stubbed"):
//   Keystroke family (AF*_Keystroke*) → Phase 2 /AA /K events
//   AFRange_Validate                  → Phase 2 /AA /V events
//   AFSpecial_*                       → later (same phase as Keystroke)
//
// Host contract installed by the shim (all names are globalThis properties):
//   __gpFieldValues   plain map fieldName -> current /V string (host-provided)
//   getField(name)    -> {name, value, valueAsString, getArray()} | null
//   event             transient per-event object (host resets around each run)
//   __gpLog           console/app.alert sink array (host reads it back)
//   __gpBlocked       blocked-verb entries (submitForm/mailDoc/... audit)
//   __gpNowProvider   clock seam: () => Date; host can pin it for tests
//   __gpEndEvent()    -> JSON snapshot {hasValue,value,isNumber,rc,logs,blocked}
const char* aformShimSource();
// Identifies the ported reference revision (pdf.js commit range + shim tier).
const char* aformShimVersion();

} // namespace gp::formjs
