// SPDX-License-Identifier: Apache-2.0
#include "ui/PresetManagerDialog.h"

#include "ui/PresetEditorDialog.h"

#include "core/BatchPreset.h"
#include "core/Capability.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace gp {

QString PresetManagerDialog::s_storeRootForTest;

PresetManagerDialog::PresetManagerDialog(const CapabilityRegistry* capabilities,
                                         QWidget* parent)
    : QDialog(parent),
      m_store(s_storeRootForTest),
      capabilities_(capabilities) {
    setWindowTitle(tr("Preset Manager"));
    setModal(true);
    resize(680, 460);

    auto* lay = new QVBoxLayout(this);

    // Broken-file disclosure: a preset file this build refuses to load is
    // never silently hidden from the user (the store's honesty surface).
    brokenLabel_ = new QLabel;
    brokenLabel_->setObjectName(QStringLiteral("presetManagerBrokenLabel"));
    brokenLabel_->setWordWrap(true);
    brokenLabel_->setStyleSheet("color:#c8442b; font-size:10px;");
    brokenLabel_->hide();
    lay->addWidget(brokenLabel_);

    auto* split = new QSplitter(this);
    presetList_ = new QListWidget;
    presetList_->setObjectName(QStringLiteral("presetManagerList"));
    split->addWidget(presetList_);

    detail_ = new QLabel;
    detail_->setObjectName(QStringLiteral("presetManagerDetail"));
    detail_->setWordWrap(true);
    detail_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detail_->setMinimumWidth(260);
    split->addWidget(detail_);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    lay->addWidget(split, 1);

    // ── Toolbar: New / Duplicate / Edit / Rename / Delete / Import / Export / Run
    auto* btns = new QHBoxLayout;
    auto* newBtn = new QPushButton(tr("New"));
    newBtn->setObjectName(QStringLiteral("presetManagerNewBtn"));
    duplicateBtn_ = new QPushButton(tr("Duplicate"));
    duplicateBtn_->setObjectName(QStringLiteral("presetManagerDuplicateBtn"));
    editBtn_ = new QPushButton(tr("Edit"));
    editBtn_->setObjectName(QStringLiteral("presetManagerEditBtn"));
    renameBtn_ = new QPushButton(tr("Rename"));
    renameBtn_->setObjectName(QStringLiteral("presetManagerRenameBtn"));
    deleteBtn_ = new QPushButton(tr("Delete"));
    deleteBtn_->setObjectName(QStringLiteral("presetManagerDeleteBtn"));
    auto* importBtn = new QPushButton(tr("Import…"));
    importBtn->setObjectName(QStringLiteral("presetManagerImportBtn"));
    exportBtn_ = new QPushButton(tr("Export…"));
    exportBtn_->setObjectName(QStringLiteral("presetManagerExportBtn"));
    runBtn_ = new QPushButton(tr("Run…"));
    runBtn_->setObjectName(QStringLiteral("presetManagerRunBtn"));
    for (QPushButton* b : { newBtn, duplicateBtn_, editBtn_, renameBtn_,
                            deleteBtn_, importBtn, exportBtn_, runBtn_ })
        btns->addWidget(b);
    btns->addStretch(1);
    lay->addLayout(btns);

    connect(newBtn, &QPushButton::clicked, this, [this] {
        PresetEditorDialog dlg(capabilities_, this);
        dlg.loadPreset(BatchPreset{});
        if (dlg.exec() == QDialog::Accepted && applyEditedPresetForTest(dlg.preset()))
            refresh();
    });
    connect(editBtn_, &QPushButton::clicked, this, [this] {
        const QString id = selectedIdForTest();
        if (id.isEmpty()) return;
        PresetEditorDialog* dlg = openEditorForTest(id);
        if (!dlg) return;
        const bool accepted = dlg->exec() == QDialog::Accepted;
        const BatchPreset edited = dlg->preset();
        dlg->deleteLater();
        if (accepted && applyEditedPresetForTest(edited))
            refresh();
    });
    connect(duplicateBtn_, &QPushButton::clicked, this, [this] {
        const QString id = selectedIdForTest();
        if (!id.isEmpty()) { duplicatePresetForTest(id); refresh(); }
    });
    connect(renameBtn_, &QPushButton::clicked, this, [this] {
        const QString id = selectedIdForTest();
        if (id.isEmpty()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Rename Preset"),
                                                   tr("New name:"), QLineEdit::Normal,
                                                   {}, &ok);
        if (ok && !name.trimmed().isEmpty()) {
            renamePresetForTest(id, name.trimmed());
            refresh();
        }
    });
    connect(deleteBtn_, &QPushButton::clicked, this, [this] {
        const QString id = selectedIdForTest();
        if (id.isEmpty()) return;
        // The interactive confirm; deletePresetForTest is the post-confirm
        // action (tests never drive this native modal).
        const auto btn = QMessageBox::question(
            this, tr("Delete Preset"),
            tr("Delete preset \"%1\"? This cannot be undone.")
                .arg(id),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (btn == QMessageBox::Yes) { deletePresetForTest(id); refresh(); }
    });
    connect(importBtn, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Import Preset"), {},
            tr("Batch presets (*.glyphpreset.json);;All Files (*)"));
        if (path.isEmpty()) return;
        QString err;
        if (!m_store.importFrom(path, false, &err)) {
            // An existing id is never silently replaced — ask, then import
            // with the confirm (the U6 conflict policy's GUI half).
            if (err.contains(QStringLiteral("already exists"))
                && QMessageBox::question(
                       this, tr("Replace Existing Preset?"), err
                           + QStringLiteral("\n\n")
                           + tr("Replace it with the imported preset?"),
                       QMessageBox::Yes | QMessageBox::Cancel,
                       QMessageBox::Cancel) == QMessageBox::Yes) {
                if (m_store.importFrom(path, true, &err)) { refresh(); return; }
            }
            QMessageBox::warning(this, tr("Import Failed"), err);
            return;
        }
        refresh();
    });
    connect(exportBtn_, &QPushButton::clicked, this, [this] {
        const QString id = selectedIdForTest();
        if (id.isEmpty()) return;
        const QString target = QFileDialog::getSaveFileName(
            this, tr("Export Preset"),
            id + QStringLiteral(".glyphpreset.json"),
            tr("Batch presets (*.glyphpreset.json);;All Files (*)"));
        if (target.isEmpty()) return;
        QString err;
        if (!m_store.exportTo(id, target, false, &err)) {
            if (err.contains(QStringLiteral("already exists"))
                && QMessageBox::question(
                       this, tr("Overwrite Existing File?"), err
                           + QStringLiteral("\n\n")
                           + tr("Overwrite it with the exported preset?"),
                       QMessageBox::Yes | QMessageBox::Cancel,
                       QMessageBox::Cancel) == QMessageBox::Yes) {
                if (m_store.exportTo(id, target, true, &err)) return;
            }
            QMessageBox::warning(this, tr("Export Failed"), err);
        }
    });
    connect(runBtn_, &QPushButton::clicked, this, [this] {
        const QString id = selectedIdForTest();
        if (id.isEmpty()) return;
        emit runRequested(id);
        accept();
    });

    connect(presetList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_presets.size())
            showDetail(m_presets.at(row));
        else
            detail_->setText(tr("No preset selected."));
    });

    refresh();
}

