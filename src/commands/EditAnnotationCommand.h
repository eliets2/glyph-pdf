#pragma once
#include <QUndoCommand>
#include <QPointer>
#include <QList>
#include "core/AnnotationTypes.h"
#include "engines/DocumentSession.h"

class PdfViewerWidget;

// Stores annotation data directly. Uses QPointer to safely access the
// viewer if it is still alive; a deleted tab will not crash undo/redo.
class EditAnnotationCommand : public QUndoCommand {
public:
    EditAnnotationCommand(PdfViewerWidget* viewer,
                          DocumentSession* doc,
                          const QList<AnnotationItem>& oldAnns,
                          const QList<AnnotationItem>& newAnns)
        : m_viewer(viewer), m_doc(doc), m_oldAnns(oldAnns), m_newAnns(newAnns) {
        setText(QObject::tr("Edit annotation"));
    }

    void redo() override {
        applyAnnotations(m_newAnns);
    }

    void undo() override {
        applyAnnotations(m_oldAnns);
    }

    int id() const override { return 0x105; }

private:
    void applyAnnotations(const QList<AnnotationItem>& anns);

    QPointer<PdfViewerWidget> m_viewer;
    DocumentSession* m_doc;
    QList<AnnotationItem> m_oldAnns;
    QList<AnnotationItem> m_newAnns;
};


