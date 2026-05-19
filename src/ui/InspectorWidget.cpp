#include "InspectorWidget.h"
#include "core/AnnotationTypes.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QPropertyAnimation>
#include <QTextEdit>
#include <QPdfDocument>
#include <QFileInfo>
#include <QDateTime>
#include <QLocale>

namespace {
    const char* BG_0  = "#1a1b1e";
    const char* BG_1  = "#1e1f22";
    const char* BG_2  = "#2b2d30";
    const char* BG_3  = "#34363b";
    const char* BG_4  = "#3d4046";
    const char* LINE  = "#393b40";
    const char* LINE_STRONG = "#4a4d52";
    const char* FG_0  = "#dfe1e5";
    const char* FG_1  = "#a8abb0";
    const char* FG_2  = "#71747a";
    const char* FG_3  = "#52555a";
    const char* ACCENT      = "#ff8c42";
    const char* ACCENT_DIM  = "rgba(255,140,66,0.15)";
    const char* ACCENT_LINE = "#ff8c4255";
}

// ─────────────────────────────────────────────────────────────────────────────
// Stylesheet helpers
// ─────────────────────────────────────────────────────────────────────────────

static QString tabBarSheet() {
    return QStringLiteral(
        "QWidget#inspTabBar {"
        "  background: %1;"
        "  border-bottom: 1px solid %2;"
        "}"
    ).arg(BG_2, LINE);
}

static QString tabBtnSheet() {
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  color: %1;"
        "  font-family: 'Manrope', sans-serif;"
        "  font-size: 10.5px;"
        "  font-weight: 600;"
        "  text-transform: uppercase;"
        "  padding: 4px 10px;"
        "  letter-spacing: 0.5px;"
        "}"
        "QPushButton:hover {"
        "  color: %2;"
        "  background: %3;"
        "}"
        "QPushButton:checked {"
        "  color: %4;"
        "  border-bottom: 2px solid %5;"
        "}"
    ).arg(FG_2, FG_1, BG_3, FG_0, ACCENT);
}

static QString iconBtnSheet() {
    return QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  border: none;"
        "  color: %1;"
        "  padding: 4px 6px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  color: %2;"
        "  background: %3;"
        "}"
    ).arg(FG_2, FG_1, BG_3);
}

