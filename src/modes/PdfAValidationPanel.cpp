#include "PdfAValidationPanel.h"
#include "util/GpTheme.h"
#include "util/Badge.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace gp {

static QWidget* issueRow(const QString& rule, const QString& descr, bool err) {
    auto* w = new QFrame;
    w->setStyleSheet(QString("background:#1a1b1e; border:1px solid #393b40; border-left:3px solid %1; padding:8px 10px; margin-bottom:6px;")
        .arg(err ? "#c8442b" : "#ff8c42"));
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto* dot = new QLabel(err ? "✕" : "▲");
    dot->setStyleSheet(QString("color:%1;font-weight:700;").arg(err ? "#c8442b" : "#ff8c42"));
    auto* lbl = new QLabel(QString("<b style='color:#dfe1e5;font-family:JetBrains Mono;font-size:10px;'>%1</b> · %2").arg(rule, descr));
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);
    auto* jump = new QPushButton("JUMP");
    jump->setStyleSheet("font-family:JetBrains Mono; font-size:9.5px; color:#ff8c42; border:1px solid rgba(255,140,66,0.33); padding:1px 6px;");
    h->addWidget(dot);
    h->addWidget(lbl, 1);
    h->addWidget(jump);
    return w;
}

PdfAValidationPanel::PdfAValidationPanel(QWidget* parent) : QFrame(parent) {
    setObjectName("rightSidebar");
    setFixedWidth(Theme::RightPaneW);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);

    auto* head = new QFrame; head->setProperty("role","modeToolbar"); head->setFixedHeight(32);
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(10,0,10,0);
    auto* t = new QLabel("PDF/A · VALIDATION"); t->setStyleSheet("font-weight:600;letter-spacing:1.2px;");
    hr->addWidget(t); hr->addStretch(1);
    outer->addWidget(head);

    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget;
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(12,12,12,12); col->setSpacing(8);

    // Summary card
    auto* sum = new QFrame;
    sum->setStyleSheet("background:#1a1b1e; border:1px solid #393b40; padding:12px;");
    auto* sumLay = new QVBoxLayout(sum);
    auto* sumRow = new QHBoxLayout;
    auto* sumTitle = new QLabel("PDF/A-2B VALIDATION");
    sumTitle->setStyleSheet("font-weight:600;letter-spacing:0.8px;");
    sumRow->addWidget(sumTitle);
    sumRow->addStretch(1);
    sumRow->addWidget(new Badge("WARNINGS", Badge::Warn));
    sumLay->addLayout(sumRow);
    auto* sumMeta = new QLabel("47 RULES · 2 FAILURES · 3 WARNINGS\nCONFORMANCE: PDF/A-2b (ISO 19005-2)");
    sumMeta->setProperty("mono", true);
    sumLay->addWidget(sumMeta);
    col->addWidget(sum);

    auto* head2 = new QLabel("ISSUES · 5");
    head2->setProperty("mono", true);
    col->addWidget(head2);

    col->addWidget(issueRow("Rule 6.2.3.3-1", "Embedded fonts required · Page 3", true));
    col->addWidget(issueRow("Rule 6.2.8-1",  "Non-embedded font: Arial · Page 7", true));
    col->addWidget(issueRow("Rule 7.1-1",    "Optional content (layers) detected", false));
    col->addWidget(issueRow("Rule 6.3.3-1",  "Annotation appearance stream missing", false));
    col->addWidget(issueRow("Rule 6.2.10-1", "Device-dependent color space in image", false));

    auto* fix = new QPushButton("Fix Automatically (3)");
    auto* conv = new QPushButton("Convert to PDF/A-2B");
    conv->setStyleSheet("background:#ff8c42;color:#1a1b1e;border:1px solid #ff8c42;font-weight:600;padding:8px 12px;");
    auto* exp = new QPushButton("Export Report");
    col->addWidget(fix);
    col->addWidget(conv);
    col->addWidget(exp);
    col->addStretch(1);

    scroll->setWidget(body);
    outer->addWidget(scroll, 1);
}

} // namespace gp
