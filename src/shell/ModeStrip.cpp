#include "ModeStrip.h"
#include "util/GpTheme.h"
#include "util/Icons.h"
#include "core/AppContext.h"
#include "core/interfaces/ISignatureManager.h"
#include "engines/DocumentSession.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>
#include <QTimer>

namespace gp {

ModeStrip::ModeStrip(QWidget* parent) : QFrame(parent) {
    setObjectName("modeStrip");
    setFixedHeight(Theme::ModeStripH);
    setAccessibleName(tr("Mode strip"));
    setAccessibleDescription(tr("Switch between View, Edit, Comment, Form, and Protect modes"));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 0, 10, 0);
    row->setSpacing(8);

    auto* label = new QLabel(tr("MODE"));
    label->setProperty("role", "modeStripLabel");
    row->addWidget(label);

    auto* pillsHost = new QFrame;
    pillsHost->setObjectName("modePills");
    pillsHost->setFixedHeight(22);
    auto* pillRow = new QHBoxLayout(pillsHost);
    pillRow->setContentsMargins(1, 1, 1, 1);
    pillRow->setSpacing(2);

    const QVector<QPair<QString,QString>> modes = {
        {"view",    tr("View")},
        {"edit",    tr("Edit")},
        {"comment", tr("Comment")},
        {"form",    tr("Form")},
        {"protect", tr("Protect")}
    };
    for (const auto& [id, lab] : modes) {
        auto* btn = new QToolButton;
        btn->setText(lab);
        btn->setProperty("variant", "pill");
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setFocusPolicy(Qt::TabFocus);
        btn->setAccessibleName(tr("%1 mode").arg(lab));
        connect(btn, &QToolButton::clicked, this, [this, id]() {
            setMode(id);
            emit modeChanged(id);
        });
        _pills.insert(id, btn);
        pillRow->addWidget(btn);
    }
    row->addWidget(pillsHost);

    auto* aiBtn = new QToolButton;
    aiBtn->setText(tr("AI"));
    aiBtn->setProperty("variant", "pill");
    aiBtn->setCheckable(true);
    aiBtn->setAccessibleName(tr("Toggle AI assistant panel"));
    connect(aiBtn, &QToolButton::clicked, this, &ModeStrip::aiToggleRequested);
    row->addWidget(aiBtn);

    row->addStretch(1);

    _autosaveLabel = new QLabel(tr("● AUTOSAVED · --:--:--"));
    _autosaveLabel->setProperty("mono", true);
    _autosaveLabel->setAccessibleName(tr("Autosave status"));
    row->addWidget(_autosaveLabel);

    _syncLabel = new QLabel(tr("⤺ SYNCED · v.0"));
    _syncLabel->setProperty("mono", true);
    _syncLabel->setAccessibleName(tr("Sync status"));
    row->addWidget(_syncLabel);

    _signLabel = new QLabel(tr("✓ SIGNED · 0 OF 0"));
    _signLabel->setProperty("mono", true);
    _signLabel->setAccessibleName(tr("Digital signature status"));
    row->addWidget(_signLabel);

    auto* themeBtn = new QToolButton;
    themeBtn->setText(tr("DARK"));
    themeBtn->setProperty("variant", "ghost");
    themeBtn->setAccessibleName(tr("Toggle color theme"));
    connect(themeBtn, &QToolButton::clicked, this, &ModeStrip::themeToggleRequested);
    row->addWidget(themeBtn);

    setMode("comment");
}

void ModeStrip::init(const AppContext* ctx) {
    _ctx = ctx;
    _lastSavedTime = QTime::currentTime();

    if (_ctx && _ctx->document) {
        connect(_ctx->document.get(), &DocumentSession::dirtyChanged, this, [this](bool dirty) {
            if (!dirty) {
                _lastSavedTime = QTime::currentTime();
            }
            updateLabels();
        });
        connect(_ctx->document.get(), &DocumentSession::lastAutosaveChanged, this, [this](const QDateTime &time) {
            if (time.isValid()) {
                _lastSavedTime = time.time();
            }
            updateLabels();
        });
    }

    // Periodically update the labels (e.g. every 2 seconds) to keep time and signatures updated
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &ModeStrip::updateLabels);
    timer->start(2000);

    updateLabels();
}

