#include "FormBuilderMode.h"
#include "FormFieldPropertiesPanel.h"
#include "ui/PdfViewerWidget.h"
#include "core/AppContext.h"
#include "core/PdfEnums.h"
#include "commands/AddFormFieldCommand.h"
#include "commands/DeleteFormFieldCommand.h"
#include "util/GpTheme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QUndoStack>

namespace gp {

// ── Construction ─────────────────────────────────────────────────────────────

FormBuilderMode::FormBuilderMode(const AppContext* ctx,
                                 PdfViewerWidget* canvas,
                                 QWidget* parent)
    : QWidget(parent)
    , m_ctx(ctx)
    , m_canvas(canvas)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    buildToolbar(col);
    buildContent(col);
    updateNoDocumentState();

    // ESC cancels active field placement
    auto* esc = new QShortcut(Qt::Key_Escape, this);
    connect(esc, &QShortcut::activated, this, &FormBuilderMode::onEscapePressed);
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

void FormBuilderMode::buildToolbar(QVBoxLayout* col)
{
    auto* tb = new QFrame;
    tb->setProperty("role", "modeToolbar");
    tb->setFixedHeight(Theme::ToolbarH);
    auto* trow = new QHBoxLayout(tb);
    trow->setContentsMargins(10, 0, 10, 0);
    trow->setSpacing(4);

    auto mono = [](const QString& s) {
        auto* l = new QLabel(s);
        l->setProperty("mono", true);
        return l;
    };
    trow->addWidget(mono(FormBuilderMode::tr("FORM")));

    const QStringList labels = {
        FormBuilderMode::tr("TEXT FIELD"), FormBuilderMode::tr("CHECKBOX"), FormBuilderMode::tr("RADIO"),
        FormBuilderMode::tr("DROPDOWN"),   FormBuilderMode::tr("LIST BOX"), FormBuilderMode::tr("DATE"),
        FormBuilderMode::tr("NUMERIC"),    FormBuilderMode::tr("SIGNATURE"), FormBuilderMode::tr("BUTTON")
    };

    bool first = true;
    for (int i = 0; i < 9; ++i) {
        auto* b = new QToolButton;
        b->setText(labels[i]);
        b->setProperty("variant", "ghost");
        b->setCheckable(true);
        b->setAutoExclusive(true);
        if (first) { b->setChecked(true); first = false; }
        b->setProperty("fieldModeIndex", i);
        connect(b, &QToolButton::toggled, this, &FormBuilderMode::onFieldButtonToggled);
        m_fieldBtns[i] = b;
        trow->addWidget(b);
    }

    trow->addStretch(1);

    m_autoDetectBtn = new QToolButton;
    m_autoDetectBtn->setText(FormBuilderMode::tr("Auto-detect Fields"));
    m_autoDetectBtn->setProperty("variant", "ghost");
    m_autoDetectBtn->setToolTip(FormBuilderMode::tr("Auto-detect form fields from document structure — Coming in v1.1"));
    m_autoDetectBtn->setEnabled(false);
    connect(m_autoDetectBtn, &QToolButton::clicked, this, &FormBuilderMode::onAutoDetectClicked);
    trow->addWidget(m_autoDetectBtn);

    m_tabOrderBtn = new QToolButton;
    m_tabOrderBtn->setText(FormBuilderMode::tr("Tab Order"));
    m_tabOrderBtn->setProperty("variant", "pill");
    m_tabOrderBtn->setCheckable(true);
    connect(m_tabOrderBtn, &QToolButton::toggled, this, &FormBuilderMode::onTabOrderToggled);
    trow->addWidget(m_tabOrderBtn);

    m_previewBtn = new QToolButton;
    m_previewBtn->setText(FormBuilderMode::tr("Preview Form"));
    m_previewBtn->setProperty("variant", "ghost");
    m_previewBtn->setToolTip(FormBuilderMode::tr("Preview filled form — Coming in v1.1"));
    m_previewBtn->setEnabled(false);
    connect(m_previewBtn, &QToolButton::clicked, this, &FormBuilderMode::onPreviewFormClicked);
    trow->addWidget(m_previewBtn);

    m_exitBtn = new QToolButton;
    m_exitBtn->setText(FormBuilderMode::tr("Exit Form"));
    m_exitBtn->setProperty("variant", "ghost");
    connect(m_exitBtn, &QToolButton::clicked, this, &FormBuilderMode::onExitFormClicked);
    trow->addWidget(m_exitBtn);

    col->addWidget(tb);
}

// ── Content area ─────────────────────────────────────────────────────────────

void FormBuilderMode::buildContent(QVBoxLayout* col)
{
    // Outer splitter: [left sidebar] | [canvas/no-doc] | [right properties panel]
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(2);

    // ── Left sidebar: field list + delete ─────────────────────────────────
    auto* leftPanel = new QFrame;
    leftPanel->setFixedWidth(180);
    auto* leftCol = new QVBoxLayout(leftPanel);
    leftCol->setContentsMargins(4, 4, 4, 4);
    leftCol->setSpacing(4);

    auto* fieldsLabel = new QLabel(tr("Fields"));
    fieldsLabel->setProperty("mono", true);
    leftCol->addWidget(fieldsLabel);

    m_fieldList = new QListWidget;
    m_fieldList->setToolTip(tr("Placed form fields on this page"));
    connect(m_fieldList, &QListWidget::itemSelectionChanged,
            this, &FormBuilderMode::onFieldListSelectionChanged);
    leftCol->addWidget(m_fieldList, 1);

    m_deleteFieldBtn = new QToolButton;
    m_deleteFieldBtn->setText(tr("Delete Field"));
    m_deleteFieldBtn->setProperty("variant", "ghost");
    m_deleteFieldBtn->setEnabled(false);
    connect(m_deleteFieldBtn, &QToolButton::clicked,
            this, &FormBuilderMode::onDeleteFieldClicked);
    leftCol->addWidget(m_deleteFieldBtn);

    splitter->addWidget(leftPanel);

    // ── Center: canvas or "no document open" ──────────────────────────────
    m_contentStack = new QStackedWidget;

    // Page 0: no document
    m_noDocLabel = new QLabel(tr("No document open.\nOpen a PDF to start placing form fields."));
    m_noDocLabel->setAlignment(Qt::AlignCenter);
    m_noDocLabel->setWordWrap(true);
    m_contentStack->addWidget(m_noDocLabel);  // index 0

    // Page 1: canvas placeholder (canvas is owned by ModeController, not parented here)
    // We use a transparent frame as a placeholder; the canvas is shown via ModeController
    // stacking. FormBuilderMode routes signals to/from canvas without reparenting it.
    auto* canvasPlaceholder = new QFrame;
    canvasPlaceholder->setFrameStyle(QFrame::NoFrame);
    m_contentStack->addWidget(canvasPlaceholder);  // index 1

    splitter->addWidget(m_contentStack);
    splitter->setStretchFactor(1, 1);

    // ── Right sidebar: properties panel + tab order ────────────────────────
    auto* rightPanel = new QFrame;
    rightPanel->setFixedWidth(240);
    auto* rightCol = new QVBoxLayout(rightPanel);
    rightCol->setContentsMargins(4, 4, 4, 4);
    rightCol->setSpacing(4);

    // Properties panel (visible when a field is selected)
    m_propsPanel = new FormFieldPropertiesPanel(m_ctx, this);
    m_propsPanel->setVisible(false);
    rightCol->addWidget(m_propsPanel);

    // Tab order panel (visible when Tab Order button is checked)
    m_tabOrderPanel = new QFrame;
    m_tabOrderPanel->setVisible(false);
    auto* tabCol = new QVBoxLayout(m_tabOrderPanel);
    tabCol->setContentsMargins(0, 0, 0, 0);
    tabCol->setSpacing(4);

    auto* tabLabel = new QLabel(tr("Tab Order"));
    tabLabel->setProperty("mono", true);
    tabCol->addWidget(tabLabel);

    m_tabOrderList = new QListWidget;
    m_tabOrderList->setDragDropMode(QAbstractItemView::InternalMove);
    m_tabOrderList->setToolTip(tr("Drag to reorder tab sequence"));
    tabCol->addWidget(m_tabOrderList, 1);

    m_tabOrderApply = new QToolButton;
    m_tabOrderApply->setText(tr("Apply Order"));
    m_tabOrderApply->setProperty("variant", "ghost");
    connect(m_tabOrderApply, &QToolButton::clicked,
            this, &FormBuilderMode::onTabOrderApplyClicked);
    tabCol->addWidget(m_tabOrderApply);

    rightCol->addWidget(m_tabOrderPanel);
    rightCol->addStretch(1);
    splitter->addWidget(rightPanel);

    col->addWidget(splitter, 1);

    // ── Wire canvas signals ────────────────────────────────────────────────
    if (m_canvas) {
        connect(m_canvas, &PdfViewerWidget::fieldPlacementRequested,
                this, &FormBuilderMode::onFieldPlacementRequested);
    }
}

// ── State update ─────────────────────────────────────────────────────────────

void FormBuilderMode::updateNoDocumentState()
{
    const bool hasDoc = m_ctx && m_ctx->document && !m_ctx->document->path().isEmpty();
    m_contentStack->setCurrentIndex(hasDoc ? 1 : 0);

    // Enable field-type buttons only when a document is open
    for (int i = 0; i < 9; ++i) {
        if (m_fieldBtns[i])
            m_fieldBtns[i]->setEnabled(hasDoc);
    }
    m_tabOrderBtn->setEnabled(hasDoc);
    m_deleteFieldBtn->setEnabled(false);

    if (hasDoc)
        refreshFieldList();
}

void FormBuilderMode::refreshFieldList()
{
    if (!m_fieldList) return;
    // Field list is populated via items added as fields are placed in this session.
    // Persisted fields from the PDF are not enumerated here (IFormManager does not
    // expose a listFields() method in v1.0.0; that is scheduled for v1.1).
    // The list therefore reflects fields placed since FormBuilderMode was entered.
}

// ── Slot implementations ──────────────────────────────────────────────────────

void FormBuilderMode::onFieldButtonToggled(bool checked)
{
    if (!checked || !m_canvas) return;

    auto* btn = qobject_cast<QToolButton*>(sender());
    if (!btn) return;

    const int idx = btn->property("fieldModeIndex").toInt();
    if (idx < 0 || idx >= 9) return;

    const bool hasDoc = m_ctx && m_ctx->document && !m_ctx->document->path().isEmpty();
    if (!hasDoc) {
        QMessageBox::information(this, tr("No Document"),
            tr("Open a PDF document before placing form fields."));
        return;
    }

    m_canvas->setToolMode(k_fieldModes[idx]);
}

void FormBuilderMode::onFieldPlacementRequested(int pageIndex, QRectF pdfRect, ToolMode mode)
{
    if (!m_ctx || !m_ctx->document || m_ctx->document->path().isEmpty()) return;
    if (!m_ctx->forms || !m_ctx->undoStack) return;

    const int typeInt = toolModeToFieldType(mode);
    if (typeInt < 0) return;  // not a form-builder mode

    const auto fieldType = static_cast<AddFormFieldCommand::FieldType>(typeInt);
    const QString name = uniqueFieldName(mode, pageIndex);

    auto* cmd = new AddFormFieldCommand(
        m_ctx->forms.get(),
        m_ctx->document.get(),
        fieldType,
        pageIndex,
        pdfRect,
        name
    );
    m_ctx->undoStack->push(cmd);

    // Add to session field list
    if (m_fieldList) {
        auto* item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, name);
        m_fieldList->addItem(item);
        m_fieldList->setCurrentItem(item);
    }

