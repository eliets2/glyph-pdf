// SPDX-License-Identifier: Apache-2.0
// T2-4 accessibility P1: pins for the CHEAP FIXES.
//
// Each fix is verified the only honest way — an INDEPENDENT PoDoFo reopen of
// the saved artifact, asserting the real PDF key landed (never in-memory
// state). Refusals must leave the destination byte-identical (SafeSave
// contract); the commit-fault pin proves the transaction shape, not just
// the happy path.
//
// The /TU fix is pinned through the SAME seam the panel uses:
// readFieldRequiredFlag + FormManager::setFieldMetadata — proving the
// Required bit is preserved exactly, never silently flipped.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>

#include "engines/AccessibilityFixes.h"
#include "engines/FormManager.h"
#include "engines/SafeSave.h"

using PoDoFo::PdfObject;
using PoDoFo::PdfName;
using PoDoFo::PdfString;
using PoDoFo::PdfDictionary;
using PoDoFo::PdfArray;

namespace {

struct Opts {
    bool lang = false;
    bool title = false;
    bool viewerPrefs = false;
    bool imageAlt = false;
    bool fieldTu = false;
    bool fieldRequired = false;  // /Ff bit 2 set
};

bool buildPdf(const QString& path, const Opts& o) {
    try {
        PoDoFo::PdfMemDocument doc;
        if (o.lang)
            doc.GetCatalog().GetDictionary().AddKey("Lang", PdfObject(PdfString("en-US")));
        if (o.title)
            doc.GetMetadata().SetTitle(PdfString("Quarterly Report"));
        if (o.viewerPrefs) {
            auto& vp = doc.GetObjects().CreateDictionaryObject();
            vp.GetDictionary().AddKey("DisplayDocTitle", PdfObject(true));
            doc.GetCatalog().GetDictionary().AddKey("ViewerPreferences",
                                                    vp.GetIndirectReference());
        }
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        auto& xobjs = doc.GetObjects().CreateDictionaryObject();
        auto& img = doc.GetObjects().CreateDictionaryObject();
        img.GetDictionary().AddKey("Type", PdfObject(PdfName("XObject")));
        img.GetDictionary().AddKey("Subtype", PdfObject(PdfName("Image")));
        img.GetDictionary().AddKey("Width", PdfObject(static_cast<int64_t>(4)));
        img.GetDictionary().AddKey("Height", PdfObject(static_cast<int64_t>(4)));
        img.GetDictionary().AddKey("ColorSpace", PdfObject(PdfName("DeviceGray")));
        img.GetDictionary().AddKey("BitsPerComponent", PdfObject(static_cast<int64_t>(8)));
        if (o.imageAlt)
            img.GetDictionary().AddKey("Alt", PdfString("scan of a receipt"));
        const unsigned char px[16] = {0};
        img.GetOrCreateStream().SetData(
            PoDoFo::bufferview(reinterpret_cast<const char*>(px), sizeof(px)));
        xobjs.GetDictionary().AddKey(PdfName("Im0"), img.GetIndirectReference());
        page.GetResources().GetDictionary().AddKey("XObject", xobjs.GetIndirectReference());

        // AcroForm with one text field.
        PdfDictionary acro;
        PdfArray fields;
        auto& field = doc.GetObjects().CreateDictionaryObject();
        field.GetDictionary().AddKey("FT", PdfObject(PdfName("Tx")));
        field.GetDictionary().AddKey("T", PdfString("f1"));
        if (o.fieldTu)
            field.GetDictionary().AddKey("TU", PdfString("Your full name"));
        if (o.fieldRequired)
            field.GetDictionary().AddKey("Ff",
                PdfObject(static_cast<int64_t>(1 << 1)));
        fields.Add(field.GetIndirectReference());
        acro.AddKey("Fields", PdfObject(fields));
        doc.GetCatalog().GetDictionary().AddKey("AcroForm", PdfObject(acro));

        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

QByteArray readBytes(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

} // namespace

class TestAccessibilityFixes : public QObject {
    Q_OBJECT
private slots:
    void setLanguageWritesCatalogKey();
    void displayDocTitleNeedsTitleAndWritesFlag();
    void imageAltWritesXObjectKey();
    void imageAltRefusalLeavesFileByteIdentical();
    void fieldTuFixPreservesRequiredFlag();
    void readRequiredFlagHandlesMissingFf();
    void commitFaultLeavesDestinationUntouched();

private:
    static QString make(const QTemporaryDir& dir, const QString& name,
                        const Opts& o) {
        const QString path = dir.filePath(name);
        if (!buildPdf(path, o)) return {};
        return path;
    }
};

void TestAccessibilityFixes::setLanguageWritesCatalogKey() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = make(tmp, "lang.pdf", {});
    QVERIFY(!pdf.isEmpty());

    gp::A11yFixRequest req;
    req.kind = gp::A11yFixKind::SetLanguage;
    req.language = QStringLiteral("de-DE");
    const gp::A11yFixOutcome out = gp::applyAccessibilityFix(pdf, req);
    QVERIFY2(out.ok, qPrintable(out.message));
    QVERIFY2(out.message.contains(QStringLiteral("de-DE")),
             qPrintable(out.message));

    // Independent reopen — the REAL catalog key, not in-memory state.
    PoDoFo::PdfMemDocument doc;
    doc.Load(pdf.toUtf8().constData());
    const PdfObject* lang = doc.GetCatalog().GetDictionary().FindKey(PdfName("Lang"));
    QVERIFY(lang != nullptr && lang->IsString());
    QCOMPARE(QString::fromUtf8(lang->GetString().GetString().data(),
                               static_cast<qsizetype>(lang->GetString().GetString().size())),
             QStringLiteral("de-DE"));
}

void TestAccessibilityFixes::displayDocTitleNeedsTitleAndWritesFlag() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Without /Info /Title the fix must REFUSE (nothing to display) and the
    // file must be untouched.
    const QString noTitle = make(tmp, "notitle.pdf", {});
    QVERIFY(!noTitle.isEmpty());
    const QByteArray before = readBytes(noTitle);
    gp::A11yFixRequest req;
    req.kind = gp::A11yFixKind::EnableDisplayDocTitle;
    const gp::A11yFixOutcome out = gp::applyAccessibilityFix(noTitle, req);
    QVERIFY(!out.ok);
    QVERIFY2(!out.message.isEmpty(), qPrintable(out.message));
    QCOMPARE(readBytes(noTitle), before);

    // With a title the fix writes /ViewerPreferences /DisplayDocTitle true.
    const QString titled = make(tmp, "titled.pdf", {false, true, false, false, false, false});
    QVERIFY(!titled.isEmpty());
    const gp::A11yFixOutcome ok = gp::applyAccessibilityFix(titled, req);
    QVERIFY2(ok.ok, qPrintable(ok.message));
    PoDoFo::PdfMemDocument doc;
    doc.Load(titled.toUtf8().constData());
    const PdfObject* vp = doc.GetCatalog().GetDictionary().FindKey(PdfName("ViewerPreferences"));
    vp = vp && vp->IsReference()
             ? &doc.GetObjects().MustGetObject(vp->GetReference()) : vp;
    QVERIFY(vp != nullptr && vp->IsDictionary());
    const PdfObject* ddt = vp->GetDictionary().FindKey(PdfName("DisplayDocTitle"));
    QVERIFY(ddt != nullptr && ddt->IsBool() && ddt->GetBool());
}

void TestAccessibilityFixes::imageAltWritesXObjectKey() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = make(tmp, "img.pdf", {});
    QVERIFY(!pdf.isEmpty());

