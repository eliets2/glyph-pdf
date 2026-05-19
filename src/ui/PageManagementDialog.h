#ifndef PAGEMANAGEMENTDIALOG_H
#define PAGEMANAGEMENTDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QSpinBox;
class QPushButton;
class QComboBox;
class QLabel;
QT_END_NAMESPACE

class PageManagementDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Operation {
        ExtractPages,
        DeletePages,
        RotatePages,
        InsertBlankPage
    };

    explicit PageManagementDialog(int totalPages, QWidget *parent = nullptr);

    Operation selectedOperation() const;
    int fromPage() const;
    int toPage() const;
    int rotationAngle() const;

private:
    void onOperationChanged(int index);

    QComboBox *operationCombo;
    QSpinBox *fromPageSpin;
    QSpinBox *toPageSpin;
    QComboBox *rotationCombo;
    QLabel *rotationLabel;
    QPushButton *okButton;
    QPushButton *cancelButton;
    int m_totalPages;
};

#endif // PAGEMANAGEMENTDIALOG_H