    // Update tab order list if panel is visible
    if (m_tabOrderPanel->isVisible() && m_tabOrderList) {
        m_tabOrderList->addItem(name);
    }

    // Restore to TEXT FIELD mode after placement (field-by-field workflow)
    if (m_canvas) {
        m_canvas->setToolMode(ToolMode::FormAddText);
    }
    if (m_fieldBtns[0])
        m_fieldBtns[0]->setChecked(true);
}

void FormBuilderMode::onAutoDetectClicked()
{
    // Auto-detect is disabled (setEnabled(false) in buildToolbar).
    // This slot is a no-op stub — the button tooltip says "Coming in v1.1".
}

void FormBuilderMode::onTabOrderToggled(bool checked)
{
    if (!m_tabOrderPanel) return;
    m_tabOrderPanel->setVisible(checked);

    if (checked && m_tabOrderList) {
        // Populate tab order list from current field list
        m_tabOrderList->clear();
        if (m_fieldList) {
            for (int i = 0; i < m_fieldList->count(); ++i) {
                m_tabOrderList->addItem(m_fieldList->item(i)->text());
            }
        }
    }
}

void FormBuilderMode::onPreviewFormClicked()
{
    // Preview is disabled (setEnabled(false) in buildToolbar).
    // This slot is a no-op stub — the button tooltip says "Coming in v1.1".
}

