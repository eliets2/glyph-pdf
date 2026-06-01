#include "InspectorWidget.h"
#include "core/AnnotationTypes.h"
#include "core/AppContext.h"
#include "ui/PdfViewerWidget.h"
#include "commands/EditAnnotationCommand.h"
#include "pdfws_djot/DjotToRichTextXhtml.h"

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
#include <QDoubleSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QListWidget>
#include <QColorDialog>
#include <QKeyEvent>
#include <QUndoStack>
#include <QUuid>
#include <QToolButton>
#include <QInputDialog>
#include <QTextCursor>
#include <QLineEdit>
#include <functional>

namespace {
    const char* BG_0  = "#1a1b1e";
    const char* BG_1  = "#1e1f22";
    const char* BG_2  = "#2b2d30";
    const char* BG_3  = "#34363b";
    const char* BG_4  = "#3d4046";
    const char* LINE  = "#393b40";
    const char* FG_0  = "#dfe1e5";
    const char* FG_1  = "#a8abb0";
    const char* FG_2  = "#71747a";
    const char* FG_3  = "#52555a";
    const char* ACCENT      = "#ff8c42";
    const char* ACCENT_DIM  = "rgba(255,140,66,0.15)";
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

static QString spinBoxSheet() {
    return QStringLiteral(
        "QDoubleSpinBox, QSpinBox {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 3px;"
        "  color: %3;"
        "  font-family: 'Consolas', monospace;"
        "  font-size: 10px;"
        "  padding: 2px 4px;"
        "}"
        "QDoubleSpinBox:focus, QSpinBox:focus {"
        "  border-color: %4;"
        "}"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button,"
        "QSpinBox::up-button, QSpinBox::down-button {"
        "  width: 12px; background: %5; border: none;"
        "}"
    ).arg(BG_0, LINE, FG_0, ACCENT, BG_3);
}

static QString comboSheet() {
    return QStringLiteral(
        "QComboBox {"
        "  background: %1; border: 1px solid %2; border-radius: 3px;"
        "  color: %3; font-family: 'Consolas', monospace; font-size: 10px;"
        "  padding: 2px 6px;"
        "}"
        "QComboBox:focus { border-color: %4; }"
        "QComboBox QAbstractItemView { background: %5; color: %3; border: 1px solid %2; }"
    ).arg(BG_0, LINE, FG_0, ACCENT, BG_2);
}

static QString sliderSheet() {
    return QStringLiteral(
        "QSlider::groove:horizontal { height: 4px; background: %1; border-radius: 2px; }"
        "QSlider::handle:horizontal {"
        "  background: %2; border: none; width: 10px; height: 10px;"
        "  margin: -3px 0; border-radius: 5px;"
        "}"
        "QSlider::sub-page:horizontal { background: %2; border-radius: 2px; }"
    ).arg(BG_4, ACCENT);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: read-only field row (label + QLabel value)
// ─────────────────────────────────────────────────────────────────────────────

static QWidget* makeFieldRow(const QString& labelText, QLabel** outVal = nullptr)
{
    auto* row = new QWidget;
    auto* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(12, 3, 12, 3);
    hbox->setSpacing(8);

    auto* lbl = new QLabel(labelText);
    lbl->setFixedWidth(78);
    lbl->setStyleSheet(fieldLabelSheet());
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hbox->addWidget(lbl);

    auto* val = new QLabel(QStringLiteral("--"));
    val->setStyleSheet(fieldValueSheet());
    val->setWordWrap(true);
    hbox->addWidget(val, 1);

    if (outVal) *outVal = val;
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
    setAccessibleName(tr("Inspector"));
    setAccessibleDescription(tr("View and edit properties of the selected annotation or document"));

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    createTabs();
    mainLayout->addWidget(m_tabBar);

    m_contentStack = new QStackedWidget;
    m_contentStack->setStyleSheet(QStringLiteral("QStackedWidget { background: %1; }").arg(BG_1));

    m_contentStack->addWidget(createEmptyState());      // 0: empty
    m_contentStack->addWidget(createPropertiesView());  // 1: annotation props
    m_contentStack->addWidget(createDocInfoView());     // 2: doc info

    mainLayout->addWidget(m_contentStack, 1);
    m_contentStack->setCurrentIndex(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// init — called by Sidebar after construction to wire up viewer + ctx
// ─────────────────────────────────────────────────────────────────────────────

void InspectorWidget::init(const AppContext* ctx, PdfViewerWidget* viewer)
{
    m_ctx    = ctx;
    m_viewer = viewer;
}

// ─────────────────────────────────────────────────────────────────────────────
// pushEditCommand — wraps annotation change in an undoable command
// ─────────────────────────────────────────────────────────────────────────────

void InspectorWidget::pushEditCommand(const AnnotationItem& newAnn)
{
    if (!m_viewer || !m_ctx || !m_ctx->undoStack) return;

    // Build old list / new list with the modified annotation
    QList<AnnotationItem> oldList = m_viewer->annotations();
    QList<AnnotationItem> newList = oldList;

    // Find by id
    bool found = false;
    for (int i = 0; i < newList.size(); ++i) {
        if (newList[i].id == newAnn.id) {
            newList[i] = newAnn;
            found = true;
            break;
        }
    }
    if (!found) return;

    // Get DocumentSession* from ctx (may be null — EditAnnotationCommand handles QPointer)
    DocumentSession* docSession = m_ctx->document ? m_ctx->document.get() : nullptr;

    m_ctx->undoStack->push(
        new EditAnnotationCommand(m_viewer, docSession, oldList, newList)
    );

    // Keep local pointer in sync (annotation list was just replaced by the command)
    // Sidebar will re-call setAnnotation via selectionChanged after annotationsChanged fires
    emit annotationModified();
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

    auto* moreBtn = new QPushButton(QChar(0x22EE));
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

    auto* glyph = new QLabel(QStringLiteral("\u25C7"));
    glyph->setAlignment(Qt::AlignCenter);
    glyph->setStyleSheet(QStringLiteral("color: %1; font-size: 32px;").arg(FG_3));
    layout->addWidget(glyph);

    auto* title = new QLabel(QStringLiteral("No selection"));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral(
        "color: %1;"
        "font-family: 'Manrope', sans-serif;"
        "font-size: 11px; font-weight: 600;"
        "text-transform: uppercase; letter-spacing: 0.5px; margin-top: 8px;"
    ).arg(FG_1));
    layout->addWidget(title);

    auto* subtitle = new QLabel(QStringLiteral("Select an annotation to\ninspect its properties"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 10.5px; margin-top: 4px;"
    ).arg(FG_2));
    layout->addWidget(subtitle);

    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
// Properties view (D1–D5 fully bound)
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

    auto* headerTop = new QHBoxLayout;
    headerTop->setSpacing(6);

    auto* headerDot = new QLabel;
    headerDot->setObjectName("headerDot");
    headerDot->setFixedSize(10, 10);
    headerDot->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 5px; min-width: 10px; min-height: 10px;"
    ).arg(ACCENT));
    headerTop->addWidget(headerDot);

    auto* typeName = new QLabel(QStringLiteral("ANNOTATION"));
    typeName->setObjectName("typeName");
    typeName->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Manrope', sans-serif;"
        "font-size: 11px; font-weight: 700; text-transform: uppercase;"
    ).arg(FG_0));
    headerTop->addWidget(typeName);

    // D1: annotation ID label (member so refreshProperties can update it)
    m_valAnnotId = new QLabel(QStringLiteral("--"));
    m_valAnnotId->setObjectName("annotId");
    m_valAnnotId->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 10px;"
    ).arg(FG_2));
    headerTop->addWidget(m_valAnnotId);
    headerTop->addStretch();

    headerLayout->addLayout(headerTop);

    auto* subHeader = new QLabel(QStringLiteral("Page 1 \xC2\xB7 Default Layer \xC2\xB7 Unlocked"));
    subHeader->setObjectName("subHeader");
    subHeader->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 9.5px;"
    ).arg(FG_2));
    headerLayout->addWidget(subHeader);
    propsLayout->addWidget(m_headerWidget);

    // ── Section 1: Identity (D1) ─────────────────────────────────────────
    auto* identityContent = new QWidget;
    auto* identityLayout = new QVBoxLayout(identityContent);
    identityLayout->setContentsMargins(0, 4, 0, 4);
    identityLayout->setSpacing(0);

    identityLayout->addWidget(makeFieldRow(tr("Author"),   &m_valAuthor));
    identityLayout->addWidget(makeFieldRow(tr("Created"),  &m_valCreated));
    identityLayout->addWidget(makeFieldRow(tr("Modified"), &m_valModified));
    identityLayout->addWidget(makeFieldRow(tr("Subject"),  &m_valSubject));

    propsLayout->addWidget(createCollapsibleSection(tr("Identity"), identityContent));

    // ── Section 2: Appearance (D3) ─────────────────────────────────────────
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

    const QStringList swatchColors = {"#f5c542", "#42a5f5", "#ef5350", "#66bb6a", "#ab47bc", "#ff8c42"};
    for (const auto& c : swatchColors) {
        auto* sw = createColorSwatch(c);
        QColor col(c);
        // D3: clicking a swatch sets annotation.color + pushes EditAnnotationCommand
        QObject::connect(sw, &QWidget::destroyed, []{}); // placeholder — real click via button overlay
        auto* overlayBtn = new QPushButton(sw);
        overlayBtn->setGeometry(0, 0, 22, 18);
        overlayBtn->setStyleSheet("QPushButton { background: transparent; border: none; }");
        overlayBtn->setCursor(Qt::PointingHandCursor);
        QObject::connect(overlayBtn, &QPushButton::clicked, this, [this, col]() {
            if (!m_currentAnnotation) return;
            AnnotationItem modified = *m_currentAnnotation;
            modified.color = col;
            pushEditCommand(modified);
        });
        swatchRow->addWidget(sw);
    }

    // "more..." custom color
    auto* moreColor = new QPushButton(QStringLiteral("\u2026"));
    moreColor->setFixedSize(22, 18);
    moreColor->setToolTip(tr("Custom color"));
    moreColor->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 3px; color: %3; font-size: 10px; }"
        "QPushButton:hover { background: %4; }"
    ).arg(BG_2, LINE, FG_1, BG_3));
    QObject::connect(moreColor, &QPushButton::clicked, this, [this]() {
        if (!m_currentAnnotation) return;
        QColor chosen = QColorDialog::getColor(m_currentAnnotation->color, this, tr("Choose annotation color"));
        if (chosen.isValid()) {
            AnnotationItem modified = *m_currentAnnotation;
            modified.color = chosen;
            pushEditCommand(modified);
        }
    });
    swatchRow->addWidget(moreColor);
    swatchRow->addStretch();
    appearLayout->addLayout(swatchRow);

    // Opacity row: QSlider 0–100
    auto* opacRow = new QHBoxLayout;
    opacRow->setSpacing(8);
    auto* opacLabel = new QLabel(QStringLiteral("Opacity"));
    opacLabel->setFixedWidth(78);
    opacLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    opacLabel->setStyleSheet(fieldLabelSheet());
    opacRow->addWidget(opacLabel);

    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(100);
    m_opacitySlider->setStyleSheet(sliderSheet());
    opacRow->addWidget(m_opacitySlider, 1);

    m_opacityLabel = new QLabel(QStringLiteral("100%"));
    m_opacityLabel->setStyleSheet(fieldValueSheet());
    m_opacityLabel->setFixedWidth(32);
    opacRow->addWidget(m_opacityLabel);
    appearLayout->addLayout(opacRow);

    QObject::connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_opacityLabel->setText(QStringLiteral("%1%").arg(val));
        if (m_refreshing || !m_currentAnnotation) return;
        AnnotationItem modified = *m_currentAnnotation;
        modified.opacity = val / 100.0;
        pushEditCommand(modified);
    });

    // Blend mode combo
    auto* blendRow = new QHBoxLayout;
    blendRow->setSpacing(8);
    auto* blendLabel = new QLabel(QStringLiteral("Blend"));
    blendLabel->setFixedWidth(78);
    blendLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    blendLabel->setStyleSheet(fieldLabelSheet());
    blendRow->addWidget(blendLabel);

    m_blendCombo = new QComboBox;
    m_blendCombo->setStyleSheet(comboSheet());
    m_blendCombo->addItems({"Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten"});
    blendRow->addWidget(m_blendCombo, 1);
    appearLayout->addLayout(blendRow);

    QObject::connect(m_blendCombo, &QComboBox::currentTextChanged, this, [this](const QString& mode) {
        if (m_refreshing || !m_currentAnnotation) return;
        AnnotationItem modified = *m_currentAnnotation;
        modified.blendMode = mode;
        pushEditCommand(modified);
    });

    // Border width spinbox
    auto* borderRow = new QHBoxLayout;
    borderRow->setSpacing(8);
    auto* borderLabel = new QLabel(QStringLiteral("Border"));
    borderLabel->setFixedWidth(78);
    borderLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    borderLabel->setStyleSheet(fieldLabelSheet());
    borderRow->addWidget(borderLabel);

    m_borderSpin = new QSpinBox;
    m_borderSpin->setRange(0, 20);
    m_borderSpin->setValue(2);
    m_borderSpin->setSuffix(QStringLiteral("px"));
    m_borderSpin->setStyleSheet(spinBoxSheet());
    borderRow->addWidget(m_borderSpin);
    borderRow->addStretch();
    appearLayout->addLayout(borderRow);

    QObject::connect(m_borderSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) {
        if (m_refreshing || !m_currentAnnotation) return;
        AnnotationItem modified = *m_currentAnnotation;
        modified.thickness = val;
        pushEditCommand(modified);
    });

    propsLayout->addWidget(createCollapsibleSection(tr("Appearance"), appearContent));

    // ── Section 3: Position (D2) ────────────────────────────────────────────
    auto* posContent = new QWidget;
    auto* posLayout = new QVBoxLayout(posContent);
    posLayout->setContentsMargins(12, 6, 12, 6);
    posLayout->setSpacing(6);

    auto* posGrid = new QGridLayout;
    posGrid->setSpacing(4);
    posGrid->setColumnStretch(1, 1);
    posGrid->setColumnStretch(3, 1);

    auto makeGridLabel = [](const QString& text) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(fieldLabelSheet());
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return lbl;
    };

    auto makeGeomSpin = [this]() {
        auto* sp = new QDoubleSpinBox;
        sp->setRange(-9999.0, 9999.0);
        sp->setDecimals(2);
        sp->setSingleStep(1.0);
        sp->setStyleSheet(spinBoxSheet());
        return sp;
    };

    m_spinX = makeGeomSpin();
    m_spinY = makeGeomSpin();
    m_spinW = makeGeomSpin();
    m_spinH = makeGeomSpin();

    posGrid->addWidget(makeGridLabel("X"), 0, 0);
    posGrid->addWidget(m_spinX, 0, 1);
    posGrid->addWidget(makeGridLabel("Y"), 0, 2);
    posGrid->addWidget(m_spinY, 0, 3);
    posGrid->addWidget(makeGridLabel("W"), 1, 0);
    posGrid->addWidget(m_spinW, 1, 1);
    posGrid->addWidget(makeGridLabel("H"), 1, 2);
    posGrid->addWidget(m_spinH, 1, 3);
    posLayout->addLayout(posGrid);

    // Wire geometry spinboxes
    auto wireGeomSpin = [this](QDoubleSpinBox* sp) {
        QObject::connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                         this, [this](double) {
            if (m_refreshing || !m_currentAnnotation) return;
            AnnotationItem modified = *m_currentAnnotation;
            modified.rect = QRectF(m_spinX->value(), m_spinY->value(),
                                   m_spinW->value(), m_spinH->value());
            pushEditCommand(modified);
        });
    };
    wireGeomSpin(m_spinX);
    wireGeomSpin(m_spinY);
    wireGeomSpin(m_spinW);
    wireGeomSpin(m_spinH);

    // Align buttons row
    auto* alignRow = new QHBoxLayout;
    alignRow->setSpacing(2);

    const QStringList alignIcons = {"\u2523", "\u2502", "\u252B", "\u2501", "\u2500", "\u2501"};
    const QStringList alignTips  = {"Align Left", "Align Center", "Align Right",
                                    "Align Top",  "Align Middle", "Align Bottom"};

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

    propsLayout->addWidget(createCollapsibleSection(tr("Position"), posContent));

    // ── Section 4: Contents (D4 / M6-P4 Djot editor) ─────────────────────────
    auto* contentsWidget = new QWidget;
    auto* contentsLayout = new QVBoxLayout(contentsWidget);
    contentsLayout->setContentsMargins(12, 6, 12, 6);
    contentsLayout->setSpacing(4);

    // ── Djot formatting toolbar (bold / italic / code / link / list / heading)
    auto* djotToolbar = new QWidget;
    auto* toolbarRow = new QHBoxLayout(djotToolbar);
    toolbarRow->setContentsMargins(0, 0, 0, 0);
    toolbarRow->setSpacing(2);

    const QString toolBtnSheet = QStringLiteral(
        "QToolButton {"
        "  background: %1; border: 1px solid %2; border-radius: 3px;"
        "  color: %3; font-family: 'Consolas', monospace; font-size: 11px;"
        "  min-width: 22px; min-height: 20px; padding: 0 4px;"
        "}"
        "QToolButton:hover { background: %4; color: %5; }"
    ).arg(BG_2, LINE, FG_1, ACCENT_DIM, ACCENT);

    auto addToolBtn = [&](const QString& label, const QString& tip,
                          std::function<void()> handler) {
        auto* b = new QToolButton;
        b->setText(label);
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(toolBtnSheet);
        QObject::connect(b, &QToolButton::clicked, this, handler);
        toolbarRow->addWidget(b);
        return b;
    };

    addToolBtn(QStringLiteral("B"), tr("Bold (**strong**)"),
               [this]() { wrapSelection(QStringLiteral("**"), QStringLiteral("**")); });
    addToolBtn(QStringLiteral("I"), tr("Italic (_emphasis_)"),
               [this]() { wrapSelection(QStringLiteral("_"), QStringLiteral("_")); });
    addToolBtn(QStringLiteral("</>"), tr("Inline code (`code`)"),
               [this]() { wrapSelection(QStringLiteral("`"), QStringLiteral("`")); });
    addToolBtn(QStringLiteral("\xF0\x9F\x94\x97"), tr("Link ([text](url))"),
               [this]() {
        bool ok = false;
        const QString url = QInputDialog::getText(this, tr("Insert link"),
                                                  tr("URL:"), QLineEdit::Normal,
                                                  QStringLiteral("https://"), &ok);
        if (ok && !url.isEmpty())
            wrapSelection(QStringLiteral("["), QStringLiteral("](") + url + QStringLiteral(")"));
    });
    addToolBtn(QStringLiteral("\xE2\x80\xA2"), tr("List item (- )"),
               [this]() { insertLinePrefix(QStringLiteral("- ")); });
    addToolBtn(QStringLiteral("H"), tr("Heading (# )"),
               [this]() { insertLinePrefix(QStringLiteral("# ")); });
    toolbarRow->addStretch();
    contentsLayout->addWidget(djotToolbar);

    // ── Djot source editor ──────────────────────────────────────────────────
    m_contentsEditor = new QTextEdit;
    m_contentsEditor->setFixedHeight(72);
    m_contentsEditor->setAcceptRichText(false);
    m_contentsEditor->setPlaceholderText(
        QStringLiteral("Djot source — *bold*, _italic_, `code`, [link](url)..."));
    m_contentsEditor->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background: %1; border: 1px solid %2; border-radius: 4px;"
        "  color: %3; font-family: 'Consolas', monospace; font-size: 10.5px; padding: 6px;"
        "}"
        "QTextEdit:focus { border-color: %4; }"
    ).arg(BG_0, LINE, FG_0, ACCENT));
    contentsLayout->addWidget(m_contentsEditor);

    // ── Live preview label + read-only render pane ──────────────────────────
    auto* previewLabel = new QLabel(tr("PREVIEW"));
    previewLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Manrope', sans-serif; font-size: 9px;"
        "font-weight: 600; letter-spacing: 1px;"
    ).arg(FG_3));
    contentsLayout->addWidget(previewLabel);

    m_djotPreview = new QTextEdit;
    m_djotPreview->setReadOnly(true);
    m_djotPreview->setFixedHeight(64);
    m_djotPreview->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background: %1; border: 1px dashed %2; border-radius: 4px;"
        "  color: %3; font-family: 'Manrope', sans-serif; font-size: 10.5px; padding: 6px;"
        "}"
    ).arg(BG_2, LINE, FG_0));
    contentsLayout->addWidget(m_djotPreview);

    m_charCountLabel = new QLabel(QStringLiteral("0 chars \xC2\xB7 Djot \xC2\xB7 UTF-8"));
    m_charCountLabel->setObjectName("charCount");
    m_charCountLabel->setAlignment(Qt::AlignRight);
    m_charCountLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Consolas', monospace; font-size: 9.5px;"
    ).arg(FG_3));
    contentsLayout->addWidget(m_charCountLabel);

    // Live char count + live HTML preview as the user types Djot.
    QObject::connect(m_contentsEditor, &QTextEdit::textChanged, this, [this]() {
        if (m_refreshing) return;
        int count = m_contentsEditor->toPlainText().length();
        m_charCountLabel->setText(
            QStringLiteral("%1 chars \xC2\xB7 Djot \xC2\xB7 UTF-8").arg(count));
        refreshDjotPreview();
    });

    // Save on focus loss (install event filter on the editor)
    m_contentsEditor->installEventFilter(this);

    propsLayout->addWidget(createCollapsibleSection(tr("Contents"), contentsWidget));

    // ── Section 5: Reply Thread (D5) ────────────────────────────────────────
    auto* replyContent = new QWidget;
    auto* replyLayout = new QVBoxLayout(replyContent);
    replyLayout->setContentsMargins(8, 6, 8, 6);
    replyLayout->setSpacing(4);

    m_replyList = new QListWidget;
    m_replyList->setMaximumHeight(120);
    m_replyList->setStyleSheet(QStringLiteral(
        "QListWidget { background: %1; border: 1px solid %2; border-radius: 4px; color: %3;"
        "  font-family: 'Consolas', monospace; font-size: 10px; }"
        "QListWidget::item { padding: 4px 6px; border-bottom: 1px solid %4; }"
        "QListWidget::item:selected { background: %5; }"
    ).arg(BG_0, LINE, FG_1, BG_2, ACCENT_DIM));
    replyLayout->addWidget(m_replyList);

    // Inline reply editor (hidden by default)
    m_replyEditorWidget = new QWidget;
    m_replyEditorWidget->hide();
    auto* replyEdLayout = new QVBoxLayout(m_replyEditorWidget);
    replyEdLayout->setContentsMargins(0, 4, 0, 0);
    replyEdLayout->setSpacing(4);

    m_replyEditor = new QTextEdit;
    m_replyEditor->setFixedHeight(56);
    m_replyEditor->setPlaceholderText(tr("Type reply..."));
    m_replyEditor->setStyleSheet(QStringLiteral(
        "QTextEdit { background: %1; border: 1px solid %2; border-radius: 4px;"
        "  color: %3; font-family: 'Consolas', monospace; font-size: 10px; padding: 4px; }"
        "QTextEdit:focus { border-color: %4; }"
    ).arg(BG_0, LINE, FG_0, ACCENT));
    replyEdLayout->addWidget(m_replyEditor);

    auto* replyBtnRow = new QHBoxLayout;
    replyBtnRow->addStretch();
    m_replySubmit = new QPushButton(tr("Post Reply"));
    m_replySubmit->setCursor(Qt::PointingHandCursor);
    m_replySubmit->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 4px;"
        "  color: %2; font-family: 'Manrope', sans-serif; font-size: 10.5px; padding: 4px 10px; }"
        "QPushButton:hover { background: %3; }"
    ).arg(ACCENT_DIM, ACCENT, BG_3));
    replyBtnRow->addWidget(m_replySubmit);

    auto* cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: 1px solid %1; border-radius: 4px;"
        "  color: %1; font-family: 'Manrope', sans-serif; font-size: 10.5px; padding: 4px 10px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(FG_2, BG_2));
    replyBtnRow->addWidget(cancelBtn);
    replyEdLayout->addLayout(replyBtnRow);
    replyLayout->addWidget(m_replyEditorWidget);

    // "Add Reply" button
    auto* addReplyBtn = new QPushButton(QStringLiteral("+ Add reply"));
    addReplyBtn->setCursor(Qt::PointingHandCursor);
    addReplyBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; border-radius: 4px;"
        "  color: %3; font-family: 'Manrope', sans-serif; font-size: 10.5px; padding: 5px 12px; }"
        "QPushButton:hover { background: %4; border-color: %5; color: %5; }"
    ).arg(BG_2, LINE, FG_1, ACCENT_DIM, ACCENT));
    replyLayout->addWidget(addReplyBtn);

    // Wire Add reply to show inline editor
    QObject::connect(addReplyBtn, &QPushButton::clicked, this, [this]() {
        m_replyEditorWidget->setVisible(true);
        m_replyEditor->setFocus();
    });

    QObject::connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        m_replyEditorWidget->hide();
        m_replyEditor->clear();
    });

    // Wire Post Reply — creates a child AnnotationItem with parentId
    QObject::connect(m_replySubmit, &QPushButton::clicked, this, [this]() {
        if (!m_currentAnnotation || !m_viewer) return;
        QString replyText = m_replyEditor->toPlainText().trimmed();
        if (replyText.isEmpty()) return;

        AnnotationItem reply;
        reply.id           = QUuid::createUuid().toString(QUuid::WithoutBraces);
        reply.parentId     = m_currentAnnotation->id;
        reply.mode         = ToolMode::AddComment;
        reply.pageIndex    = m_currentAnnotation->pageIndex;
        // M6-P4: reply text is (trivial) Djot source; text is the plain
        // projection. Dual-write to /RC + /PieceInfo happens on save.
        reply.djotSource   = replyText;
        reply.text         = pdfws_djot::djotToPlainText(replyText);
        reply.creationDate = QDateTime::currentDateTime().toString(Qt::ISODate);
        reply.color        = m_currentAnnotation->color;

        // Add reply.id to parent.replies + append reply annotation
        QList<AnnotationItem> oldList = m_viewer->annotations();
        QList<AnnotationItem> newList = oldList;
        for (auto& ann : newList) {
            if (ann.id == m_currentAnnotation->id) {
                ann.replies.append(reply.id);
                break;
            }
        }
        newList.append(reply);

        DocumentSession* docSession = m_ctx ? (m_ctx->document ? m_ctx->document.get() : nullptr) : nullptr;
        if (m_ctx && m_ctx->undoStack) {
            m_ctx->undoStack->push(new EditAnnotationCommand(m_viewer, docSession, oldList, newList));
        } else {
            m_viewer->setAnnotations(newList);
        }

        m_replyEditor->clear();
        m_replyEditorWidget->hide();

        // Refresh the reply list immediately
        // The selectionChanged → setAnnotation path will also fire, but refresh directly too
        if (m_currentAnnotation) {
            const auto& updated = newList;
            for (const auto& ann : updated) {
                if (ann.id == m_currentAnnotation->id) {
                    // Rebuild display
                    m_replyList->clear();
                    const auto& allAnns = newList;
                    for (const QString& rid : ann.replies) {
                        for (const auto& r : allAnns) {
                            if (r.id == rid) {
                                auto* item = new QListWidgetItem(
                                    QStringLiteral("    %1\n    %2").arg(r.text, r.creationDate));
                                m_replyList->addItem(item);
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }

        emit annotationModified();
    });

    // Default closed
    replyContent->setMaximumHeight(0);
    replyContent->setVisible(false);
    propsLayout->addWidget(createCollapsibleSection(tr("Reply Thread"), replyContent));

    propsLayout->addStretch();
    m_propsScroll->setWidget(m_propsContent);
    return m_propsScroll;
}

// ─────────────────────────────────────────────────────────────────────────────
// eventFilter — save Contents on focus-out (D4)
// ─────────────────────────────────────────────────────────────────────────────

bool InspectorWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_contentsEditor && event->type() == QEvent::FocusOut) {
        commitDjotEdit();
    }
    return QWidget::eventFilter(obj, event);
}

// ─────────────────────────────────────────────────────────────────────────────
// M6-P4 D2 — Djot editor helpers
// ─────────────────────────────────────────────────────────────────────────────

// Commit the Djot source in the editor: write djotSource and auto-derive the
// plain-text projection into annotation.text. Pushed as one undoable edit.
void InspectorWidget::commitDjotEdit()
{
    if (!m_currentAnnotation || m_refreshing) return;

    const QString newDjot = m_contentsEditor->toPlainText();
    const QString newPlain = pdfws_djot::djotToPlainText(newDjot);

    if (newDjot == m_currentAnnotation->djotSource
        && newPlain == m_currentAnnotation->text) {
        return;  // nothing changed
    }

    AnnotationItem modified = *m_currentAnnotation;
    modified.djotSource = newDjot;
    modified.text       = newPlain;   // plain-text projection kept in sync
    pushEditCommand(modified);
}

// Re-render the live HTML preview pane from the current Djot source.
void InspectorWidget::refreshDjotPreview()
{
    if (!m_djotPreview) return;
    const QString djot = m_contentsEditor ? m_contentsEditor->toPlainText() : QString();
    if (djot.trimmed().isEmpty()) {
        m_djotPreview->clear();
        return;
    }
    // Qt rich text understands the XHTML body fragment our transcoder emits.
    m_djotPreview->setHtml(pdfws_djot::djotToHtmlFragment(djot));
}

// Wrap the current selection (or insert at cursor) with prefix/suffix markup.
void InspectorWidget::wrapSelection(const QString& prefix, const QString& suffix)
{
    if (!m_contentsEditor) return;
    QTextCursor cur = m_contentsEditor->textCursor();
    const QString selected = cur.selectedText();
    cur.insertText(prefix + selected + suffix);
    if (selected.isEmpty()) {
        // Park the cursor between the delimiters for immediate typing.
        cur.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, suffix.length());
        m_contentsEditor->setTextCursor(cur);
    }
    m_contentsEditor->setFocus();
}

// Prepend a line-level marker (e.g. "- " or "# ") to the cursor's current line.
void InspectorWidget::insertLinePrefix(const QString& prefix)
{
    if (!m_contentsEditor) return;
    QTextCursor cur = m_contentsEditor->textCursor();
    cur.movePosition(QTextCursor::StartOfLine);
    cur.insertText(prefix);
    m_contentsEditor->setFocus();
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

    auto* header = new QWidget;
    header->setObjectName("sectionHeader");
    header->setFixedHeight(24);
    header->setCursor(Qt::PointingHandCursor);
    header->setStyleSheet(sectionHeaderSheet());

    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    headerLayout->setSpacing(6);

    auto* chevron = new QLabel(QStringLiteral("\u25BE"));
    chevron->setObjectName("chevron");
    chevron->setFixedWidth(10);
    chevron->setStyleSheet(QStringLiteral("color: %1; font-size: 9px;").arg(FG_3));
    headerLayout->addWidget(chevron);

    auto* titleLabel = new QLabel(title.toUpper());
    titleLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-family: 'Manrope', sans-serif;"
        "font-size: 10.5px; font-weight: 600; letter-spacing: 1.4px;"
    ).arg(FG_2));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    sectionLayout->addWidget(header);

    auto* sep = new QWidget;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("background: %1;").arg(LINE));
    sectionLayout->addWidget(sep);

    content->setObjectName("sectionContent");
    sectionLayout->addWidget(content);

    section->setProperty("collapsed",
        !content->isVisible() || content->maximumHeight() == 0);

    auto* toggleBtn = new QPushButton(header);
    toggleBtn->setGeometry(0, 0, 288, 24);
    toggleBtn->setStyleSheet("QPushButton { background: transparent; border: none; }");
    toggleBtn->setCursor(Qt::PointingHandCursor);

    QObject::connect(toggleBtn, &QPushButton::clicked, [content, chevron]() {
        bool isVisible = content->isVisible();
        content->setVisible(!isVisible);
        content->setMaximumHeight(isVisible ? 0 : 16777215);
        chevron->setText(isVisible ? QStringLiteral("\u25B8") : QStringLiteral("\u25BE"));
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
    m_refreshing = true;  // block re-entrant signal→pushEditCommand loops

    const AnnotationItem& ann = *m_currentAnnotation;

    // ── Header dot color ──────────────────────────────────────────────────
    if (auto* dot = m_headerWidget->findChild<QLabel*>("headerDot"))
        dot->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 5px; min-width: 10px; min-height: 10px;"
        ).arg(ann.color.name()));

    // ── Type name ─────────────────────────────────────────────────────────
    if (auto* typeName = m_headerWidget->findChild<QLabel*>("typeName")) {
        QString name;
        switch (ann.mode) {
            case ToolMode::Highlight:     name = "HIGHLIGHT";  break;
            case ToolMode::Underline:     name = "UNDERLINE";  break;
            case ToolMode::DrawFreehand:  name = "FREEHAND";   break;
            case ToolMode::DrawShape:     name = "SHAPE";      break;
            case ToolMode::DrawRectangle: name = "RECTANGLE";  break;
            case ToolMode::DrawEllipse:   name = "ELLIPSE";    break;
            case ToolMode::DrawLine:      name = "LINE";        break;
            case ToolMode::DrawArrow:     name = "ARROW";      break;
            case ToolMode::AddTextBox:    name = "TEXT BOX";   break;
            case ToolMode::AddComment:    name = "COMMENT";    break;
            case ToolMode::Redact:        name = "REDACTION";  break;
            case ToolMode::AddSignature:  name = "SIGNATURE";  break;
            default:                      name = "ANNOTATION"; break;
        }
        typeName->setText(name);
    }

    // ── D1: Identity fields ───────────────────────────────────────────────
    // Annotation ID (short form)
    if (m_valAnnotId)
        m_valAnnotId->setText(ann.id.isEmpty() ? QStringLiteral("--")
                                               : QStringLiteral("#%1").arg(ann.id.left(8)));

    if (m_valAuthor)
        m_valAuthor->setText(ann.author.isEmpty() ? QStringLiteral("Unknown") : ann.author);

    if (m_valCreated)
        m_valCreated->setText(ann.creationDate.isEmpty() ? QStringLiteral("--") : ann.creationDate);

    if (m_valModified)
        m_valModified->setText(ann.modificationDate.isEmpty() ? QStringLiteral("--")
                                                               : ann.modificationDate);

    if (m_valSubject)
        m_valSubject->setText(QStringLiteral("--"));  // Not yet in AnnotationItem spec

    // ── Sub-header: page / layer / lock ──────────────────────────────────
    if (auto* subHeader = m_headerWidget->findChild<QLabel*>("subHeader"))
        subHeader->setText(QStringLiteral("Page %1 \xC2\xB7 Default Layer \xC2\xB7 %2")
            .arg(ann.pageIndex + 1)
            .arg(ann.locked ? tr("Locked") : tr("Unlocked")));

    // ── D2: Geometry spinboxes ────────────────────────────────────────────
    if (m_spinX) m_spinX->setValue(ann.rect.x());
    if (m_spinY) m_spinY->setValue(ann.rect.y());
    if (m_spinW) m_spinW->setValue(ann.rect.width());
    if (m_spinH) m_spinH->setValue(ann.rect.height());

    // ── D3: Appearance controls ───────────────────────────────────────────
    if (m_opacitySlider) {
        int pct = qRound(ann.opacity * 100.0);
        m_opacitySlider->setValue(pct);
        if (m_opacityLabel) m_opacityLabel->setText(QStringLiteral("%1%").arg(pct));
    }

    if (m_blendCombo) {
        int idx = m_blendCombo->findText(ann.blendMode);
        m_blendCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    if (m_borderSpin) m_borderSpin->setValue(ann.thickness);

    // ── D4 / M6-P4: Djot source editor + live preview ─────────────────────
    if (m_contentsEditor) {
        // Show djotSource when present; otherwise treat the plain text as the
        // (trivial) Djot source so older annotations remain editable.
        const QString djot = ann.djotSource.isEmpty() ? ann.text : ann.djotSource;
        m_contentsEditor->blockSignals(true);
        m_contentsEditor->setPlainText(djot);
        m_contentsEditor->blockSignals(false);
        if (m_charCountLabel)
            m_charCountLabel->setText(
                QStringLiteral("%1 chars \xC2\xB7 Djot \xC2\xB7 UTF-8").arg(djot.length()));
        refreshDjotPreview();
    }

    // ── D5: Reply thread ──────────────────────────────────────────────────
    if (m_replyList) {
        m_replyList->clear();
        const QList<AnnotationItem> allAnns = m_viewer ? m_viewer->annotations()
                                                       : QList<AnnotationItem>{};
        if (ann.replies.isEmpty()) {
            auto* placeholder = new QListWidgetItem(tr("No replies yet"));
            placeholder->setForeground(QColor(FG_3));
            m_replyList->addItem(placeholder);
        } else {
            for (const QString& rid : ann.replies) {
                for (const auto& r : allAnns) {
                    if (r.id == rid) {
                        QString display = QStringLiteral("[%1]  %2").arg(
                            r.creationDate.isEmpty() ? QStringLiteral("--") : r.creationDate,
                            r.text);
                        auto* item = new QListWidgetItem(display);
                        item->setForeground(QColor(FG_1));
                        m_replyList->addItem(item);
                        break;
                    }
                }
            }
        }
    }

    m_refreshing = false;
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

    // Use makeFieldRow with dummy QLabel* capture (we'll findChild by label text in refreshDocInfo)
    const QStringList fields = {"Title", "Author", "Subject", "Creator",
                                "Producer", "Created", "Modified", "Pages",
                                "File Size", "PDF Ver."};
    for (const QString& f : fields)
        propsLayout->addWidget(makeFieldRow(f));

    layout->addWidget(createCollapsibleSection(tr("Properties"), propsContent));

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

    layout->addWidget(createCollapsibleSection(tr("Signatures"), sigContent));
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

    QDateTime created  = m_pdfDocument->metaData(QPdfDocument::MetaDataField::CreationDate).toDateTime();
    QDateTime modified = m_pdfDocument->metaData(QPdfDocument::MetaDataField::ModificationDate).toDateTime();
    setValue("Created",  created.isValid()  ? created.toString("yyyy-MM-dd hh:mm")  : QStringLiteral("--"));
    setValue("Modified", modified.isValid() ? modified.toString("yyyy-MM-dd hh:mm") : QStringLiteral("--"));
    setValue("Pages",    QString::number(m_pdfDocument->pageCount()));

    qint64 size = fi.size();
    QString sizeStr;
    if (size < 1024)
        sizeStr = QStringLiteral("%1 B").arg(size);
    else if (size < 1024 * 1024)
        sizeStr = QStringLiteral("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    else
        sizeStr = QStringLiteral("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
    setValue("File Size", sizeStr);
}
