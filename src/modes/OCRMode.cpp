// SPDX-License-Identifier: Apache-2.0
#include "OCRMode.h"
#include "engines/ocr/RapidOcrEngine.h"
#include "util/GpTheme.h"
#include "util/Badge.h"
#include "docmodel/Block.h"
#include "docmodel/Inline.h"
#include "pdfws_djot/LuaDjotCodec.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextCharFormat>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <QStandardItemModel>
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace gp {

// Shared QSettings key for the user's selected OCR language code (e.g. "EN").
// StatusBar reads the same key to display the real selected language.
static const char* kOcrLanguageKey = "ocr/language";

// Empty-state shown in the scan/confidence pane before any OCR has run.
static const char* kOcrEmptyStateHtml =
    "<span style='color:#8a8a8a;font-size:13px;'>No OCR results yet.<br><br>"
    "Open a scanned PDF and run OCR to review the recognized text and "
    "per-word confidence here.</span>";

// ── B10: per-language user dictionary ────────────────────────────────────────
// One word per line under <AppDataLocation>/ocr-dict/<lang>.txt; consulted
// before flagging so accepted words stop being flagged across sessions.

QString OCRMode::userDictionaryPath(const QString &langCode)
{
    const QString base = QStandardPaths::writableLocation(
                             QStandardPaths::AppDataLocation);
    return QStringLiteral("%1/ocr-dict/%2.txt").arg(base, langCode);
}

QStringList OCRMode::loadUserDictionary(const QString &langCode)
{
    QFile f(userDictionaryPath(langCode));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QStringList words;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (!line.isEmpty()) words.append(line);
    }
    return words;
}

bool OCRMode::addUserDictionaryWord(const QString &langCode, const QString &word)
{
    if (word.trimmed().isEmpty()) return false;
    const QString path = userDictionaryPath(langCode);
    QDir().mkpath(QFileInfo(path).absolutePath());

    // Keep the list duplicate-free.
    QStringList existing = loadUserDictionary(langCode);
    if (existing.contains(word, Qt::CaseInsensitive)) return true;
    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text)) return false;
    f.write((word + QLatin1Char('\n')).toUtf8());
    return true;
}

// B9: bounded Damerau-Levenshtein distance (optimal string alignment).
static int editDistance(const QString &a, const QString &b)
{
    const int n = a.size(), m = b.size();
    QVector<QVector<int>> d(n + 1, QVector<int>(m + 1, 0));
    for (int i = 0; i <= n; ++i) d[i][0] = i;
    for (int j = 0; j <= m; ++j) d[0][j] = j;
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            const int cost = (a.at(i - 1) == b.at(j - 1)) ? 0 : 1;
            int best = std::min({ d[i-1][j] + 1, d[i][j-1] + 1, d[i-1][j-1] + cost });
            if (i > 1 && j > 1 &&
                a.at(i - 1) == b.at(j - 2) && a.at(i - 2) == b.at(j - 1))
                best = std::min(best, d[i-2][j-2] + 1); // transposition
            d[i][j] = best;
        }
    }
    return d[n][m];
}

QStringList OCRMode::suggestCorrections(const QString &word,
                                        const QStringList &vocabulary)
{
    // upgrade path: replace with Hunspell suggest() behind this same seam.
    struct Cand { QString text; int dist; };
    QList<Cand> cands;
    for (const QString &v : vocabulary) {
        if (v.isEmpty() || v.compare(word, Qt::CaseInsensitive) == 0) continue;
        const int d = editDistance(word.toLower(), v.toLower());
        if (d <= 2) cands.append({v, d});
    }
    std::stable_sort(cands.begin(), cands.end(),
                     [](const Cand &a, const Cand &b) { return a.dist < b.dist; });
    QStringList out;
    for (const auto &c : cands) {
        if (!out.contains(c.text, Qt::CaseInsensitive)) out.append(c.text);
        if (out.size() >= 5) break;   // FineReader shows a short ranked list
    }
    return out;
}

// ── helpers ─────────────────────────────────────────────────────────────────

static QFrame* makeStrip(const char* role, int h) {
    auto* f = new QFrame;
    f->setProperty("role", role);
    f->setFixedHeight(h);
    return f;
}

static QLabel* monoLab(const QString& s) {
    auto* l = new QLabel(s);
    l->setProperty("mono", true);
    return l;
}

static QLabel* infoLab(const QString& s) {
    auto* l = new QLabel(s);
    l->setProperty("role", "infoStrip");
    return l;
}

// ── construction ────────────────────────────────────────────────────────────

OCRMode::OCRMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    buildToolbar(col);
    buildInfoStrip(col);
    buildPanes(col);
}

// ── toolbar ─────────────────────────────────────────────────────────────────

