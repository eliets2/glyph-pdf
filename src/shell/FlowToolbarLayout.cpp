// SPDX-License-Identifier: Apache-2.0
// F1 (SWEEP-W3-UI-2026-09-20): wrapping toolbar layout — see the header for
// the contract. Single-line geometry mirrors QHBoxLayout-with-stretch so the
// 1920x1080 rendering is unchanged; narrow widths wrap instead of inflating
// the window minimum.
#include "shell/FlowToolbarLayout.h"

#include <QSpacerItem>
#include <QWidget>

namespace gp {

FlowToolbarLayout::FlowToolbarLayout(QWidget* parent)
    : QLayout(parent)
{
    // The host strip follows the wrapped height; the parent QVBoxLayout
    // queries heightForWidth through the size policy.
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sp.setHeightForWidth(true);
    parent->setSizePolicy(sp);
}

FlowToolbarLayout::~FlowToolbarLayout()
{
    while (!m_items.isEmpty())
        delete takeAt(0);
}

void FlowToolbarLayout::addStretch(int stretch)
{
    Q_UNUSED(stretch);   // one stretch point per strip: it IS the break point
    if (m_stretchIndex == -1)
        m_stretchIndex = m_items.size();
    addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
}

void FlowToolbarLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
    invalidate();
}

Qt::Orientations FlowToolbarLayout::expandingDirections() const
{
    return { };
}

bool FlowToolbarLayout::hasHeightForWidth() const
{
    return true;
}

int FlowToolbarLayout::count() const
{
    return m_items.size();
}

QLayoutItem* FlowToolbarLayout::itemAt(int index) const
{
    return (index >= 0 && index < m_items.size()) ? m_items.at(index) : nullptr;
}

QLayoutItem* FlowToolbarLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
    QLayoutItem* item = m_items.takeAt(index);
    if (index < m_stretchIndex)
        --m_stretchIndex;
    else if (index == m_stretchIndex)
        m_stretchIndex = -1;   // stretch removed; single break point gone
    invalidate();
    return item;
}

void FlowToolbarLayout::invalidate()
{
    m_dirty = true;
    QLayout::invalidate();
}

QSize FlowToolbarLayout::sizeHint() const
{
    if (m_dirty) {
        m_hint = QSize(0, 0);
        m_min = QSize(0, 0);
        const QMargins m = contentsMargins();
        int total = 0;
        int spacingTotal = 0;
        for (int i = 0; i < m_items.size(); ++i) {
            const QSize h = m_items.at(i)->sizeHint();
            total += h.width();
            m_min = m_min.expandedTo(QSize(h.width(),
                                           qMax(m_lineHeightFloor, m_items.at(i)->minimumSize().height())));
            m_hint.setHeight(qMax(m_hint.height(), qMax(m_lineHeightFloor, h.height())));
            if (i > 0)
                spacingTotal += spacing();
        }
        m_hint.setWidth(total + spacingTotal + m.left() + m.right());
        m_min.setHeight(m_min.height() + m.top() + m.bottom());
        m_dirty = false;
    }
    return m_hint;
}

QSize FlowToolbarLayout::minimumSize() const
{
    if (m_dirty)
        (void)sizeHint();   // refreshes m_min as a side effect
    return m_min;
}

namespace {

// One wrapped output line: the visible items it carries plus its height.
struct Line {
    QList<int> items;
    int h = 0;
};

QList<Line> flowLines(const QList<QLayoutItem*>& items, int stretchIndex,
                      int spacing, int lineHeightFloor, int availW)
{
    QList<Line> lines;
    lines.append(Line{});
    int x = 0;
    for (int i = 0; i < items.size(); ++i) {
        if (i == stretchIndex)
            continue;   // the slack absorber has no meaning when wrapped
        const int w = items.at(i)->sizeHint().width();
        const int nextX = x == 0 ? w : x + spacing + w;
        if (x > 0 && nextX > availW) {
            lines.append(Line{});
            x = 0;
        }
        Line& line = lines.last();
        if (!line.items.isEmpty())
            x += spacing;
        line.items.append(i);
        x += w;
        line.h = qMax(line.h, qMax(lineHeightFloor, items.at(i)->sizeHint().height()));
    }
    return lines;
}

} // namespace

int FlowToolbarLayout::heightForWidth(int w) const
{
    const QSize hint = sizeHint();
    const QMargins m = contentsMargins();
    const int availW = qMax(1, w - m.left() - m.right());
    int height = hint.height();
    if (availW < hint.width() - m.left() - m.right()) {
        const QList<Line> lines = flowLines(m_items, m_stretchIndex, spacing(), m_lineHeightFloor, availW);
        height = 0;
        for (const Line& line : lines)
            height += line.h;
        height += qMax(0, lines.size() - 1) * spacing();
    }
    return height + m.top() + m.bottom();
}

void FlowToolbarLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    const QMargins m = contentsMargins();
    const int spacingV = spacing();
    const QSize hint = sizeHint();
    const int availW = rect.width() - m.left() - m.right();
    const int availH = rect.height() - m.top() - m.bottom();

    if (availW >= hint.width() - m.left() - m.right()) {
        // Single line — IDENTICAL geometry to QHBoxLayout with one stretch:
        // every item at sizeHint, the stretch spacer absorbs all the slack.
        const int slack = availW - (hint.width() - m.left() - m.right());
        int x = rect.x() + m.left();
        for (int i = 0; i < m_items.size(); ++i) {
            QLayoutItem* it = m_items.at(i);
            int w = it->sizeHint().width();
            if (i == m_stretchIndex)
                w += slack;
            const int h = it->sizeHint().height();
            it->setGeometry(QRect(x, rect.y() + m.top() + (availH - h) / 2, w, h));
            x += w + spacingV;
        }
    } else {
        // Wrapped — continuation lines instead of a wider window. The line
        // structure comes from the same flowLines() the height uses.
        const QList<Line> lines = flowLines(m_items, m_stretchIndex, spacingV, m_lineHeightFloor, availW);
        int y = rect.y() + m.top();
        for (const Line& line : lines) {
            int x = rect.x() + m.left();
            for (const int i : line.items) {
                QLayoutItem* it = m_items.at(i);
                const int w = it->sizeHint().width();
                const int h = it->sizeHint().height();
                it->setGeometry(QRect(x, y + (line.h - h) / 2, w, h));
                x += w + spacingV;
            }
            y += line.h + spacingV;
        }
    }
}

} // namespace gp

#include "moc_FlowToolbarLayout.cpp"
