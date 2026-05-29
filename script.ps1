$content = Get-Content src/ui/InspectorWidget.cpp -Raw
$includes = @'
#include <QDoubleSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include "core/AppContext.h"
#include "ui/PdfViewerWidget.h"
#include "commands/EditAnnotationCommand.h"
'@
$content = $content -replace '#include <QVBoxLayout>', "$includes
#include <QVBoxLayout>"

$oldRow = @'
static QWidget\* createFieldRow\(const QString& labelText, const QString& valueText\)
\{
    auto\* row = new QWidget;
    auto\* layout = new QHBoxLayout\(row\);
    layout->setContentsMargins\(12, 3, 12, 3\);
    layout->setSpacing\(8\);

    auto\* label = new QLabel\(labelText\);
    label->setFixedWidth\(78\);
    label->setStyleSheet\(fieldLabelSheet\(\)\);
    label->setAlignment\(Qt::AlignRight \| Qt::AlignVCenter\);
    layout->addWidget\(label\);

    auto\* value = new QLabel\(valueText\);
    value->setStyleSheet\(fieldValueSheet\(\)\);
    value->setWordWrap\(true\);
    layout->addWidget\(value, 1\);

    return row;
\}
'@

$newRow = @'
static QWidget* createFieldRow(const QString& labelText, const QString& valueText, const QString& objectName = QString())
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 3, 12, 3);
    layout->setSpacing(8);

    auto* label = new QLabel(labelText);
    label->setFixedWidth(78);
    label->setStyleSheet(fieldLabelSheet());
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(label);

    auto* value = new QLabel(valueText);
    value->setStyleSheet(fieldValueSheet());
    value->setWordWrap(true);
    if (!objectName.isEmpty()) {
        value->setObjectName(objectName);
    }
    layout->addWidget(value, 1);

    return row;
}
'@

$content = $content -replace $oldRow, $newRow
Set-Content src/ui/InspectorWidget.cpp $content
