// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include "core/interfaces/IPdfEditorEngine.h"  // MrcMode

class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QSpinBox;
class QButtonGroup;
class QToolButton;
struct AppContext;

namespace gp {
class Badge;

class CompressDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompressDialog(const AppContext* ctx, QWidget* parent = nullptr);

    // R12 honesty seam, re-scoped when the last unimplemented compress pass
    // shipped (font-subsetting-plan-2026-10-01): with BOTH the subset pass
    // (route A) and the unused-object sweep (21a387c) implemented, the only
    // compress capability that still cannot run in this dialog is MRC — the
    // seam now explains THAT (the Degraded MRC wording from the registry),
    // instead of the retired "font subsetting not implemented" label.
    static QString unsupportedPassExplanation();

    // Subset-scope seam: single source of truth for what the "Subset fonts"
    // pass does and which fonts it leaves untouched (CFF/Type1/OpenType,
    // unprovable usage, signed documents). Delegates to the canonical
    // CapabilityRegistry wording; used as the checkbox tooltip when no
    // registry is present (tests) — the enabled-with-scope-disclosure state
    // pinned by TestCompressDialogHonesty.
    static QString subsetScopeExplanation();

    // §9.13 measured-completion seam: builds the post-completion message from
    // the two MEASURED on-disk sizes (untouched original vs committed output,
    // both read after the write) — never the pre-execution estimate. Reports
    // the delta, and says so explicitly when the result is not smaller than
    // the original (R12 honesty precedent at the completion site). Static and
    // pure so TestCompressDialogHonesty can pin the exact wording without
    // driving the modal save dialog.
    static QString formatCompletionReport(qint64 originalBytes, qint64 newBytes,
                                          const QString& outputFileName);

private slots:
    void onPresetChanged(int id);
    void refreshEstimate();
    void onCompress();

private:
    const AppContext* _ctx = nullptr;

    QButtonGroup* _presetGroup = nullptr;

    // Advanced controls
    QCheckBox* _chkDownsample     = nullptr;
    QSpinBox*  _dpiSpin           = nullptr;
    QSpinBox*  _qualitySpin       = nullptr;
    QCheckBox* _chkDedup          = nullptr;
    QCheckBox* _chkSubsetFonts    = nullptr;
    QCheckBox* _chkRemoveUnused   = nullptr;
    QCheckBox* _chkStripMetadata  = nullptr;

    // MRC mode selector (M7-P3 D5)
    QComboBox* _mrcModeCombo      = nullptr;

    // Size display
    QLabel*       _fileLabel   = nullptr;
    QProgressBar* _origBar     = nullptr;
    QLabel*       _origVal     = nullptr;
    QProgressBar* _estBar      = nullptr;
    QLabel*       _estVal      = nullptr;
    Badge*        _reductBadge = nullptr;
    QLabel*       _detailLabel = nullptr;
    QLabel*       _mrcEstLabel = nullptr;  ///< MRC size estimate label
};

} // namespace gp