static QString scrollAreaSheet() {
    return QStringLiteral(
        "QScrollArea { border: none; background: %1; }"
        "QScrollBar:vertical { width: 6px; background: %1; }"
        "QScrollBar::handle:vertical { background: %2; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(BG_1, BG_4);
}

static QString sectionHeaderSheet() {
    return QStringLiteral(
        "QWidget#sectionHeader {"
        "  background: transparent;"
        "}"
        "QWidget#sectionHeader:hover {"
        "  background: %1;"
        "}"
    ).arg(BG_2);
}

static QString fieldLabelSheet() {
    return QStringLiteral(
        "color: %1;"
        "font-family: 'Consolas', monospace;"
        "font-size: 10px;"
    ).arg(FG_2);
}

static QString fieldValueSheet() {
    return QStringLiteral(
        "color: %1;"
        "font-family: 'Manrope', sans-serif;"
        "font-size: 11px;"
    ).arg(FG_0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: field row (label + value)
// ─────────────────────────────────────────────────────────────────────────────

static QWidget* createFieldRow(const QString& labelText, const QString& valueText)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 3, 12, 3);
    layout->setSpacing(8);

    auto* label = new QLabel(labelText);
    label->setFixedWidth(78);
    label->setStyleSheet(fieldLabelSheet());
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(label);

    auto* value = new QLabel(valueText);
    value->setStyleSheet(fieldValueSheet());
    value->setWordWrap(true);
    layout->addWidget(value, 1);

    return row;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: color swatch
// ─────────────────────────────────────────────────────────────────────────────

static QWidget* createColorSwatch(const QString& color)
{
    auto* swatch = new QWidget;
    swatch->setFixedSize(22, 18);
    swatch->setCursor(Qt::PointingHandCursor);
    swatch->setStyleSheet(QStringLiteral(
        "background: %1; border: 1px solid %2; border-radius: 3px;"
    ).arg(color, LINE));
    return swatch;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

InspectorWidget::InspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(288);
    setStyleSheet(QStringLiteral("background: %1;").arg(BG_1));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    createTabs();
    mainLayout->addWidget(m_tabBar);

    m_contentStack = new QStackedWidget;
    m_contentStack->setStyleSheet(QStringLiteral("QStackedWidget { background: %1; }").arg(BG_1));

    // Page 0: empty state
    m_contentStack->addWidget(createEmptyState());

    // Page 1: annotation properties view
    m_contentStack->addWidget(createPropertiesView());

    // Page 2: document info view
    m_contentStack->addWidget(createDocInfoView());

    mainLayout->addWidget(m_contentStack, 1);

    m_contentStack->setCurrentIndex(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab bar
// ─────────────────────────────────────────────────────────────────────────────

void InspectorWidget::createTabs()
{
    m_tabBar = new QWidget;
    m_tabBar->setObjectName("inspTabBar");
    m_tabBar->setFixedHeight(28);
    m_tabBar->setStyleSheet(tabBarSheet());

    auto* layout = new QHBoxLayout(m_tabBar);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(0);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);

    const QStringList tabNames = {
        QStringLiteral("Properties"),
        QStringLiteral("Comments"),
        QStringLiteral("Layers")
    };

    for (int i = 0; i < tabNames.size(); ++i) {
        auto* btn = new QPushButton(tabNames[i]);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(tabBtnSheet());
        if (i == 0) btn->setChecked(true);
        m_tabGroup->addButton(btn, i);
        layout->addWidget(btn);
    }

    layout->addStretch();

    // More button
    auto* moreBtn = new QPushButton(QChar(0x22EE)); // ⋮
    moreBtn->setFixedSize(24, 24);
    moreBtn->setStyleSheet(iconBtnSheet());
    moreBtn->setToolTip(QStringLiteral("More options"));
    layout->addWidget(moreBtn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Empty state
// ─────────────────────────────────────────────────────────────────────────────

QWidget* InspectorWidget::createEmptyState()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);

    // Diamond glyph
    auto* glyph = new QLabel(QStringLiteral("◇"));
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 32px;"
    ).arg(FG_3));
    layout->addWidget(glyph);

    // Title
    auto* title = new QLabel(QStringLiteral("No selection"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral(
        "color: %1;"
        "font-family: 'Manrope', sans-serif;"
        "font-size: 11px;"
        "font-weight: 600;"
        "text-transform: uppercase;"
        "letter-spacing: 0.5px;"
        "margin-top: 8px;"
    ).arg(FG_1));
    layout->addWidget(title);

    // Subtitle
    auto* subtitle = new QLabel(QStringLiteral("Select an annotation to\ninspect its properties"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QStringLiteral(
        "color: %1;"
        "font-family: 'Consolas', monospace;"
        "font-size: 10.5px;"
        "margin-top: 4px;"
    ).arg(FG_2));
    layout->addWidget(subtitle);

    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
// Properties view
// ─────────────────────────────────────────────────────────────────────────────

QWidget* InspectorWidget::createPropertiesView()
{
    m_propsScroll = new QScrollArea;
    m_propsScroll->setWidgetResizable(true);
    m_propsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_propsScroll->setStyleSheet(scrollAreaSheet());

    m_propsContent = new QWidget;
    m_propsContent->setStyleSheet(QStringLiteral("background: %1;").arg(BG_1));
    auto* propsLayout = new QVBoxLayout(m_propsContent);
    propsLayout->setContentsMargins(0, 0, 0, 0);
    propsLayout->setSpacing(0);

    // ── Header ──────────────────────────────────────────────────────────────
    m_headerWidget = new QWidget;
    m_headerWidget->setStyleSheet(QStringLiteral(
        "background: %1; border-bottom: 1px solid %2;"
    ).arg(BG_1, LINE));
    auto* headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(4);

    // Top row: dot + type + ID
    auto* headerTop = new QHBoxLayout;
    headerTop->setSpacing(6);

    auto* headerDot = new QLabel;
    headerDot->setObjectName("headerDot");
    headerDot->setFixedSize(10, 10);
    headerDot->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 5px; min-width: 10px; min-height: 10px;"
    ).arg(ACCENT));
    headerTop->addWidget(headerDot);

    auto* typeName = new QLabel(QStringLiteral("HIGHLIGHT"));
    typeName->setObjectName("typeName");
    typeName->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Manrope', sans-serif;"
        "font-size: 11px; font-weight: 700; text-transform: uppercase;"
    ).arg(FG_0));
    headerTop->addWidget(typeName);

    auto* annotId = new QLabel(QStringLiteral("#001"));
    annotId->setObjectName("annotId");
    annotId->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 10px;"
    ).arg(FG_2));
    headerTop->addWidget(annotId);
    headerTop->addStretch();

    headerLayout->addLayout(headerTop);

    // Sub-header: page / layer / locked
    auto* subHeader = new QLabel(QStringLiteral("Page 1 \xC2\xB7 Default Layer \xC2\xB7 Unlocked"));
    subHeader->setObjectName("subHeader");
    subHeader->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 9.5px;"
    ).arg(FG_2));
    headerLayout->addWidget(subHeader);

    propsLayout->addWidget(m_headerWidget);

    // ── Section 1: Identity ─────────────────────────────────────────────────
    auto* identityContent = new QWidget;
    auto* identityLayout = new QVBoxLayout(identityContent);
    identityLayout->setContentsMargins(0, 4, 0, 4);
    identityLayout->setSpacing(0);

    identityLayout->addWidget(createFieldRow("Author", "Unknown"));
    identityLayout->addWidget(createFieldRow("Created", "--"));
    identityLayout->addWidget(createFieldRow("Modified", "--"));
    identityLayout->addWidget(createFieldRow("Subject", "--"));

    propsLayout->addWidget(createCollapsibleSection("Identity", identityContent));

    // ── Section 2: Appearance ───────────────────────────────────────────────
    auto* appearContent = new QWidget;
    auto* appearLayout = new QVBoxLayout(appearContent);
    appearLayout->setContentsMargins(12, 6, 12, 6);
    appearLayout->setSpacing(6);

    // Color swatches row
    auto* swatchRow = new QHBoxLayout;
    swatchRow->setSpacing(4);

    auto* swatchLabel = new QLabel(QStringLiteral("Color"));
    swatchLabel->setFixedWidth(78);
    swatchLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    swatchLabel->setStyleSheet(fieldLabelSheet());
    swatchRow->addWidget(swatchLabel);

    const QStringList colors = {"#f5c542", "#42a5f5", "#ef5350", "#66bb6a", "#ab47bc", "#ff8c42"};
    for (const auto& c : colors) {
        swatchRow->addWidget(createColorSwatch(c));
    }
    swatchRow->addStretch();
    appearLayout->addLayout(swatchRow);

    // Opacity
    auto* opacityRow = createFieldRow("Opacity", "100%");
    appearLayout->addWidget(opacityRow);

    // Blend mode
    auto* blendRow = createFieldRow("Blend", "Normal");
    appearLayout->addWidget(blendRow);

    // Border width
    auto* borderRow = createFieldRow("Border", "2px");
    appearLayout->addWidget(borderRow);

    propsLayout->addWidget(createCollapsibleSection("Appearance", appearContent));

    // ── Section 3: Position ─────────────────────────────────────────────────
    auto* posContent = new QWidget;
    auto* posLayout = new QVBoxLayout(posContent);
    posLayout->setContentsMargins(12, 6, 12, 6);
    posLayout->setSpacing(6);

    // 2-column grid: X/Y/W/H
    auto* posGrid = new QGridLayout;
    posGrid->setSpacing(6);

    auto makeGridLabel = [](const QString& text) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(fieldLabelSheet());
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return lbl;
    };
    auto makeGridValue = [](const QString& text) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(fieldValueSheet());
        return lbl;
    };

    posGrid->addWidget(makeGridLabel("X"), 0, 0);
    posGrid->addWidget(makeGridValue("0.00"), 0, 1);
    posGrid->addWidget(makeGridLabel("Y"), 0, 2);
    posGrid->addWidget(makeGridValue("0.00"), 0, 3);
    posGrid->addWidget(makeGridLabel("W"), 1, 0);
    posGrid->addWidget(makeGridValue("0.00"), 1, 1);
    posGrid->addWidget(makeGridLabel("H"), 1, 2);
    posGrid->addWidget(makeGridValue("0.00"), 1, 3);

    posLayout->addLayout(posGrid);

    // Align/Distribute buttons row
    auto* alignRow = new QHBoxLayout;
    alignRow->setSpacing(2);

    const QStringList alignIcons = {
        QStringLiteral("┃"), // ┃ align-left
        QStringLiteral("│"), // │ align-center
        QStringLiteral("┃"), // ┃ align-right
        QStringLiteral("━"), // ━ align-top
        QStringLiteral("─"), // ─ align-middle
        QStringLiteral("━"), // ━ align-bottom
    };
    const QStringList alignTips = {
        "Align Left", "Align Center", "Align Right",
        "Align Top", "Align Middle", "Align Bottom"
    };

    for (int i = 0; i < alignIcons.size(); ++i) {
        auto* btn = new QPushButton(alignIcons[i]);
        btn->setFixedSize(28, 22);
        btn->setToolTip(alignTips[i]);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background: %1; border: 1px solid %2; border-radius: 3px;"
            "  color: %3; font-size: 11px;"
            "}"
            "QPushButton:hover { background: %4; color: %5; }"
        ).arg(BG_2, LINE, FG_2, BG_3, FG_1));
        alignRow->addWidget(btn);
    }
    alignRow->addStretch();
    posLayout->addLayout(alignRow);

    propsLayout->addWidget(createCollapsibleSection("Position", posContent));

    // ── Section 4: Contents ─────────────────────────────────────────────────
    auto* contentsWidget = new QWidget;
    auto* contentsLayout = new QVBoxLayout(contentsWidget);
    contentsLayout->setContentsMargins(12, 6, 12, 6);
    contentsLayout->setSpacing(4);

    auto* editor = new QTextEdit;
    editor->setFixedHeight(80);
    editor->setPlaceholderText(QStringLiteral("Annotation text content..."));
    editor->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "  color: %3;"
        "  font-family: 'Consolas', monospace;"
        "  font-size: 10.5px;"
        "  padding: 6px;"
        "}"
        "QTextEdit:focus {"
        "  border-color: %4;"
        "}"
    ).arg(BG_0, LINE, FG_0, ACCENT));
    contentsLayout->addWidget(editor);

    auto* charCount = new QLabel(QStringLiteral("0 chars \xC2\xB7 UTF-8"));
    charCount->setObjectName("charCount");
    charCount->setAlignment(Qt::AlignRight);
    charCount->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 9.5px;"
    ).arg(FG_3));
    contentsLayout->addWidget(charCount);

    propsLayout->addWidget(createCollapsibleSection("Contents", contentsWidget));

    // ── Section 5: Reply Thread (closed by default) ─────────────────────────
    auto* replyContent = new QWidget;
    auto* replyLayout = new QVBoxLayout(replyContent);
    replyLayout->setContentsMargins(12, 6, 12, 6);
    replyLayout->setSpacing(6);

    // Empty state for replies
    auto* noReplies = new QLabel(QStringLiteral("No replies yet"));
    noReplies->setAlignment(Qt::AlignCenter);
    noReplies->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 10px; padding: 12px;"
    ).arg(FG_3));
    replyLayout->addWidget(noReplies);

    // Add reply button
    auto* addReplyBtn = new QPushButton(QStringLiteral("+ Add reply"));
    addReplyBtn->setCursor(Qt::PointingHandCursor);
    addReplyBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "  color: %3;"
        "  font-family: 'Manrope', sans-serif;"
        "  font-size: 10.5px;"
        "  padding: 5px 12px;"
        "}"
        "QPushButton:hover {"
        "  background: %4;"
        "  border-color: %5;"
        "  color: %5;"
        "}"
    ).arg(BG_2, LINE, FG_1, ACCENT_DIM, ACCENT));
    replyLayout->addWidget(addReplyBtn);

    auto* replySection = createCollapsibleSection("Reply Thread", replyContent);
    // Default closed: hide the content
    replyContent->setMaximumHeight(0);
    replyContent->setVisible(false);
    propsLayout->addWidget(replySection);

    propsLayout->addStretch();
    m_propsScroll->setWidget(m_propsContent);

    return m_propsScroll;
}

