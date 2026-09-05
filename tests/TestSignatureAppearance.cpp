// SPDX-License-Identifier: Apache-2.0
// §9.7 P0: visible signature appearance for the cryptographic signing path
// (ETSI EN 319 142-6 §5.2 / Acrobat convention: one /AP /N form XObject drawn
// in the SAME incremental update as the /Contents digest; optional image left,
// text right; auto-fit with a ~6pt floor; Reason/Location dropped first;
// name-only below ~120x36pt; NO validation status / TSA time — ISO 32000-2
// §12.7.5.5 forbids it inside a field appearance).
//
// Two layers are pinned here:
//  1. The pure planning seam (SignatureManager::planSignatureAppearance):
//     deterministic measurer in, lines + font size out. No PDF needed.
//  2. The end-to-end signing path (signDocument with the real test_signer.p12):
//     the written PDF must carry an intact /AP /N form XObject whose stream
//     contains exactly the planned lines (and never forbidden ones).
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QImage>

#include "engines/SignatureManager.h"

#include <podofo/podofo.h>

#ifdef SOURCE_DIR
static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
static const QString kP12Path  = kFixtureDir + "/test_signer.p12";
static const QString kInputPdf = kFixtureDir + "/test_input.pdf";
static const QString kCaPath   = kFixtureDir + "/test_ca.pem";
static const QString kP12Pass  = QStringLiteral("test");

#define REQUIRE_FIXTURES() \
    do { \
        if (!QFileInfo::exists(kP12Path) || !QFileInfo::exists(kInputPdf) || !QFileInfo::exists(kCaPath)) { \
            QSKIP("Signing fixtures missing — skipping signature appearance test. " \
                  "Run tests/fixtures/signing/generate.bat (or generate_fixtures.cmake) to create them."); \
        } \
    } while(0)