void OCRMode::buildToolbar(QVBoxLayout* col)
{
    auto* tb = makeStrip("modeToolbar", Theme::ToolbarH);
    auto* row = new QHBoxLayout(tb);
    row->setContentsMargins(10, 0, 10, 0);
    row->setSpacing(6);

    row->addWidget(monoLab(tr("OCR")));

    // ── Language selector ───────────────────────────────────────────────
    m_langCombo = new QComboBox;
    m_langCombo->setObjectName("ocrLangCombo");
    m_langCombo->addItems({"EN · English", "DE · Deutsch", "FR · Français",
                           "ES · Español", "IT · Italiano", "PT · Português",
                           "RU · Русский", "ZH · 中文 (简)", "JA · 日本語",
                           "KO · 한국어", "AR · العربية", "NL · Nederlands"});
    m_langCombo->setProperty("variant", "ghost");

    // Restore the previously-selected OCR language (code before " · ") and
    // persist any change, so the StatusBar OCR cell reflects a real choice.
    {
        QSettings settings;
        const QString savedCode = settings.value(kOcrLanguageKey, "EN").toString();
        m_dictLang = savedCode;   // B10: dictionary language follows the OCR language
        for (int i = 0; i < m_langCombo->count(); ++i) {
            if (m_langCombo->itemText(i).section(QStringLiteral(" · "), 0, 0) == savedCode) {
                m_langCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    connect(m_langCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        const QString code = text.section(QStringLiteral(" · "), 0, 0);
        QSettings settings;
        settings.setValue(kOcrLanguageKey, code);
        // B10: the user dictionary is per-language — re-flag on change.
        if (m_dictLang != code) {
            m_dictLang = code;
            rebuildTextWordIndex();
            applyUncertainHighlights();
        }
    });

    row->addWidget(m_langCombo);

    // ── Engine selector ─────────────────────────────────────────────────
    m_engineCombo = new QComboBox;
    m_engineCombo->setObjectName("ocrEngineCombo");
    m_engineCombo->addItem("Tesseract 5");
#ifdef HAS_RAPIDOCR
    m_engineCombo->addItem("RapidOCR (PP-OCRv5)");
    // Runtime gate: disable the selector only while the engine is still a Mock.
    // RapidOcrEngine now runs the real PP-OCRv5 pipeline, so it stays enabled.
    if (RapidOcrEngine().isMockImplementation()) {
        auto* model = qobject_cast<QStandardItemModel*>(m_engineCombo->model());
        if (model) {
            auto* item = model->item(1);
            if (item) {
                item->setEnabled(false);
                item->setToolTip(tr("Available in a future release"));
            }
        }
    }
#endif
    m_engineCombo->setCurrentIndex(0); // Default to first non-mock engine (Tesseract)
    m_engineCombo->setProperty("variant", "ghost");
    row->addWidget(m_engineCombo);

    // ── Strategy selector ───────────────────────────────────────────────
    m_strategyCombo = new QComboBox;
    m_strategyCombo->setObjectName("ocrStrategyCombo");
    m_strategyCombo->addItem(tr("Primary Only"));
    m_strategyCombo->addItem(tr("Confidence Weighted"));
    m_strategyCombo->addItem(tr("ROVER Vote"));
    m_strategyCombo->setProperty("variant", "ghost");
    row->addWidget(m_strategyCombo);

    // ── Preprocessing toggles ───────────────────────────────────────────
    auto* sep1 = new QFrame; sep1->setFrameShape(QFrame::VLine);
    sep1->setFixedWidth(1); sep1->setStyleSheet("color:#ffffff20;");
    row->addWidget(sep1);

    m_chkDeskew = new QCheckBox(tr("Deskew"));
    m_chkDeskew->setObjectName("ocrChkDeskew");
    m_chkDeskew->setChecked(true);
    m_chkDeskew->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkDeskew);

    m_chkBinarize = new QCheckBox(tr("Binarize"));
    m_chkBinarize->setObjectName("ocrChkBinarize");
    m_chkBinarize->setChecked(true);
    m_chkBinarize->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkBinarize);

    m_chkDenoise = new QCheckBox(tr("Denoise"));
    m_chkDenoise->setObjectName("ocrChkDenoise");
    m_chkDenoise->setChecked(false);
    m_chkDenoise->setStyleSheet("color:#c0c0c0; spacing:4px;");
    row->addWidget(m_chkDenoise);

    // ── B15: verification thresholds (persisted; drive flagging + colors) ──
    {
        QSettings settings;
        m_lowThreshold  = settings.value("ocr/lowThreshold", 70).toInt();
        m_highThreshold = settings.value("ocr/highThreshold", 90).toInt();
        // Sanity: keep low < high within [0,100].
        m_lowThreshold  = qBound(0, m_lowThreshold, 99);
        m_highThreshold = qBound(m_lowThreshold + 1, m_highThreshold, 100);
    }

    auto* lblLow = monoLab(tr("LOW <"));
    row->addWidget(lblLow);
    m_spinLowThresh = new QSpinBox;
    m_spinLowThresh->setObjectName("ocrSpinLowThresh");
    m_spinLowThresh->setRange(0, 99);
    m_spinLowThresh->setValue(m_lowThreshold);
    m_spinLowThresh->setToolTip(tr("Words below this confidence are flagged uncertain"));
    row->addWidget(m_spinLowThresh);

    auto* lblHigh = monoLab(tr("HIGH ≥"));
    row->addWidget(lblHigh);
    m_spinHighThresh = new QSpinBox;
    m_spinHighThresh->setObjectName("ocrSpinHighThresh");
    m_spinHighThresh->setRange(1, 100);
    m_spinHighThresh->setValue(m_highThreshold);
    m_spinHighThresh->setToolTip(tr("Words at or above this confidence are high-confidence"));
    row->addWidget(m_spinHighThresh);

    auto onThresholdChanged = [this]() {
        int low  = m_spinLowThresh->value();
        int high = m_spinHighThresh->value();
        // Keep the invariant low < high by adjusting the other spinbox.
        if (low >= high) {
            if (sender() == m_spinLowThresh) {
                high = qMin(100, low + 1);
                m_spinHighThresh->blockSignals(true);
                m_spinHighThresh->setValue(high);
                m_spinHighThresh->blockSignals(false);
            } else {
                low = qMax(0, high - 1);
                m_spinLowThresh->blockSignals(true);
                m_spinLowThresh->setValue(low);
                m_spinLowThresh->blockSignals(false);
            }
        }
        m_lowThreshold  = low;
        m_highThreshold = high;
        QSettings settings;
        settings.setValue("ocr/lowThreshold", m_lowThreshold);
        settings.setValue("ocr/highThreshold", m_highThreshold);
        // Re-derive all threshold-driven UI from the current results.
        updateConfidenceOverlay();
        rebuildTextWordIndex();
        applyUncertainHighlights();
        updateInfoStrip();
    };
    connect(m_spinLowThresh, &QSpinBox::valueChanged, this, onThresholdChanged);
    connect(m_spinHighThresh, &QSpinBox::valueChanged, this, onThresholdChanged);

    // ── B15: verification-option toggles (persisted) ────────────────────
    {
        QSettings settings;
        m_spellCheckEnabled    = settings.value("ocr/spellCheck", false).toBool();
        m_lowConfVerifyEnabled = settings.value("ocr/lowConfVerify", true).toBool();
    }
    m_chkSpell = new QCheckBox(tr("Spell-check"));
    m_chkSpell->setObjectName("ocrChkSpell");
    m_chkSpell->setChecked(m_spellCheckEnabled);
    m_chkSpell->setStyleSheet("color:#c0c0c0; spacing:4px;");
    m_chkSpell->setToolTip(tr("Also flag words that are not in the user dictionary"));
    row->addWidget(m_chkSpell);

    m_chkLowConf = new QCheckBox(tr("Verify low-conf"));
    m_chkLowConf->setObjectName("ocrChkLowConf");
    m_chkLowConf->setChecked(m_lowConfVerifyEnabled);
    m_chkLowConf->setStyleSheet("color:#c0c0c0; spacing:4px;");
    m_chkLowConf->setToolTip(
        tr("When off, Verify Text skips low-confidence words (they stay highlighted)"));
    row->addWidget(m_chkLowConf);

    connect(m_chkSpell, &QCheckBox::toggled, this, [this](bool on) {
        m_spellCheckEnabled = on;
        QSettings settings;
        settings.setValue("ocr/spellCheck", on);
        rebuildTextWordIndex();
        applyUncertainHighlights();
        updateInfoStrip();
    });
    connect(m_chkLowConf, &QCheckBox::toggled, this, [this](bool on) {
        m_lowConfVerifyEnabled = on;
        QSettings settings;
        settings.setValue("ocr/lowConfVerify", on);
    });

    row->addStretch(1);

    // ── Run / Review actions ────────────────────────────────────────────
    m_btnRun = new QToolButton;
    m_btnRun->setObjectName("ocrBtnRun");
    m_btnRun->setText(tr("Run OCR"));
    m_btnRun->setProperty("variant", "accent");
    m_btnRun->setAccessibleName(tr("Run OCR"));
    m_btnRun->setAccessibleDescription(tr("Run optical character recognition on the current page"));
    connect(m_btnRun, &QToolButton::clicked, this, &OCRMode::onRunOcr);
    row->addWidget(m_btnRun);

    // B3: FineReader's "Verify Text" entry point (Ctrl+F7).
    auto* btnVerify = new QToolButton;
    btnVerify->setObjectName("ocrBtnVerify");
    btnVerify->setText(tr("Verify Text"));
    btnVerify->setProperty("variant", "ghost");
    btnVerify->setToolTip(tr("Step through low-confidence words (Ctrl+F7)"));
    connect(btnVerify, &QToolButton::clicked, this, &OCRMode::openVerifyDialog);
    row->addWidget(btnVerify);

    auto* verifyShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F7), this);
    verifyShortcut->setObjectName("ocrScVerifyDialog");
    connect(verifyShortcut, &QShortcut::activated,
            this, &OCRMode::openVerifyDialog);

    // B12: FineReader's Ctrl+T "Mark Text as Verified".
    m_btnPageVerified = new QToolButton;
    m_btnPageVerified->setObjectName("ocrBtnPageVerified");
    m_btnPageVerified->setText(tr("Page Verified"));
    m_btnPageVerified->setCheckable(true);
    m_btnPageVerified->setProperty("variant", "ghost");
    m_btnPageVerified->setToolTip(tr("Mark this page as verified (Ctrl+T)"));
    connect(m_btnPageVerified, &QToolButton::toggled,
            this, [this](bool on) { setPageVerified(on); });
    row->addWidget(m_btnPageVerified);

    auto* pageVerifiedShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), this);
    pageVerifiedShortcut->setObjectName("ocrScPageVerified");
    connect(pageVerifiedShortcut, &QShortcut::activated, this, [this]() {
        m_btnPageVerified->toggle();
    });

    m_btnAccept = new QToolButton;
    m_btnAccept->setObjectName("ocrBtnAccept");
    m_btnAccept->setText(tr("✓ Accept"));
    m_btnAccept->setProperty("variant", "ghost");
    m_btnAccept->setEnabled(false);
    m_btnAccept->setAccessibleName(tr("Accept OCR results"));
    m_btnAccept->setAccessibleDescription(tr("Keep the recognised text and confidence overlay for this page"));
    connect(m_btnAccept, &QToolButton::clicked, this, &OCRMode::onAcceptResults);
    row->addWidget(m_btnAccept);

    m_btnReject = new QToolButton;
    m_btnReject->setObjectName("ocrBtnReject");
    m_btnReject->setText(tr("✗ Reject"));
    m_btnReject->setProperty("variant", "ghost");
    m_btnReject->setEnabled(false);
    m_btnReject->setAccessibleName(tr("Reject OCR results"));
    m_btnReject->setAccessibleDescription(tr("Discard the recognised text and clear the OCR overlay"));
    connect(m_btnReject, &QToolButton::clicked, this, &OCRMode::onRejectResults);
    row->addWidget(m_btnReject);

    auto* sep2 = new QFrame; sep2->setFrameShape(QFrame::VLine);
    sep2->setFixedWidth(1); sep2->setStyleSheet("color:#ffffff20;");
    row->addWidget(sep2);

    auto* exit = new QToolButton;
    exit->setText(tr("Exit OCR"));
    exit->setProperty("variant", "ghost");
    connect(exit, &QToolButton::clicked, this, [this]() {
        // Walk up to the top-level window and request close. The host shell
        // intercepts this and returns to the previous mode page in v1.0.0.
        QWidget* w = this;
        while (w && w->parentWidget()) w = w->parentWidget();
        if (w) w->close();
    });
    row->addWidget(exit);

    col->addWidget(tb);
}

