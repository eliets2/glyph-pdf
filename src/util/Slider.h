#pragma once
#include <QWidget>
#include <QSlider>

namespace gp {
// Wrapper for QSlider that draws a numeric value label to the right.
class LabeledSlider : public QWidget {
    Q_OBJECT
public:
    explicit LabeledSlider(int min, int max, int value, const QString& suffix = "%", QWidget* parent = nullptr);
    QSlider* slider() const { return _slider; }
signals:
    void valueChanged(int v);
private:
    QSlider* _slider;
    QString  _suffix;
};
} // namespace gp
