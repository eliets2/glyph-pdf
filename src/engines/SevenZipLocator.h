// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>

// ── 7-Zip tool locator (wave-2b row 14, r3-api harmonization) ────────────────
// The encrypted-package feature's external-writer transactions launch ONE
// specific external tool: the 7-Zip console binary vendored with the
// application (third_party/7zip/, SHA-256-pinned at configure time and
// staged beside the app/test executables). Locating that tool is 7-Zip
// install policy — it does not belong on the generic SafeSave transaction
// layer, which is deliberately program-agnostic (its own header says so).
// This unit is the single owner of that policy; SafeSave carries no
// 7-Zip knowledge and nothing else knows the bundle layout.
//
// Resolution contract (wave-2b security audit F-02, CWE-427): the
// APPLICATION-OWNED (vendored, hash-pinned) copy beside the executable is
// the ONLY legitimate resolution. The earlier PATH and Program-Files
// fallback legs were REMOVED — a planted 7z.exe there would receive the
// document bytes AND the package password with no hash verification,
// silently reintroducing the external-binary dependency the vendoring
// removed. An EMPTY result means "the vendored bundle is absent": the
// caller must disclose that honestly (HomeController::createEncryptedPackage),
// never guess and never fall back to a system tool.
namespace gp::SevenZipLocator {

// Locate the vendored 7-Zip console tool beside the running executable.
// Pure lookup (no side effects). Returns the native-separated path to 7z.exe
// when the APPLICATION-OWNED bundle is present, otherwise an empty string.
// The bundled branch requires BOTH 7z.exe and 7z.dll (7z.exe is only a
// launcher — without its format engine it fails at process start), so a
// half-copied bundle reports as absent instead of failing at launch.
QString locate();

// Test seam: the same resolution against an EXPLICIT application directory
// instead of QCoreApplication::applicationDirPath(). Deliberately not part
// of `locate()` — a production caller has no business overriding the app
// dir, so the override lives behind a name that says what it is for.
QString locateForTesting(const QString& appDir);

} // namespace gp::SevenZipLocator