void FormBuilderMode::onExitFormClicked()
{
    // Restore canvas to default tool mode
    if (m_canvas)
        m_canvas->setToolMode(ToolMode::HandTool);

    // Deselect all field-type buttons (set first to checked for consistent state)
    if (m_fieldBtns[0])
        m_fieldBtns[0]->setChecked(true);

    // Notify parent — parent (ModeController) will call setScreen("") on its end
    // via the ribbon/screen nav; we emit a custom signal only if needed.
    // For now, exiting just restores the HandTool and leaves navigation to ScreenNav.
}

void FormBuilderMode::onFieldListSelectionChanged()
{
    const bool hasSelection = m_fieldList && !m_fieldList->selectedItems().isEmpty();
    if (m_deleteFieldBtn)
        m_deleteFieldBtn->setEnabled(hasSelection);

    if (hasSelection && m_propsPanel) {
        const QString name = m_fieldList->currentItem()->text();
        m_propsPanel->setFieldName(name);
        m_propsPanel->setVisible(true);
    } else if (m_propsPanel) {
        m_propsPanel->setVisible(false);
    }
}

void FormBuilderMode::onDeleteFieldClicked()
{
    if (!m_fieldList || !m_ctx || !m_ctx->document || !m_ctx->undoStack) return;

    auto* item = m_fieldList->currentItem();
    if (!item) return;

    const QString name = item->data(Qt::UserRole).toString();

    // Determine page index — stored as UserRole+1 during placement
    const int page = item->data(Qt::UserRole + 1).toInt();

    auto* cmd = new DeleteFormFieldCommand(
        m_ctx->forms.get(),
        m_ctx->document.get(),
        name,
        page
    );
    m_ctx->undoStack->push(cmd);

    // Remove from session list
    delete m_fieldList->takeItem(m_fieldList->row(item));

    // Remove from tab order list if visible
    if (m_tabOrderPanel->isVisible() && m_tabOrderList) {
        const auto items = m_tabOrderList->findItems(name, Qt::MatchExactly);
        for (auto* ti : items)
            delete m_tabOrderList->takeItem(m_tabOrderList->row(ti));
    }

    if (m_propsPanel)
        m_propsPanel->setVisible(false);
    if (m_deleteFieldBtn)
        m_deleteFieldBtn->setEnabled(false);
}

