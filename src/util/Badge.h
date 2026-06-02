// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QLabel>

namespace gp {

// A flat single-line badge: ok / warn / err / info / beta variants.
// Reads QSS via the "variant" property.
class Badge : public QLabel {
    Q_OBJECT
public:
    enum Kind { Ok, Warn, Err, Info, Beta };
    explicit Badge(const QString& text, Kind kind, QWidget* parent = nullptr);
    void setKind(Kind kind);
};

} // namespace gp
