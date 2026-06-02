// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include "core/PdfEnums.h"

class QToolButton;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QFrame;
class QStackedWidget;

struct AppContext;
class PdfViewerWidget;

namespace gp {

class FormFieldPropertiesPanel;

/// FormBuilderMode — drag-to-place 9 field types on the active document.
/// Replaces the v1.0.0 preview banner. Wired to AppContext + PdfViewerWidget.
class FormBuilderMode : public QWidget {
    Q_OBJECT
public:
    /// \param ctx    Application context (FormManager, DocumentSession, UndoStack).
    ///               May be nullptr (mode shows "No document open" state).
    /// \param canvas The shared PdfViewerWidget from ModeController::viewer().
    ///               The canvas is NOT reparented — FormBuilderMode installs an event
    ///               filter and routes form-mode signals; ownership stays with ModeController.
    /// \param parent Qt parent widget (ModeController / QStackedWidget).
    explicit FormBuilderMode(const AppContext* ctx,
                             PdfViewerWidget* canvas,
                             QWidget* parent = nullptr);

private slots:
    void onFieldButtonToggled(bool checked);
    void onFieldPlacementRequested(int pageIndex, QRectF pdfRect, ToolMode mode);
    void onAutoDetectClicked();
    void onTabOrderToggled(bool checked);
    void onPreviewFormClicked();
    void onExitFormClicked();
    void onFieldListSelectionChanged();
    void onDeleteFieldClicked();
    void onTabOrderApplyClicked();
    void onEscapePressed();

private:
    void buildToolbar(class QVBoxLayout* col);
    void buildContent(class QVBoxLayout* col);
    void updateNoDocumentState();
    void refreshFieldList();
    /// Map a FormAddXxx ToolMode to the matching AddFormFieldCommand::FieldType.
    static int toolModeToFieldType(ToolMode mode);   // returns -1 if not a form mode
    /// Generate a unique field name for the given type on the given page.
    QString uniqueFieldName(ToolMode mode, int pageIndex) const;

    const AppContext*        m_ctx     = nullptr;
    PdfViewerWidget*         m_canvas  = nullptr;

    // Toolbar field-type buttons (9 total — order matches ToolMode FormAddXxx enum run)
    QToolButton* m_fieldBtns[9] = {};
    static constexpr ToolMode k_fieldModes[9] = {
        ToolMode::FormAddText,
        ToolMode::FormAddCheckbox,
        ToolMode::FormAddRadio,
        ToolMode::FormAddDropdown,
        ToolMode::FormAddListBox,
        ToolMode::FormAddDate,
        ToolMode::FormAddNumeric,
        ToolMode::FormAddSignature,
        ToolMode::FormAddButton
    };

    QToolButton* m_autoDetectBtn  = nullptr;
    QToolButton* m_tabOrderBtn    = nullptr;
    QToolButton* m_previewBtn     = nullptr;
    QToolButton* m_exitBtn        = nullptr;

    // Status label when no document is open
    QLabel*      m_noDocLabel     = nullptr;

    // Field list sidebar
    QListWidget* m_fieldList      = nullptr;
    QToolButton* m_deleteFieldBtn = nullptr;

    // Properties panel (right sidebar, shown when a field is selected)
    FormFieldPropertiesPanel* m_propsPanel = nullptr;

    // Tab order panel
    QListWidget* m_tabOrderList   = nullptr;
    QToolButton* m_tabOrderApply  = nullptr;
    QFrame*      m_tabOrderPanel  = nullptr;

    // Content area that shows either the canvas or "no doc" label
    QStackedWidget* m_contentStack = nullptr;

    // Track placed field names for uniqueness (per-session; not persisted)
    mutable int m_fieldCounter = 0;
};

} // namespace gp