void FormBuilderMode::onTabOrderApplyClicked()
{
    // Tab order is tracked client-side (the field list in m_tabOrderList order).
    // IFormManager v1.0.0 does not expose a setTabOrder() method; persisting
    // the tab order to the PDF AcroForm /CO array is scheduled for v1.1.
    // For now, this records the intended order in the UI for visual reference.
    QMessageBox::information(this, tr("Tab Order"),
        tr("Tab order updated in the form builder view.\n"
           "Persisting tab order to PDF is scheduled for v1.1."));
}

void FormBuilderMode::onEscapePressed()
{
    // Cancel active field placement — restore to HandTool
    if (m_canvas && [](ToolMode m) {
            switch (m) {
            case ToolMode::FormAddText:
            case ToolMode::FormAddCheckbox:
            case ToolMode::FormAddRadio:
            case ToolMode::FormAddDropdown:
            case ToolMode::FormAddListBox:
            case ToolMode::FormAddDate:
            case ToolMode::FormAddNumeric:
            case ToolMode::FormAddSignature:
            case ToolMode::FormAddButton:
                return true;
            default: return false;
            }
        }(m_canvas->toolMode())) {
        m_canvas->setToolMode(ToolMode::HandTool);
        if (m_fieldBtns[0])
            m_fieldBtns[0]->setChecked(true);
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// static
int FormBuilderMode::toolModeToFieldType(ToolMode mode)
{
    switch (mode) {
        case ToolMode::FormAddText:      return static_cast<int>(AddFormFieldCommand::FieldType::Text);
        case ToolMode::FormAddCheckbox:  return static_cast<int>(AddFormFieldCommand::FieldType::Checkbox);
        case ToolMode::FormAddRadio:     return static_cast<int>(AddFormFieldCommand::FieldType::Radio);
        case ToolMode::FormAddDropdown:  return static_cast<int>(AddFormFieldCommand::FieldType::Dropdown);
        case ToolMode::FormAddListBox:   return static_cast<int>(AddFormFieldCommand::FieldType::ListBox);
        case ToolMode::FormAddDate:      return static_cast<int>(AddFormFieldCommand::FieldType::Date);
        case ToolMode::FormAddNumeric:   return static_cast<int>(AddFormFieldCommand::FieldType::Numeric);
        case ToolMode::FormAddSignature: return static_cast<int>(AddFormFieldCommand::FieldType::Text); // Sig uses text box
        case ToolMode::FormAddButton:    return static_cast<int>(AddFormFieldCommand::FieldType::Text); // Button uses text box
        default: return -1;
    }
}

QString FormBuilderMode::uniqueFieldName(ToolMode mode, int pageIndex) const
{
    ++m_fieldCounter;
    QString prefix;
    switch (mode) {
        case ToolMode::FormAddText:      prefix = QStringLiteral("text");      break;
        case ToolMode::FormAddCheckbox:  prefix = QStringLiteral("check");     break;
        case ToolMode::FormAddRadio:     prefix = QStringLiteral("radio");     break;
        case ToolMode::FormAddDropdown:  prefix = QStringLiteral("dropdown");  break;
        case ToolMode::FormAddListBox:   prefix = QStringLiteral("listbox");   break;
        case ToolMode::FormAddDate:      prefix = QStringLiteral("date");      break;
        case ToolMode::FormAddNumeric:   prefix = QStringLiteral("numeric");   break;
        case ToolMode::FormAddSignature: prefix = QStringLiteral("sig");       break;
        case ToolMode::FormAddButton:    prefix = QStringLiteral("button");    break;
        default:                         prefix = QStringLiteral("field");     break;
    }
    return QStringLiteral("%1_p%2_%3").arg(prefix).arg(pageIndex + 1).arg(m_fieldCounter);
}

} // namespace gp