    gp::A11yFixRequest req;
    req.kind = gp::A11yFixKind::SetImageAltText;
    req.page = 0;
    req.resourceName = QStringLiteral("Im0");
    req.text = QStringLiteral("Scanned receipt, total 42 euros");
    const gp::A11yFixOutcome out = gp::applyAccessibilityFix(pdf, req);
    QVERIFY2(out.ok, qPrintable(out.message));

    PoDoFo::PdfMemDocument doc;
    doc.Load(pdf.toUtf8().constData());
    auto& page = doc.GetPages().GetPageAt(0);
    const PdfObject* xobjs = page.GetResources().GetObject()
                                 .GetDictionary().FindKey(PdfName("XObject"));
    QVERIFY(xobjs != nullptr);
    if (xobjs->IsReference())
        xobjs = &doc.GetObjects().MustGetObject(xobjs->GetReference());
    const PdfObject* img = xobjs->GetDictionary().FindKey(PdfName("Im0"));
    QVERIFY(img != nullptr);
    if (img->IsReference())
        img = &doc.GetObjects().MustGetObject(img->GetReference());
    const PdfObject* alt = img->GetDictionary().FindKey(PdfName("Alt"));
    QVERIFY(alt != nullptr && alt->IsString());
    QCOMPARE(QString::fromUtf8(alt->GetString().GetString().data(),
                               static_cast<qsizetype>(alt->GetString().GetString().size())),
             req.text);
}

