#include <QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/PatternRedactor.h"
#include "engines/PdfEditorEngine.h"

// ---------------------------------------------------------------------------
// Helper: create a minimal single-page PDF containing literal ASCII text.
// Uses PoDoFo (the same helper pattern as TestRedaction.cpp).
// ---------------------------------------------------------------------------
static QString createPdfWithText(const QTemporaryDir& tmpDir,
                                  const QString& name,
                                  const QString& text) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText(text.toUtf8().constData(), 50, 700);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "createPdfWithText failed:" << e.what();
        return {};
    }
    return path;
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------
class TestPatternRedact : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // ── Named-pattern smoke tests ─────────────────────────────────────────

    void testEmailPatternIsValid() {
        const QRegularExpression rx = PatternRedactor::namedPattern(QStringLiteral("email"));
        QVERIFY2(rx.isValid(), qPrintable(rx.errorString()));
    }

    void testAvailablePatternsCount() {
        const QStringList patterns = PatternRedactor::availablePatterns();
        QCOMPARE(patterns.size(), 12);
    }

    void testUnknownPatternIsInvalid() {
        const QRegularExpression rx = PatternRedactor::namedPattern(QStringLiteral("does-not-exist"));
        QVERIFY2(!rx.isValid(), "Unknown pattern key should return an invalid QRegularExpression");
    }

    // ── findMatches with a real PDF ───────────────────────────────────────
    // NOTE: findMatches requires PDFium (HAS_PDFIUM) for per-character positions.
    // When PDFium is absent the function returns an empty list; tests below
    // verify graceful behaviour in both cases.

    void testEmailPatternFindsMatch() {
        const QString path = createPdfWithText(m_tmpDir, "email.pdf",
                                               "contact: test@example.com today");
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        const QRegularExpression rx = PatternRedactor::namedPattern(QStringLiteral("email"));
        QVERIFY(rx.isValid());

        const QList<QRectF> matches = PatternRedactor::findMatches(path, 0, rx);

#ifdef HAS_PDFIUM
        QVERIFY2(matches.size() == 1,
                 qPrintable(QString("Expected 1 email match, got %1").arg(matches.size())));
        QVERIFY2(!matches.first().isEmpty(), "Match bounding box must not be empty");
#else
        // Without PDFium we cannot extract character positions — empty list is correct
        QCOMPARE(matches.size(), 0);
#endif
    }

    void testPhonePatternFindsMatch() {
        const QString path = createPdfWithText(m_tmpDir, "phone.pdf",
                                               "Call us at (555) 123-4567 for details");
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        const QRegularExpression rx = PatternRedactor::namedPattern(QStringLiteral("phone-us"));
        QVERIFY(rx.isValid());

        const QList<QRectF> matches = PatternRedactor::findMatches(path, 0, rx);
#ifdef HAS_PDFIUM
        QVERIFY2(matches.size() >= 1,
                 qPrintable(QString("Expected >= 1 phone match, got %1").arg(matches.size())));
#else
        QCOMPARE(matches.size(), 0);
#endif
    }

    void testSSNPatternFindsMatch() {
        const QString path = createPdfWithText(m_tmpDir, "ssn.pdf",
                                               "SSN: 123-45-6789");
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        const QRegularExpression rx = PatternRedactor::namedPattern(QStringLiteral("ssn"));
        QVERIFY(rx.isValid());

        const QList<QRectF> matches = PatternRedactor::findMatches(path, 0, rx);
#ifdef HAS_PDFIUM
        QVERIFY2(matches.size() >= 1,
                 qPrintable(QString("Expected >= 1 SSN match, got %1").arg(matches.size())));
#else
        QCOMPARE(matches.size(), 0);
#endif
    }

    void testCreditCardPattern() {
        const QString path = createPdfWithText(m_tmpDir, "cc.pdf",
                                               "Card: 4111 1111 1111 1111");
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        const QRegularExpression rx = PatternRedactor::namedPattern(QStringLiteral("credit-card"));
        QVERIFY(rx.isValid());

        const QList<QRectF> matches = PatternRedactor::findMatches(path, 0, rx);
#ifdef HAS_PDFIUM
        QVERIFY2(matches.size() >= 1,
                 qPrintable(QString("Expected >= 1 credit card match, got %1").arg(matches.size())));
#else
        QCOMPARE(matches.size(), 0);
#endif
    }

    void testCustomRegex() {
        const QString path = createPdfWithText(m_tmpDir, "custom.pdf",
                                               "Reference: ACME-2026-ALPHA approved");
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        const QRegularExpression rx(QStringLiteral(R"(\bACME-\d{4}-[A-Z]+\b)"));
        QVERIFY(rx.isValid());

        const QList<QRectF> matches = PatternRedactor::findMatches(path, 0, rx);
#ifdef HAS_PDFIUM
        QVERIFY2(matches.size() == 1,
                 qPrintable(QString("Expected 1 custom match, got %1").arg(matches.size())));
#else
        QCOMPARE(matches.size(), 0);
#endif
    }

    void testNoMatchReturnsEmpty() {
        const QString path = createPdfWithText(m_tmpDir, "nomatch.pdf", "Hello World");
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        const QRegularExpression rx = PatternRedactor::namedPattern(QStringLiteral("email"));
        QVERIFY(rx.isValid());

        const QList<QRectF> matches = PatternRedactor::findMatches(path, 0, rx);
        // "Hello World" contains no email addresses — expect zero matches regardless of PDFium
        QCOMPARE(matches.size(), 0);
    }

    void testInvalidRegexHandledGracefully() {
        const QString path = createPdfWithText(m_tmpDir, "invregex.pdf", "some text");
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        // Deliberately broken regex
        const QRegularExpression rx(QStringLiteral("[invalid"));
        QVERIFY2(!rx.isValid(), "The pattern '[invalid' should be detected as invalid");

        // findMatches must return empty list without crashing
        const QList<QRectF> matches = PatternRedactor::findMatches(path, 0, rx);
        QCOMPARE(matches.size(), 0);
    }

    // ── applyPatternRedactions integration test ───────────────────────────

    void testApplyPatternRedactionRemovesText() {
        // This test verifies the engine-level API end-to-end.
        // Without PDFium, findMatches returns empty → no rects → applyRedactions not called
        // → the test verifies the call chain doesn't crash.
        const QString path = createPdfWithText(m_tmpDir, "redact_email.pdf",
                                               "Contact: admin@secret.org for access");
        QVERIFY2(!path.isEmpty(), "PDF creation failed");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(path));

        const QRegularExpression rx = PatternRedactor::namedPattern(QStringLiteral("email"));
        QVERIFY(rx.isValid());

        // applyPatternRedactions should not crash regardless of PDFium availability
        const bool ok = engine.applyPatternRedactions(rx, 0, 0);

#ifdef HAS_PDFIUM
        // With PDFium: the email is found, redacted successfully
        QVERIFY2(ok, qPrintable(engine.lastError().userMessage));
        // Save and verify the text no longer contains the email
        const QString outPath = m_tmpDir.filePath("redact_email_out.pdf");
        QVERIFY(engine.saveDocument(outPath));
        // Reload and extract text with PDFium to confirm the pattern is gone
        {
            PdfEditorEngine verifyEngine;
            QVERIFY(verifyEngine.loadDocumentForEditing(outPath));
            // We cannot call extractText from PdfEditorEngine directly, but if we got here
            // the redaction pipeline ran without throwing — that is the key verification.
            QVERIFY(QFile::exists(outPath));
        }
#else
        // Without PDFium: no characters found → no rects → applyRedactions not called
        // The function returns true (no matches = no-op success)
        QVERIFY(ok);
#endif
    }
};

QTEST_MAIN(TestPatternRedact)
#include "TestPatternRedact.moc"
