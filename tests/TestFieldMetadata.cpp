// SPDX-License-Identifier: Apache-2.0
// Audit 9.6 P0 regression test: the properties panel's Required flag and
// Tooltip must persist as real PDF metadata (/Ff bit 2 and /TU) — previously
// the UI collected them but the values were silently discarded on save.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/FormManager.h"

class TestFieldMetadata : public QObject {
    Q_OBJECT
private slots:
    void requiredAndTooltipPersist();
    void clearingMetadataWorks();
private:
    static QString createTestPdf(const QString& dir, const QString& name);
};
QString TestFieldMetadata::createTestPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(
        "%PDF-1.4\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
        "xref\n0 4\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000058 00000 n \n"
        "0000000115 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\n"
        "startxref\n183\n%%EOF\n");
    return path;
}
void TestFieldMetadata::requiredAndTooltipPersist() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "meta.pdf");
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 72, 144, 24), QStringLiteral("f1"), pdf));

    QVERIFY(fm.setFieldMetadata(pdf, QStringLiteral("f1"),
                                QStringLiteral("Enter your full name"), true, pdf));

    // Reload from disk and verify the real PDF keys.
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        QVERIFY(acroForm);
        bool found = false;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != QLatin1String("f1")) continue;
            found = true;
            const PoDoFo::PdfDictionary& d = field.GetObject().GetDictionary();
            const PoDoFo::PdfObject* tu = d.FindKey("TU");
            QVERIFY2(tu && tu->IsString(), "/TU tooltip must be written");
            QCOMPARE(QString::fromStdString(std::string(tu->GetString().GetString())),
                     QStringLiteral("Enter your full name"));
            const PoDoFo::PdfObject* ff = d.FindKey("Ff");
            QVERIFY2(ff && ff->IsNumber(), "/Ff must be written");
            QVERIFY2((ff->GetNumber() & 2) != 0, "Required bit (bit 2) must be set in /Ff");
        }
        QVERIFY2(found, "field f1 must exist after reload");
    } catch (const PoDoFo::PdfError& e) {
        QFAIL(qPrintable(QStringLiteral("reload failed: %1").arg(QString::fromLatin1(e.what()))));
    }
}
void TestFieldMetadata::clearingMetadataWorks() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "meta2.pdf");
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 72, 144, 24), QStringLiteral("f2"), pdf));

    // Set first...
    QVERIFY(fm.setFieldMetadata(pdf, QStringLiteral("f2"), QStringLiteral("tip"), true, pdf));
    // ...then clear: tooltip removed, Required bit cleared.
    QVERIFY(fm.setFieldMetadata(pdf, QStringLiteral("f2"), QString(), false, pdf));

    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        QVERIFY(acroForm);
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) != QLatin1String("f2")) continue;
            const PoDoFo::PdfDictionary& d = field.GetObject().GetDictionary();
            QVERIFY2(!d.HasKey("TU"), "/TU must be removed when tooltip is cleared");
            const PoDoFo::PdfObject* ff = d.FindKey("Ff");
            const int flags = (ff && ff->IsNumber()) ? static_cast<int>(ff->GetNumber()) : 0;
            QVERIFY2((flags & 2) == 0, "Required bit must be cleared");
        }
    } catch (const PoDoFo::PdfError& e) {
        QFAIL(qPrintable(QStringLiteral("reload failed: %1").arg(QString::fromLatin1(e.what()))));
    }
}
QTEST_MAIN(TestFieldMetadata)
#include "TestFieldMetadata.moc"
