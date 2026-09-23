// SPDX-License-Identifier: Apache-2.0
// N2 (research backlog 2026-09-10): XFA honesty banner — the okular §1.4
// pattern (`HasUnsupportedXfaForm`): the user is TOLD the form is unsupported
// rather than it silently mis-filling.
//
// Acceptance (backlog §2.1 N2):
//   * opening an XFA document raises a CapId with a non-empty whyNot AND a
//     non-empty alternative ("fill/print in the source application");
//   * a non-XFA document raises nothing (Available, no disclosure);
//   * the registry query enforces the contract (whyNot/alternative non-empty
//     is a CapabilityRegistry core rule).
// The fixtures are real PDFs: a plain one-page document and one carrying an
// /AcroForm dictionary with an /XFA entry, both written by PoDoFo. Detection
// goes through the REAL FormManager::hasXfaForms — the same probe the app's
// Bootstrapper registers (see the mirrored lambda below).
#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/Capability.h"
#include "core/interfaces/IFormManager.h"
#include "engines/FormManager.h"

#include <podofo/podofo.h>

using gp::Availability;
using gp::CapId;
using gp::Capability;
using gp::CapabilityRegistry;

namespace {

// The XFA probe exactly as Bootstrapper::createContext registers it: param =
// file path; wraps IFormManager::hasXfaForms; Available when absent, Degraded
// with the canonical wording when present. Tests exercise the SAME probe body
// against a standalone registry.
Capability xfaProbe(std::weak_ptr<IFormManager> formsWeak, const QVariant& param)
{
    Capability c;
    c.status = Availability::Available;
    const QString path = param.toString();
    auto forms = formsWeak.lock();
    if (path.isEmpty() || !forms) return c;
    if (!forms->hasXfaForms(path)) return c;
    c.status = Availability::Degraded;
    c.whyNot = gp::xfaFormsWhyNot();
    c.alternative = gp::xfaFormsAlternative();
    c.detail = QStringLiteral(
        "IFormManager::hasXfaForms found an /AcroForm /XFA entry in %1").arg(path);
    return c;
}

QString writePlainPdf(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        Q_UNUSED(page);
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "writePlainPdf failed:" << e.what();
        return {};
    }
    return path;
}

QString writeXfaPdf(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        Q_UNUSED(page);
        // An /AcroForm dictionary carrying /XFA — the exact structure
        // FormManager::hasXfaForms detects (the ISO-deprecated dynamic-form
        // entry point).
        auto& acroForm = doc.GetObjects().CreateDictionaryObject();
        acroForm.GetDictionary().AddKey("XFA",
            PoDoFo::PdfString(QStringLiteral("dynamic form stream").toStdString()));
        doc.GetCatalog().GetDictionary().AddKey("AcroForm",
            acroForm.GetIndirectReference());
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "writeXfaPdf failed:" << e.what();
        return {};
    }
    return path;
}

} // namespace

class TestXfaHonestyBanner : public QObject {
    Q_OBJECT

    std::shared_ptr<IFormManager> m_forms;
    QTemporaryDir m_dir;

    QString plainPath() const { return m_dir.filePath(QStringLiteral("plain.pdf")); }
    QString xfaPath()   const { return m_dir.filePath(QStringLiteral("xfa.pdf")); }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_forms = std::make_shared<FormManager>();
        QVERIFY(!writePlainPdf(plainPath()).isEmpty());
        QVERIFY(!writeXfaPdf(xfaPath()).isEmpty());
    }

    // The detector itself, against real files: XFA present vs absent.
    void hasXfaFormsDetectsRealFixtures() {
        FormManager fm;
        QVERIFY(fm.hasXfaForms(xfaPath()));
        QVERIFY(!fm.hasXfaForms(plainPath()));
        // A missing file is honestly "no XFA", not a crash.
        QVERIFY(!fm.hasXfaForms(m_dir.filePath(QStringLiteral("absent.pdf"))));
    }

    // Acceptance: an XFA document raises a CapId with non-empty whyNot +
    // alternative; the alternative names the source application route.
    void xfaDocumentRaisesDegradedCapWithContractWording() {
        CapabilityRegistry reg;
        reg.registerProbe(CapId::XfaForms,
                          [this](const QVariant& p) { return xfaProbe(m_forms, p); });

        const Capability c = reg.query(CapId::XfaForms, xfaPath());
        QVERIFY2(c.status == Availability::Degraded, "XFA = Degraded, never fake-Available");
        QVERIFY2(!c.whyNot.trimmed().isEmpty(), "whyNot non-empty (registry contract)");
        QVERIFY2(!c.alternative.trimmed().isEmpty(), "alternative non-empty");
        QVERIFY2(c.whyNot.contains("XFA"), "whyNot names XFA");
        QVERIFY2(c.whyNot.contains("does not run XFA"),
                 "whyNot states the honest limit: no XFA execution");
        QVERIFY2(c.alternative.contains("application"),
                 "alternative points at the source application (okular wording)");
    }

    // Acceptance: a non-XFA document raises NOTHING.
    void nonXfaDocumentRaisesNothing() {
        CapabilityRegistry reg;
        reg.registerProbe(CapId::XfaForms,
                          [this](const QVariant& p) { return xfaProbe(m_forms, p); });

        const Capability c = reg.query(CapId::XfaForms, plainPath());
        QVERIFY2(c.status == Availability::Available, "no XFA = Available, no disclosure");
        QVERIFY2(c.whyNot.isEmpty() && c.alternative.isEmpty(),
                 "no wording fabricated for an unaffected document");
    }

    // The canonical wording functions are the single source of truth and are
    // non-empty on their own (what the banner shows).
    void canonicalWordingIsPresent() {
        QVERIFY2(!gp::xfaFormsWhyNot().trimmed().isEmpty(), "whyNot wording exists");
        QVERIFY2(!gp::xfaFormsAlternative().trimmed().isEmpty(),
                 "alternative wording exists");
        QVERIFY2(gp::xfaFormsWhyNot() != gp::xfaFormsAlternative(),
                 "the two clauses say different things");
    }

    // Re-query after invalidation (the open path invalidates per open so a
    // re-opened/changed document is re-probed, never served a stale cache).
    void invalidateReprombesTheFile() {
        CapabilityRegistry reg;
        reg.registerProbe(CapId::XfaForms,
                          [this](const QVariant& p) { return xfaProbe(m_forms, p); });
        QVERIFY(reg.query(CapId::XfaForms, plainPath()).status == Availability::Available);
        // The plain file is REPLACED by an XFA one on the same path.
        QVERIFY(QFile::remove(plainPath()));
        QVERIFY(!writeXfaPdf(plainPath()).isEmpty());
        reg.invalidate(CapId::XfaForms);
        QVERIFY2(reg.query(CapId::XfaForms, plainPath()).status == Availability::Degraded,
                 "post-invalidate the probe sees the new reality");
    }
};

QTEST_MAIN(TestXfaHonestyBanner)
#include "TestXfaHonestyBanner.moc"
