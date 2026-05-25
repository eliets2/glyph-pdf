#pragma once
#include <QDialog>
#include <QPageSize>
#include <QPageLayout>

class QComboBox;
class QDoubleSpinBox;
class QRadioButton;
class QSpinBox;

namespace gp {

/**
 * Page setup dialog for print configuration (Session 19 D2).
 *
 * Controls: paper size, orientation, margins (mm), scaling mode
 * (fit-to-page, actual size, custom %).  Settings persist per-document
 * via the filePath key in QSettings.
 */
class PageSetupDialog : public QDialog {
    Q_OBJECT
public:
    struct Settings {
        QPageSize::PageSizeId pageSize = QPageSize::A4;
        QPageLayout::Orientation orientation = QPageLayout::Portrait;
        double marginTop    = 10.0;  // mm
        double marginBottom = 10.0;
        double marginLeft   = 10.0;
        double marginRight  = 10.0;
        int    scaleMode    = 0;     // 0=Fit, 1=Actual, 2=Custom
        int    scalePercent = 100;
    };

    explicit PageSetupDialog(const QString& docPath, QWidget* parent = nullptr);

    Settings settings() const;

    /** Load / save per-document settings from QSettings. */
    static Settings loadForDocument(const QString& docPath);
    static void saveForDocument(const QString& docPath, const Settings& s);

private slots:
    void onAccept();

private:
    QString m_docPath;

    QComboBox*      m_paperSize    = nullptr;
    QRadioButton*   m_portrait     = nullptr;
    QRadioButton*   m_landscape    = nullptr;
    QDoubleSpinBox* m_marginTop    = nullptr;
    QDoubleSpinBox* m_marginBottom = nullptr;
    QDoubleSpinBox* m_marginLeft   = nullptr;
    QDoubleSpinBox* m_marginRight  = nullptr;
    QComboBox*      m_scaleMode    = nullptr;
    QSpinBox*       m_scalePercent = nullptr;
};

} // namespace gp
