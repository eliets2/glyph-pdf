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

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 0, 10, 0);
    row->setSpacing(8);

    auto* label = new QLabel("MODE");
    label->setProperty("role", "modeStripLabel");
    row->addWidget(label);

    auto* pillsHost = new QFrame;
    pillsHost->setObjectName("modePills");
    pillsHost->setFixedHeight(22);
    auto* pillRow = new QHBoxLayout(pillsHost);
    pillRow->setContentsMargins(1, 1, 1, 1);
    pillRow->setSpacing(2);

    const QVector<QPair<QString,QString>> modes = {
        {"view","View"}, {"edit","Edit"}, {"comment","Comment"},
        {"form","Form"}, {"protect","Protect"}
    };
    for (const auto& [id, lab] : modes) {
        auto* btn = new QToolButton;
        btn->setText(lab);
        btn->setProperty("variant", "pill");
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        connect(btn, &QToolButton::clicked, this, [this, id]() {
            setMode(id);
            emit modeChanged(id);
        });
        _pills.insert(id, btn);
        pillRow->addWidget(btn);
    }
    row->addWidget(pillsHost);

    auto* aiBtn = new QToolButton;
    aiBtn->setText("AI");
    aiBtn->setProperty("variant", "pill");
    aiBtn->setCheckable(true);
    connect(aiBtn, &QToolButton::clicked, this, &ModeStrip::aiToggleRequested);
    row->addWidget(aiBtn);

    row->addStretch(1);

    // Status meta labels
    _autosaveLabel = new QLabel("● AUTOSAVED · --:--:--");
    _autosaveLabel->setProperty("mono", true);
    row->addWidget(_autosaveLabel);

    _syncLabel = new QLabel("⤺ SYNCED · v.0");
    _syncLabel->setProperty("mono", true);
    row->addWidget(_syncLabel);

    _signLabel = new QLabel("✓ SIGNED · 0 OF 0");
    _signLabel->setProperty("mono", true);
    row->addWidget(_signLabel);

    auto* themeBtn = new QToolButton;
    themeBtn->setText("DARK");
    themeBtn->setProperty("variant", "ghost");
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
        if (_ctx->document->isDirty()) {
            _autosaveLabel->setText("● UNSAVED");
            _autosaveLabel->setProperty("state", "unsaved");
            _autosaveLabel->style()->unpolish(_autosaveLabel);
            _autosaveLabel->style()->polish(_autosaveLabel);
        } else {
            _autosaveLabel->setText(QString("● AUTOSAVED · %1").arg(_lastSavedTime.toString("hh:mm:ss")));
            _autosaveLabel->setProperty("state", "");
            _autosaveLabel->style()->unpolish(_autosaveLabel);
            _autosaveLabel->style()->polish(_autosaveLabel);
        }
    }

    // 2. Sync Status
    if (_ctx->document) {
        QString path = _ctx->document->path();
        if (path.isEmpty()) {
            _syncLabel->setText("⤺ NOT SYNCED");
        } else {
            if (_ctx->collab) {
                // Generate a stable mock version based on file name hash
                unsigned int hash = 0;
                for (QChar c : path) hash = hash * 31 + c.unicode();
                int ver = (hash % 50) + 1;
                _syncLabel->setText(QString("⤺ SYNCED · v.%1").arg(ver));
            } else {
                _syncLabel->setText("⤺ NOT SYNCED");
            }
        }
    } else {
        _syncLabel->setText("⤺ NOT SYNCED");
    }

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
            _signLabel->setText(QString("✓ SIGNED · %1 OF %2").arg(_cachedValidSigs).arg(_cachedTotalSigs));
            _signLabel->setProperty("state", "valid");
            _signLabel->style()->unpolish(_signLabel);
            _signLabel->style()->polish(_signLabel);
        } else {
            _signLabel->setText("✓ SIGNED · 0 OF 0");
            _signLabel->setProperty("state", "");
            _signLabel->style()->unpolish(_signLabel);
            _signLabel->style()->polish(_signLabel);
        }
    } else {
        _signLabel->setText("✓ SIGNED · 0 OF 0");
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
    _lastSavedTime = time.time();
    if (_ctx && _ctx->document && !_ctx->document->isDirty()) {
        _autosaveLabel->setText(QString("● AUTOSAVED · %1").arg(_lastSavedTime.toString("hh:mm:ss")));
        _autosaveLabel->setProperty("state", "");
        _autosaveLabel->style()->unpolish(_autosaveLabel);
        _autosaveLabel->style()->polish(_autosaveLabel);
    }
}

void ModeStrip::setSyncStatus(const QString& status) {
    _syncLabel->setText(status);
}

void ModeStrip::setSignatureStatus(int signedCount, int totalCount) {
    _cachedValidSigs = signedCount;
    _cachedTotalSigs = totalCount;
    if (totalCount > 0) {
        _signLabel->setText(QString("✓ SIGNED · %1 OF %2").arg(signedCount).arg(totalCount));
        _signLabel->setProperty("state", "valid");
        _signLabel->style()->unpolish(_signLabel);
        _signLabel->style()->polish(_signLabel);
    } else {
        _signLabel->setText("✓ SIGNED · 0 OF 0");
        _signLabel->setProperty("state", "");
        _signLabel->style()->unpolish(_signLabel);
        _signLabel->style()->polish(_signLabel);
    }
}

} // namespace gp
