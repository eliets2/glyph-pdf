// SPDX-License-Identifier: Apache-2.0
#include "commands/EditAnnotationCommand.h"
#include "ui/PdfViewerWidget.h"

void EditAnnotationCommand::applyAnnotations(const QList<AnnotationItem>& anns) {
    if (m_viewer)
        m_viewer->setAnnotations(anns);
}
