// SPDX-License-Identifier: Apache-2.0
// §9.4 honesty regression test: the interactive OCR Accept flow persists a
// ONE-PAGE MRC PDF/A (runOcr recognises the current page only). The save
// dialog title and the success status must say "current page only" for
// multi-page documents instead of silently implying a whole-document
// searchable copy. Single-page documents need no scope note.
#include <QtTest/QtTest>
#include "shell/controllers/EditController.h"

using gp::EditController;

class TestOcrAcceptScope : public QObject {
    Q_OBJECT
private slots:
    void multiPageDialogTitleSaysCurrentPageOnly();
    void multiPageStatusSaysCurrentPageOnly();
    void singlePageDialogNeedsNoScopeNote();
    void singlePageStatusNeedsNoScopeNote();
    void scopeNoteCarriesPageNumbers();
};

void TestOcrAcceptScope::multiPageDialogTitleSaysCurrentPageOnly() {
    const QString title = EditController::ocrSaveDialogTitle(40, 2);
    QVERIFY(title.contains(QStringLiteral("Current Page Only"), Qt::CaseInsensitive));
    QVERIFY(title.contains(QStringLiteral("Searchable (OCR) Copy")));
}

void TestOcrAcceptScope::multiPageStatusSaysCurrentPageOnly() {
    const QString status = EditController::ocrSavedStatus(40, 2, QStringLiteral("doc_ocr.pdf"));
    QVERIFY(status.contains(QStringLiteral("current page"), Qt::CaseInsensitive));
    QVERIFY(status.contains(QStringLiteral("only"), Qt::CaseInsensitive));
    QVERIFY(status.contains(QStringLiteral("doc_ocr.pdf")));
}

void TestOcrAcceptScope::singlePageDialogNeedsNoScopeNote() {
    const QString title = EditController::ocrSaveDialogTitle(1, 0);
    QCOMPARE(title, QStringLiteral("Save Searchable (OCR) Copy"));
}

void TestOcrAcceptScope::singlePageStatusNeedsNoScopeNote() {
    const QString status = EditController::ocrSavedStatus(1, 0, QStringLiteral("doc_ocr.pdf"));
    QCOMPARE(status, QStringLiteral("Searchable copy saved: doc_ocr.pdf"));
}

void TestOcrAcceptScope::scopeNoteCarriesPageNumbers() {
    // 1-based page numbers in the user-facing strings (page index 2 → page 3).
    const QString title = EditController::ocrSaveDialogTitle(40, 2);
    QVERIFY(title.contains(QStringLiteral("3 of 40")));
    const QString status = EditController::ocrSavedStatus(40, 2, QStringLiteral("x.pdf"));
    QVERIFY(status.contains(QStringLiteral("page 3 of 40")));
}

QTEST_MAIN(TestOcrAcceptScope)
#include "TestOcrAcceptScope.moc"