// ── info strip ──────────────────────────────────────────────────────────────

void OCRMode::buildInfoStrip(QVBoxLayout* col)
{
    auto* info = makeStrip("infoStrip", Theme::InfoStripH);
    auto* row = new QHBoxLayout(info);
    row->setContentsMargins(12, 0, 12, 0);
    row->setSpacing(14);

    m_lblPage    = infoLab(tr("PAGE — OF —"));
    m_lblLanguage= infoLab(tr("LANGUAGE —"));
    m_lblAvgConf = infoLab(tr("AVG CONFIDENCE —"));
    m_lblLowWords= infoLab(tr("UNCERTAIN —"));
    m_lblVerified= infoLab(tr("VERIFIED —"));
    m_lblZoom    = infoLab(tr("ZOOM 400%"));
    m_lblEngine  = infoLab(tr("ENGINE: Tesseract 5"));

    row->addWidget(m_lblPage);
    row->addWidget(m_lblLanguage);
    row->addWidget(m_lblAvgConf);
    row->addWidget(m_lblLowWords);
    row->addWidget(m_lblVerified);
    row->addWidget(m_lblZoom);
    row->addWidget(m_lblEngine);
    row->addStretch(1);

    col->addWidget(info);
}

// ── 4-pane splitter ─────────────────────────────────────────────────────────

