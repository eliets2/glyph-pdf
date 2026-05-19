#pragma once

#include <QString>

class QpdfBackend {
public:
    static bool linearize(const QString &inputPath, const QString &outputPath);
    static bool repair(const QString &inputPath, const QString &outputPath);
    static bool isLinearized(const QString &inputPath);
    static QString inspectJson(const QString &inputPath);
};
