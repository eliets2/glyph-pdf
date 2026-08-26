// SPDX-License-Identifier: Apache-2.0
// §9.1 P0 DEFECT 2(A) regression test: a malicious PDF /URI must never be
// handed to QDesktopServices::openUrl. Only http/https/mailto are safe to
// open on a single click; file://, javascript:, data:, ms-msdt:, UNC, etc.
// must be rejected.
#include <QtTest>
#include "ui/PdfViewerWidget.h"

class TestLinkSchemeSafety : public QObject {
    Q_OBJECT
private slots:
    void allowsSafeSchemes();
    void rejectsUnsafeSchemes();
    void schemeComparisonIsCaseInsensitive();
};

void TestLinkSchemeSafety::allowsSafeSchemes()
{
    QVERIFY(PdfViewerWidget::isSafeLinkScheme(QStringLiteral("http://example.com/a")));
    QVERIFY(PdfViewerWidget::isSafeLinkScheme(QStringLiteral("https://example.com/a?b=1")));
    QVERIFY(PdfViewerWidget::isSafeLinkScheme(QStringLiteral("mailto:user@example.com")));
    QVERIFY(PdfViewerWidget::isSafeLinkScheme(QStringLiteral("HTTPS://EXAMPLE.COM")));
}

void TestLinkSchemeSafety::rejectsUnsafeSchemes()
{
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("file:///etc/passwd")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("file://server/share")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("javascript:alert(1)")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("data:text/html,<script>1</script>")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("ms-msdt:/id/PCWDiagnostic")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("\\\\server\\share\\file")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("ftp://example.com/file")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("not a url")));
}

void TestLinkSchemeSafety::schemeComparisonIsCaseInsensitive()
{
    QVERIFY(PdfViewerWidget::isSafeLinkScheme(QStringLiteral("HTTP://EXAMPLE.COM")));
    QVERIFY(PdfViewerWidget::isSafeLinkScheme(QStringLiteral("MailTo:user@example.com")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("FILE:///etc/passwd")));
    QVERIFY(!PdfViewerWidget::isSafeLinkScheme(QStringLiteral("JAVASCRIPT:alert(1)")));
}

QTEST_MAIN(TestLinkSchemeSafety)
#include "TestLinkSchemeSafety.moc"
