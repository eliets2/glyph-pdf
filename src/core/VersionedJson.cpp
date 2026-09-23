// SPDX-License-Identifier: Apache-2.0
// See VersionedJson.h — the one atomic versioned-file write (SWEEP-QUALITY-NEW D1).
#include "core/VersionedJson.h"

#include <QSaveFile>

namespace gp {
namespace VersionedJson {

bool atomicWrite(const QString& path, const QByteArray& bytes, QString* err)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (err)
            *err = QStringLiteral("%1: cannot open for writing — %2")
                       .arg(path, f.errorString());
        return false;
    }
    if (f.write(bytes) < 0) {
        if (err)
            *err = QStringLiteral("%1: write failed — %2").arg(path, f.errorString());
        return false;
    }
    if (!f.commit()) {
        if (err)
            *err = QStringLiteral("%1: commit failed — %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

} // namespace VersionedJson
} // namespace gp