void OCRMode::buildPanes(QVBoxLayout* col)
{
    auto* split = new QSplitter(Qt::Horizontal);
    split->setHandleWidth(1);
    m_splitter = split;   // B14: layout presets manipulate this splitter

    // ── Page list ───────────────────────────────────────────────────────
    // Populated by setOcrResults() from real document pages; starts empty.
    m_pageList = new QListWidget;
    m_pageList->setObjectName("ocrPageList");
    m_pageList->setFixedWidth(180);
    split->addWidget(m_pageList);

    // ── Image / scan pane ───────────────────────────────────────────────
    m_imagePane = new QFrame;
    auto* impLay = new QVBoxLayout(m_imagePane);
    impLay->setContentsMargins(0,0,0,0); impLay->setSpacing(0);

    auto* impHead = makeStrip("modeToolbar", 24);
    auto* impHeadRow = new QHBoxLayout(impHead);
    impHeadRow->setContentsMargins(12,0,12,0);
    impHeadRow->addWidget(monoLab(tr("IMAGE · SCAN")));
    impHeadRow->addStretch(1);
    impHeadRow->addWidget(monoLab(tr("4× PIXELS")));
    impLay->addWidget(impHead);

    // ── Confidence overlay: scrollable paper with per-word colored spans ──────
    // m_scanContentLabel is updated by updateConfidenceOverlay() each time
    // OCR results arrive.  Right-click opens the per-region context menu.
    m_scanContentLabel = new QLabel;
    m_scanContentLabel->setObjectName("ocrScanContent");
    m_scanContentLabel->setTextFormat(Qt::RichText);
    m_scanContentLabel->setWordWrap(true);
    m_scanContentLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scanContentLabel->setStyleSheet(
        "background:#f4f1ea; color:#1a1a1a; padding:24px; "
        "border:1px solid #000; min-width:380px;");
    m_scanContentLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_scanContentLabel, &QLabel::customContextMenuRequested,
            this, &OCRMode::onImagePaneContextMenu);
    // B5: clicking a word in the scan pane selects it in the text pane.
    connect(m_scanContentLabel, &QLabel::linkActivated,
            this, &OCRMode::onScanWordLinkActivated);

    // Empty state until a document is OCR'd (replaced by updateConfidenceOverlay).
    m_scanContentLabel->setText(kOcrEmptyStateHtml);

    auto* scrollArea = new QScrollArea;
    scrollArea->setWidget(m_scanContentLabel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background:#2a2a2a;");

    impLay->addWidget(scrollArea, 1);
    split->addWidget(m_imagePane);

    // ── Text pane (editable) ────────────────────────────────────────────
    auto* textPane = new QFrame;
    auto* txtLay = new QVBoxLayout(textPane);
    txtLay->setContentsMargins(0,0,0,0); txtLay->setSpacing(0);

    auto* txtHead = makeStrip("modeToolbar", 24);
    auto* txtHeadRow = new QHBoxLayout(txtHead);
    txtHeadRow->setContentsMargins(12,0,12,0);
    txtHeadRow->addWidget(monoLab(tr("RECOGNIZED · EDITABLE")));
    txtHeadRow->addStretch(1);

    // B2: FineReader-style prev/next low-confidence navigation (Alt+Up/Down).
    auto makeNavButton = [this](const char* objName, const QString& text,
                                const QString& tip, const QKeySequence& key) {
        auto* b = new QToolButton;
        b->setObjectName(objName);
        b->setText(text);
        b->setToolTip(tip + QStringLiteral(" (%1)").arg(key.toString(QKeySequence::NativeText)));
        connect(b, &QToolButton::clicked, this, [this, objName]() {
            if (qstrcmp(objName, "ocrBtnNextUncertain") == 0)
                gotoNextUncertain();
            else
                gotoPrevUncertain();
        });
        return b;
    };
    m_btnPrevUncertain = makeNavButton("ocrBtnPrevUncertain", tr("◀ Prev"),
                                       tr("Previous low-confidence word"),
                                       QKeySequence(Qt::ALT | Qt::Key_Up));
    m_btnNextUncertain = makeNavButton("ocrBtnNextUncertain", tr("Next ▶"),
                                       tr("Next low-confidence word"),
                                       QKeySequence(Qt::ALT | Qt::Key_Down));
    txtHeadRow->addWidget(m_btnPrevUncertain);
    txtHeadRow->addWidget(m_btnNextUncertain);

    // Alt+Down / Alt+Up — FineReader's Next/Previous Error hotkeys.
    auto* nextShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Down), this);
    nextShortcut->setObjectName("ocrScNextUncertain");
    connect(nextShortcut, &QShortcut::activated, this, &OCRMode::gotoNextUncertain);
    auto* prevShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Up), this);
    prevShortcut->setObjectName("ocrScPrevUncertain");
    connect(prevShortcut, &QShortcut::activated, this, &OCRMode::gotoPrevUncertain);

    // B1: FineReader-style "Uncertain characters" show/hide toggle.
    m_btnUncertainToggle = new QToolButton;
    m_btnUncertainToggle->setObjectName("ocrBtnUncertainToggle");
    m_btnUncertainToggle->setText(tr("Uncertain"));
    m_btnUncertainToggle->setCheckable(true);
    m_btnUncertainToggle->setChecked(true);
    m_btnUncertainToggle->setToolTip(tr("Highlight low-confidence words in the text pane"));
    connect(m_btnUncertainToggle, &QToolButton::toggled, this, [this](bool on) {
        m_uncertainEnabled = on;
        applyUncertainHighlights();
    });
    txtHeadRow->addWidget(m_btnUncertainToggle);

    txtHeadRow->addWidget(monoLab(tr("UTF-8")));
    txtLay->addWidget(txtHead);

    m_textEdit = new QPlainTextEdit;
    m_textEdit->setObjectName("ocrTextEdit");
    m_textEdit->setPlaceholderText(
        tr("Recognized text appears here after you run OCR on a document."));
    // B5: caret moves drive image/zoom synchronization.
    connect(m_textEdit, &QPlainTextEdit::cursorPositionChanged,
            this, &OCRMode::onTextCursorMoved);
    txtLay->addWidget(m_textEdit, 1);
    split->addWidget(textPane);

    // ── Zoom / metadata pane ────────────────────────────────────────────
    m_zoomPane = new QFrame;
    m_zoomPane->setFixedWidth(200);
    auto* zLay = new QVBoxLayout(m_zoomPane);
    zLay->setContentsMargins(0,0,0,0); zLay->setSpacing(0);

    auto* zHead = makeStrip("modeToolbar", 24);
    auto* zHeadRow = new QHBoxLayout(zHead);
    zHeadRow->setContentsMargins(12,0,12,0);
    zHeadRow->addWidget(monoLab(tr("ZOOM · 4×")));
    zHeadRow->addStretch(1);
    zLay->addWidget(zHead);

    m_zoomBig = new QLabel(QStringLiteral("\xE2\x80\x94"));  // em dash — no selection
    m_zoomBig->setAlignment(Qt::AlignCenter);
    m_zoomBig->setStyleSheet(
        "background:#e8e6df; color:#1a1a1a; font-family:Manrope; "
        "font-size:42px; font-weight:600; padding:16px; margin:24px 16px; "
        "border:1px solid #000;");
    zLay->addWidget(m_zoomBig);

    m_zoomMeta = new QLabel(tr("No word selected"));
    m_zoomMeta->setProperty("mono", true);
    m_zoomMeta->setStyleSheet("padding:8px 12px;");
    m_zoomMeta->setAlignment(Qt::AlignLeft);
    zLay->addWidget(m_zoomMeta);

    // ── Confidence legend ───────────────────────────────────────────────
    auto* legend = new QFrame;
    auto* legendLay = new QVBoxLayout(legend);
    legendLay->setContentsMargins(12, 8, 12, 8);
    legendLay->setSpacing(4);

    legendLay->addWidget(monoLab(tr("CONFIDENCE")));

    auto makeLegendRow = [&](const QString &color, const QString &label) {
        auto* row = new QHBoxLayout;
        auto* swatch = new QFrame;
        swatch->setFixedSize(12, 12);
        swatch->setStyleSheet(QString("background:%1; border:1px solid %1; border-radius:2px;").arg(color));
        row->addWidget(swatch);
        row->addWidget(monoLab(label));
        row->addStretch(1);
        legendLay->addLayout(row);
    };

    makeLegendRow("#22c55e", tr("HIGH (≥ 80%)"));
    makeLegendRow("#eab308", tr("MEDIUM (50-79%)"));
    makeLegendRow("#ef4444", tr("LOW (< 50%)"));

    zLay->addWidget(legend);
    zLay->addStretch(1);
    split->addWidget(m_zoomPane);

    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 4);
    split->setStretchFactor(2, 3);
    split->setStretchFactor(3, 0);
    col->addWidget(split, 1);

    // ── B14: FineReader layout presets ──────────────────────────────────
    // F6 image only · F7 image+text · F8 text only · F5 pages pane ·
    // Ctrl+F5 zoom pane. Presets collapse the other panes to zero width.
    auto setPaneWidth = [this](int index) {
        QList<int> sizes = m_splitter->sizes();
        if (sizes.size() != 4) return;
        const int total = qMax(400, m_splitter->width());
        sizes[0] = 0;                    // hide pages
        sizes[3] = 0;                    // hide zoom
        sizes[index] = total;            // target takes everything
        const int other = (index == 1) ? 2 : 1;
        sizes[other] = 0;
        m_splitter->setSizes(sizes);
    };

    auto* f6 = new QShortcut(QKeySequence(Qt::Key_F6), this);
    connect(f6, &QShortcut::activated, this, [this, setPaneWidth]() {
        setPaneWidth(1);   // image only
    });
    auto* f7 = new QShortcut(QKeySequence(Qt::Key_F7), this);
    connect(f7, &QShortcut::activated, this, [this]() {
        QList<int> sizes = m_splitter->sizes();
        if (sizes.size() == 4) {
            const int total = qMax(400, m_splitter->width());
            sizes[0] = 0;                       // hide pages
            sizes[1] = total / 2;               // image
            sizes[2] = total - total / 2;       // text
            sizes[3] = 0;                       // zoom stays hidden
            m_splitter->setSizes(sizes);
        }
    });
    auto* f8 = new QShortcut(QKeySequence(Qt::Key_F8), this);
    connect(f8, &QShortcut::activated, this, [this, setPaneWidth]() {
        setPaneWidth(2);   // text only
    });

    auto* f5 = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(f5, &QShortcut::activated, this, [this]() {
        QList<int> sizes = m_splitter->sizes();
        if (sizes.size() != 4) return;
        if (sizes.at(0) > 0) {
            m_pagesWidth = sizes.at(0);         // remember for restore
            sizes[0] = 0;
        } else {
            sizes[0] = m_pagesWidth > 0 ? m_pagesWidth : 180;
        }
        m_splitter->setSizes(sizes);
    });

    auto* ctrlF5 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F5), this);
    connect(ctrlF5, &QShortcut::activated, this, [this]() {
        QList<int> sizes = m_splitter->sizes();
        if (sizes.size() != 4) return;
        if (sizes.at(3) > 0) {
            m_zoomWidth = sizes.at(3);
            sizes[3] = 0;
        } else {
            sizes[3] = m_zoomWidth > 0 ? m_zoomWidth : 200;
        }
        m_splitter->setSizes(sizes);
    });

    // Ctrl+Tab / Ctrl+Shift+Tab — cycle focus next/previous pane.
    const QList<QWidget*> focusOrder = { m_pageList, m_scanContentLabel,
                                         m_textEdit, m_zoomMeta };
    auto cycleFocus = [this, focusOrder](int dir) {
        QWidget* current = focusWidget();
        int idx = focusOrder.indexOf(current);
        if (idx < 0) idx = (dir > 0) ? -1 : 0;
        for (int step = 0; step < focusOrder.size(); ++step) {
            idx = (idx + dir + focusOrder.size()) % focusOrder.size();
            QWidget* w = focusOrder.at(idx);
            if (w && w->isVisible() && w->focusPolicy() != Qt::NoFocus) {
                w->setFocus(Qt::ShortcutFocusReason);
                return;
            }
        }
    };
    auto* ctrlTab = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), this);
    connect(ctrlTab, &QShortcut::activated, this, [cycleFocus]() { cycleFocus(+1); });
    auto* ctrlShiftTab = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), this);
    connect(ctrlShiftTab, &QShortcut::activated, this, [cycleFocus]() { cycleFocus(-1); });

    // B6: zoom-pane magnification hotkeys (FineReader's Ctrl++ / Ctrl+- / Ctrl+0).
    auto* zoomIn = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
    connect(zoomIn, &QShortcut::activated, this, [this]() {
        m_zoomFactor = qMin(6.0, m_zoomFactor + 1.0);
        if (m_selectedWord >= 0) renderZoomCrop(m_selectedWord);
        updateInfoStrip();
    });
    auto* zoomOut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
    connect(zoomOut, &QShortcut::activated, this, [this]() {
        m_zoomFactor = qMax(1.0, m_zoomFactor - 1.0);
        if (m_selectedWord >= 0) renderZoomCrop(m_selectedWord);
        updateInfoStrip();
    });
    auto* zoomReset = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);
    connect(zoomReset, &QShortcut::activated, this, [this]() {
        m_zoomFactor = 4.0;
        if (m_selectedWord >= 0) renderZoomCrop(m_selectedWord);
        updateInfoStrip();
    });
}

