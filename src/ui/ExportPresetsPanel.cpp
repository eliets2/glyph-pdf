#include "ExportPresetsPanel.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace gp {

// ── Preset serialization ────────────────────────────────────────────────

QJsonObject ExportPresetsPanel::Preset::toJson() const {
    QJsonObject o;
    o["name"]        = name;
    o["dpi"]         = dpi;
    o["linearized"]  = linearized;
    o["compressed"]  = compressed;
    o["pdfA"]        = pdfA;
    o["pdfALevel"]   = pdfALevel;
    o["batesNumber"] = batesNumber;
    o["batesPrefix"] = batesPrefix;
    return o;
}

ExportPresetsPanel::Preset ExportPresetsPanel::Preset::fromJson(const QJsonObject& o) {
    Preset p;
    p.name        = o["name"].toString();
    p.dpi         = o["dpi"].toInt(150);
    p.linearized  = o["linearized"].toBool(false);
    p.compressed  = o["compressed"].toBool(true);
    p.pdfA        = o["pdfA"].toBool(false);
    p.pdfALevel   = o["pdfALevel"].toString();
    p.batesNumber = o["batesNumber"].toBool(false);
    p.batesPrefix = o["batesPrefix"].toString();
    return p;
}

// ── Storage ─────────────────────────────────────────────────────────────

QList<ExportPresetsPanel::Preset> ExportPresetsPanel::loadPresets() {
    QSettings settings;
    QByteArray data = settings.value("export/presets").toByteArray();
    if (data.isEmpty()) return {};

    QJsonArray arr = QJsonDocument::fromJson(data).array();
    QList<Preset> out;
    for (auto v : arr)
        out.append(Preset::fromJson(v.toObject()));
    return out;
}

void ExportPresetsPanel::savePresets(const QList<Preset>& presets) {
    QJsonArray arr;
    for (const auto& p : presets)
        arr.append(p.toJson());
    QSettings settings;
    settings.setValue("export/presets", QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void ExportPresetsPanel::ensureDefaults() {
    if (!loadPresets().isEmpty()) return;

    QList<Preset> defaults;

    Preset hq;
    hq.name = "High Quality PDF/A";
    hq.dpi = 300;
    hq.pdfA = true;
    hq.pdfALevel = "2b";
    hq.compressed = true;
    defaults.append(hq);

    Preset web;
    web.name = "Web Optimized";
    web.dpi = 150;
    web.linearized = true;
    web.compressed = true;
    defaults.append(web);

    Preset legal;
    legal.name = "Legal Archive";
    legal.dpi = 300;
    legal.pdfA = true;
    legal.pdfALevel = "3b";
    legal.batesNumber = true;
    legal.batesPrefix = "DOC-";
    defaults.append(legal);

    savePresets(defaults);
}

// ── UI ──────────────────────────────────────────────────────────────────

ExportPresetsPanel::ExportPresetsPanel(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Export Presets"));
    setMinimumSize(480, 340);
    setAccessibleName(tr("Export Presets"));

    ensureDefaults();
    m_presets = loadPresets();

    auto* root = new QHBoxLayout(this);

    // Left: list
    auto* leftCol = new QVBoxLayout;
    m_list = new QListWidget;
    m_list->setAccessibleName(tr("Preset list"));
    leftCol->addWidget(m_list, 1);

    auto* btnRow = new QHBoxLayout;
    auto* addBtn = new QPushButton(tr("Add"));
    addBtn->setAccessibleName(tr("Add new export preset"));
    auto* editBtn = new QPushButton(tr("Edit"));
    editBtn->setAccessibleName(tr("Edit selected preset"));
    auto* delBtn = new QPushButton(tr("Delete"));
    delBtn->setAccessibleName(tr("Delete selected preset"));
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(delBtn);
    leftCol->addLayout(btnRow);
    root->addLayout(leftCol, 1);

    // Right: detail + apply
    auto* rightCol = new QVBoxLayout;
    m_detail = new QLabel(tr("Select a preset to view details."));
    m_detail->setWordWrap(true);
    m_detail->setAccessibleName(tr("Preset details"));
    m_detail->setMinimumWidth(200);
    rightCol->addWidget(m_detail, 1);

    auto* applyBtn = new QPushButton(tr("Apply"));
    applyBtn->setAccessibleName(tr("Apply selected preset"));
    applyBtn->setDefault(true);
    rightCol->addWidget(applyBtn);

    auto* closeBtn = new QPushButton(tr("Close"));
    closeBtn->setAccessibleName(tr("Close export presets"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    rightCol->addWidget(closeBtn);

    root->addLayout(rightCol);

    // Wiring
    connect(addBtn,  &QPushButton::clicked, this, &ExportPresetsPanel::onAdd);
    connect(editBtn, &QPushButton::clicked, this, &ExportPresetsPanel::onEdit);
    connect(delBtn,  &QPushButton::clicked, this, &ExportPresetsPanel::onDelete);
    connect(applyBtn,&QPushButton::clicked, this, &ExportPresetsPanel::onApply);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= m_presets.size()) {
            m_detail->setText(tr("Select a preset to view details."));
            return;
        }
        const Preset& p = m_presets[row];
        QString info;
        info += tr("<b>%1</b><br><br>").arg(p.name);
        info += tr("DPI: %1<br>").arg(p.dpi);
        info += tr("Linearized: %1<br>").arg(p.linearized ? tr("Yes") : tr("No"));
        info += tr("Compressed: %1<br>").arg(p.compressed ? tr("Yes") : tr("No"));
        if (p.pdfA)
            info += tr("PDF/A: %1<br>").arg(p.pdfALevel);
        if (p.batesNumber)
            info += tr("Bates: %1*<br>").arg(p.batesPrefix);
        m_detail->setText(info);
    });

    refreshList();
}

void ExportPresetsPanel::refreshList() {
    m_list->clear();
    for (const auto& p : m_presets)
        m_list->addItem(p.name);
    if (!m_presets.isEmpty())
        m_list->setCurrentRow(0);
}

void ExportPresetsPanel::onAdd() {
    bool ok;
    QString name = QInputDialog::getText(this, tr("New Preset"),
        tr("Preset name:"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    Preset p;
    p.name = name.trimmed();
    p.dpi = 150;
    p.compressed = true;
    m_presets.append(p);
    savePresets(m_presets);
    refreshList();
    m_list->setCurrentRow(m_presets.size() - 1);
}

void ExportPresetsPanel::onEdit() {
    int row = m_list->currentRow();
    if (row < 0 || row >= m_presets.size()) return;

    bool ok;
    QString name = QInputDialog::getText(this, tr("Edit Preset"),
        tr("Preset name:"), QLineEdit::Normal, m_presets[row].name, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    m_presets[row].name = name.trimmed();
    savePresets(m_presets);
    refreshList();
    m_list->setCurrentRow(row);
}

void ExportPresetsPanel::onDelete() {
    int row = m_list->currentRow();
    if (row < 0 || row >= m_presets.size()) return;

    int r = QMessageBox::question(this, tr("Delete Preset"),
        tr("Delete preset \"%1\"?").arg(m_presets[row].name));
    if (r != QMessageBox::Yes) return;

    m_presets.removeAt(row);
    savePresets(m_presets);
    refreshList();
}

void ExportPresetsPanel::onApply() {
    int row = m_list->currentRow();
    if (row < 0 || row >= m_presets.size()) return;
    emit presetSelected(m_presets[row]);
    accept();
}

} // namespace gp
