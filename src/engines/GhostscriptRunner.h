// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <functional>

namespace gp {

// ── Ghostscript-assisted unlock (PRD §9.11 unlock bullet, SPEC-TRACEABILITY
//    9.17; design notes: docs/research/ghostscript-unlock-notes.md) ──────────
//
// Ghostscript re-distillation (pdfwrite) is the one practical way to BOTH drop
// owner-password restrictions AND repair structurally broken protected files:
// the source is re-interpreted page by page and written out as a fresh PDF
// that carries no /Encrypt dictionary. PoDoFo is a strict parser (it refuses
// broken files outright) and qpdf repairs structure but keeps the security
// semantics; Ghostscript does both jobs at once — which is why this feature
// is an EXTERNAL-process integration instead of an in-process engine.
//
// LICENSING CONTRACT (never violate without an owner decision): Ghostscript
// is AGPL-3.0 — unlike the LGPL 7-Zip bundle it must NEVER be vendored into
// this repository, staged beside the executable, or shipped in any installer.
// The app resolves the user's OWN Ghostscript installation at runtime
// (locator below) and discloses its absence honestly. See
// docs/research/ghostscript-unlock-notes.md §1 for the AGPL analysis the
// owner needs if bundling is ever proposed.

// ── Install-policy owner (the SevenZipLocator pattern) ──────────────────────
// Which external tool drives an operation is that tool's install policy; it
// lives HERE, not in SafeSave (program-agnostic) and not in the controllers.
// Resolution contract: the STANDARD Ghostscript install roots only
// (C:/Program Files/gs/gs*/bin, C:/Program Files (x86)/gs/gs*/bin — the AGPL
// installer's default targets), highest version directory wins. There is
// deliberately NO PATH leg: a `gs` found on PATH is whatever the ambient
// environment points at — a different version with different distiller
// behavior, or a planted binary — and this feature never launches a program
// it cannot name and place (same reasoning that removed the 7-Zip PATH legs
// in the wave-2b audit F-02, CWE-427). An EMPTY result means "not found":
// the caller MUST disclose that honestly (SecurityController::unlockPdf),
// naming the searched locations — never guess, never fall back.
namespace GhostscriptLocator {

// The standard install roots this build searches, in search order.
QStringList standardInstallRoots();

// Locate the Ghostscript console binary (gswin64c.exe / gswin32c.exe /
// gs) in the standard install roots. Pure lookup (no side effects, no
// PATH consult). Returns the native-separated absolute path of the
// highest-version installation, or an empty string when absent.
QString locate();

// Test seam: the same scan against ONE explicit root directory (the root
// that contains a `gs/gs<version>/bin/` tree). Production code has no
// business overriding the roots, so the override lives behind a name that
// says what it is for.
QString locateForTesting(const QString& rootDir);

// The console-binary filename this platform expects ("gswin64c.exe" on
// Windows, "gs" elsewhere) — exposed so tests plant the right name.
QString consoleBinaryName();

// Honest disclosure for the absent case: names every standard location the
// search covered, so a missing install is diagnosable. Never claims a
// technical fault — the tool is simply not installed where it is looked for.
QString absenceDisclosure();

} // namespace GhostscriptLocator

// ── The unlock transaction ──────────────────────────────────────────────────
//
// One bounded, cancellable, Secret-safe run of Ghostscript's pdfwrite device:
//
//   gswin64c -dSAFER -dBATCH -dNOPAUSE -sDEVICE=pdfwrite
//            -sOutputFile=<SafeSave candidate> <source>
//
// - -dSAFER is MANDATORY and emitted unconditionally (pinned by test): SAFER
//   strips PostScript-level file access from the interpreted document; a
//   hostile document must never reach the filesystem through the interpreter.
//   -dNOSAFER is never emitted by this code.
// - Owner-password-only files re-distill WITHOUT any password: Ghostscript
//   opens them with the empty user password and writes an unrestricted copy.
//   A user-password file cannot be opened at all; if a user password was
//   supplied it is delivered OFF the command line (M-1, CWE-214): the retry
//   pass expands a `-sPDFPassword="<password>"` line from a `@response file`
//   staged owner-only in the SafeSave candidates dir and deleted the moment
//   the run ends — argv carries only the `@path`, never the secret (the
//   buildUnlockArgs contract below holds: no password argument exists; pinned
//   by test). The value is additionally piped to the child's stdin for
//   prompt-reading interpreter builds. The first attempt always runs without
//   any password; the password is offered only on a second attempt after the
//   password-less attempt failed.
// - The transaction shape is the SafeSave external-writer transaction
//   (WP-R04): Ghostscript writes a unique OWNED candidate, never the
//   destination; the candidate is validated (readable, non-empty, %PDF) and
//   only then committed atomically through SafeSave::commitFileToDestination.
//   On ANY failure the destination is byte-identical to what it was and the
//   candidate is discarded — commit-or-discard, never a partial file. The
//   runner has its own bounded-process body (rather than calling
//   SafeSave::runBoundedProcess) for exactly one reason: Ghostscript's own
//   error output must be captured (merged channels, drained while the child
//   runs so a verbose run cannot deadlock on a full pipe) so failures can
//   QUOTE the tool instead of a generic exit code. The lifecycle discipline
//   is the idiom's: owned process, bounded wait, cancel/timeout KILLs it.
namespace GhostscriptRunner {

struct UnlockResult {
    enum class Stage {
        None,               // nothing attempted (e.g. source unreadable)
        Candidate,          // the SafeSave candidate could not be reserved
        Launch,             // the process could not start / was killed
        Tool,               // Ghostscript ran and failed (error quotes its output)
        ValidateCandidate,  // output missing/empty/not a PDF
        Commit              // the atomic commit refused (destination untouched)
    };
    bool ok = false;
    bool canceled = false;       // isCanceled fired (process was killed)
    Stage stage = Stage::None;   // where a !ok run stopped
    int exitCode = -1;           // Ghostscript's exit code when observed
    bool usedPassword = false;   // the stdin-password retry ran
    QString error;               // user-presentable when !ok (quotes gs output)
    QString outputPath;          // the destination, valid exactly when ok
};

// The FULL argument list for one unlock pass. Takes NO password — the secret
// travels via the @response file staged by the runner, never as an argument
// (M-1). Returns exactly: -dSAFER -dBATCH -dNOPAUSE -sDEVICE=pdfwrite
// -sOutputFile=<candidate> followed by the source path last. Pure function;
// pinned by the argv-shape test (no secret in argv, -dSAFER always present).
QStringList buildUnlockArgs(const QString& candidate, const QString& sourcePdf);

// Run the unlock. `program` is the resolved gswin64c/gs path (GhostscriptLocator).
// `destinationPdf` may equal `sourcePdf` (in-place unlock): the commit carries a
// destination-identity precondition (SafeSave captureDestinationIdentity) so a
// document that changes on disk mid-run is never overwritten. `userPassword` is
// the SOURCE's user password, offered via stdin only if the password-less pass
// fails; empty means no password available (owner-password-only files need none).
// `timeoutMs` bounds EACH pass; the wait is never unbounded.
UnlockResult unlockPdf(const QString& program,
                       const QString& sourcePdf,
                       const QString& destinationPdf,
                       const QByteArray& userPassword = {},
                       qint64 timeoutMs = 180000,
                       const std::function<bool()>& isCanceled = {});

} // namespace GhostscriptRunner
} // namespace gp
