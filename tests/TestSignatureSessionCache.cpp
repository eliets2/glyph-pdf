// SPDX-License-Identifier: Apache-2.0
// §9.7 P1: the session signature cache. Typing/uploading a signature is work
// the user should not have to redo on every activation of the signature tool
// within the same document session. The cache is a QObject PARENTED TO the
// DocumentSession (its lifetime IS the session's), scoped to one document:
//   * store() remembers the last ACCEPTED signature (kind + image + text);
//   * noteDocument() clears it when the document path switches;
//   * the picker surfaces a "Reuse last signature" checkbox that is offered
//     (visible) and default-CHECKED exactly when the cache holds a signature,
//     and a fresh acceptance REPLACES the cached signature.
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include "ui/SignaturePicker.h"

namespace {

QImage inkImage()
{
    QImage img(40, 24, QImage::Format_RGBA8888);
    img.fill(QColor(220, 20, 20, 255));
    return img;
}

} // namespace

class TestSignatureSessionCache : public QObject {
    Q_OBJECT
private slots:
    void storeAndReuseLastSignature();
    void freshAcceptReplacesCache();
    void noteDocumentClearsOnSwitch();
    void emptyStoreAndNoCacheAreHidden();
};

void TestSignatureSessionCache::storeAndReuseLastSignature()
{
    SignatureSessionCache cache;
    QVERIFY2(!cache.hasSignature(), "a fresh cache must not offer a signature");

    const QImage ink = SignatureContent::renderTyped(
        QStringLiteral("John Hancock"), QStringLiteral("Arial"),
        SignatureContent::kTypedPointSizeDefault, Qt::darkBlue);
    QVERIFY(!ink.isNull());
    cache.store(SignatureContent::Kind::Typed, ink, QStringLiteral("John Hancock"));
    QVERIFY(cache.hasSignature());
    QCOMPARE(cache.kind(), SignatureContent::Kind::Typed);
    QCOMPARE(cache.image(), ink);
    QCOMPARE(cache.typedText(), QStringLiteral("John Hancock"));

    // A NEW picker instance — what EditController constructs per activation —
    // must offer the cached signature, default-checked.
    SignaturePickerDialog dlg;
    dlg.setSessionCache(&cache);
    auto *reuse = dlg.findChild<QCheckBox *>(QStringLiteral("signatureReuseCheck"));
    QVERIFY2(reuse, "the picker must expose the reuse checkbox");
    QVERIFY2(!reuse->isHidden(),
             "reuse must be OFFERED when the session cache holds a signature");
    QVERIFY2(reuse->isChecked(),
             "reuse must default to CHECKED when a cached signature exists");

    auto *buttons = dlg.findChild<QDialogButtonBox *>();
    QVERIFY2(buttons, "dialog must expose its button box");
    buttons->button(QDialogButtonBox::Ok)->click();
    QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(dlg.acceptedKind(), SignatureContent::Kind::Typed);
    QVERIFY2(!dlg.acceptedImage().isNull(),
             "reuse must deliver the cached signature image");
    QCOMPARE(dlg.acceptedImage(), ink);
    QCOMPARE(dlg.acceptedText(), QStringLiteral("John Hancock"));
}

void TestSignatureSessionCache::freshAcceptReplacesCache()
{
    SignatureSessionCache cache;
    cache.store(SignatureContent::Kind::Upload, inkImage(), QString());
    QVERIFY(cache.hasSignature());

    SignaturePickerDialog dlg;
    dlg.setSessionCache(&cache);
    auto *reuse = dlg.findChild<QCheckBox *>(QStringLiteral("signatureReuseCheck"));
    QVERIFY(reuse);
    QVERIFY(reuse->isChecked());
    // The user opts OUT of reuse and produces a fresh signature instead.
    reuse->setChecked(false);
    dlg.showTab(SignatureContent::Kind::Typed);
    auto *edit = dlg.findChild<QLineEdit *>(QStringLiteral("signatureTypeEdit"));
    QVERIFY2(edit, "type tab must expose its text field");
    edit->setText(QStringLiteral("New Name"));
    auto *buttons = dlg.findChild<QDialogButtonBox *>();
    buttons->button(QDialogButtonBox::Ok)->click();
    QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(dlg.acceptedKind(), SignatureContent::Kind::Typed);
    QCOMPARE(dlg.acceptedText(), QStringLiteral("New Name"));

    // The fresh signature REPLACES the cached one for the next activation.
    QVERIFY(cache.hasSignature());
    QCOMPARE(cache.kind(), SignatureContent::Kind::Typed);
    QCOMPARE(cache.typedText(), QStringLiteral("New Name"));
    QVERIFY(!cache.image().isNull());
}

void TestSignatureSessionCache::noteDocumentClearsOnSwitch()
{
    SignatureSessionCache cache;
    cache.noteDocument(QStringLiteral("docA.pdf"));
    cache.store(SignatureContent::Kind::Typed, inkImage(), QStringLiteral("John Hancock"));
    QVERIFY(cache.hasSignature());

    // Re-noting the SAME document keeps the signature.
    cache.noteDocument(QStringLiteral("docA.pdf"));
    QVERIFY2(cache.hasSignature(),
             "re-noting the same document path must keep the cached signature");

    // A document switch clears it — the cache is per-document-session scoped.
    cache.noteDocument(QStringLiteral("docB.pdf"));
    QVERIFY2(!cache.hasSignature(),
             "switching documents must clear the session signature cache");
    QVERIFY(cache.image().isNull());
    QVERIFY(cache.typedText().isEmpty());
}

void TestSignatureSessionCache::emptyStoreAndNoCacheAreHidden()
{
    SignatureSessionCache cache;
    // Draw has no image form — storing a null image must not arm the cache.
    cache.store(SignatureContent::Kind::Draw, QImage(), QString());
    QVERIFY2(!cache.hasSignature(),
             "a null image must not mark the cache as holding a signature");

    // An empty cache must NOT offer the reuse checkbox.
    SignaturePickerDialog dlg;
    dlg.setSessionCache(&cache);
    auto *reuse = dlg.findChild<QCheckBox *>(QStringLiteral("signatureReuseCheck"));
    QVERIFY2(reuse, "the reuse checkbox must exist for findChild-based testing");
    QVERIFY2(reuse->isHidden(), "reuse must not be offered while the cache is empty");
    QVERIFY2(!reuse->isChecked(), "reuse must not default to checked with no signature");

    // No cache at all behaves exactly like an empty one, and the picker keeps
    // working the pre-cache way (typed flow unaffected).
    SignaturePickerDialog plain;
    auto *plainReuse = plain.findChild<QCheckBox *>(QStringLiteral("signatureReuseCheck"));
    QVERIFY2(plainReuse, "the reuse checkbox must exist even without a session cache");
    QVERIFY2(plainReuse->isHidden(), "reuse checkbox must be hidden without a session cache");
    plain.showTab(SignatureContent::Kind::Typed);
    QVERIFY2(!plain.isAcceptEnabled(), "typed gating must be unaffected by cache wiring");
}

QTEST_MAIN(TestSignatureSessionCache)
#include "TestSignatureSessionCache.moc"
