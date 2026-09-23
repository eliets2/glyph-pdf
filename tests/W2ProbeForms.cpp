// SWEEP-W2 guarantee-verification probe — forms family (R18(f) keystroke tier).
// NOT a lane artifact. Written 2026-09-20 by the W2 verification wave.
//
// Independence vs TestFormKeystroke (which drives the PANEL with real
// QTest::keyClicks): this probe drives the ENGINE seam
// IFormManager::runKeystrokeEvent DIRECTLY over a saved fixture, with this
// probe's own scripts and event shapes, then closes the loop at the
// user-trust boundary: the gate-transformed value is persisted through
// applyFieldSnapshot and re-read from the SAVED artifact by an independent
// fresh-PoDoFo walk of the field dictionary.
//
// Contracts probed (falsifiable):
//   C1 no /AA /K      → ran=false, typing stands (no gate exists)
//   C2 reject script  → allowed=false, fail-closed, field-attributed failure
//   C3 transform      → Acrobat merge (change spliced at [selStart,selEnd))
//                       then transformed; valueToApply carries it
//   C4 broken script  → allowed=false, honest failure (never silent allow)
//   C5 artifact loop  → the transformed value, applied through the
//                       transactional seam, is what the SAVED artifact carries
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPainter>

#include <podofo/podofo.h>

#include "core/interfaces/IFormManager.h"
#include "engines/FormManager.h"

// IFormManager / FormManager / FormJsFailure live in the GLOBAL namespace.

namespace {

struct FieldSpec {
    QString name;
    QString keystrokeScript; // /AA /K body (may be empty)
    QString initial;
};

// One-page form fixture with optional /AA /K scripts (own writer; different
// geometry/field names from the lane's fixture).
QString makeFormPdf(const QString& path, const QList<FieldSpec>& fields)
{
    const QString base = path + ".base.pdf";
    {
        QPdfWriter writer(base);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        p.drawText(60, 80, QStringLiteral("w2 forms probe fixture"));
        p.end();
    }
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(base.toUtf8().constData());
        PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(0);
        double y = 700;
        for (const FieldSpec& spec : fields) {
            const PoDoFo::Rect rect(80, y, 220, 18);
            auto& field = page.CreateField<PoDoFo::PdfTextBox>(
                spec.name.toStdString(), rect);
            dynamic_cast<PoDoFo::PdfTextBox*>(&field)->SetText(
                PoDoFo::PdfString(spec.initial.toStdString()));
            if (!spec.keystrokeScript.isEmpty()) {
                PoDoFo::PdfDictionary action;
                action.AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
                action.AddKey(PoDoFo::PdfName("JS"),
                              PoDoFo::PdfString(spec.keystrokeScript.toStdString()));
                PoDoFo::PdfDictionary aa;
                aa.AddKey(PoDoFo::PdfName("K"), action);
                field.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aa);
            }
            y -= 50;
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "fixture build failed:" << e.what();
        return {};
    }
    return path;
}

// Independent read: fresh PoDoFo walk of the AcroForm field tree for /V.
QString readFieldValueFromArtifact(const QString& pdfPath, const QString& fieldName)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfPath.toUtf8().constData());
        auto* acro = doc.GetAcroForm();
        if (acro == nullptr) return QString();
        const unsigned n = acro->GetFieldCount();
        for (unsigned i = 0; i < n; ++i) {
            auto& f = acro->GetFieldAt(i);
            const auto name = f.GetName();
            if (name.has_value() && QString::fromStdString(std::string(name->GetString())) == fieldName) {
                auto v = f.GetDictionary().FindKey("V");
                if (v && v->IsString())
                    return QString::fromStdString(std::string(v->GetString()));
                return QString();
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "artifact read failed:" << e.what();
    }
    return QString();
}

} // namespace

class W2ProbeForms : public QObject {
    Q_OBJECT

private slots:
    void keystrokeTierContractsAndArtifactLoop()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString rejectScript  = QStringLiteral("event.rc = false;");
        const QString transformScript =
            QStringLiteral("event.value = event.value.toUpperCase();");
        const QString brokenScript  = QStringLiteral("this is not ( valid js");

