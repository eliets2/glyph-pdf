// SPDX-License-Identifier: Apache-2.0
#include "ui/EditAnnotationCommand.h"
#include "ui/PdfViewerWidget.h"

void EditAnnotationCommand::applyAnnotations(const QList<AnnotationItem>& anns) {
    if (m_viewer)
        m_viewer->setAnnotations(anns);
}

void EditAnnotationCommand::redo() {
    // ARC07: an annotation edit on a read-only session must not apply and
    // must not enter history — setObsolete makes QUndoStack::push delete the
    // command before it is added (Qt 5.15+ contract; see the step-3 commands).
    if (m_doc && m_doc->isReadOnly()) {
        setObsolete(true);
        return;
    }
    applyAnnotations(m_newAnns);
}