// ─────────────────────────────────────────────────────────────────────────────
// Collapsible section
// ─────────────────────────────────────────────────────────────────────────────

QWidget* InspectorWidget::createCollapsibleSection(const QString& title, QWidget* content)
{
    auto* section = new QWidget;
    auto* sectionLayout = new QVBoxLayout(section);
    sectionLayout->setContentsMargins(0, 0, 0, 0);
    sectionLayout->setSpacing(0);

    // Header bar
    auto* header = new QWidget;
    header->setObjectName("sectionHeader");
    header->setFixedHeight(24);
    header->setCursor(Qt::PointingHandCursor);
    header->setStyleSheet(sectionHeaderSheet());

    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    headerLayout->setSpacing(6);

    auto* chevron = new QLabel(QStringLiteral("▾")); // ▾
    chevron->setObjectName("chevron");
    chevron->setFixedWidth(10);
    chevron->setStyleSheet(QStringLiteral("color: %1; font-size: 9px;").arg(FG_3));
    headerLayout->addWidget(chevron);

    auto* titleLabel = new QLabel(title.toUpper());
    titleLabel->setStyleSheet(QStringLiteral(
        "color: %1;"
        "font-family: 'Manrope', sans-serif;"
        "font-size: 10.5px;"
        "font-weight: 600;"
        "letter-spacing: 1.4px;"
    ).arg(FG_2));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    sectionLayout->addWidget(header);

    // Separator below header
    auto* sep = new QWidget;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("background: %1;").arg(LINE));
    sectionLayout->addWidget(sep);

    // Content area
    content->setObjectName("sectionContent");
    sectionLayout->addWidget(content);

    // Toggle on click
    header->installEventFilter(this);
    QObject::connect(header, &QObject::destroyed, []{}); // prevent warning

    // Use a lambda connected to a button click workaround via event filter on header
    // We use the section's property to track state
    section->setProperty("collapsed", !content->isVisible() || content->maximumHeight() == 0);

    // Install click handler via mouse release on header
    auto* clickFilter = new QObject(header);
    header->installEventFilter(clickFilter);
    QObject::connect(clickFilter, &QObject::destroyed, []{}); // prevent warning

    // Simple toggle approach using dynamic property
    QObject::connect(header, &QWidget::customContextMenuRequested, [](const QPoint&){});

    // Instead, use a QPushButton overlay approach for the header
    // Re-implement: make header a QPushButton styled as widget
    auto* toggleBtn = new QPushButton(header);
    toggleBtn->setGeometry(0, 0, 288, 24);
    toggleBtn->setStyleSheet("QPushButton { background: transparent; border: none; }");
    toggleBtn->setCursor(Qt::PointingHandCursor);

    QObject::connect(toggleBtn, &QPushButton::clicked, [content, chevron]() {
        bool isVisible = content->isVisible();
        content->setVisible(!isVisible);
        content->setMaximumHeight(isVisible ? 0 : 16777215);
        chevron->setText(isVisible ? QStringLiteral("▸") : QStringLiteral("▾"));
    });

    return section;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void InspectorWidget::setAnnotation(const AnnotationItem* item)
{
    m_currentAnnotation = item;
    if (!item) {
        clearAnnotation();
        return;
    }

    m_contentStack->setCurrentIndex(1);
    refreshProperties();
}

