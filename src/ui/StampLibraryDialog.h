// SPDX-License-Identifier: Apache-2.0
#ifndef STAMPLIBRARYDIALOG_H
#define STAMPLIBRARYDIALOG_H

#include <QDialog>
#include <QImage>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QLabel;
class QPushButton;

// ── T2-6: stamp library management ──────────────────────────────────────────
// Lists the five built-in dynamic stamps (Approved / Draft / Confidential /
// Received / Reviewed) plus the user's persisted custom stamps, and is the
// wire target of the previously dead "Custom Stamp…" action.
//
// Honesty: the dialog discloses WHERE substitution happens — placeholders are
// resolved when a stamp is PLACED, and the saved annotation carries the
// concrete values (never a live field that re-evaluates on reopen).
//
// Row 18: an "Import Image…" entry point adds an image-variant stamp
// (pick image → name it → it lands in the catalog). Unusable images are
// refused with their typed reason — never a silent accept.
//
// Programmatically testable: addCustomStamp/addCustomImageStamp/deleteSelected
// and the preview refresh avoid modals; the Place button emits
// placeRequested(id) which the host routes through
// EditController::armDynamicStamp (one stamp flow).
class StampLibraryDialog : public QDialog {
    Q_OBJECT
public:
    explicit StampLibraryDialog(QWidget* parent = nullptr);

    // Rebuild the list from StampLibrary::all() (built-ins + persisted custom).
    void reload();

    // Custom stamp management (persists immediately via StampLibrary).
    bool addCustomStamp(const QString& name, const QString& textTemplate);
    // Row 18 image import (programmatic seam — the button handler adds the
    // file/name prompts around it). Validates + copies the image, persists
    // an image-variant stamp, reloads and selects it. On failure returns
    // false and fills *error with a typed, user-facing reason.
    bool addCustomImageStamp(const QString& name, const QString& sourceImagePath,
                             QString* error = nullptr);
    bool deleteSelectedCustom();
    QString selectedTemplateId() const;
    QString selectedPreviewText() const;   // resolved with the current author/time
    // The selected stamp's image (null for text stamps / missing files) —
    // what the preview shows and what placement would place.
    QImage selectedStampImage() const;

    QString authorName() const;
    void setAuthorName(const QString& name);

signals:
    void placeRequested(const QString& templateId);

private:
    void refreshPreview();

    QListWidget* m_list = nullptr;
    QLabel* m_preview = nullptr;
    QLabel* m_disclosure = nullptr;
    QLineEdit* m_author = nullptr;
    QPushButton* m_placeBtn = nullptr;
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
};

#endif // STAMPLIBRARYDIALOG_H
