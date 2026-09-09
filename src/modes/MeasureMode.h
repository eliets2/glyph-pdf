// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QWidget>
#include "core/MeasureCore.h"

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QRadioButton;
class QToolButton;
class PdfViewerWidget;

// T1 measurement toolset — the right-dock Measure panel (same hosting pattern
// as SignaturesPanel/PdfAValidationPanel: the page stays visible in the center
// while the panel owns calibration, tool selection and readouts).
//
// Honesty contract (clause 4): the panel DISCLOSES what persists (standard
// Line/Polyline/Polygon annotations carrying an ISO 32000-1 /Measure dict on
// save) and what is session-only (the calibration itself), and it never
// claims a real-world unit without a calibration (uncalibrated = pt, and it
// says so). disclosureText()/calibrationScopeText() pin the exact wording.
namespace gp {

class MeasureMode : public QWidget {
    Q_OBJECT
public:
    explicit MeasureMode(QWidget* parent = nullptr);

    // Host wiring (GpMainWindow): the panel drives the viewer's tool modes and
    // reads completed measurements back from its annotation layer.
    void setViewer(PdfViewerWidget* viewer);

    // ── Headless-testable seams ──────────────────────────────────────────────
    // The calibration the user configured (may be uncalibrated pt).
    gp::measure::Scale pendingScale() const { return m_scale; }
    void setPendingScale(const gp::measure::Scale& s);

    // The scale that ACTUALLY applies to a new measurement on `page` — page-
    // scoped calibration yields the truthful pt scale everywhere else.
    gp::measure::Scale activeScaleForPage(int page) const;

    // "This page only" vs "Whole document" (default: whole document).
    bool pageScopeOnly() const;
    void setPageScopeOnly(bool on);
    // The page a page-scoped calibration was made on (0-based). The host
    // resolves it from the viewer; tests can pin it headless.
    void setCalibratedPage(int page);

    bool snapEnabled() const;

    // The truthful persistence disclosure (unit-tested wording).
    static QString disclosureText();
    // The scope disclosure for the current radio state.
    QString calibrationScopeText() const;

    // Pure seam for the "calibrate by dragging" flow: the panel consumes the
    // calibration stroke (a MeasureDistance item in pt scale) plus the user's
    // declared real-world length. Dialog flow around this is GUI-only.
    static gp::measure::Scale scaleFromCalibrationStroke(
        const gp::measure::Scale& strokeScale, double strokeLengthPt,
        double realLength, gp::measure::Unit unit);

signals:
    // §9.8-style status relay for the host status bar.
    void statusMessageRequested(const QString& message);

private slots:
    void onScalePresetChanged(int index);
    void onUnitChanged();
    void onCustomRatioEdited();
    void onToolSelected(int tool);       // 0=distance 1=perimeter 2=area
    void onCalibrateFromDrawing();
    void onSnapToggled(bool on);
    void onAnnotationsChanged();
    void onMeasurePreview(const QString& text);

private:
    void applyPendingScale();
    void refreshMeasurementList();

    PdfViewerWidget* m_viewer = nullptr;

    // calibration state
    gp::measure::Scale m_scale = gp::measure::ptScale();
    bool   m_pageScopeOnly = false;
    int    m_calibratedPage = -1;   // meaningful only when m_pageScopeOnly
    bool   m_calibrating = false;   // "calibrate from drawing" armed

    // UI
    QComboBox*    m_presetCombo = nullptr;
    QComboBox*    m_unitCombo = nullptr;
    QLineEdit*    m_customRatio = nullptr;
    QRadioButton* m_scopePage = nullptr;
    QRadioButton* m_scopeDoc = nullptr;
    QToolButton*  m_calibrateStrokeBtn = nullptr;
    QToolButton*  m_clearCalibrationBtn = nullptr;
    QList<QToolButton*> m_toolButtons;   // distance/perimeter/area
    QCheckBox*    m_snapCheck = nullptr;
    QLabel*       m_readout = nullptr;
    QLabel*       m_scopeInfo = nullptr;
    QLabel*       m_persistInfo = nullptr;
    QListWidget*  m_measurements = nullptr;

    QString m_lastReadout;
};

} // namespace gp
