// SPDX-License-Identifier: Apache-2.0
// F1 (SWEEP-W3-UI-2026-09-20): wrapping toolbar layout.
//
// Drop-in replacement for the single-row QHBoxLayout used by the mode
// toolbar strips. Those rows cannot wrap, so their layout minimum is the SUM
// of every control's minimum — which is what pushed the task screens'
// minimum widths past a 1366-wide viewport (finding F1).
//
// Contract:
//   * When the available width fits every item, the geometry is IDENTICAL to
//     a QHBoxLayout with one expanding stretch: left group at sizeHint, the
//     stretch absorbs all slack, right group pushed to the far edge. At
//     1920x1080 the screens render exactly as before.
//   * Below the single-line requirement the row wraps onto continuation
//     lines (breaking at the stretch point first) instead of forcing the
//     window wider. Every control stays visible and reachable — nothing is
//     hidden, scrolled away or collapsed silently.
//   * Height-for-width: the host strip grows taller only when wrapped.
#ifndef GP_FLOW_TOOLBAR_LAYOUT_H
#define GP_FLOW_TOOLBAR_LAYOUT_H

#include <QLayout>
#include <QList>

namespace gp {

class FlowToolbarLayout : public QLayout {
    Q_OBJECT

public:
    explicit FlowToolbarLayout(QWidget* parent);
    ~FlowToolbarLayout() override;

    // QHBoxLayout-compatible API used by the toolbar builders.
    void addStretch(int stretch = 0);

    // Floor for every rendered line's height, so an unwrapped strip keeps the
    // exact height the old setFixedHeight(Theme::ToolbarH) strip had.
    void setLineHeightFloor(int h) { m_lineHeightFloor = h; invalidate(); }

    // QLayout overrides.
    void addItem(QLayoutItem* item) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int w) const override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void invalidate() override;

private:
    QList<QLayoutItem*> m_items;
    int m_stretchIndex = -1;   // first expanding spacer (the row break point)
    int m_lineHeightFloor = 0;
    mutable QSize m_hint;
    mutable QSize m_min;
    mutable bool m_dirty = true;
};

} // namespace gp

#endif // GP_FLOW_TOOLBAR_LAYOUT_H
