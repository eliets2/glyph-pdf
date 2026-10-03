// SPDX-License-Identifier: Apache-2.0
#include "engines/SevenZipLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace gp::SevenZipLocator {

// Contract: see the header note. Bundled-only since the wave-2b security
// audit (F-02, CWE-427); the body is verbatim from its previous home in
// SafeSave.cpp (r3-api lane extraction — SafeSave stays tool-agnostic).
QString locateForTesting(const QString& appDir)
{
    if (!appDir.isEmpty()) {
        const QString bundled = appDir + QStringLiteral("/7z.exe");
        if (QFileInfo::exists(bundled)
            && QFileInfo::exists(appDir + QStringLiteral("/7z.dll")))
            return QDir::toNativeSeparators(bundled);
    }
    return {};
}

QString locate()
{
    return locateForTesting(QCoreApplication::applicationDirPath());
}

} // namespace gp::SevenZipLocator
