// SPDX-License-Identifier: Apache-2.0
#include "Slider.h"

#include <QHBoxLayout>
#include <QLabel>

namespace gp {

LabeledSlider::LabeledSlider(int min, int max, int value, const QString& suffix, QWidget* parent)
    : QWidget(parent), _suffix(suffix) {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(10);

    _slider = new QSlider(Qt::Horizontal, this);
    _slider->setRange(min, max);
    _slider->setValue(value);

    auto* val = new QLabel(QString::number(value) + suffix, this);
    val->setProperty("mono", true);
    val->setMinimumWidth(46);
    val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    connect(_slider, &QSlider::valueChanged, this, [this, val](int v) {
        val->setText(QString::number(v) + _suffix);
        emit valueChanged(v);
    });

    lay->addWidget(_slider, 1);
    lay->addWidget(val);
}

} // namespace gp