namespace {

// A fixed claimed signing time so the Date line is byte-pinnable:
// "Date: 2026-09-05 14:30:00 UTC+02:00"
static QDateTime fixedClaimedTime()
{
    return QDateTime(QDate(2026, 9, 5), QTime(14, 30, 0), Qt::OffsetFromUTC, 2 * 3600);
}
static const char *kFixedDateLine = "Date: 2026-09-05 14:30:00 UTC+02:00";

// Deterministic measurer: width(text, fontSize) = k * fontSize * length.
// Monotone in both arguments, so every ladder decision is computable by hand.
static SignatureManager::AppearanceMeasureFn scaledMeasurer(double k)
{
    return [k](const QString &text, double fontSize) -> double {
        return k * fontSize * static_cast<double>(text.size());
    };
}

// Appearance streams render text as literal/hex strings possibly split by
// kerning arrays:  [(Rea)-3(son:)] Tj.  Keeping only letters and ':' makes
// the content-stream text robust to those splits.
static QString normalizeAppearanceText(const QByteArray &raw)
{
    QString out;
    const QString s = QString::fromLatin1(raw);
    for (const QChar &c : s) {
        if (c.isLetter() || c == QLatin1Char(':'))
            out.append(c);
    }
    return out;
}

struct ApInspection {
    bool found = false;          // a signature widget with /AP /N exists
    bool isFormXObject = false;  // /Subtype /Form
    bool hasBBox = false;
    QByteArray streamBytes;      // decoded N content stream
    bool hasImageXObject = false;// /Resources /XObject contains an /Image
};

// Reload `pdfPath` with PoDoFo and inspect the FIRST signature field that has
// an appearance. Returns .found == false when no /AP /N exists anywhere.
static ApInspection inspectSignatureAppearance(const QString &pdfPath)
{
    ApInspection result;
    PoDoFo::PdfMemDocument doc;
    doc.Load(pdfPath.toStdString());

    // Resolve a key that may live directly or behind an indirect reference.
    auto findKeyResolved = [&doc](const PoDoFo::PdfDictionary &dict,
                                  const char *key) -> const PoDoFo::PdfObject * {
        const PoDoFo::PdfObject *obj = dict.FindKey(PoDoFo::PdfName(key));
        if (obj && obj->IsReference())
            obj = &doc.GetObjects().MustGetObject(obj->GetReference());
        return obj;
    };

    for (auto *field : doc.GetFieldsIterator()) {
        if (field->GetType() != PoDoFo::PdfFieldType::Signature)
            continue;

        const PoDoFo::PdfObject *apObj = field->GetDictionary().FindKey(PoDoFo::PdfName("AP"));
        if (!apObj && field->GetWidget())
            apObj = field->GetWidget()->GetDictionary().FindKey(PoDoFo::PdfName("AP"));
        if (!apObj)
            continue;
        if (apObj->IsReference())
            apObj = &doc.GetObjects().MustGetObject(apObj->GetReference());
        if (!apObj->IsDictionary())
            continue;

        const PoDoFo::PdfObject *nObj = findKeyResolved(apObj->GetDictionary(), "N");
        if (!nObj || !nObj->IsDictionary())
            continue;

        result.found = true;
        const PoDoFo::PdfDictionary &nDict = nObj->GetDictionary();

        auto *subtype = nDict.FindKey(PoDoFo::PdfName("Subtype"));
        result.isFormXObject = subtype && subtype->IsName() &&
                               subtype->GetName().GetString() == "Form";
        result.hasBBox = nDict.HasKey(PoDoFo::PdfName("BBox"));

        if (const PoDoFo::PdfObjectStream *stream = nObj->GetStream()) {
            const PoDoFo::charbuff copy = stream->GetCopy();
            result.streamBytes = QByteArray(copy.data(), static_cast<int>(copy.size()));
        }

        if (const PoDoFo::PdfObject *res = findKeyResolved(nDict, "Resources")) {
            const PoDoFo::PdfObject *xo = findKeyResolved(res->GetDictionary(), "XObject");
            if (xo && xo->IsDictionary()) {
                for (const auto &entry : xo->GetDictionary()) {
                    const PoDoFo::PdfObject *val = &entry.second;
                    if (val && val->IsReference())
                        val = &doc.GetObjects().MustGetObject(val->GetReference());
                    if (val && val->IsDictionary()) {
                        auto *st = val->GetDictionary().FindKey(PoDoFo::PdfName("Subtype"));
                        if (st && st->IsName() && st->GetName().GetString() == "Image")
                            result.hasImageXObject = true;
                    }
                }
            }
        }
        break; // first signature field with an appearance is enough
    }
    return result;
}

} // namespace

class TestSignatureAppearance : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString outPath(const QString &name) const { return m_tmpDir.filePath(name); }

