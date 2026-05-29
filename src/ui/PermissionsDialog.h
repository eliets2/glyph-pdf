#ifndef PERMISSIONSDIALOG_H
#define PERMISSIONSDIALOG_H

#include <QDialog>
#include "core/interfaces/IPdfEditorEngine.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QCheckBox;
QT_END_NAMESPACE

class PermissionsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PermissionsDialog(QWidget *parent = nullptr);

    QString userPassword() const;
    QString ownerPassword() const;
    DocumentPermissions permissions() const;

private:
    QLineEdit *m_userPasswordEdit;
    QLineEdit *m_ownerPasswordEdit;
    QCheckBox *m_printCheck;
    QCheckBox *m_printHighCheck;
    QCheckBox *m_copyCheck;
    QCheckBox *m_modifyCheck;
    QCheckBox *m_annotateCheck;
    QCheckBox *m_fillFormsCheck;
    QCheckBox *m_accessibilityCheck;
    QCheckBox *m_assembleCheck;
};

#endif // PERMISSIONSDIALOG_H