void InspectorWidget::clearAnnotation()
{
    m_currentAnnotation = nullptr;
    m_contentStack->setCurrentIndex(0);
}

void InspectorWidget::refreshProperties()
{
    if (!m_currentAnnotation) return;

    // Update header dot color
    auto* headerDot = m_headerWidget->findChild<QLabel*>("headerDot");
    if (headerDot) {
        headerDot->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 5px; min-width: 10px; min-height: 10px;"
        ).arg(m_currentAnnotation->color.name()));
    }

    // Update type name based on tool mode
    auto* typeName = m_headerWidget->findChild<QLabel*>("typeName");
    if (typeName) {
        QString name;
        switch (m_currentAnnotation->mode) {
            case ToolMode::Highlight:    name = "HIGHLIGHT"; break;
            case ToolMode::Underline:    name = "UNDERLINE"; break;
            case ToolMode::DrawFreehand: name = "FREEHAND"; break;
            case ToolMode::DrawShape:    name = "SHAPE"; break;
            case ToolMode::DrawRectangle:name = "RECTANGLE"; break;
            case ToolMode::DrawEllipse:  name = "ELLIPSE"; break;
            case ToolMode::DrawLine:     name = "LINE"; break;
            case ToolMode::DrawArrow:    name = "ARROW"; break;
            case ToolMode::AddTextBox:   name = "TEXT BOX"; break;
            case ToolMode::AddComment:   name = "COMMENT"; break;
            case ToolMode::Redact:       name = "REDACTION"; break;
            case ToolMode::AddSignature: name = "SIGNATURE"; break;
            default:                     name = "ANNOTATION"; break;
        }
        typeName->setText(name);
    }

    // Update subheader
    auto* subHeader = m_headerWidget->findChild<QLabel*>("subHeader");
    if (subHeader) {
        subHeader->setText(QStringLiteral("Page %1 \xC2\xB7 Default Layer \xC2\xB7 Unlocked")
            .arg(m_currentAnnotation->pageIndex + 1));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Document info view (page 2 of stack)
// ─────────────────────────────────────────────────────────────────────────────

QWidget* InspectorWidget::createDocInfoView()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(scrollAreaSheet());

    m_docInfoContent = new QWidget;
    m_docInfoContent->setStyleSheet(QStringLiteral("background: %1;").arg(BG_1));
    auto* layout = new QVBoxLayout(m_docInfoContent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto* header = new QWidget;
    header->setStyleSheet(QStringLiteral(
        "background: %1; border-bottom: 1px solid %2;"
    ).arg(BG_1, LINE));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(4);

    auto* headerTop = new QHBoxLayout;
    headerTop->setSpacing(6);
    auto* dot = new QLabel;
    dot->setFixedSize(10, 10);
    dot->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 5px; min-width: 10px; min-height: 10px;"
    ).arg(ACCENT));
    headerTop->addWidget(dot);

    auto* titleLabel = new QLabel(QStringLiteral("DOCUMENT"));
    titleLabel->setObjectName("docInfoTitle");
    titleLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Manrope', sans-serif;"
        "font-size: 11px; font-weight: 700; text-transform: uppercase;"
    ).arg(FG_0));
    headerTop->addWidget(titleLabel);
    headerTop->addStretch();
    headerLayout->addLayout(headerTop);

    auto* subLabel = new QLabel(QStringLiteral("No document loaded"));
    subLabel->setObjectName("docInfoSub");
    subLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 9.5px;"
    ).arg(FG_2));
    headerLayout->addWidget(subLabel);

    layout->addWidget(header);

    // Properties section
    auto* propsContent = new QWidget;
    propsContent->setObjectName("docPropsSection");
    auto* propsLayout = new QVBoxLayout(propsContent);
    propsLayout->setContentsMargins(0, 4, 0, 4);
    propsLayout->setSpacing(0);

    propsLayout->addWidget(createFieldRow("Title", "--"));
    propsLayout->addWidget(createFieldRow("Author", "--"));
    propsLayout->addWidget(createFieldRow("Subject", "--"));
    propsLayout->addWidget(createFieldRow("Creator", "--"));
    propsLayout->addWidget(createFieldRow("Producer", "--"));
    propsLayout->addWidget(createFieldRow("Created", "--"));
    propsLayout->addWidget(createFieldRow("Modified", "--"));
    propsLayout->addWidget(createFieldRow("Pages", "--"));
    propsLayout->addWidget(createFieldRow("File Size", "--"));
    propsLayout->addWidget(createFieldRow("PDF Ver.", "--"));

    layout->addWidget(createCollapsibleSection("Properties", propsContent));

    // Signatures section
    auto* sigContent = new QWidget;
    sigContent->setObjectName("docSigSection");
    auto* sigLayout = new QVBoxLayout(sigContent);
    sigLayout->setContentsMargins(12, 6, 12, 6);
    sigLayout->setSpacing(6);

    auto* noSigs = new QLabel(QStringLiteral("No signatures found"));
    noSigs->setObjectName("sigPlaceholder");
    noSigs->setAlignment(Qt::AlignCenter);
    noSigs->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 10px; padding: 12px;"
    ).arg(FG_3));
    sigLayout->addWidget(noSigs);

    layout->addWidget(createCollapsibleSection("Signatures", sigContent));

    layout->addStretch();
    scroll->setWidget(m_docInfoContent);
    return scroll;
}

