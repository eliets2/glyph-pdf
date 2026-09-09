// SPDX-License-Identifier: Apache-2.0

#include "modes/MeasureMode.h"

#include "ui/PdfViewerWidget.h"
#include "ui/AnnotationLayer.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

// ── Honest, pinned disclosures (clause 4; pinned by TestMeasurePanelHonesty)
//
// The panel states exactly what persists and what does not, including the
// lane's deferred scopes — no silent gaps:
//  * measurements persist as standard Line/Polyline/Polygon annotations with
//    an ISO 32000-1 §12.9 /Measure dictionary; the value is snapshotted into
//    the annotation's contents text;
//  * the CALIBRATION is session-only (never written to the document);
//  * CSV export (PDF-XChange parity) is deferred;
//  * the value caption lives in the annotation's comment popup — a drawn
//    AP-stream caption for other viewers is deferred;
//  * per-viewport (Bluebeam-style) scales are deferred — one calibration;
//  * ft/in results are written to the PDF as decimal ft (/U (ft)) and shown
//    as the composite "1 ft 3 in" on screen.
QString MeasureMode::disclosureText()
{
    return QStringLiteral(
        "Measurements save as standard PDF Line / Polyline / Polygon "
        "annotations carrying an ISO 32000-1 \u00A712.9 /Measure dictionary; "
        "the measured value is also written into the annotation's contents "
        "text. The calibration itself is session-only: it is never saved with "
        "the document and resets on close.\n"
        "Disclosed limitations: no CSV export yet; other viewers show the "
        "value only in the annotation's comment popup, not as a drawn caption "
        "on the page; per-viewport scales are not supported (one calibration "
        "per document or page); feet-and-inches results are written to the "
        "PDF as decimal ft and shown as ft + in on screen.");
}

// Pure seam for the "calibrate by dragging" flow: the stroke is a Measure-
// Distance item drawn in user space; its span in pt and the user's declared
// real-world length are all the calibration needs. Recalibrating from a
// stroke drawn under an older calibration is still correct — user space is
// the invariant.
gp::measure::Scale MeasureMode::scaleFromCalibrationStroke(
    const gp::measure::Scale& strokeScale, double strokeLengthPt,
    double realLength, gp::measure::Unit unit)
{
    Q_UNUSED(strokeScale);   // user space is the invariant; kept for clarity
    return gp::measure::fromKnownLength(strokeLengthPt, realLength, unit);
}

