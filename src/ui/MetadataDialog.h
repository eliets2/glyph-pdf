#ifndef METADATADIALOG_H
#define METADATADIALOG_H

#include <QDialog>
#include "core/interfaces/IPdfEditorEngine.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
QT_END_NAMESPACE

class MetadataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MetadataDialog(const PdfMetadata &metadata, QWidget *parent = nullptr);
    PdfMetadata metadata() const;

private:
    QLineEdit *titleEdit;
    QLineEdit *authorEdit;
    QLineEdit *subjectEdit;
    QLineEdit *keywordsEdit;
    QLineEdit *creatorEdit;
    QLineEdit *producerEdit;
};

#endif // METADATADIALOG_H
