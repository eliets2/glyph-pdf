// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "core/AnnotationTypes.h"
#include "core/interfaces/ICollaboration.h"

class CollaborationManager : public QObject, public ICollaboration
{
    Q_OBJECT

public:
    explicit CollaborationManager(QObject *parent = nullptr);

    // Export annotations to a standalone JSON package
    bool exportAnnotationPackage(const QList<AnnotationItem> &annotations, const QString &outputPath) override;

    // Import annotations from a JSON package
    QList<AnnotationItem> importAnnotationPackage(const QString &inputPath) override;

    // Cloud Sync Stub (Simulation)
    bool pushToCloud(const QString &packagePath, const QString &endpoint) override;
    bool pullFromCloud(const QString &endpoint, const QString &destinationPath) override;

private:
};
