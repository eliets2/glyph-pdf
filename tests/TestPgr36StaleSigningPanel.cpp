// SPDX-License-Identifier: Apache-2.0
// TestPgr36StaleSigningPanel.cpp
//
// PGR-36 (D2 delta review 2026-09-23) — cross-document replay through the
// stale signing-progress panel. SigningProgressPanel is MODELESS and binds
// its document path at construction; SendForSigningController::showProgressPanel
// used to RE-RAISE whatever panel existed (bound to the PREVIOUS document),
// and runSignStep executes against the CURRENT document's sidecar. A panel
// left open across a document switch therefore displayed request A's signers
// while executing signer N of document B's request — a wrong-signer /
// cross-document replay hazard in the signing trust model.
//
// Fix under test: showProgressPanel replaces a panel whose document changed;
// SigningProgressPanel::documentPath() exposes the binding so runSignStep can
// refuse mismatches (defense in depth — not driven here, it opens a modal).
//
// Fail-before seed: two documents, each with its own sidecar; open A, open B.
// Pre-fix there is exactly ONE panel and it is still bound to A while the
// open document is B.
//
// Run: QT_QPA_PLATFORM=offscreen ctest -R TestPgr36StaleSigningPanel

#include <QtTest/QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

#include "core/AppContext.h"
#include "core/SigningRequestModel.h"
#include "engines/DocumentSession.h"
#include "engines/SignatureManager.h"
#include "shell/controllers/SendForSigningController.h"
#include "ui/SigningProgressPanel.h"

using gp::SendForSigningController;
using ::SigningProgressPanel;

namespace {

// Minimal one-page PDF (the controller only needs an existing path).
bool writeStubPdf(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> off;
    auto addObj = [&out, &off](const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
    };
    addObj("<</Type/Catalog/Pages 2 0 R>>");
    addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>");
    const qint64 xref = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
               .arg(off.size() + 1).arg(xref).toLatin1();
    f.write(out);
    return true;
}

// A valid sidecar prepared against `docPath`'s bytes with `who` as signer 1.
bool writeSidecar(const QString& docPath, const QString& who)
{
    SigningRequestModel model;
    model.preparedSha256 = SigningRequestRunner::documentSha256(docPath);
    model.preparedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    model.createdUtc = model.preparedUtc;
    model.sourcePdfName = QFileInfo(docPath).fileName();
    SigningRequestModel::Signer s;
    s.name = who;
    s.fieldName = QStringLiteral("Sig_%1").arg(who);
    s.anchorPage = 0;
    s.anchorRect = QRectF(400, 500, 120, 40);
    model.signers.append(s);
    QString err;
    return model.save(SigningRequestModel::sidecarPathFor(docPath), &err);
}

QList<SigningProgressPanel *> livePanels()
{
    QList<SigningProgressPanel *> out;
    const auto all = QApplication::topLevelWidgets();
    for (QWidget *w : all)
        if (auto *p = qobject_cast<SigningProgressPanel *>(w))
            out.append(p);
    return out;
}

} // namespace

class TestPgr36StaleSigningPanel : public QObject {
    Q_OBJECT
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY(m_tmpDir.isValid());
    }

    // THE REGRESSION: open prepared doc A (panel appears, bound to A), then
    // open prepared doc B. Pre-fix the SAME panel was re-raised — still bound
    // to A — while document B is the open one, so clicking its Sign button
    // executed signer 1 of B's request behind A's rendered request.
    void stalePanelIsReplacedOnDocumentSwitch() {
        const QString docA = m_tmpDir.filePath(QStringLiteral("contractA.pdf"));
        const QString docB = m_tmpDir.filePath(QStringLiteral("contractB.pdf"));
        QVERIFY(writeStubPdf(docA));
        QVERIFY(writeStubPdf(docB));
        QVERIFY(writeSidecar(docA, QStringLiteral("Alice")));
        QVERIFY(writeSidecar(docB, QStringLiteral("Bob")));

        AppContext ctx;
        ctx.signing = std::make_shared<SignatureManager>();
        ctx.document = std::make_shared<DocumentSession>();

        SendForSigningController controller(&ctx, nullptr, this);

        // Open prepared document A: the panel must appear, bound to A.
        ctx.document->setPath(docA);
        controller.onDocumentOpened(docA);
        QCOMPARE(livePanels().size(), 1);
        SigningProgressPanel *panelA = livePanels().first();
        QVERIFY(panelA);

        // Switch to prepared document B: the panel must follow the document.
        ctx.document->setPath(docB);
        controller.onDocumentOpened(docB);

        // Give WA_DeleteOnClose's deleteLater a spin of the loop.
        QTest::qWait(50);

        const auto panels = livePanels();
        QCOMPARE(panels.size(), 1);
        // The FAIL-BEFORE observable (compiles against pre-fix code): pre-fix
        // showProgressPanel RE-RAISED the same instance — the stale surface
        // for document A. Post-fix it is closed and replaced.
        QVERIFY2(panels.first() != panelA,
                 "the stale panel for document A was re-raised over document B");
#ifdef PGR36_DOCUMENT_PATH
        // The binding assert (fixed code only — the accessor is part of the
        // fix): the surviving panel is bound to the OPEN document.
        QCOMPARE(panels.first()->documentPath(), docB);
#endif
    }
};

QTEST_MAIN(TestPgr36StaleSigningPanel)
#include "TestPgr36StaleSigningPanel.moc"