// ── slots ───────────────────────────────────────────────────────────────────

void OCRMode::onRunOcr()
{
    // R2: do NOT pre-enable Accept/Reject here. They must stay disabled until
    // setOcrResults() actually delivers recognised words — otherwise the user
    // can "accept" a result that does not exist yet. Show a processing state
    // instead and let setOcrResults() enable the review buttons on arrival.
    m_btnRun->setEnabled(false);
    m_btnRun->setText(tr("Running…"));
    m_btnAccept->setEnabled(false);
    m_btnReject->setEnabled(false);

    // Update engine label
    m_lblEngine->setText(tr("ENGINE: %1 · %2")
        .arg(m_engineCombo->currentText(),
             m_strategyCombo->currentText()));

    // Confidence stats will be updated by setOcrResults() when results arrive.
    // Clear the stats now to avoid showing stale values while OCR runs.
    m_lblAvgConf->setText(tr("AVG CONFIDENCE —"));
    m_lblLowWords->setText(tr("UNCERTAIN —"));
    m_lblVerified->setText(tr("VERIFIED —"));

    emit ocrRequested();
}

void OCRMode::onAcceptResults()
{
    m_btnAccept->setEnabled(false);
    m_btnReject->setEnabled(false);
    emit reviewAccepted();
}

void OCRMode::onRejectResults()
{
    // Reject clears the current OCR overlay/results so the page returns to its
    // pre-OCR state; the host is notified to drop any pending applied text.
    m_currentWords.clear();
    m_wordRanges.clear();
    m_lowConfWords.clear();
    m_flagReasons.clear();
    m_verifiedWords.clear();
    m_skipAllTokens.clear();
    m_selectedWord = -1;
    m_uncertainCursor = -1;
    setPageVerified(false);
    updateConfidenceOverlay();
    updateInfoStrip();
    if (m_textEdit) {
        m_textEdit->clear();
        m_textEdit->setExtraSelections({});
    }
    m_btnAccept->setEnabled(false);
    m_btnReject->setEnabled(false);
    emit reviewRejected();
}

// ── Context menu (right-click on scan pane) ──────────────────────────────────

void OCRMode::onImagePaneContextMenu(const QPoint &pos)
{
    // For the rich-text label, we use a fixed "current page" region
    // as the re-OCR target.  Future work: map pos to individual LayoutRegion bboxes.
    m_contextRegionBbox = QRectF();  // empty = whole current page

    QMenu menu(this);

    QAction *reOcrAction = menu.addAction(tr("Re-OCR this region"));
    connect(reOcrAction, &QAction::triggered, this, &OCRMode::onReOcrRegion);

    // B11: reading-order editing for the currently selected word.
    if (m_selectedWord >= 0 && m_selectedWord < m_currentWords.size()) {
        menu.addSeparator();
        QAction *earlier = menu.addAction(
            tr("Move word %1 earlier").arg(m_selectedWord + 1));
        connect(earlier, &QAction::triggered, this, [this]() {
            moveWord(m_selectedWord, -1);
        });
        QAction *later = menu.addAction(
            tr("Move word %1 later").arg(m_selectedWord + 1));
        connect(later, &QAction::triggered, this, [this]() {
            moveWord(m_selectedWord, +1);
        });
    }

    menu.addSeparator();

    // Per-region accept / reject workflow
    QAction *acceptRegion = menu.addAction(tr("Accept this region"));
    connect(acceptRegion, &QAction::triggered, this, [this]() {
        // Accept: mark region as reviewed (future: remove yellow/red overlay for this region)
        // For now: enable accept/reject buttons so user can confirm the full page
        m_btnAccept->setEnabled(true);
        m_btnReject->setEnabled(true);
    });

    QAction *rejectRegion = menu.addAction(tr("Reject this region"));
    connect(rejectRegion, &QAction::triggered, this, &OCRMode::onRejectResults);

    menu.exec(m_scanContentLabel->mapToGlobal(pos));
}

void OCRMode::onReOcrRegion()
{
    emit reOcrRegionRequested(m_contextRegionBbox);
}

// ── B5: image ↔ text ↔ zoom synchronization ─────────────────────────────────

void OCRMode::onTextCursorMoved()
{
    if (m_syncing || !m_textEdit) return;
    const int pos = m_textEdit->textCursor().position();

    // Find the word whose [start, start+len) contains the caret.
    for (int i = 0; i < m_wordRanges.size(); ++i) {
        const int start = m_wordRanges.at(i).first;
        const int len   = m_wordRanges.at(i).second;
        if (pos >= start && pos <= start + len) {
            syncWordTo(i);
            return;
        }
    }
}

void OCRMode::onScanWordLinkActivated(const QString &link)
{
    if (!link.startsWith(QStringLiteral("ocrword:"))) return;
    bool ok = false;
    const int idx = link.mid(QStringLiteral("ocrword:").size()).toInt(&ok);
    if (ok && idx >= 0 && idx < m_currentWords.size())
        syncWordTo(idx);
}

void OCRMode::syncWordTo(int wordIndex)
{
    if (wordIndex < 0 || wordIndex >= m_currentWords.size()) return;
    if (wordIndex == m_selectedWord) return;
    m_selectedWord = wordIndex;

    // 1. Scan pane: re-render so the selected word is outlined.
    updateConfidenceOverlay();

    // 2. Text pane: select the word's character range (drives the caret).
    if (m_textEdit && wordIndex < m_wordRanges.size()) {
        m_syncing = true;
        QTextCursor c = m_textEdit->textCursor();
        c.setPosition(m_wordRanges.at(wordIndex).first);
        c.setPosition(m_wordRanges.at(wordIndex).first + m_wordRanges.at(wordIndex).second,
                      QTextCursor::KeepAnchor);
        m_textEdit->setTextCursor(c);
        m_syncing = false;
    }

    // 3. Zoom pane: magnified crop of the word (B6) or recognized-string
    //    fallback, plus provenance beside it.
    const auto &w = m_currentWords.at(wordIndex);
    renderZoomCrop(wordIndex);
    m_zoomMeta->setText(tr("conf %1% · %2 · bbox (%3,%4 %5×%6)")
                            .arg(w.confidence)
                            .arg(w.sourceEngine)
                            .arg(w.boundingBox.x(), 0, 'f', 0)
                            .arg(w.boundingBox.y(), 0, 'f', 0)
                            .arg(w.boundingBox.width(), 0, 'f', 0)
                            .arg(w.boundingBox.height(), 0, 'f', 0));

    emit wordSelected(wordIndex);
}

// ── B6: zoom-pane magnified crop ─────────────────────────────────────────────

void OCRMode::setPageImage(const QImage &pageImage)
{
    m_pageImage = pageImage;
    // Re-render the current selection so the pane reflects the new raster.
    if (m_selectedWord >= 0)
        renderZoomCrop(m_selectedWord);
}

