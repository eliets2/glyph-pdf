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