void PresetManagerDialog::setStoreRootForTest(const QString& dir) {
    s_storeRootForTest = dir;
}

void PresetManagerDialog::refresh() {
    m_presets = m_store.list();
    presetList_->clear();
    for (const BatchPreset& p : m_presets) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1  (%2 step%3)\n%4")
                .arg(p.name)
                .arg(p.steps.size())
                .arg(p.steps.size() == 1 ? QString() : QStringLiteral("s"))
                .arg(p.modified.isValid()
                         ? p.modified.toString(QStringLiteral("yyyy-MM-dd"))
                         : QString()),
            presetList_);
        if (!p.description.isEmpty())
            item->setToolTip(p.description);
    }
    // The honesty surface: broken files with their diagnostics, never hidden.
    const auto broken = m_store.brokenFiles();
    if (broken.isEmpty()) {
        brokenLabel_->hide();
    } else {
        QStringList lines;
        for (const auto& b : broken)
            lines << QStringLiteral("%1: %2").arg(b.path, b.error);
        brokenLabel_->setText(tr("Broken preset files in the store:") + "\n"
                              + lines.join(QStringLiteral("\n")));
        brokenLabel_->show();
    }
    if (!m_presets.isEmpty())
        presetList_->setCurrentRow(0);
    else
        detail_->setText(tr("No presets yet — create one with New, or import a "
                            "preset file."));
}

