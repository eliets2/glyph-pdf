# GlyphPDF Round-2 Remediation — SUMMARY

Track the status of each finding. Add a row when a session closes a finding.

## Status legend
- **OPEN** — not yet started
- **IN PROGRESS** — branch active
- **CLOSED** — committed, tested, build green

---

## R2-4 — Ribbon triage + "Planned" disclosure + cloud-orphan removal

| Finding | Status | Evidence |
|---------|--------|----------|
| UX-01: ~52 handler-less ribbon tools silently failing | **CLOSED** | commits c036fb2 + 9b40a47 |
| UX-02: cloud-orphans sendForm/collect/submit/auditLog/dlp/policy still enabled | **CLOSED** | commit 9b40a47 |

### UX-01 closure detail

**D1 — Enumeration:**
Cross-referenced all ribbon tool string IDs in RibbonModel.cpp against ToolId.cpp alias table
and each controller handledTools(). Found: 6 tools with real engine backing but missing
wiring/aliases wired (D2), ~42 tools with no engine backing added to planned set (D3),
6 cloud-orphan tools removed (D4).

**D2 — Wired tools (real engine backing):**
- toImage: new ToolId::ToImage wired to ConvertController::exportToImage() -> IConversionEngine::TargetFormat::Image (src/engines/ConversionManager.cpp:310)
- merge: alias -> ToolId::Combine (Organize>Document)
- pageNums: alias -> ToolId::AddPageNumbers
- export (Forms>Data): alias -> ToolId::ExportData
- import (Forms>Data): alias -> ToolId::ImportData
- fromFile (Convert>To PDF): alias -> ToolId::ImportOffice

Files: src/core/ToolId.h:85 (ToImage enum), src/core/ToolId.cpp (toString+fromString),
src/shell/controllers/ConvertController.h:29, src/shell/controllers/ConvertController.cpp:29,48,490

**D3 — Planned-tools set:**
RibbonModel::plannedTools() at src/shell/RibbonModel.cpp:8 — 42 string IDs.
Ribbon::buildBody at src/shell/Ribbon.cpp:108,122 applies setEnabled(false) +
setToolTip("Planned for a future release"), no clicked signal connected.

**D4 — Cloud-orphans removed:**
src/shell/RibbonModel.cpp: Forms>Distribute group (sendForm,collect,submit) deleted.
Protect>Compliance group (auditLog,dlp,policy) deleted.
ToolId::Cloud removed from src/core/ToolId.h and src/core/ToolId.cpp.

**D5 — Integrity test:**
tests/TestRibbonIntegrity.cpp: 3 test methods covering handler coverage,
cloud-orphan absence, and planned/handler conflict detection.
CMakeLists.txt:881 — registered. Test count: 32 -> 33.

### UX-02 closure detail

Forms>Distribute and Protect>Compliance groups entirely removed from RibbonModel.cpp.
These were dead cloud infrastructure items from WP-1 cloud removal.

---

## Other findings (pending other sessions)

| Finding | Status |
|---------|--------|
| R2-1 CHAIN-1 (silent un-signing) | OPEN |
| R2-2 Silent-failure sweep | OPEN |
| R2-3 Redaction + parser hardening | OPEN |
| R2-5 Theme correctness | OPEN |
| R2-6 FormBuilder persistence | OPEN |
| R2-7 Test integrity | OPEN |
| R2-8 B-01 key purge | OPEN |
| R2-9 Capstone + release | OPEN |
