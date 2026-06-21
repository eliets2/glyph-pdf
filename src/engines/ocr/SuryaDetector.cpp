// SPDX-License-Identifier: Apache-2.0
// src/engines/ocr/SuryaDetector.cpp
//
// Surya OCR not built — define HAS_SURYA and link surya libs to enable.
// See SuryaDetector.h for the licensing decision and build instructions.

#include "engines/ocr/SuryaDetector.h"
#include <QDebug>

class SuryaDetector::Private {};

SuryaDetector::SuryaDetector() : d(std::make_unique<Private>()) {}
SuryaDetector::~SuryaDetector() = default;

bool SuryaDetector::isAvailable() const
{
    return false;
}

QString SuryaDetector::name() const
{
    return QStringLiteral("Surya");
}

QList<LayoutRegion> SuryaDetector::detect(const QImage &page, gp::Lane /*lane*/)
{
    Q_UNUSED(page)
    return {};
}
