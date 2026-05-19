#pragma once

#include "core/AnnotationTypes.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>

class AnnotationSerializer {
public:
    static QJsonDocument toJson(const QList<AnnotationItem>& items);
    static QList<AnnotationItem> fromJson(const QJsonDocument& doc);
};
