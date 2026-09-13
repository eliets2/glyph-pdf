// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/ISecretStore.h"

// L07 (NATIVE-LINUX-READINESS-2026-09-10) — Linux secret storage backend.
//
// The review's finding: the pre-existing non-Windows write path derived the
// store's AES key from PUBLIC identifiers (home path + machineUniqueId + a
// constant), which the source itself called obfuscation — unacceptable as the
// default credential store for a native Linux build.
//
// LibSecretStore implements ISecretStore on top of libsecret, the reference
// client for the freedesktop.org Secret Service standard: secrets are stored
// in the user's default keyring collection, encrypted and access-controlled by
// the OS keystore daemon (e.g. gnome-keyring / kwallet's secret-service
// bridge) — no app-managed key material exists or is persisted.
//
// Failure semantics (honest, never silent):
//   * Keyring locked / unavailable / D-Bus absent → store/delete return FALSE
//     with a logged reason. CredentialManager does NOT fall back to the
//     identifier-derived encrypted-file store when this backend is compiled in.
//   * readSecret of an absent entry returns {} (not an error).
//
// Threading: the synchronous libsecret calls block on D-Bus round-trips. This
// matches the CredentialManager call sites (interactive preferences flows);
// do not call from render/save worker threads.
//
// Compile gate: HAS_LIBSECRET (CMake: pkg-config libsecret-1). Runtime
// availability is probed per call — the container/CI negative path (no keyring
// present) must return definite failures, never garbage and never a silent
// "success".
class LibSecretStore : public ISecretStore {
public:
    // Persist `secret` in the user's DEFAULT collection under the GlyphPDF
    // schema with attribute service=<service>. Returns true ONLY on a
    // confirmed write.
    bool storeSecret(const QString& service, const QString& secret) override;

    // Read the secret for `service`. Empty string if absent or unreadable.
    QString readSecret(const QString& service) const override;

    // Remove every entry matching service=<service>. True when gone
    // (including the already-absent case); false on a real failure.
    bool deleteSecret(const QString& service) override;

    bool hasSecret(const QString& service) const override;

    Backend backend() const override { return Backend::SecretService; }
};
