// SPDX-License-Identifier: Apache-2.0
// Audit 9.6 P0 regression test: fillForm's default-value path must NOT set
// fields read-only. Only the explicit fill+lock path (lockFields=true) may
// write the /Ff read-only bit.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/FormManager.h"

class TestFillFormLock : public QObject {
    Q_OBJECT
private slots:
    void nonLockingFillKeepsFieldEditable();
    void defaultFillStillLocks();
private:
    static QString createTestPdf(const QString& dir, const QString& name);
    static bool fieldIsReadOnly(const QString& pdfPath, const QString& fieldName);
};
QString TestFillFormLock::createTestPdf(const QString& dir, const QString& name) {
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
bool TestFillFormLock::fieldIsReadOnly(const QString& pdfPath, const QString& fieldName) {
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfPath.toUtf8().constData());
        auto* acroForm = doc.GetAcroForm();
        if (!acroForm) return false;
        for (unsigned i = 0; i < acroForm->GetFieldCount(); ++i) {
            auto& field = acroForm->GetFieldAt(i);
            if (QString::fromStdString(field.GetFullName()) == fieldName) {
                // /Ff bit position 1 (value 1) = ReadOnly per ISO 32000-1 Table 220
                const PoDoFo::PdfObject& obj = field.GetObject();
                const PoDoFo::PdfDictionary& dict = obj.GetDictionary();
                if (!dict.HasKey("Ff")) return false;
                auto ff = dict.FindKey("Ff");
                if (!ff) return false;
                return (ff->GetNumber() & 1) != 0;
            }
        }
    } catch (...) {}
    return false;
}
void TestFillFormLock::nonLockingFillKeepsFieldEditable() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "lock_probe.pdf");
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 72, 144, 24), QStringLiteral("f1"), pdf));
    QVariantMap data;
    data[QStringLiteral("f1")] = QStringLiteral("default text");
    QVERIFY(fm.fillForm(pdf, data, pdf, /*lockFields=*/false));
    QVERIFY2(!fieldIsReadOnly(pdf, "f1"), "default-value edit must not set /Ff ReadOnly");
}
void TestFillFormLock::defaultFillStillLocks() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "lock_probe2.pdf");
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 72, 144, 24), QStringLiteral("f2"), pdf));
    QVariantMap data;
    data[QStringLiteral("f2")] = QStringLiteral("final value");
    QVERIFY(fm.fillForm(pdf, data, pdf)); // default lockFields=true
    QVERIFY2(fieldIsReadOnly(pdf, "f2"), "fill+lock path must set /Ff ReadOnly");
}
QTEST_MAIN(TestFillFormLock)
#include "TestFillFormLock.moc"