// ─────────────────────────────────────────────────────────────────────────────
// Document-level API
// ─────────────────────────────────────────────────────────────────────────────

void InspectorWidget::setDocument(QPdfDocument* doc, const QString& path)
{
    m_pdfDocument = doc;
    m_documentPath = path;
    refreshDocInfo();
}

void InspectorWidget::setCurrentPage(int page)
{
    m_currentPageNum = page;
}

void InspectorWidget::showPropertiesTab()
{
    m_contentStack->setCurrentIndex(2);
    refreshDocInfo();
}

void InspectorWidget::showSignaturesTab()
{
    m_contentStack->setCurrentIndex(2);
    refreshDocInfo();
}

void InspectorWidget::refreshDocInfo()
{
    if (!m_docInfoContent) return;

    auto* subLabel = m_docInfoContent->findChild<QLabel*>("docInfoSub");
    auto* propsSection = m_docInfoContent->findChild<QWidget*>("docPropsSection");

    if (!m_pdfDocument || m_documentPath.isEmpty()) {
        if (subLabel) subLabel->setText(QStringLiteral("No document loaded"));
        return;
    }

    QFileInfo fi(m_documentPath);
    if (subLabel)
        subLabel->setText(fi.fileName());

    if (!propsSection) return;

    auto rows = propsSection->findChildren<QLabel*>();
    auto setValue = [&](const QString& label, const QString& value) {
        for (int i = 0; i < rows.size() - 1; ++i) {
            if (rows[i]->text() == label) {
                rows[i + 1]->setText(value.isEmpty() ? QStringLiteral("--") : value);
                return;
            }
        }
    };

    setValue("Title",     m_pdfDocument->metaData(QPdfDocument::MetaDataField::Title).toString());
    setValue("Author",    m_pdfDocument->metaData(QPdfDocument::MetaDataField::Author).toString());
    setValue("Subject",   m_pdfDocument->metaData(QPdfDocument::MetaDataField::Subject).toString());
    setValue("Creator",   m_pdfDocument->metaData(QPdfDocument::MetaDataField::Creator).toString());
    setValue("Producer",  m_pdfDocument->metaData(QPdfDocument::MetaDataField::Producer).toString());

    QDateTime created = m_pdfDocument->metaData(QPdfDocument::MetaDataField::CreationDate).toDateTime();
    QDateTime modified = m_pdfDocument->metaData(QPdfDocument::MetaDataField::ModificationDate).toDateTime();
    setValue("Created",   created.isValid()  ? created.toString("yyyy-MM-dd hh:mm")  : QStringLiteral("--"));
    setValue("Modified",  modified.isValid() ? modified.toString("yyyy-MM-dd hh:mm") : QStringLiteral("--"));

    setValue("Pages",     QString::number(m_pdfDocument->pageCount()));

    qint64 size = fi.size();
    QString sizeStr;
    if (size < 1024)
        sizeStr = QStringLiteral("%1 B").arg(size);
    else if (size < 1024 * 1024)
        sizeStr = QStringLiteral("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    else
        sizeStr = QStringLiteral("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
    setValue("File Size", sizeStr);

    // QPdfDocument doesn't expose PDF version directly; leave as --
}
