# M4-PROMPT-5 Walkthrough — Security Tools (Permissions, RemoveSecurity, Certify, Timestamp, Redact)

**Date:** 2026-05-29
**Result:** Shipped (single commit)
**Commits:** 6cbfd73

---

## COMPLETED

### D1 — Permissions dialog
- `ToolMode::SetPermissions` opens `PermissionsDialog` (print/copy/edit/annotate checkboxes + owner password).
- Calls `IPdfEditorEngine::setPermissions(PermissionFlags, ownerPassword)`.

### D2 — RemoveSecurity tool
- `ToolMode::RemoveSecurity` confirms with user → calls `IPdfEditorEngine::removeEncryption(ownerPassword)`.
- New `IPdfEditorEngine::removeEncryption` method implemented (qpdf-backed).

### D3 — Certify document
- `ToolMode::CertifyDocument` opens certificate picker → calls `ISignatureManager::certifyDocument(cert, level)`.
- Certifying signature (DocMDP) constrains allowed modification level.
- New `ISignatureManager::certifyDocument` method.

### D4 — Timestamp document
- `ToolMode::Timestamp` opens TSA URL dialog → calls `ISignatureManager::addDocumentTimestamp(tsaUrl)`.
- Document-level RFC 3161 timestamp (not signature-level); uses existing TSA client infrastructure.

### D5 — Pattern + Regex Redact from Security ribbon
- `ToolMode::PatternRedact` and `ToolMode::RegexRedact` from Security ribbon now delegate to `SecurityController::applyRedaction()` (same path as RedactMode).
- Closes the ribbon stub that previously showed `QMessageBox("scheduled for future update")`.

---

## DEFERRED

None.

---

## Notes

- Single-commit delivery in 6cbfd73. All 5 dialog implementations in one pass.
- New engine methods `removeEncryption`, `certifyDocument`, `addDocumentTimestamp` added to `IPdfEditorEngine` + `ISignatureManager` interfaces respectively.
- No walkthrough at execution time (Pattern 11). Reconstructed from commit history.

---

## CHANGELOG confirmation

Covered under `[Unreleased]` "23 ribbon tools wired" scope.