        const QString pdf = makeFormPdf(tmp.filePath("w2-form.pdf"), {
            { QStringLiteral("plainField"),   QString(),        QString() },
            { QStringLiteral("rejectField"),  rejectScript,     QString() },
            { QStringLiteral("upperField"),   transformScript,  QString() },
            { QStringLiteral("brokenField"),  brokenScript,     QString() },
        });
        QVERIFY(!pdf.isEmpty());

        FormManager forms;
        FormJsFailure failure;

        // C1: no script → ran=false, typing stands.
        {
            const auto r = forms.runKeystrokeEvent(
                pdf, QStringLiteral("plainField"),
                QStringLiteral("abc"), QStringLiteral("d"), 3, 3, &failure);
            QVERIFY2(!r.ran, "a field without /AA /K must NOT run a gate");
            QVERIFY2(r.allowed, "typing stands when no gate exists");
            QVERIFY2(r.valueToApply.isEmpty(), "no transformation without a script");
        }

        // C2: reject script → fail closed.
        {
            const auto r = forms.runKeystrokeEvent(
                pdf, QStringLiteral("rejectField"),
                QStringLiteral("ab"), QStringLiteral("X"), 1, 2, &failure);
            QVERIFY2(r.ran, "the /AA /K gate must run when the script exists");
            QVERIFY2(!r.allowed,
                     "rc=false must REJECT the keystroke (fail closed)");
            QVERIFY2(!failure.fieldName.isEmpty() || !failure.kind.isEmpty(),
                     "the rejection must carry a field-attributed reason");
        }

        // C3: transform script with Acrobat merge semantics. valueBefore "ab",
        // change "X" replacing [1,2) → merged "aXb" → upper "AXB".
        {
            const auto r = forms.runKeystrokeEvent(
                pdf, QStringLiteral("upperField"),
                QStringLiteral("ab"), QStringLiteral("X"), 1, 2, &failure);
            QVERIFY2(r.ran, "the /AA /K gate must run when the script exists");
            QVERIFY2(r.allowed, "the transform script must allow the edit");
            QVERIFY2(r.valueToApply == QLatin1String("AXB"),
                     qPrintable(QStringLiteral("AFMergeChange splice+transform: "
                                              "expected 'AXB', got '%1'")
                                    .arg(r.valueToApply)));
        }

        // C4: broken script → fail closed with an honest failure.
        {
            const auto r = forms.runKeystrokeEvent(
                pdf, QStringLiteral("brokenField"),
                QStringLiteral("ab"), QStringLiteral("X"), 1, 2, &failure);
            QVERIFY2(r.ran, "a present but broken script still ran");
            QVERIFY2(!r.allowed,
                     "any script failure must REJECT the keystroke (fail closed)");
        }

        // C5: the artifact loop — apply the gate-transformed value through the
        // transactional seam and re-read the SAVED file independently.
        {
            const auto r = forms.runKeystrokeEvent(
                pdf, QStringLiteral("upperField"),
                QString(), QStringLiteral("w2 gate"), 0, 0, &failure);
            QVERIFY(r.ran && r.allowed);
            const QString transformed =
                r.valueToApply.isEmpty() ? QStringLiteral("w2 gate")
                                         : r.valueToApply;

            auto snap = forms.captureFieldSnapshot(
                pdf, QStringLiteral("upperField"));
            QVERIFY(snap.found);
            snap.value = transformed;
            snap.valuePresent = true;
            const QString out = tmp.filePath("w2-form-saved.pdf");
            QList<FormJsFailure> jsFailures;
            QVERIFY2(forms.applyFieldSnapshot(pdf, snap, out, &jsFailures),
                     "applyFieldSnapshot must commit the gated value");
            QVERIFY(QFile::exists(out));
            const QString persisted =
                readFieldValueFromArtifact(out, QStringLiteral("upperField"));
            QVERIFY2(persisted == transformed,
                     qPrintable(QStringLiteral("SAVED artifact /V must equal the "
                                              "gate-transformed value '%1', got '%2'")
                                    .arg(transformed, persisted)));
        }
    }
};

#include "W2ProbeForms.moc"
QTEST_MAIN(W2ProbeForms)