MeasureMode::MeasureMode(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);

    auto* title = new QLabel(QStringLiteral("<b>Measure</b>"), this);
    lay->addWidget(title);

    // Persistence + deferred-scope disclosure (the honesty contract).
    m_persistInfo = new QLabel(disclosureText(), this);
    m_persistInfo->setWordWrap(true);
    m_persistInfo->setStyleSheet(QStringLiteral("color: #666;"));
    lay->addWidget(m_persistInfo);

    // ── Calibration ─────────────────────────────────────────────────────────
    auto* calGroup = new QWidget(this);
    auto* calForm = new QFormLayout(calGroup);
    calForm->setContentsMargins(0, 0, 0, 0);
    m_presetCombo = new QComboBox(calGroup);
    m_presetCombo->addItems({
        tr("Uncalibrated (pt)"),
        tr("1:100"), tr("1:200"), tr("1:500"), tr("1:1000"), tr("1:2000"),
    });
    calForm->addRow(tr("Scale preset"), m_presetCombo);
    m_unitCombo = new QComboBox(calGroup);
    m_unitCombo->addItems({ "mm", "cm", "m", "in", "ft", "pt" });
    calForm->addRow(tr("Real-world unit"), m_unitCombo);
    m_customRatio = new QLineEdit(calGroup);
    m_customRatio->setPlaceholderText(
        tr("e.g. 1:100  or  1/4 in = 1 ft"));
    calForm->addRow(tr("Custom ratio"), m_customRatio);

    auto* calBtns = new QHBoxLayout();
    m_calibrateStrokeBtn = new QToolButton(calGroup);
    m_calibrateStrokeBtn->setText(tr("Calibrate from drawing"));
    m_calibrateStrokeBtn->setToolTip(tr(
        "Measure a line of known length, then enter its true length"));
    m_clearCalibrationBtn = new QToolButton(calGroup);
    m_clearCalibrationBtn->setText(tr("Clear calibration"));
    calBtns->addWidget(m_calibrateStrokeBtn);
    calBtns->addWidget(m_clearCalibrationBtn);
    calBtns->addStretch(1);
    calForm->addRow(QString(), calBtns);

    m_scopePage = new QRadioButton(tr("This page only"), calGroup);
    m_scopeDoc = new QRadioButton(tr("Whole document"), calGroup);
    m_scopeDoc->setChecked(true);
    auto* scopeBox = new QHBoxLayout();
    scopeBox->addWidget(m_scopePage);
    scopeBox->addWidget(m_scopeDoc);
    calForm->addRow(tr("Applies to"), scopeBox);

    m_scopeInfo = new QLabel(calibrationScopeText(), this);
    m_scopeInfo->setWordWrap(true);
    m_scopeInfo->setStyleSheet(QStringLiteral("color: #666;"));
    calForm->addRow(QString(), m_scopeInfo);
    lay->addWidget(calGroup);

    // ── Tools ────────────────────────────────────────────────────────────────
    auto* toolsRow = new QHBoxLayout();
    const QStringList tools = { tr("Distance"), tr("Perimeter"), tr("Area") };
    for (int i = 0; i < tools.size(); ++i) {
        auto* b = new QToolButton(this);
        b->setText(tools[i]);
        b->setCheckable(true);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        m_toolButtons.append(b);
        toolsRow->addWidget(b);
        connect(b, &QToolButton::clicked, this, [this, i]() { onToolSelected(i); });
    }
    toolsRow->addStretch(1);
    lay->addLayout(toolsRow);

    m_snapCheck = new QCheckBox(tr("Snap to existing vertices"), this);
    lay->addWidget(m_snapCheck);

    // ── Readout + measurement list ───────────────────────────────────────────
    lay->addWidget(new QLabel(tr("Readout"), this));
    m_readout = new QLabel(QStringLiteral("—"), this);
    m_readout->setWordWrap(true);
    QFont f = m_readout->font();
    f.setBold(true);
    m_readout->setFont(f);
    lay->addWidget(m_readout);

    lay->addWidget(new QLabel(tr("Measurements"), this));
    m_measurements = new QListWidget(this);
    lay->addWidget(m_measurements, 1);

    // ── Wiring ───────────────────────────────────────────────────────────────
    connect(m_presetCombo, &QComboBox::currentIndexChanged,
            this, &MeasureMode::onScalePresetChanged);
    connect(m_unitCombo, &QComboBox::currentIndexChanged,
            this, &MeasureMode::onUnitChanged);
    connect(m_customRatio, &QLineEdit::editingFinished,
            this, &MeasureMode::onCustomRatioEdited);
    connect(m_calibrateStrokeBtn, &QToolButton::clicked,
            this, &MeasureMode::onCalibrateFromDrawing);
    connect(m_clearCalibrationBtn, &QToolButton::clicked, this, [this]() {
        m_calibratedPage = -1;
        setPendingScale(gp::measure::ptScale());
        emit statusMessageRequested(tr("Calibration cleared — measurements are in pt."));
    });
    connect(m_scopePage, &QRadioButton::toggled, this, [this](bool on) {
        m_pageScopeOnly = on;
        if (on && m_scale.calibrated && m_calibratedPage < 0 && m_viewer)
            m_calibratedPage = m_viewer->currentPage();
        m_scopeInfo->setText(calibrationScopeText());
        applyPendingScale();
    });
    connect(m_snapCheck, &QCheckBox::toggled, this, &MeasureMode::onSnapToggled);
}

void MeasureMode::setViewer(PdfViewerWidget* viewer)
{
    if (m_viewer == viewer) return;
    m_viewer = viewer;
    if (!viewer) return;
    if (auto* layer = viewer->annotationLayer()) {
        connect(layer, &AnnotationLayer::measurePreviewChanged,
                this, &MeasureMode::onMeasurePreview);
        connect(layer, &AnnotationLayer::measurementFinished,
                this, &MeasureMode::onAnnotationsChanged);
        connect(layer, &AnnotationLayer::annotationsChanged,
                this, &MeasureMode::onAnnotationsChanged);
        layer->setActiveMeasureScale(activeScaleForPage(viewer->currentPage()));
        layer->setSnapEnabled(snapEnabled());
    }
    connect(viewer, &PdfViewerWidget::pageChanged, this, [this](int, int) {
        // Page scope: the applying calibration changed with the page.
        m_scopeInfo->setText(calibrationScopeText());
        applyPendingScale();
    });
    applyPendingScale();
}

void MeasureMode::setPendingScale(const gp::measure::Scale& s)
{
    m_scale = s;
    if (s.calibrated && m_pageScopeOnly && m_calibratedPage < 0 && m_viewer)
        m_calibratedPage = m_viewer->currentPage();
    m_scopeInfo->setText(calibrationScopeText());
    applyPendingScale();
}