void TestAccessibilityFixes::imageAltRefusalLeavesFileByteIdentical() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = make(tmp, "badimg.pdf", {});
    QVERIFY(!pdf.isEmpty());
    const QByteArray before = readBytes(pdf);

    gp::A11yFixRequest req;
    req.kind = gp::A11yFixKind::SetImageAltText;
    req.page = 0;
    req.resourceName = QStringLiteral("NoSuchImage");
    req.text = QStringLiteral("irrelevant");
    const gp::A11yFixOutcome out = gp::applyAccessibilityFix(pdf, req);
    QVERIFY(!out.ok);
    QVERIFY(!out.message.isEmpty());
    // SafeSave contract: original byte-identical after a refused fix.
    QCOMPARE(readBytes(pdf), before);
}

void TestAccessibilityFixes::fieldTuFixPreservesRequiredFlag() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    for (const bool required : {false, true}) {
        Opts o;
        o.fieldRequired = required;
        const QString pdf = make(tmp, required ? "req.pdf" : "noreq.pdf", o);
        QVERIFY(!pdf.isEmpty());

        // The panel's exact fix path: read the bit, call the EXISTING seam.
        bool reqBit = true;
        QString err;
        QVERIFY(gp::readFieldRequiredFlag(pdf, QStringLiteral("f1"), &reqBit, &err));
        QCOMPARE(reqBit, required);

        FormManager forms;
        QVERIFY(forms.setFieldMetadata(pdf, QStringLiteral("f1"),
                                       QStringLiteral("Your full name"),
                                       reqBit, pdf));

        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        auto* acro = doc.GetAcroForm();
        QVERIFY(acro != nullptr);
        bool sawTu = false, sawFf = false;
        for (unsigned i = 0; i < acro->GetFieldCount(); ++i) {
            auto& f = acro->GetFieldAt(i);
            if (QString::fromStdString(f.GetFullName()) != QLatin1String("f1")) continue;
            const PdfDictionary& d = f.GetObject().GetDictionary();
            const PdfObject* tu = d.FindKey(PdfName("TU"));
            QVERIFY(tu != nullptr && tu->IsString());
            sawTu = true;
            const PdfObject* ff = d.FindKey(PdfName("Ff"));
            const int flags = (ff && ff->IsNumber()) ? static_cast<int>(ff->GetNumber()) : 0;
            QCOMPARE((flags & (1 << 1)) != 0, required);
            sawFf = true;
        }
        QVERIFY(sawTu);
        QVERIFY(sawFf);
    }
}

void TestAccessibilityFixes::readRequiredFlagHandlesMissingFf() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = make(tmp, "noff.pdf", {});
    QVERIFY(!pdf.isEmpty());
    bool required = true;
    QString err;
    QVERIFY(gp::readFieldRequiredFlag(pdf, QStringLiteral("f1"), &required, &err));
    QVERIFY(!required);  // absent /Ff = not required

    QVERIFY(!gp::readFieldRequiredFlag(pdf, QStringLiteral("missing"), &required, &err));
    QVERIFY(!err.isEmpty());
}

void TestAccessibilityFixes::commitFaultLeavesDestinationUntouched() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = make(tmp, "fault.pdf", {});
    QVERIFY(!pdf.isEmpty());
    const QByteArray before = readBytes(pdf);

    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
    gp::A11yFixRequest req;
    req.kind = gp::A11yFixKind::SetLanguage;
    req.language = QStringLiteral("fr-FR");
    const gp::A11yFixOutcome out = gp::applyAccessibilityFix(pdf, req);
    gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);

    QVERIFY(!out.ok);
    QCOMPARE(readBytes(pdf), before);  // atomicity: original never touched
}

#include "TestAccessibilityFixes.moc"
QTEST_MAIN(TestAccessibilityFixes)
