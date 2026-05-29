$content = Get-Content src/ui/InspectorWidget.cpp -Raw

$idOld = @'
    identityLayout->addWidget(createFieldRow("Author", "Unknown"));
    identityLayout->addWidget(createFieldRow("Created", "--"));
    identityLayout->addWidget(createFieldRow("Modified", "--"));
    identityLayout->addWidget(createFieldRow("Subject", "--"));
'@
$idNew = @'
    identityLayout->addWidget(createFieldRow("Author", "Unknown", "propAuthor"));
    identityLayout->addWidget(createFieldRow("Created", "--", "propCreated"));
    identityLayout->addWidget(createFieldRow("Modified", "--", "propModified"));
    identityLayout->addWidget(createFieldRow("Layer", "Default Layer", "propLayer"));
    
    auto* lockRow = new QWidget;
    auto* lockLayout = new QHBoxLayout(lockRow);
    lockLayout->setContentsMargins(12, 3, 12, 3);
    auto* lockLbl = new QLabel("Lock");
    lockLbl->setFixedWidth(78);
    lockLbl->setStyleSheet(fieldLabelSheet());
    lockLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lockLayout->addWidget(lockLbl);
    auto* lockCheck = new QCheckBox;
    lockCheck->setObjectName("propLocked");
    lockLayout->addWidget(lockCheck, 1);
    identityLayout->addWidget(lockRow);
'@
$content = $content.Replace($idOld, $idNew)

$appOld = @'
    // Opacity
    auto* opacityRow = createFieldRow("Opacity", "100%");
    appearLayout->addWidget(opacityRow);

    // Blend mode
    auto* blendRow = createFieldRow("Blend", "Normal");
    appearLayout->addWidget(blendRow);

    // Border width
    auto* borderRow = createFieldRow("Border", "2px");
    appearLayout->addWidget(borderRow);
'@
$appNew = @'
    auto makeField = [](const QString& lbl, QWidget* w) {
        auto* row = new QWidget;
        auto* lyt = new QHBoxLayout(row);
        lyt->setContentsMargins(0, 2, 0, 2);
        auto* label = new QLabel(lbl);
        label->setFixedWidth(78);
        label->setStyleSheet(fieldLabelSheet());
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lyt->addWidget(label);
        lyt->addWidget(w, 1);
        return row;
    };
    
    auto* opacitySpin = new QSlider(Qt::Horizontal);
    opacitySpin->setRange(0, 100);
    opacitySpin->setObjectName("propOpacity");
    appearLayout->addWidget(makeField("Opacity", opacitySpin));
    
    auto* blendCombo = new QComboBox;
    blendCombo->addItems({"Normal", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "ColorDodge", "ColorBurn", "HardLight", "SoftLight", "Difference", "Exclusion"});
    blendCombo->setObjectName("propBlend");
    appearLayout->addWidget(makeField("Blend", blendCombo));
    
    auto* borderSpin = new QSpinBox;
    borderSpin->setRange(0, 50);
    borderSpin->setObjectName("propBorder");
    appearLayout->addWidget(makeField("Border", borderSpin));
'@
$content = $content.Replace($appOld, $appNew)

$posOld = @'
    auto makeGridValue = [](const QString& text) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(fieldValueSheet());
        return lbl;
    };

    posGrid->addWidget(makeGridLabel("X"), 0, 0);
    posGrid->addWidget(makeGridValue("0.00"), 0, 1);
    posGrid->addWidget(makeGridLabel("Y"), 0, 2);
    posGrid->addWidget(makeGridValue("0.00"), 0, 3);
    posGrid->addWidget(makeGridLabel("W"), 1, 0);
    posGrid->addWidget(makeGridValue("0.00"), 1, 1);
    posGrid->addWidget(makeGridLabel("H"), 1, 2);
    posGrid->addWidget(makeGridValue("0.00"), 1, 3);
'@
$posNew = @'
    auto makeGridValue = [](const QString& objName) {
        auto* spin = new QDoubleSpinBox;
        spin->setRange(-99999, 99999);
        spin->setObjectName(objName);
        return spin;
    };

    posGrid->addWidget(makeGridLabel("X"), 0, 0);
    posGrid->addWidget(makeGridValue("propX"), 0, 1);
    posGrid->addWidget(makeGridLabel("Y"), 0, 2);
    posGrid->addWidget(makeGridValue("propY"), 0, 3);
    posGrid->addWidget(makeGridLabel("W"), 1, 0);
    posGrid->addWidget(makeGridValue("propW"), 1, 1);
    posGrid->addWidget(makeGridLabel("H"), 1, 2);
    posGrid->addWidget(makeGridValue("propH"), 1, 3);
'@
$content = $content.Replace($posOld, $posNew)

$contOld = @'
    auto* editor = new QTextEdit;
    editor->setFixedHeight(80);
'@
$contNew = @'
    auto* editor = new QTextEdit;
    editor->setObjectName("propText");
    editor->setFixedHeight(80);
'@
$content = $content.Replace($contOld, $contNew)

Set-Content src/ui/InspectorWidget.cpp $content