gp::measure::Scale MeasureMode::activeScaleForPage(int page) const
{
    // Page-scoped calibration is truthful elsewhere: plain pt, and it says so.
    if (m_scale.calibrated && m_pageScopeOnly && page != m_calibratedPage)
        return gp::measure::ptScale();
    return m_scale;
}

bool MeasureMode::pageScopeOnly() const { return m_pageScopeOnly; }

void MeasureMode::setCalibratedPage(int page)
{
    m_calibratedPage = page;
    if (m_scopeInfo) m_scopeInfo->setText(calibrationScopeText());
    applyPendingScale();
}

void MeasureMode::setPageScopeOnly(bool on)
{
    m_pageScopeOnly = on;
    if (m_scopePage) m_scopePage->setChecked(on);
    m_scopeInfo->setText(calibrationScopeText());
    applyPendingScale();
}

bool MeasureMode::snapEnabled() const
{
    return m_snapCheck && m_snapCheck->isChecked();
}

QString MeasureMode::calibrationScopeText() const
{
    if (!m_scale.calibrated)
        return tr("No calibration — measurements are in pt (not calibrated).");
    if (m_pageScopeOnly)
        return tr("Calibration applies to page %1 only; other pages measure "
                  "in pt (not calibrated).")
            .arg(m_calibratedPage + 1);
    return tr("Calibration applies to the whole document.");
}

// ── Private slots ────────────────────────────────────────────────────────────

void MeasureMode::onScalePresetChanged(int index)
{
    if (index <= 0) {
        setPendingScale(gp::measure::ptScale());
        return;
    }
    const QString text = m_presetCombo->itemText(index);
    const auto unit = gp::measure::parseUnit(m_unitCombo ? m_unitCombo->currentText()
                                                         : QString());
    const auto s = gp::measure::parseScaleRatio(
        text, unit.value_or(gp::measure::Unit::Mm),
        unit.value_or(gp::measure::Unit::Mm));
    if (!s.has_value()) {
        setPendingScale(gp::measure::ptScale());
        return;
    }
    setPendingScale(*s);
}

void MeasureMode::onUnitChanged()
{
    // The unit combo is the real-world unit for the ratio forms that omit one
    // and the display unit for drag/preset calibration — re-apply the current
    // ratio text so the change takes effect immediately.
    onCustomRatioEdited();
}

void MeasureMode::onCustomRatioEdited()
{
    if (!m_customRatio) return;
    const QString text = m_customRatio->text().trimmed();
    if (text.isEmpty()) return;   // no text = no opinion; presets rule
    const auto unit = gp::measure::parseUnit(m_unitCombo ? m_unitCombo->currentText()
                                                         : QString());
    const auto s = gp::measure::parseScaleRatio(
        text, unit.value_or(gp::measure::Unit::Mm),
        unit.value_or(gp::measure::Unit::Mm));
    if (!s.has_value()) {
        // Honest error: an unparsable (or invalid-unit) ratio never becomes a
        // calibration — the gate's G23 boundary, surfaced in the UI.
        emit statusMessageRequested(
            tr("Could not read scale \"%1\" — use \"1:100\" or "
               "\"1/4 in = 1 ft\" with known units.").arg(text));
        return;
    }
    setPendingScale(*s);
    emit statusMessageRequested(tr("Calibration set: %1").arg(s->ratio));
}

void MeasureMode::onToolSelected(int tool)
{
    const ToolMode mode = tool == 0 ? ToolMode::MeasureDistance
                        : tool == 1 ? ToolMode::MeasurePerimeter
                                    : ToolMode::MeasureArea;
    for (int i = 0; i < m_toolButtons.size(); ++i)
        m_toolButtons[i]->setChecked(i == tool);
    if (!m_viewer) return;
    m_viewer->setToolMode(mode);
    // Measure tools finalize with Enter and cancel with Esc — the layer needs
    // keyboard focus while a measure tool is armed.
    if (m_viewer->annotationLayer())
        m_viewer->annotationLayer()->setFocus();
}

void MeasureMode::onCalibrateFromDrawing()
{
    m_calibrating = true;
    emit statusMessageRequested(
        tr("Calibrate: draw a Distance line along a known length, then enter "
           "its true length. The line is discarded."));
}

void MeasureMode::onSnapToggled(bool on)
{
    if (m_viewer && m_viewer->annotationLayer())
        m_viewer->annotationLayer()->setSnapEnabled(on);
}

