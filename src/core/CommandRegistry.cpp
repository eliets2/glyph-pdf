// SPDX-License-Identifier: Apache-2.0
#include "core/CommandRegistry.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

// The resource lives in the pdfws_ui static library (resources/commands.qrc).
// Q_INIT_RESOURCE must run at global scope and is what makes the linker keep
// the resource object, so the app and every test binary see the same file.
static void initCommandsResource() { Q_INIT_RESOURCE(commands); }

namespace gp {

namespace {

QString str(const QJsonObject& o, const char* key) {
    return o.value(QLatin1String(key)).toString();
}

} // namespace

const CommandRegistry& CommandRegistry::instance() {
    static const CommandRegistry registry = [] {
        initCommandsResource();
        CommandRegistry r;
        QFile file(QStringLiteral(":/gp/commands.json"));
        QString error;
        if (!file.open(QIODevice::ReadOnly))
            qWarning() << "CommandRegistry: :/gp/commands.json is not bundled";
        else if (!r.load(file.readAll(), &error))
            qWarning() << "CommandRegistry: could not load :/gp/commands.json:" << error;
        return r;
    }();
    return registry;
}

bool CommandRegistry::load(const QByteArray& json, QString* error) {
    auto fail = [&](const QString& why) {
        m_specs.clear();
        m_internal.clear();
        m_schema = 0;
        if (error) *error = why;
        return false;
    };

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(QStringLiteral("not valid JSON: %1").arg(parseError.errorString()));
    if (!doc.isObject())
        return fail(QStringLiteral("the document is not a JSON object"));

    const QJsonObject root = doc.object();
    const QJsonObject meta = root.value(QLatin1String("_meta")).toObject();
    const int schema = meta.value(QLatin1String("schema")).toInt();
    if (schema != kSchema)
        return fail(QStringLiteral("unsupported schema %1 (expected %2)").arg(schema).arg(kSchema));
    const QJsonValue commandsValue = root.value(QLatin1String("commands"));
    if (!commandsValue.isObject())
        return fail(QStringLiteral("\"commands\" is missing or not an object"));

    QHash<QString, CommandSpec> specs;
    const QJsonObject commands = commandsValue.toObject();
    for (auto it = commands.begin(); it != commands.end(); ++it) {
        if (!it.value().isObject())
            return fail(QStringLiteral("command \"%1\" is not an object").arg(it.key()));
        const QJsonObject o = it.value().toObject();
        CommandSpec spec;
        spec.id = it.key();
        spec.label = str(o, "label");
        spec.name = str(o, "name");
        spec.description = str(o, "description");
        spec.icon = str(o, "icon");
        spec.shortcut = str(o, "shortcut");
        spec.action = str(o, "action");
        spec.control = str(o, "control");
        spec.status = str(o, "status");
        spec.placeholder = o.value(QLatin1String("placeholder")).toBool();
        const QJsonObject planned = o.value(QLatin1String("planned")).toObject();
        spec.plannedReason = str(planned, "reason");
        spec.plannedAlternative = str(planned, "alternative");
        if (spec.id.trimmed().isEmpty())
            return fail(QStringLiteral("a command has an empty id"));
        if (spec.label.trimmed().isEmpty())
            return fail(QStringLiteral("command \"%1\" has no label").arg(spec.id));
        if (o.contains(QLatin1String("planned")) && spec.plannedReason.trimmed().isEmpty())
            return fail(QStringLiteral("planned command \"%1\" has no reason").arg(spec.id));
        specs.insert(spec.id, spec);
    }

    QHash<QString, QString> internal;
    const QJsonObject internalObj = meta.value(QLatin1String("internalToolIds")).toObject();
    for (auto it = internalObj.begin(); it != internalObj.end(); ++it)
        internal.insert(it.key(), it.value().toString());

    m_specs = std::move(specs);
    m_internal = std::move(internal);
    m_schema = schema;
    if (error) error->clear();
    return true;
}

const CommandSpec* CommandRegistry::find(const QString& id) const {
    const auto it = m_specs.constFind(id);
    return it == m_specs.constEnd() ? nullptr : &it.value();
}

const CommandSpec* CommandRegistry::find(ToolId id) const {
    return find(toolIdToString(id));
}

QStringList CommandRegistry::ids() const {
    QStringList out = m_specs.keys();
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace gp
