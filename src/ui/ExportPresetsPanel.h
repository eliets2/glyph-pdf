// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDialog>
#include <QJsonObject>
#include <QList>

class QListWidget;
class QLabel;

namespace gp {

/**
 * Export presets panel (Session 19 D3).
 *
 * Shows saved export configurations: "High Quality PDF/A" (300 DPI, PDF/A-2b),
 * "Web Optimized" (150 DPI, linearized, compressed), "Legal Archive" (PDF/A-3b,
 * Bates numbered). User can create, edit, and delete presets. Stored in QSettings.
 */
class ExportPresetsPanel : public QDialog {
    Q_OBJECT
public:
    struct Preset {
        QString name;
        int     dpi         = 150;
        bool    linearized  = false;
        bool    compressed  = true;
        bool    pdfA        = false;
        QString pdfALevel;          // "2b", "3b", etc.
        bool    batesNumber = false;
        QString batesPrefix;

        QJsonObject toJson() const;
        static Preset fromJson(const QJsonObject& obj);
    };

    explicit ExportPresetsPanel(QWidget* parent = nullptr);

    /** Retrieve all presets from QSettings. */
    static QList<Preset> loadPresets();

    /** Save all presets to QSettings. */
    static void savePresets(const QList<Preset>& presets);

    /** Ensure the three default presets exist if no presets are stored. */
    static void ensureDefaults();

signals:
    void presetSelected(const Preset& p);

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onApply();

private:
    void refreshList();

    QListWidget*    m_list    = nullptr;
    QLabel*         m_detail  = nullptr;
    QList<Preset>   m_presets;
};

} // namespace gp
