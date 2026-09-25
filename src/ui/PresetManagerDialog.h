// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include "core/BatchPreset.h"

class QLabel;
class QListWidget;
class QPushButton;

namespace gp {

class CapabilityRegistry;
class PresetEditorDialog;

// ── R26-P2 U7 (plan §4.9): the preset manager dialog ─────────────────────────
// The §4.2 manager surface, now with real steps: left list (name,
// description, step-count badge, modified date) + toolbar New / Duplicate /
// Edit / Rename / Delete / Import… / Export… / Run…; right detail pane with
// per-step capability badges (the batchPresetStepCapability mapping —
// Available / Degraded+detail / Unavailable+whyNot); broken preset files in
// the store root listed with their diagnostics (the brokenFiles() honesty
// surface — never silently hidden).
//
// Import/export ride the U6 store mechanics (§4.7): an existing-id import
// refuses until the caller confirms replace ("Replace existing preset 'X'?"),
// an existing export target is never silently overwritten. The interactive
// confirmations happen HERE (GUI); the store stays GUI-free. The test seams
// below are the post-dialog / post-confirm actions — tests never drive a
// native modal.
class PresetManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PresetManagerDialog(const CapabilityRegistry* capabilities,
                                 QWidget* parent = nullptr);

    // Settings-isolation seam (the BatchMode::setPresetStoreDirForTest
    // idiom): empty = defaultRootDir().
    static void setStoreRootForTest(const QString& dir);

    // Post-dialog / post-confirm actions. All refresh the list on success.
    bool importPresetForTest(const QString& path, bool confirmReplace);
    bool exportPresetForTest(const QString& id, const QString& targetPath,
                             bool confirmOverwrite);
    bool duplicatePresetForTest(const QString& id);
    bool renamePresetForTest(const QString& id, const QString& newName);
    bool deletePresetForTest(const QString& id);
    bool applyEditedPresetForTest(const BatchPreset& preset);  // editor's save

    // Seam: construct + load the editor for `id` WITHOUT exec() — the caller
    // drives it and completes through applyEditedPresetForTest. Null when the
    // id is unknown.
    PresetEditorDialog* openEditorForTest(const QString& id);

    QString selectedIdForTest() const;

signals:
    // Run…: emitted with the selected id before the dialog accepts.
    void runRequested(const QString& id);

private:
    void refresh();
    void showDetail(const BatchPreset& preset);

    BatchPresetStore m_store;
    const CapabilityRegistry* capabilities_;
    QList<BatchPreset> m_presets;
    QListWidget* presetList_   = nullptr;
    QLabel*      detail_       = nullptr;
    QLabel*      brokenLabel_  = nullptr;
    QPushButton* editBtn_      = nullptr;
    QPushButton* duplicateBtn_ = nullptr;
    QPushButton* renameBtn_    = nullptr;
    QPushButton* deleteBtn_    = nullptr;
    QPushButton* exportBtn_    = nullptr;
    QPushButton* runBtn_       = nullptr;
    QString m_lastError;         // the store's diagnostic on a failed seam call
    static QString s_storeRootForTest;
};

} // namespace gp
