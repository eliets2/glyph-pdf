// SPDX-License-Identifier: Apache-2.0
#include "core/LibSecretStore.h"

#include <QDebug>

// GLib/GDBus headers use `signals` as a plain struct-member identifier, but
// Qt defines `signals` as a macro (QObject's signal access specifier, expanded
// from the project PCH before this TU's own includes). Save/restore the macro
// around the libsecret block — the portable bridge between the two ecosystems.
#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>   // canonical path under pkg-config -I/usr/include/libsecret-1
#pragma pop_macro("signals")

namespace {

// The GlyphPDF attribute schema. SECRET_SCHEMA_DONT_MATCH_NAME: match items by
// attributes (not by the schema name), so lookups stay self-describing and
// cannot silently alias another client's named schema. Built by value-init
// (`s{}`) + field assignment so libsecret's reserved tail is guaranteed
// zero-filled on every version — no missing-field-initializer warnings under
// -Wextra, and no assumptions about the struct's private padding count.
const SecretSchema* glyphSchema()
{
    static const SecretSchema schema = []() {
        SecretSchema s{};
        s.name = "com.glyphpdf.AiKey";
        s.flags = SECRET_SCHEMA_DONT_MATCH_NAME;
        s.attributes[0] = { "service", SECRET_SCHEMA_ATTRIBUTE_STRING };
        s.attributes[1] = { nullptr, static_cast<SecretSchemaAttributeType>(0) };
        return s;
    }();
    return &schema;
}

void logError(const char* what, const QString& service, GError* err)
{
    if (err) {
        qWarning() << "LibSecretStore:" << what << "failed for" << service
                   << "-" << err->message;
    } else {
        qWarning() << "LibSecretStore:" << what << "failed for" << service;
    }
}

} // namespace

bool LibSecretStore::storeSecret(const QString& service, const QString& secret)
{
    if (service.isEmpty() || secret.isEmpty()) return false;

    const QByteArray serviceUtf8 = service.toUtf8();
    const QByteArray secretUtf8 = secret.toUtf8();
    const QString label = QStringLiteral("GlyphPDF AI key: %1").arg(service);

    GError* err = nullptr;
    const gboolean ok = secret_password_store_sync(
        glyphSchema(),
        SECRET_COLLECTION_DEFAULT,
        label.toUtf8().constData(),
        secretUtf8.constData(),
        nullptr,                              // cancellable
        &err,
        "service", serviceUtf8.constData(),   // attributes …
        nullptr);                             // … NULL-terminated
    if (!ok) {
        // L07 honesty contract: a locked/unavailable keyring is a LOUD
        // failure. CredentialManager must not degrade this to the
        // identifier-derived encrypted-file store.
        logError("Secret Service store", service, err);
        g_clear_error(&err);
        return false;
    }
    return true;
}

QString LibSecretStore::readSecret(const QString& service) const
{
    if (service.isEmpty()) return {};

    GError* err = nullptr;
    gchar* password = secret_password_lookup_sync(
        glyphSchema(),
        nullptr,
        &err,
        "service", service.toUtf8().constData(),
        nullptr);
    if (err) {
        logError("Secret Service lookup", service, err);
        g_clear_error(&err);
        return {};
    }
    if (!password) return {};   // absent — not an error
    const QString result = QString::fromUtf8(password);
    secret_password_free(password);
    return result;
}

bool LibSecretStore::deleteSecret(const QString& service)
{
    if (service.isEmpty()) return false;

    GError* err = nullptr;
    const gint removed = secret_password_clear_sync(
        glyphSchema(),
        nullptr,
        &err,
        "service", service.toUtf8().constData(),
        nullptr);
    if (removed < 0) {
        logError("Secret Service clear", service, err);
        g_clear_error(&err);
        return false;
    }
    return true;   // removed >= 0: the entry is gone (including already-absent)
}

bool LibSecretStore::hasSecret(const QString& service) const
{
    return !readSecret(service).isEmpty();
}