void OCRMode::renderZoomCrop(int wordIndex)
{
    if (!m_zoomBig || wordIndex < 0 || wordIndex >= m_currentWords.size()) return;

    const auto &w = m_currentWords.at(wordIndex);
    if (!m_pageImage.isNull()) {
        // Crop the word's bbox with a small margin, then scale up 4×
        // (smooth transform) for eyeballing ambiguous glyphs.
        QRectF r = w.boundingBox;
        const qreal margin = 4.0;
        QRect crop((r.left() - margin), (r.top() - margin),
                   (r.width() + 2 * margin), (r.height() + 2 * margin));
        crop = crop.intersected(m_pageImage.rect());
        if (!crop.isEmpty()) {
            const QImage cropped = m_pageImage.copy(crop);
            // Scale by the user zoom factor, capped so it fits the pane.
            const int targetW = int(cropped.width()  * m_zoomFactor);
            const int targetH = int(cropped.height() * m_zoomFactor);
            QPixmap mag = QPixmap::fromImage(cropped).scaled(
                targetW, targetH,
                Qt::KeepAspectRatio, Qt::SmoothTransformation);
            if (mag.width() > m_zoomBig->width() - 8 ||
                mag.height() > m_zoomBig->height() - 8) {
                mag = mag.scaled(m_zoomBig->width() - 8, m_zoomBig->height() - 8,
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            m_zoomBig->setPixmap(mag);
            return;
        }
    }

    // Fallback: show the recognized string in large type.
    m_zoomBig->setText(w.text.toHtmlEscaped());
}

// ── B2: next/previous low-confidence word navigation ─────────────────────────

void OCRMode::gotoNextUncertain()
{
    if (m_lowConfWords.isEmpty()) return;
    // Wrap forward; starting from "not started" lands on the first item.
    m_uncertainCursor = (m_uncertainCursor + 1) % m_lowConfWords.size();
    syncWordTo(m_lowConfWords.at(m_uncertainCursor));
}

void OCRMode::gotoPrevUncertain()
{
    if (m_lowConfWords.isEmpty()) return;
    m_uncertainCursor = (m_uncertainCursor <= 0)
                            ? m_lowConfWords.size() - 1 : m_uncertainCursor - 1;
    syncWordTo(m_lowConfWords.at(m_uncertainCursor));
}

// ── B3/B4: Verify Text dialog ────────────────────────────────────────────────

void OCRMode::openVerifyDialog()
{
    if (m_lowConfWords.isEmpty()) return; // nothing to verify

    if (!m_verifyDialog) {
        m_verifyDialog = new OcrVerifyDialog(this);
        connect(m_verifyDialog, &OcrVerifyDialog::confirmRequested,
                this, &OCRMode::onVerifyConfirm);
        connect(m_verifyDialog, &OcrVerifyDialog::skipRequested,
                this, &OCRMode::onVerifySkip);
        connect(m_verifyDialog, &OcrVerifyDialog::addToDictionaryRequested,
                this, [this](const QString &word) {
                    addUserDictionaryWord(m_dictLang, word);
                    // Re-flag: the word (and all matches) stop being uncertain.
                    rebuildTextWordIndex();
                    applyUncertainHighlights();
                });
        connect(m_verifyDialog, &OcrVerifyDialog::skipAllRequested,
                this, [this](const QString &token) {
                    if (!m_skipAllTokens.contains(token))
                        m_skipAllTokens.append(token);
                    rebuildTextWordIndex();
                    applyUncertainHighlights();
                });
        connect(m_verifyDialog, &OcrVerifyDialog::replaceAllRequested,
                this, [this](const QString &from, const QString &to) {
                    for (int i = 0; i < m_currentWords.size(); ++i) {
                        if (m_currentWords.at(i).text == from) {
                            onVerifyConfirm(i, to);
                            markWordVerified(i);
                        }
                    }
                });
        connect(m_verifyDialog, &OcrVerifyDialog::reRecognizeRequested,
                this, [this](int wordIndex) {
                    if (wordIndex < 0 || wordIndex >= m_currentWords.size()) return;
                    // Reuse the existing per-region Re-OCR pathway; the host
                    // brokers reOcrRegionRequested to EditController.
                    emit reOcrRegionRequested(m_currentWords.at(wordIndex).boundingBox);
                });
    }

    QList<OcrVerifyDialog::Item> items;

    // B9: candidate pool = user dictionary + every word in the document.
    QStringList vocabulary = loadUserDictionary(m_dictLang);
    for (const auto &w : m_currentWords)
        if (!vocabulary.contains(w.text)) vocabulary.append(w.text);

    for (int li = 0; li < m_lowConfWords.size(); ++li) {
        const int wi = m_lowConfWords.at(li);
        // B15: with "Verify low-conf" off, the dialog skips words flagged
        // for low confidence; purely spell-flagged words still queue.
        if (!m_lowConfVerifyEnabled &&
            li < m_flagReasons.size() &&
            (m_flagReasons.at(li) & 0x1))
            continue;
        OcrVerifyDialog::Item it;
        it.wordIndex  = wi;
        it.text       = m_currentWords.at(wi).text;
        it.confidence = m_currentWords.at(wi).confidence;
        it.suggestions = suggestCorrections(it.text, vocabulary);
        if (!m_pageImage.isNull()) {
            // Same magnified-crop treatment as the zoom pane (B6).
            const QRectF r = m_currentWords.at(wi).boundingBox;
            const qreal margin = 4.0;
            QRect crop((r.left() - margin), (r.top() - margin),
                       (r.width() + 2 * margin), (r.height() + 2 * margin));
            crop = crop.intersected(m_pageImage.rect());
            if (!crop.isEmpty())
                it.crop = m_pageImage.copy(crop);
        }
        items.append(it);
    }
    m_verifyDialog->setItems(items);
    m_verifyDialog->show();
    m_verifyDialog->raise();
    m_verifyDialog->activateWindow();
}

void OCRMode::onVerifyConfirm(int wordIndex, const QString &correctedText)
{
    if (wordIndex < 0 || wordIndex >= m_currentWords.size()) return;

    // Apply the correction into the editable text pane at the word's range.
    if (m_textEdit && wordIndex < m_wordRanges.size()) {
        QTextCursor c = m_textEdit->textCursor();
        const int start = m_wordRanges.at(wordIndex).first;
        c.setPosition(start);
        c.setPosition(start + m_wordRanges.at(wordIndex).second,
                      QTextCursor::KeepAnchor);
        c.insertText(correctedText);
    }

    // Keep the model in sync so later indices stay valid.
    m_currentWords[wordIndex].text = correctedText;
    markWordVerified(wordIndex);

    // Ranges after this word shifted by the length delta — rebuild.
    rebuildTextWordIndex();
    applyUncertainHighlights();
}

void OCRMode::onVerifySkip(int wordIndex)
{
    if (wordIndex < 0 || wordIndex >= m_currentWords.size()) return;
    syncWordTo(wordIndex); // show where we are; no state change
}

// ── B11: reading-order editing ───────────────────────────────────────────────

void OCRMode::moveWord(int wordIndex, int delta)
{
    const int target = wordIndex + delta;
    if (wordIndex < 0 || wordIndex >= m_currentWords.size()) return;
    if (target < 0 || target >= m_currentWords.size()) return;

    m_currentWords.swapItemsAt(wordIndex, target);
    // The verified-state vector is parallel to m_currentWords, so it must move
    // with the words — otherwise a reorder misaligns which words read as verified.
    if (wordIndex < m_verifiedWords.size() && target < m_verifiedWords.size())
        m_verifiedWords.swapItemsAt(wordIndex, target);

    // Re-derive every view from the reordered model.
    updateConfidenceOverlay();
    if (m_textEdit) {
        QStringList lines;
        for (const auto &w : m_currentWords)
            lines.append(w.text);
        m_wordRanges.clear();
        m_lowConfWords.clear();
        m_flagReasons.clear();
        m_textEdit->setPlainText(lines.join(QStringLiteral(" ")));
        rebuildTextWordIndex();
        applyUncertainHighlights();
    }
    updateInfoStrip();
}

// ── setOcrResults ─────────────────────────────────────────────────────────────

void OCRMode::setOcrResults(const QList<MergedOcrWord> &words)
{
    m_currentWords = words;
    m_selectedWord = -1;   // new results: nothing synchronized yet
    m_uncertainCursor = -1;
    m_verifiedWords.clear();
    for (int i = 0; i < words.size(); ++i) m_verifiedWords.append(false);
    setPageVerified(false);   // B12: new results start unverified
    updateConfidenceOverlay();
    updateInfoStrip();

    // Populate plain-text editor with the recognized text, recording each
    // word's character range so low-confidence words can be highlighted (B1).
    if (m_textEdit) {
        // Clear ranges first: setPlainText fires cursorPositionChanged and
        // must not resolve against the PREVIOUS document's word index.
        m_wordRanges.clear();
        m_lowConfWords.clear();
        m_flagReasons.clear();
        QStringList lines;
        for (const auto &w : words)
            lines.append(w.text);
        m_textEdit->setPlainText(lines.join(QStringLiteral(" ")));
        rebuildTextWordIndex();
        applyUncertainHighlights();
    }

    // Restore the Run button (it was disabled + relabelled while OCR ran).
    if (m_btnRun) {
        m_btnRun->setEnabled(true);
        m_btnRun->setText(tr("Run OCR"));
    }

    // Enable review buttons only when there is something to review.
    const bool hasWords = !words.isEmpty();
    if (m_btnAccept) m_btnAccept->setEnabled(hasWords);
    if (m_btnReject) m_btnReject->setEnabled(hasWords);
}

// ── updateConfidenceOverlay ───────────────────────────────────────────────────

void OCRMode::updateConfidenceOverlay()
{
    if (!m_scanContentLabel) return;

    if (m_currentWords.isEmpty()) {
        // No results yet — show the empty state, not stale content.
        m_scanContentLabel->setText(kOcrEmptyStateHtml);
        return;
    }

    // Build a rich-text paragraph with per-word confidence coloring.
    // Thresholds are user-editable (B15); defaults per M5-P2 D6 spec:
    //   green: confidence ≥ highThreshold (default 90)
    //   yellow: between the cutoffs
    //   red: confidence < lowThreshold (default 70)
    QString html;
    html.reserve(m_currentWords.size() * 80);

    for (int i = 0; i < m_currentWords.size(); ++i) {
        const auto &w = m_currentWords.at(i);
        const int conf = w.confidence;
        QString bgColor, borderColor;
        if (conf >= m_highThreshold) {
            bgColor     = QStringLiteral("#22c55e33");
            borderColor = QStringLiteral("#22c55e99");
        } else if (conf >= m_lowThreshold) {
            bgColor     = QStringLiteral("#eab30833");
            borderColor = QStringLiteral("#eab30899");
        } else {
            bgColor     = QStringLiteral("#ef444433");
            borderColor = QStringLiteral("#ef444499");
        }

        // B5: the currently synchronized word gets a strong outline.
        QString selStyle;
        if (i == m_selectedWord)
            selStyle = QStringLiteral("outline:2px solid #1d4ed8;background:#bfdbfe;");

        // Escape HTML special chars in the word text.  Wrapped in an anchor so
        // clicking it syncs the text/zoom panes (B5); href carries the index.
        QString escaped = w.text.toHtmlEscaped();

        html += QStringLiteral(
            "<a href='ocrword:%1' style='text-decoration:none;'>"
            "<span style='background:%5;%7outline:1px solid %6;padding:1px;margin:1px;' "
            "title='%3% | %4 | word %1'>"
            "<sup style='color:#666;font-size:9px;'>%1</sup>%8</span></a> ")
            .arg(QString::number(i))   // %1 (both occurrences)
            .arg(conf)                 // %3
            .arg(w.sourceEngine)       // %4
            .arg(bgColor)              // %5
            .arg(borderColor)          // %6
            .arg(selStyle)             // %7
            .arg(escaped);             // %8 last: word text may contain '%'
    }

    m_scanContentLabel->setText(html);
}

// ── updateInfoStrip ───────────────────────────────────────────────────────────

void OCRMode::updateInfoStrip()
{
    // B7: page indicator (set via setPageProgress; em-dash state when unset).
    if (m_pageTotal > 0)
        m_lblPage->setText(tr("PAGE %1 OF %2").arg(m_pageCurrent).arg(m_pageTotal));
    else
        m_lblPage->setText(tr("PAGE — OF —"));

    // B7: recognition language, mirrored from the persisted OCR language.
    QSettings settings;
    const QString code = settings.value(kOcrLanguageKey, "EN").toString();
    m_lblLanguage->setText(tr("LANGUAGE %1").arg(code));

    // B6/B7: zoom-pane magnification.
    m_lblZoom->setText(tr("ZOOM %1%").arg(int(m_zoomFactor * 100)));

    if (m_currentWords.isEmpty()) {
        m_lblAvgConf->setText(tr("AVG CONFIDENCE —"));
        m_lblLowWords->setText(tr("UNCERTAIN —"));
        m_lblVerified->setText(tr("VERIFIED —"));
        return;
    }

    double totalConf = 0.0;
    int lowCount     = 0;
    for (const auto &w : m_currentWords) {
        totalConf += w.confidence;
        if (w.confidence < m_lowThreshold) ++lowCount;
    }
    const double avgConf = totalConf / m_currentWords.size();

    m_lblAvgConf->setText(
        tr("AVG CONFIDENCE %1%").arg(static_cast<int>(std::round(avgConf))));
    m_lblLowWords->setText(
        tr("UNCERTAIN %1 REMAINING").arg(lowCount));
    m_lblVerified->setText(
        tr("VERIFIED %1%").arg(verifiedPercent()));
}

// ── B7: page progress + verification state ───────────────────────────────────

void OCRMode::setPageProgress(int current, int total)
{
    m_pageCurrent = current;
    m_pageTotal   = total;
    updateInfoStrip();
}

void OCRMode::markWordVerified(int wordIndex)
{
    if (wordIndex < 0 || wordIndex >= m_verifiedWords.size()) return;
    m_verifiedWords[wordIndex] = true;
    updateInfoStrip();
}

int OCRMode::verifiedPercent() const
{
    const int total = m_verifiedWords.size();
    if (total == 0) return 0;
    int verified = 0;
    for (bool v : m_verifiedWords) if (v) ++verified;
    return static_cast<int>(std::lround(verified * 100.0 / total));
}

void OCRMode::setPageVerified(bool verified)
{
    m_pageVerified = verified;
    // Keep the toolbar toggle in sync when set programmatically.
    if (m_btnPageVerified && m_btnPageVerified->isChecked() != verified)
        m_btnPageVerified->setChecked(verified);
}

// ── B1: uncertain-word highlighting in the editable text pane ────────────────

int OCRMode::uncertainHighlightCount() const
{
    return m_textEdit ? m_textEdit->extraSelections().size() : 0;
}

void OCRMode::rebuildTextWordIndex()
{
    m_wordRanges.clear();
    m_lowConfWords.clear();
    m_flagReasons.clear();
    if (!m_textEdit) return;

    const QString text = m_textEdit->toPlainText();

    // B10: words in the per-language user dictionary are considered known
    // and are not flagged as uncertain.
    const QStringList userDict = loadUserDictionary(m_dictLang);

    int from = 0;
    for (int i = 0; i < m_currentWords.size(); ++i) {
        const auto &w = m_currentWords.at(i);
        // Words were joined with single spaces in document order, so a
        // forward search always finds the next occurrence.
        const int idx = text.indexOf(w.text, from);
        if (idx < 0) break; // user-edited or mismatched text — stop flagging
        const int len = w.text.length();
        m_wordRanges.append({idx, len});
        const bool lowConf = w.confidence < m_lowThreshold;
        const bool notInDict = !userDict.contains(w.text, Qt::CaseInsensitive);
        const bool suppressed = m_skipAllTokens.contains(w.text);
        // B15: flag on low confidence (unless suppressed/known), and
        // optionally on spelling (word missing from the user dictionary).
        if (((lowConf && notInDict) || (m_spellCheckEnabled && notInDict))
            && !suppressed) {
            m_lowConfWords.append(i); // B2: nav index = word position
            m_flagReasons.append((lowConf ? 1 : 0) | (notInDict ? 2 : 0));
        }
        from = idx + len;
    }
}

void OCRMode::applyUncertainHighlights()
{
    if (!m_textEdit) return;

    QList<QTextEdit::ExtraSelection> sels;
    if (m_uncertainEnabled) {
        // FineReader's light-blue "uncertain characters" background.
        QTextCharFormat fmt;
        fmt.setBackground(QColor(QStringLiteral("#b9d7f2")));

        sels.reserve(m_lowConfWords.size());
        for (const int wi : m_lowConfWords) {
            if (wi < 0 || wi >= m_wordRanges.size()) continue;
            const auto &r = m_wordRanges.at(wi);
            QTextEdit::ExtraSelection sel;
            sel.cursor = QTextCursor(m_textEdit->document());
            sel.cursor.setPosition(r.first);
            sel.cursor.setPosition(r.first + r.second, QTextCursor::KeepAnchor);
            sel.format = fmt;
            sel.cursor.clearSelection();
            sels.append(sel);
        }
    }
    m_textEdit->setExtraSelections(sels);
}

// ── setSemanticDocument — Djot-aware review UI ────────────────────────────────

namespace {

/// Escape HTML special characters for inline display.
static QString htmlEsc(const std::string& s) {
    return QString::fromStdString(s).toHtmlEscaped();
}

/// Walk a Block and produce simple inline-styled HTML.
static void blockToHtml(const docmodel::Block& block, std::ostringstream& html)
{
    using BT = docmodel::Block::Type;

    // Helper lambda: render a vector of Inline nodes to HTML
    auto inlinesToHtml = [](const std::vector<std::shared_ptr<docmodel::Inline>>& inlines,
                            std::ostringstream& out) {
        for (const auto& inl : inlines) {
            if (!inl) continue;
            switch (inl->getType()) {
            case docmodel::Inline::Type::Text:
                out << htmlEsc(inl->getText()).toStdString();
                break;
            case docmodel::Inline::Type::Strong:
                out << "<b>";
                for (const auto& ch : inl->getChildren())
                    if (ch) out << htmlEsc(ch->getText()).toStdString();
                out << "</b>";
                break;
            case docmodel::Inline::Type::Emph:
                out << "<i>";
                for (const auto& ch : inl->getChildren())
                    if (ch) out << htmlEsc(ch->getText()).toStdString();
                out << "</i>";
                break;
            case docmodel::Inline::Type::Code:
                out << "<code style='font-family:monospace;background:#e0e0e0;padding:1px 3px;'>";
                for (const auto& ch : inl->getChildren())
                    if (ch) out << htmlEsc(ch->getText()).toStdString();
                out << "</code>";
                break;
            }
        }
    };

    switch (block.getType()) {
    case BT::Heading:
        html << "<h1 style='font-size:18px;font-weight:600;margin:8px 0 4px;color:#1a1a1a;'>";
        inlinesToHtml(block.getInlines(), html);
        html << "</h1>";
        break;
    case BT::Paragraph:
        html << "<p style='margin:4px 0;color:#1a1a1a;'>";
        inlinesToHtml(block.getInlines(), html);
        html << "</p>";
        break;
    case BT::Figure:
        html << "<p style='margin:4px 0;color:#555;font-style:italic;'>"
             << "<span style='background:#dbeafe;padding:1px 4px;border-radius:2px;'>"
             << "Figure: </span> ";
        inlinesToHtml(block.getInlines(), html);
        html << "</p>";
        break;
    case BT::List:
        html << "<ul style='margin:4px 0 4px 20px;color:#1a1a1a;'>";
        for (const auto& item : block.getBlocks()) {
            if (!item) continue;
            html << "<li>";
            inlinesToHtml(item->getInlines(), html);
            html << "</li>";
        }
        html << "</ul>";
        break;
    case BT::Table: {
        html << "<table style='border-collapse:collapse;margin:6px 0;width:100%;'>";
        bool firstRow = true;
        for (const auto& row : block.getBlocks()) {
            if (!row) continue;
            html << "<tr>";
            const auto& cells = row->getBlocks();
            if (cells.empty()) {
                // Row with inline content
                const char* cellTag = firstRow ? "th" : "td";
                const char* cellStyle = firstRow
                    ? "border:1px solid #999;padding:3px 6px;background:#e8e8e8;font-weight:600;"
                    : "border:1px solid #ccc;padding:3px 6px;";
                html << "<" << cellTag << " style='" << cellStyle << "'>";
                inlinesToHtml(row->getInlines(), html);
                html << "</" << cellTag << ">";
            } else {
                for (const auto& cell : cells) {
                    if (!cell) continue;
                    const char* cellTag = firstRow ? "th" : "td";
                    const char* cellStyle = firstRow
                        ? "border:1px solid #999;padding:3px 6px;background:#e8e8e8;font-weight:600;"
                        : "border:1px solid #ccc;padding:3px 6px;";
                    html << "<" << cellTag << " style='" << cellStyle << "'>";
                    inlinesToHtml(cell->getInlines(), html);
                    html << "</" << cellTag << ">";
                }
            }
            html << "</tr>";
            firstRow = false;
        }
        html << "</table>";
        break;
    }
    case BT::CodeBlock:
        html << "<pre style='background:#f5f5f5;padding:6px;font-family:monospace;"
                "font-size:11px;margin:4px 0;overflow:auto;'>";
        for (const auto& child : block.getBlocks()) {
            if (!child) continue;
            inlinesToHtml(child->getInlines(), html);
            html << "\n";
        }
        html << "</pre>";
        break;
    case BT::ListItem:
        // Standalone ListItem (shouldn't normally appear outside a List)
        inlinesToHtml(block.getInlines(), html);
        break;
    }
}

/// Walk a SemanticDocument and produce inline-styled HTML preview.
static QString semanticDocToHtml(const docmodel::SemanticDocument& doc)
{
    std::ostringstream html;
    html << "<html><body style='font-family:Manrope,sans-serif;font-size:13px;"
            "background:#f4f1ea;color:#1a1a1a;padding:16px;'>";

    for (const auto& section : doc.getSections()) {
        if (!section) continue;

        // Page header (section-level separator if not the first section)
        if (!section->getTitle().empty()) {
            html << "<h2 style='font-size:14px;font-weight:700;margin:12px 0 4px;"
                    "color:#666;border-top:1px solid #ccc;padding-top:8px;'>"
                 << htmlEsc(section->getTitle()).toStdString()
                 << "</h2>";
        }

        for (const auto& block : section->getBlocks()) {
            if (block) blockToHtml(*block, html);
        }
    }

    html << "</body></html>";
    return QString::fromStdString(html.str());
}

} // anonymous namespace

void OCRMode::setSemanticDocument(const docmodel::SemanticDocument &doc,
                                  const QString &djotLibPath)
{
    // 1. Render SemanticDocument → inline-styled HTML for the scan pane
    if (m_scanContentLabel) {
        const QString html = semanticDocToHtml(doc);
        m_scanContentLabel->setText(html);
    }

    // 2. Populate the Djot text editor for Djot-aware edit-in-place.
    //    LuaDjotCodec::documentToDjot uses the C++ emitter (no Lua required for encode).
    //    The djotLibPath is only needed for djotToDocument (the decode stub) — not here.
    if (m_textEdit) {
        pdfws::LuaDjotCodec codec(djotLibPath.toStdString());
        try {
            std::string djotText = codec.documentToDjot(doc);
            m_textEdit->setPlainText(QString::fromStdString(djotText));
        } catch (const std::exception& e) {
            m_textEdit->setPlainText(
                tr("[Djot encode error: %1]").arg(QString::fromLatin1(e.what())));
        }
    }

    // 3. Enable the review buttons (accept/reject) — per-region workflow
    if (m_btnAccept) m_btnAccept->setEnabled(true);
    if (m_btnReject) m_btnReject->setEnabled(true);
}

} // namespace gp