private slots:
    void initTestCase()
    {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    // ------------------------------------------------------------------
    // Layer 1 — the pure planning seam (no PDF, deterministic measurer)
    // ------------------------------------------------------------------

    void roomyRectKeepsAllLines()
    {
        auto plan = SignatureManager::planSignatureAppearance(
            300.0, 100.0, QStringLiteral("TestSigner"), fixedClaimedTime(),
            QStringLiteral("I approve"), QStringLiteral("Berlin"),
            /*hasSignatureImage=*/false, scaledMeasurer(0.6));

        QCOMPARE(plan.lines.size(), 4);
        QCOMPARE(plan.lines.at(0), QStringLiteral("Digitally signed by TestSigner"));
        QCOMPARE(plan.lines.at(1), QString::fromLatin1(kFixedDateLine));
        QCOMPARE(plan.lines.at(2), QStringLiteral("Reason: I approve"));
        QCOMPARE(plan.lines.at(3), QStringLiteral("Location: Berlin"));
        QVERIFY(plan.fontSize >= 6.0 && plan.fontSize <= 9.0);
        QVERIFY(!plan.nameOnly);
        QVERIFY(!plan.imageLeft);
    }

    void reasonAndLocationOnlyWhenSet()
    {
        auto plan = SignatureManager::planSignatureAppearance(
            300.0, 100.0, QStringLiteral("TestSigner"), fixedClaimedTime(),
            QString(), QString(), false, scaledMeasurer(0.6));

        QCOMPARE(plan.lines.size(), 2);
        QCOMPARE(plan.lines.at(0), QStringLiteral("Digitally signed by TestSigner"));
        QCOMPARE(plan.lines.at(1), QString::fromLatin1(kFixedDateLine));
        for (const QString &line : plan.lines) {
            QVERIFY2(!line.startsWith(QStringLiteral("Reason:")), "Reason must be omitted when unset");
            QVERIFY2(!line.startsWith(QStringLiteral("Location:")), "Location must be omitted when unset");
        }
    }

    void autoFitDropsReasonFirstThenLocation()
    {
        // k=0.7 measurer, rect 180x40 (not tiny): the long Reason line cannot
        // fit at any size >= 6pt, so Reason is dropped; the rest fits at 7pt.
        auto dropReason = SignatureManager::planSignatureAppearance(
            180.0, 40.0, QStringLiteral("TestSigner"), fixedClaimedTime(),
            QStringLiteral("A reason line long enough to never fit the box"),
            QStringLiteral("Berlin"), false, scaledMeasurer(0.7));
        QCOMPARE(dropReason.lines.size(), 3);
        QVERIFY(!dropReason.nameOnly);
        QCOMPARE(dropReason.lines.at(0), QStringLiteral("Digitally signed by TestSigner"));
        QVERIFY(dropReason.lines.at(1).startsWith(QStringLiteral("Date: ")));
        QCOMPARE(dropReason.lines.at(2), QStringLiteral("Location: Berlin"));
        QCOMPARE(dropReason.fontSize, 7.0);

        // Same rect, Location also long: Location drops too; name+date remain.
        auto dropBoth = SignatureManager::planSignatureAppearance(
            180.0, 40.0, QStringLiteral("TestSigner"), fixedClaimedTime(),
            QStringLiteral("A reason line long enough to never fit the box"),
            QStringLiteral("A location line long enough to never fit too"),
            false, scaledMeasurer(0.7));
        QCOMPARE(dropBoth.lines.size(), 2);
        QVERIFY(!dropBoth.nameOnly);
        QCOMPARE(dropBoth.lines.at(0), QStringLiteral("Digitally signed by TestSigner"));
        QVERIFY(dropBoth.lines.at(1).startsWith(QStringLiteral("Date: ")));
    }

    void tinyRectIsNameOnly()
    {
        // Below ~120x36pt the identity line only is rendered (no date,
        // reason, or location), shrinking toward the 4pt absolute floor.
        // "Digitally signed by TestSigner" is 30 chars: with the k=0.6
        // measurer the first fitting size in the 92pt text width is 5pt
        // (0.6*5*30 = 90 <= 92; 0.6*6*30 = 108 > 92).
        auto plan = SignatureManager::planSignatureAppearance(
            100.0, 30.0, QStringLiteral("TestSigner"), fixedClaimedTime(),
            QStringLiteral("I approve"), QStringLiteral("Berlin"),
            false, scaledMeasurer(0.6));

        QVERIFY(plan.nameOnly);
        QCOMPARE(plan.lines.size(), 1);
        QCOMPARE(plan.lines.at(0), QStringLiteral("Digitally signed by TestSigner"));
        QCOMPARE(plan.fontSize, 5.0);
    }

    void imageShiftsTextRight()
    {
        auto plan = SignatureManager::planSignatureAppearance(
            300.0, 100.0, QStringLiteral("TestSigner"), fixedClaimedTime(),
            QStringLiteral("I approve"), QStringLiteral("Berlin"),
            /*hasSignatureImage=*/true, scaledMeasurer(0.6));

        QVERIFY(plan.imageLeft);
        QCOMPARE(plan.lines.size(), 4); // roomy rect still holds every line
        QCOMPARE(plan.lines.at(2), QStringLiteral("Reason: I approve"));
    }

    void emptyNameOmitsNameLine()
    {
        auto plan = SignatureManager::planSignatureAppearance(
            300.0, 100.0, QString(), fixedClaimedTime(),
            QString(), QString(), false, scaledMeasurer(0.6));

        QVERIFY(!plan.lines.isEmpty());
        QCOMPARE(plan.lines.first(), QString::fromLatin1(kFixedDateLine));
    }

    void neverReturnsNothingWhenTextRequested()
    {
        // Absurd measurer: nothing ever "fits". The planner must still emit
        // the identity line at the absolute floor rather than empty output.
        auto plan = SignatureManager::planSignatureAppearance(
            300.0, 100.0, QStringLiteral("TestSigner"), fixedClaimedTime(),
            QString(), QString(), false, scaledMeasurer(100000.0));

        QCOMPARE(plan.lines.size(), 1);
        QCOMPARE(plan.lines.at(0), QStringLiteral("Digitally signed by TestSigner"));
        QCOMPARE(plan.fontSize, 4.0);
        QVERIFY(plan.nameOnly);
    }

    // ------------------------------------------------------------------
    // Layer 2 — end-to-end: the signing path must write the appearance
    // into the SAME incremental update as the digest.
    // ------------------------------------------------------------------

    void signAddsFormAppearanceWithAllLines()
    {
        REQUIRE_FIXTURES();

        SignatureManager mgr;
        SignOutcome outcome = mgr.signDocument(
            kInputPdf, outPath("signed_appear.pdf"), kP12Path, kP12Pass,
            QStringLiteral("I approve this document"), QStringLiteral("Test Location"));
        QVERIFY2(outcome == SignOutcome::Success, "signDocument must succeed with valid P12");

        ApInspection ap = inspectSignatureAppearance(outPath("signed_appear.pdf"));
        QVERIFY2(ap.found, "signature field must carry an /AP /N after signing");
        QVERIFY2(ap.isFormXObject, "/AP /N must be a form XObject (ETSI EN 319 142-6 §5.2)");
        QVERIFY(ap.hasBBox);
        QVERIFY2(!ap.streamBytes.isEmpty(), "AP content stream must not be empty");

        const QString norm = normalizeAppearanceText(ap.streamBytes);
        QVERIFY2(norm.contains(QStringLiteral("DigitallysignedbyTestSigner")),
                 "AP must show the certificate CN");
        QVERIFY2(norm.contains(QStringLiteral("Iapprovethisdocument")),
                 "AP must contain the Reason line when set");
        QVERIFY2(norm.contains(QStringLiteral("Location:TestLocation")),
                 "AP must contain the Location line when set");
        QVERIFY2(norm.contains(QStringLiteral("Date:")), "AP must contain the claimed date line");

        // ISO 32000-2 §12.7.5.5: no validation status in the appearance; the
        // TSA time is validation info and must never be rendered either.
        QVERIFY2(!norm.contains(QStringLiteral("Timestamp")), "no TSA timestamp in AP");
        QVERIFY2(!norm.contains(QStringLiteral("TSA")), "no TSA marker in AP");
        QVERIFY2(!norm.contains(QStringLiteral("Valid")), "no validation status in AP");

        // PoDoFo reload finds the AP intact AND the signature integrity holds
        // (the appearance was written inside the signed byte range).
        SignatureManager verifier;
        const QList<SignatureInfo> infos = verifier.validateSignatures(outPath("signed_appear.pdf"));
        QVERIFY(!infos.isEmpty());
        bool intact = false;
        for (const SignatureInfo &info : infos)
            intact = intact || info.integrityIntact;
        QVERIFY2(intact, "signature integrity must remain intact with the AP added");
    }

    void signWithoutReasonLocationOmitsLines()
    {
        REQUIRE_FIXTURES();

        SignatureManager mgr;
        SignOutcome outcome = mgr.signDocument(
            kInputPdf, outPath("signed_plain.pdf"), kP12Path, kP12Pass,
            QString(), QString());
        QVERIFY2(outcome == SignOutcome::Success, "signDocument must succeed with valid P12");

        ApInspection ap = inspectSignatureAppearance(outPath("signed_plain.pdf"));
        QVERIFY2(ap.found, "signature field must carry an /AP /N after signing");
        const QString norm = normalizeAppearanceText(ap.streamBytes);
        QVERIFY2(norm.contains(QStringLiteral("DigitallysignedbyTestSigner")),
                 "AP must show the certificate CN");
        QVERIFY2(!norm.contains(QStringLiteral("Reason")), "Reason must be omitted when unset");
        QVERIFY2(!norm.contains(QStringLiteral("Location")), "Location must be omitted when unset");
    }

    void smallRectFieldDropsToNameOnly()
    {
        REQUIRE_FIXTURES();

        // Build a PDF whose unsigned signature field is deliberately tiny
        // (90x30 < 120x36): the auto-fit ladder must fall back to the
        // identity line only, even though reason/location were supplied.
        QString tinyPdf = outPath("tiny_field_input.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(kInputPdf.toStdString());
            PoDoFo::PdfPage &page = doc.GetPages().GetPageAt(0);
            page.CreateField<PoDoFo::PdfSignature>("TinySig", PoDoFo::Rect(10, 10, 90, 30));
            doc.Save(tinyPdf.toStdString());
        }

        SignatureManager mgr;
        SignOutcome outcome = mgr.signDocument(
            tinyPdf, outPath("signed_tiny.pdf"), kP12Path, kP12Pass,
            QStringLiteral("I approve this document"), QStringLiteral("Test Location"));
        QVERIFY2(outcome == SignOutcome::Success, "signDocument must reuse the tiny field and succeed");

        ApInspection ap = inspectSignatureAppearance(outPath("signed_tiny.pdf"));
        QVERIFY2(ap.found, "tiny signature field must carry an /AP /N after signing");
        const QString norm = normalizeAppearanceText(ap.streamBytes);
        QVERIFY2(norm.contains(QStringLiteral("DigitallysignedbyTestSigner")),
                 "name-only fallback must still show the CN");
        QVERIFY2(!norm.contains(QStringLiteral("Reason")), "tiny rect must drop the Reason line");
        QVERIFY2(!norm.contains(QStringLiteral("Location")), "tiny rect must drop the Location line");
        QVERIFY2(!norm.contains(QStringLiteral("Date")), "tiny rect must drop the Date line");
    }

    void pendingImageEmbedsImageXObject()
    {
        REQUIRE_FIXTURES();

        // The dialog->engine handoff is a consume-once pending slot. A picked
        // image must be embedded as an /Image XObject resource of the AP and
        // the slot must be drained afterwards.
        QImage img(30, 15, QImage::Format_ARGB32);
        img.fill(QColor(200, 30, 30));
        SignatureManager::setPendingAppearanceImage(img);

        SignatureManager mgr;
        SignOutcome outcome = mgr.signDocument(
            kInputPdf, outPath("signed_image.pdf"), kP12Path, kP12Pass,
            QStringLiteral("I approve this document"), QStringLiteral("Test Location"));
        QVERIFY2(outcome == SignOutcome::Success, "signDocument must succeed with a pending image");

        ApInspection ap = inspectSignatureAppearance(outPath("signed_image.pdf"));
        QVERIFY2(ap.found, "signature field must carry an /AP /N after signing");
        QVERIFY2(ap.hasImageXObject, "pending signature image must be embedded as an /Image XObject");

        QVERIFY2(SignatureManager::takePendingAppearanceImage().isNull(),
                 "pending slot must be drained by the next signing call");
    }
};

QTEST_MAIN(TestSignatureAppearance)
#include "TestSignatureAppearance.moc"