void PresetManagerDialog::showDetail(const BatchPreset& preset) {
    QString html = QStringLiteral("<b>%1</b>").arg(preset.name.toHtmlEscaped());
    if (!preset.description.isEmpty())
        html += QStringLiteral("<br>%1").arg(preset.description.toHtmlEscaped());
    html += QStringLiteral("<br><span style=\"color:#71747a;\">id: %1 · %2 step(s)"
                           " · modified %3</span>")
                .arg(preset.id)
                .arg(preset.steps.size())
                .arg(preset.modified.isValid()
                         ? preset.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                         : QStringLiteral("?"));
    for (int i = 0; i < preset.steps.size(); ++i) {
        const BatchPresetStep& step = preset.steps.at(i);
        const Capability cap = batchPresetStepCapability(step, capabilities_);
        const char* color = cap.status == Availability::Available ? "#5b9bd5"
                            : cap.status == Availability::Degraded ? "#c8a000"
                                                                   : "#c8442b";
        html += QStringLiteral("<br><font color=\"%1\">●</font> %2. %3 — %4")
                    .arg(color)
                    .arg(i + 1)
                    .arg(step.op.toHtmlEscaped())
                    .arg(cap.status == Availability::Available
                             ? QStringLiteral("available")
                             : cap.whyNot.toHtmlEscaped());
    }
    detail_->setText(html);
}

QString PresetManagerDialog::selectedIdForTest() const {
    const int row = presetList_->currentRow();
    if (row < 0 || row >= m_presets.size())
        return {};
    return m_presets.at(row).id;
}

bool PresetManagerDialog::importPresetForTest(const QString& path,
                                              bool confirmReplace) {
    QString err;
    if (m_store.importFrom(path, confirmReplace, &err)) {
        refresh();
        return true;
    }
    m_lastError = err;
    return false;
}

bool PresetManagerDialog::exportPresetForTest(const QString& id,
                                              const QString& targetPath,
                                              bool confirmOverwrite) {
    QString err;
    if (m_store.exportTo(id, targetPath, confirmOverwrite, &err))
        return true;
    m_lastError = err;
    return false;
}

bool PresetManagerDialog::duplicatePresetForTest(const QString& id) {
    QString err;
    BatchPreset p;
    if (!m_store.get(id, &p, &err)) {
        m_lastError = err;
        return false;
    }
    p.id.clear();                       // save assigns a fresh de-conflicted slug
    p.name = p.name + QStringLiteral(" (copy)");
    p.created = QDateTime();
    p.modified = QDateTime();
    if (!m_store.save(&p, &err)) {
        m_lastError = err;
        return false;
    }
    refresh();
    return true;
}

bool PresetManagerDialog::renamePresetForTest(const QString& id,
                                              const QString& newName) {
    QString err;
    if (!m_store.rename(id, newName, &err)) {
        m_lastError = err;
        return false;
    }
    refresh();
    return true;
}

bool PresetManagerDialog::deletePresetForTest(const QString& id) {
    QString err;
    if (!m_store.remove(id, &err)) {
        m_lastError = err;
        return false;
    }
    refresh();
    return true;
}

PresetEditorDialog* PresetManagerDialog::openEditorForTest(const QString& id) {
    QString err;
    BatchPreset p;
    if (!m_store.get(id, &p, &err))
        return nullptr;
    auto* dlg = new PresetEditorDialog(capabilities_, this);
    dlg->loadPreset(p);
    return dlg;
}

bool PresetManagerDialog::applyEditedPresetForTest(const BatchPreset& preset) {
    BatchPreset out = preset;
    QString err;
    if (!m_store.save(&out, &err)) {
        m_lastError = err;
        return false;
    }
    refresh();
    return true;
}

} // namespace gp