void ModeStrip::updateLabels() {
    if (!_ctx) return;

    // 1. Autosave Status
    if (_ctx->document) {
        bool dirty = _ctx->document->isDirty();
        QDateTime lastAutosave = _ctx->document->lastAutosave();

        if (dirty) {
            if (lastAutosave.isValid()) {
                _autosaveLabel->setText(tr("● AUTOSAVED · %1").arg(lastAutosave.time().toString("hh:mm:ss")));
                _autosaveLabel->setProperty("state", "autosaved");
            } else {
                _autosaveLabel->setText(tr("● UNSAVED"));
                _autosaveLabel->setProperty("state", "unsaved");
            }
        } else {
            QString saveTimeStr = _lastSavedTime.isValid() ? _lastSavedTime.toString("hh:mm:ss") : QTime::currentTime().toString("hh:mm:ss");
            _autosaveLabel->setText(tr("● SAVED · %1").arg(saveTimeStr));
            _autosaveLabel->setProperty("state", "saved");
        }
        _autosaveLabel->style()->unpolish(_autosaveLabel);
        _autosaveLabel->style()->polish(_autosaveLabel);
    }

    // 2. Sync Status
    _syncLabel->setText(tr("⤺ LOCAL ONLY"));

    // 3. Signatures Status
    if (_ctx->document && _ctx->signing) {
        QString path = _ctx->document->path();
        if (!path.isEmpty() && path != _lastPath) {
            _lastPath = path;
            auto sigs = _ctx->signing->validateSignatures(path);
            _cachedTotalSigs = sigs.size();
            _cachedValidSigs = 0;
            for (const auto& s : sigs) {
                if (s.isValid) _cachedValidSigs++;
            }
        }
        
        if (_cachedTotalSigs > 0) {
            _signLabel->setText(tr("✓ SIGNED · %1 OF %2").arg(_cachedValidSigs).arg(_cachedTotalSigs));
            _signLabel->setProperty("state", "valid");
            _signLabel->style()->unpolish(_signLabel);
            _signLabel->style()->polish(_signLabel);
        } else {
            _signLabel->setText(tr("✓ SIGNED · 0 OF 0"));
            _signLabel->setProperty("state", "");
            _signLabel->style()->unpolish(_signLabel);
            _signLabel->style()->polish(_signLabel);
        }
    } else {
        _signLabel->setText(tr("✓ SIGNED · 0 OF 0"));
        _signLabel->setProperty("state", "");
        _signLabel->style()->unpolish(_signLabel);
        _signLabel->style()->polish(_signLabel);
    }
}

void ModeStrip::setMode(const QString& id) {
    if (_active == id) return;
    if (auto* p = _pills.value(_active)) p->setChecked(false);
    _active = id;
    if (auto* p = _pills.value(id))      p->setChecked(true);
}

void ModeStrip::setTheme(int) { /* style is updated via QSS swap at app level */ }

void ModeStrip::setAutosaveTime(const QDateTime& time) {
    if (time.isValid()) {
        _lastSavedTime = time.time();
    }
    updateLabels();
}

void ModeStrip::setSyncStatus(const QString& status) {
    _syncLabel->setText(status);
}

void ModeStrip::setSignatureStatus(int signedCount, int totalCount) {
    _cachedValidSigs = signedCount;
    _cachedTotalSigs = totalCount;
    if (totalCount > 0) {
        _signLabel->setText(tr("✓ SIGNED · %1 OF %2").arg(signedCount).arg(totalCount));
        _signLabel->setProperty("state", "valid");
        _signLabel->style()->unpolish(_signLabel);
        _signLabel->style()->polish(_signLabel);
    } else {
        _signLabel->setText(tr("✓ SIGNED · 0 OF 0"));
        _signLabel->setProperty("state", "");
        _signLabel->style()->unpolish(_signLabel);
        _signLabel->style()->polish(_signLabel);
    }
}

} // namespace gp