void MeasureMode::onAnnotationsChanged()
{
    if (!m_viewer || !m_viewer->annotationLayer()) return;
    auto* layer = m_viewer->annotationLayer();
    const QList<AnnotationItem> items = layer->annotations();

    if (m_calibrating) {
        // Consume the newest finished Distance stroke (if any) as the
        // calibration stroke. The item is then DISCARDED — disclosed in the
        // status message and in disclosureText().
        int strokeIdx = -1;
        for (int i = 0; i < items.size(); ++i)
            if (items[i].mode == ToolMode::MeasureDistance) strokeIdx = i;
        if (strokeIdx < 0) { refreshMeasurementList(); return; }

        const double strokeLen = gp::measure::polylineLength(items[strokeIdx].points);
        m_calibrating = false;   // disarm BEFORE any re-entrant signals
        if (!(strokeLen > 0.0)) {
            emit statusMessageRequested(tr("Calibration stroke too short."));
            refreshMeasurementList();
            return;
        }
        const auto unit = gp::measure::parseUnit(
            m_unitCombo ? m_unitCombo->currentText() : QString());
        if (!unit.has_value() || *unit == gp::measure::Unit::Pt) {
            emit statusMessageRequested(
                tr("Pick a real-world unit (not pt) to calibrate."));
            refreshMeasurementList();
            return;
        }
        bool ok = false;
        const double realLen = QInputDialog::getDouble(
            this, tr("Calibrate"),
            tr("True length of the measured line (%1):")
                .arg(gp::measure::unitLabel(*unit)),
            1.0, 1e-9, 1e12, 6, &ok);
        if (!ok || !(realLen > 0.0)) {
            emit statusMessageRequested(tr("Calibration cancelled."));
            refreshMeasurementList();
            return;
        }
        m_calibratedPage = -1;   // re-resolve for the (possibly) new scope
        setPendingScale(scaleFromCalibrationStroke(
            gp::measure::ptScale(), strokeLen, realLen, *unit));
        // Discard the consumed stroke from the document's items.
        QList<AnnotationItem> rest;
        for (int i = 0; i < items.size(); ++i)
            if (i != strokeIdx) rest.append(items[i]);
        QSignalBlocker block(layer);
        layer->setAnnotations(rest);
        emit statusMessageRequested(
            tr("Calibrated: %1. The calibration stroke was discarded.")
                .arg(m_scale.ratio));
        refreshMeasurementList();
        return;
    }

    refreshMeasurementList();
}

void MeasureMode::onMeasurePreview(const QString& text)
{
    m_lastReadout = text;
    m_readout->setText(text.isEmpty() ? QStringLiteral("—") : text);
}

// ── Private ──────────────────────────────────────────────────────────────────

void MeasureMode::applyPendingScale()
{
    if (!m_viewer || !m_viewer->annotationLayer()) return;
    m_viewer->annotationLayer()->setActiveMeasureScale(
        activeScaleForPage(m_viewer->currentPage()));
    if (m_lastReadout.isEmpty())
        m_readout->setText(
            m_scale.calibrated
                ? tr("Draw a measurement to read a value.")
                : tr("Draw a measurement to read a value — measurements are "
                     "in pt (not calibrated)."));
    m_scopeInfo->setText(calibrationScopeText());
}

void MeasureMode::refreshMeasurementList()
{
    if (!m_measurements) return;
    QSignalBlocker block(m_measurements);
    m_measurements->clear();
    if (!m_viewer || !m_viewer->annotationLayer()) return;
    for (const auto& a : m_viewer->annotationLayer()->annotations()) {
        if (!gp::measure::isMeasureToolMode(a.mode)) continue;
        const auto s = gp::measure::scaleFrom(
            a.measureUnitsPerPt, a.measureUnit, a.measureCalibrated, a.measureRatio);
        QString value;
        switch (a.mode) {
        case ToolMode::MeasureDistance:
            value = gp::measure::formatLengthTruthful(
                gp::measure::polylineLength(a.points), s); break;
        case ToolMode::MeasurePerimeter:
            value = gp::measure::formatLengthTruthful(
                gp::measure::closedPerimeter(a.points), s); break;
        case ToolMode::MeasureArea:
            value = gp::measure::formatAreaTruthful(
                gp::measure::polygonArea(a.points), s); break;
        default: break;
        }
        auto* row = new QListWidgetItem(
            tr("Page %1 — %2").arg(a.pageIndex + 1).arg(value), m_measurements);
        row->setToolTip(a.measureRatio);
    }
}

} // namespace gp
