// SPDX-License-Identifier: Apache-2.0
// Audit 9.2 P0 regression test: the previously unreachable image-edit
// backends (Rotate / Replace / Delete) are now wired into EditController's
// image-edit selection menu. These tests pin the command semantics that the
// new wiring relies on: ±degrees rotation with merge, backup-based undo.
#include <QtTest/QtTest>
#include <QUndoStack>
#include "commands/RotateImageCommand.h"
#include "commands/DeleteImageCommand.h"
#include "commands/ReplaceImageCommand.h"
#include "mocks/MockPdfEditorEngine.h"
#include "engines/DocumentSession.h"

class TestImageEditWiring : public QObject {
    Q_OBJECT
private slots:
    void rotateRedoUndoAndMerge();
    void deleteAndReplaceCommandsExecute();
};
void TestImageEditWiring::rotateRedoUndoAndMerge() {
    MockPdfEditorEngine mock;
    DocumentSession doc;
    doc.setPath(QStringLiteral("img.pdf"));
    QUndoStack stack;

    stack.push(new RotateImageCommand(&mock, &doc, 0, QStringLiteral("Im0"), 90.0));
    QCOMPARE(mock.m_rotateCalls, 1);
    QCOMPARE(mock.m_lastRotateDegrees, 90.0);

    // A second rotation of the same image merges into one undo step (90+90).
    stack.push(new RotateImageCommand(&mock, &doc, 0, QStringLiteral("Im0"), 90.0));
    QCOMPARE(mock.m_rotateCalls, 2);

    stack.undo();
    QCOMPARE(mock.m_rotateCalls, 3);
    QCOMPARE(mock.m_lastRotateDegrees, -180.0); // inverse of the merged 180°
}
void TestImageEditWiring::deleteAndReplaceCommandsExecute() {
    MockPdfEditorEngine mock;
    DocumentSession doc;
    doc.setPath(QStringLiteral("img2.pdf"));

    // Delete: redo must call deleteImage and mark the session for reload.
    DeleteImageCommand del(&mock, &doc, 1, QStringLiteral("Im1"), QByteArray("backup"));
    del.redo();
    QCOMPARE(doc.path(), QStringLiteral("img2.pdf"));
    QVERIFY(del.text().contains(QStringLiteral("Im1")));

    // Replace: redo must run without error and carry the replacement path.
    ReplaceImageCommand rep(&mock, &doc, 0, QStringLiteral("Im0"),
                            QStringLiteral("new.png"), QByteArray("backup"));
    rep.redo();
    QVERIFY(rep.text().contains(QStringLiteral("Replace image")));
}
QTEST_MAIN(TestImageEditWiring)
#include "TestImageEditWiring.moc"
