#pragma once
#include <QString>
#include <QList>
#include "core/AnnotationTypes.h"

class ICollaboration {
public:
    virtual ~ICollaboration() = default;
    virtual bool exportAnnotationPackage(const QList<AnnotationItem> &annotations, const QString &outputPath) = 0;
    virtual QList<AnnotationItem> importAnnotationPackage(const QString &inputPath) = 0;
    virtual bool pushToCloud(const QString &packagePath, const QString &endpoint) = 0;
    virtual bool pullFromCloud(const QString &endpoint, const QString &destinationPath) = 0;
protected:
    ICollaboration() = default;
    ICollaboration(const ICollaboration&) = delete;
    ICollaboration& operator=(const ICollaboration&) = delete;
};
