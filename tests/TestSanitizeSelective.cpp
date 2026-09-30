// SPDX-License-Identifier: Apache-2.0
// PARITY-SCORECARD-2026-09-30 §4 #4 (July rows 72-73): Selective Sanitize +
// pre-commit summary. R7 pins, in order:
//   a. Summary pin — a fixture dirty in ALL NINE categories classifies to the
//      exact per-category counts, and the classify walk mutates nothing.
//   b. Selective pin — unchecking one category removes the others and
//      provably preserves that category's content (read back after save).
//   c. All-checked equivalence pin — an all-checked selective run reports the
//      same removed counts the classify promised and its saved outcome is
//      category-count equivalent to the legacy full sanitize.
//   d. Refusal pin — a zero-checked commit is refused at the engine boundary
//      (no output written, honest reason) and by the dialog with the honest
//      message; the dialog also defaults to all-checked and follows the
//      checkboxes.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>

#include <podofo/podofo.h>

#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/PdfEditorEngine.h"
#include "commands/SanitizeDocumentHelper.h"
#include "ui/SanitizeSummaryDialog.h"

namespace {

using namespace PoDoFo;

// ── fixture ────────────────────────────────────────────────────────────────
// One document dirty in every one of the nine sanitize categories. The exact
// counts below are the contract the classify walk must report.
struct ExpectedCounts {
    int metadata = 4;          // 3 /Info keys + catalog XMP /Metadata
    int attachments = 2;       // legacy.doc, notes.xlsx
    int annotations = 1;       // /Text annotation with /Contents
    int formFields = 1;        // text field with /V
    int jsActions = 5;         // /Names /JavaScript (1) + /OpenAction +
                               // catalog /AA + page /AA + Launch annotation /A
    int hiddenLayers = 1;      // one OCG
    int bookmarks = 2;         // "Chapter 1", "Chapter 2"
    int privateData = 1;       // catalog /PieceInfo
    int structureAltText = 2;  // /Alt + /ActualText on one struct element
};

const QByteArray kAttachmentDocPayload = "ATTACH-PAYLOAD-legacy-doc-SECRET";
const QByteArray kAttachmentXlsxPayload = "ATTACH-PAYLOAD-notes-xlsx-SECRET";

// Decode-level scan: flate-decoded stream bytes of every object contain the
// needle (a raw-byte contains() would pass vacuously while the data survives).
bool anyStreamContains(const QString &path, const QByteArray &needle)
{
    try {
        PdfMemDocument d;
        d.Load(path.toUtf8().constData());
        for (PdfObject *obj : d.GetObjects()) {
            if (obj == nullptr || !obj->HasStream()) continue;
            try {
                const auto copy = obj->GetStream()->GetCopy();
                if (std::string_view(copy.data(), copy.size())
                        .find(needle.constData()) != std::string_view::npos)
                    return true;
            } catch (const PdfError &) {
                // media/undecodable stream — not our payload's shape
            }
        }
    } catch (const std::exception &) {
        return false;
    }
    return false;
}

// Count the entries listed under /Names /EmbeddedFiles after a fresh load
// (the node may be a direct [key, value] array or a dict carrying /Names).
int nameTreeEntryCount(PdfMemDocument &doc)
{
    auto *namesObj = doc.GetCatalog().GetDictionary().FindKey("Names");
    if (!namesObj) return 0;
    if (namesObj->IsReference())
        namesObj = &doc.GetObjects().MustGetObject(namesObj->GetReference());
    if (!namesObj || !namesObj->IsDictionary()) return 0;
    auto *ef = namesObj->GetDictionary().FindKey("EmbeddedFiles");
    if (!ef) return 0;
    if (ef->IsReference())
        ef = &doc.GetObjects().MustGetObject(ef->GetReference());
    if (!ef) return 0;
    const PdfArray *arr = nullptr;
    if (ef->IsArray())
        arr = &ef->GetArray();
    else if (ef->IsDictionary()) {
        if (auto *n = ef->GetDictionary().FindKey("Names"); n && n->IsArray())
            arr = &n->GetArray();
    }
    if (!arr) return 0;
    return int(arr->GetSize() / 2);
}

bool catalogKeyAbsentAfterFreshLoad(const QString &path, const char *key)
{
    PdfMemDocument d;
    d.Load(path.toUtf8().constData());
    return !d.GetCatalog().GetDictionary().HasKey(PdfName(key));
}

QString makeDirtyFixture(const QString &pdf)
{
    QString error;
    try {
        PdfMemDocument doc;
        auto &page = doc.GetPages().CreatePage(
            PdfPage::CreateStandardPageSize(PdfPageSize::A4));
        auto &catalog = doc.GetCatalog();

        // ── Metadata: /Info keys + catalog XMP /Metadata stream ──
        auto &info = doc.GetObjects().CreateDictionaryObject();
        info.GetDictionary().AddKey(PdfName("Title"), PdfString("Quarterly Report"));
        info.GetDictionary().AddKey(PdfName("Author"), PdfString("Jane Doe"));
        info.GetDictionary().AddKey(PdfName("Producer"), PdfString("SecretLib 1.0"));
        doc.GetTrailer().GetDictionary().AddKey(PdfName("Info"),
                                                info.GetIndirectReference());
        auto &xmp = doc.GetObjects().CreateDictionaryObject();
        const QByteArray xmpPacket =
            "<?xpacket begin=''?><x:xmpmeta xmlns:x='adobe:ns:meta/'>"
            "<glyph:SecretXmpEntity/>"
            "</x:xmpmeta><?xpacket end='w'?>";
        xmp.GetOrCreateStream().SetData(bufferview(xmpPacket.constData(),
                                                   size_t(xmpPacket.size())));
        catalog.GetDictionary().AddKey(PdfName("Metadata"), xmp.GetIndirectReference());

        // ── Attachments: /Names /EmbeddedFiles with two real file specs ──
        auto embedFile = [&](const QByteArray &payload) {
            auto &stream = doc.GetObjects().CreateDictionaryObject();
            stream.GetDictionary().AddKey(PdfName("Type"), PdfName("EmbeddedFile"));
            stream.GetOrCreateStream().SetData(bufferview(payload.constData(),
                                                          size_t(payload.size())));
            auto &spec = doc.GetObjects().CreateDictionaryObject();
            spec.GetDictionary().AddKey(PdfName("Type"), PdfName("Filespec"));
            spec.GetDictionary().AddKey(PdfName("EF"), stream.GetIndirectReference());
            return spec.GetIndirectReference();
        };
        auto &namesDict = doc.GetObjects().CreateDictionaryObject();
        PdfArray efNames;
        efNames.Add(PdfString("legacy.doc"));
        efNames.Add(embedFile(kAttachmentDocPayload));
        efNames.Add(PdfString("notes.xlsx"));
        efNames.Add(embedFile(kAttachmentXlsxPayload));
        namesDict.GetDictionary().AddKey(PdfName("EmbeddedFiles"), efNames);
        catalog.GetDictionary().AddKey(PdfName("Names"), namesDict.GetIndirectReference());

        // ── JavaScript and actions: /Names /JavaScript + /OpenAction +
        //    catalog /AA + page /AA + Launch annotation /A ──
        auto &jsAction = doc.GetObjects().CreateDictionaryObject();
        jsAction.GetDictionary().AddKey(PdfName("S"), PdfName("JavaScript"));
        jsAction.GetDictionary().AddKey(PdfName("JS"), PdfString("app.alert('pwned');"));
        PdfArray jsNames;
        jsNames.Add(PdfString("EvilScript"));
        jsNames.Add(jsAction.GetIndirectReference());
        namesDict.GetDictionary().AddKey(PdfName("JavaScript"), jsNames);

        auto &openAction = doc.GetObjects().CreateDictionaryObject();
        openAction.GetDictionary().AddKey(PdfName("S"), PdfName("JavaScript"));
        openAction.GetDictionary().AddKey(PdfName("JS"), PdfString("app.alert('open');"));
        catalog.GetDictionary().AddKey(PdfName("OpenAction"),
                                       openAction.GetIndirectReference());

        auto &catAA = doc.GetObjects().CreateDictionaryObject();
        catAA.GetDictionary().AddKey(PdfName("WC"), PdfString("onclose"));
        catalog.GetDictionary().AddKey(PdfName("AA"), catAA.GetIndirectReference());

        auto &pageAA = doc.GetObjects().CreateDictionaryObject();
        pageAA.GetDictionary().AddKey(PdfName("O"), PdfString("app.alert('page');"));
        page.GetDictionary().AddKey(PdfName("AA"), pageAA.GetIndirectReference());

        auto &launchAction = doc.GetObjects().CreateDictionaryObject();
        launchAction.GetDictionary().AddKey(PdfName("S"), PdfName("Launch"));
        launchAction.GetDictionary().AddKey(PdfName("F"), PdfString("cmd.exe"));
        auto &linkAnnot = doc.GetObjects().CreateDictionaryObject();
        linkAnnot.GetDictionary().AddKey(PdfName("Type"), PdfName("Annot"));
        linkAnnot.GetDictionary().AddKey(PdfName("Subtype"), PdfName("Link"));
        PdfArray linkRect;
        linkRect.Add(PdfObject(int64_t(0)));
        linkRect.Add(PdfObject(int64_t(0)));
        linkRect.Add(PdfObject(int64_t(50)));
        linkRect.Add(PdfObject(int64_t(50)));
        linkAnnot.GetDictionary().AddKey(PdfName("Rect"), linkRect);
        linkAnnot.GetDictionary().AddKey(PdfName("A"), launchAction.GetIndirectReference());

        // ── Annotations: a /Text annotation carrying a /Contents body ──
        auto &textAnnot = doc.GetObjects().CreateDictionaryObject();
        textAnnot.GetDictionary().AddKey(PdfName("Type"), PdfName("Annot"));
        textAnnot.GetDictionary().AddKey(PdfName("Subtype"), PdfName("Text"));
        PdfArray textRect;
        textRect.Add(PdfObject(int64_t(120)));
        textRect.Add(PdfObject(int64_t(120)));
        textRect.Add(PdfObject(int64_t(180)));
        textRect.Add(PdfObject(int64_t(160)));
        textAnnot.GetDictionary().AddKey(PdfName("Rect"), textRect);
        textAnnot.GetDictionary().AddKey(PdfName("Contents"),
                                         PdfString("secret reviewer note"));
        PdfArray annots;
        annots.Add(linkAnnot.GetIndirectReference());
        annots.Add(textAnnot.GetIndirectReference());
        page.GetDictionary().AddKey(PdfName("Annots"), annots);

        // ── Form fields: a text field with a /V value ──
        auto &field = page.CreateField<PdfTextBox>("SecretField",
                                                   Rect(100, 100, 100, 20));
        field.SetText(PdfString("FieldSecretValue"));

        // ── Hidden layers: one OCG listed ON in the default config ──
        auto &ocgObj = doc.GetObjects().CreateDictionaryObject();
        ocgObj.GetDictionary().AddKey(PdfName("Type"), PdfName("OCG"));
        ocgObj.GetDictionary().AddKey(PdfName("Name"), PdfString("HiddenReviewers"));
        PdfArray ocgsArr;
        ocgsArr.Add(ocgObj.GetIndirectReference());
        PdfArray onArr;
        onArr.Add(ocgObj.GetIndirectReference());
        auto &dDictObj = doc.GetObjects().CreateDictionaryObject();
        dDictObj.GetDictionary().AddKey(PdfName("ON"), onArr);
        auto &ocpObj = doc.GetObjects().CreateDictionaryObject();
        ocpObj.GetDictionary().AddKey(PdfName("OCGs"), ocgsArr);
        ocpObj.GetDictionary().AddKey(PdfName("D"), dDictObj.GetIndirectReference());
        catalog.GetDictionary().AddKey(PdfName("OCProperties"),
                                       ocpObj.GetIndirectReference());

        // ── Bookmarks: two top-level items ──
        auto &b1 = doc.GetObjects().CreateDictionaryObject();
        b1.GetDictionary().AddKey(PdfName("Title"), PdfString("Chapter 1"));
        auto &b2 = doc.GetObjects().CreateDictionaryObject();
        b2.GetDictionary().AddKey(PdfName("Title"), PdfString("Chapter 2"));
        b1.GetDictionary().AddKey(PdfName("Next"), b2.GetIndirectReference());
        auto &outlines = doc.GetObjects().CreateDictionaryObject();
        outlines.GetDictionary().AddKey(PdfName("First"), b1.GetIndirectReference());
        outlines.GetDictionary().AddKey(PdfName("Last"), b2.GetIndirectReference());
        catalog.GetDictionary().AddKey(PdfName("Outlines"),
                                       outlines.GetIndirectReference());

        // ── Private application data: catalog /PieceInfo ──
        auto &pieceInfo = doc.GetObjects().CreateDictionaryObject();
        pieceInfo.GetDictionary().AddKey(PdfName("Creator"), PdfString("PrivateApp"));
        catalog.GetDictionary().AddKey(PdfName("PieceInfo"),
                                       pieceInfo.GetIndirectReference());

        // ── Structure replacement text: /Alt + /ActualText on one element ──
        auto &elem = doc.GetObjects().CreateDictionaryObject();
        elem.GetDictionary().AddKey(PdfName("Alt"), PdfString("secret alt text"));
        elem.GetDictionary().AddKey(PdfName("ActualText"), PdfString("secret actual"));
        auto &structRoot = doc.GetObjects().CreateDictionaryObject();
        structRoot.GetDictionary().AddKey(PdfName("K"), elem.GetIndirectReference());
        catalog.GetDictionary().AddKey(PdfName("StructTreeRoot"),
                                       structRoot.GetIndirectReference());

        doc.Save(pdf.toUtf8().constData());
    } catch (const std::exception &e) {
        error = QString::fromLatin1(e.what());
    }
    return error;
}

// Assert a classified plan carries exactly the expected per-category counts.
// Returns an empty string when equal, else a failure description.
QString expectPlanCounts(const SanitizePlan &plan, const ExpectedCounts &e)
{
    struct Row { SanitizeCategory c; int n; };
    const Row rows[] = {
        { SanitizeCategory::Metadata, e.metadata },
        { SanitizeCategory::Attachments, e.attachments },
        { SanitizeCategory::Annotations, e.annotations },
        { SanitizeCategory::FormFields, e.formFields },
        { SanitizeCategory::JavaScriptActions, e.jsActions },
        { SanitizeCategory::HiddenLayers, e.hiddenLayers },
        { SanitizeCategory::Bookmarks, e.bookmarks },
        { SanitizeCategory::PrivateData, e.privateData },
        { SanitizeCategory::StructureAltText, e.structureAltText },
    };
    for (const auto &row : rows) {
        const auto *p = plan.find(row.c);
        const int actual = p ? p->count : 0;
        if (actual != row.n)
            return QStringLiteral("%1: expected %2, got %3")
                .arg(sanitizeCategoryLabel(row.c),
                     QString::number(row.n), QString::number(actual));
    }
    if (plan.totalCount() != e.metadata + e.attachments + e.annotations
                           + e.formFields + e.jsActions + e.hiddenLayers
                           + e.bookmarks + e.privateData + e.structureAltText)
        return QStringLiteral("totalCount mismatch");
    return QString();
}

// Byte identity of the untouched fixture file (classify must not mutate).
QByteArray fileHash(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
}

// The honest "sanitized" post-state a fresh classify of a sanitized output
// may show: no USER data in any category. Two categories are special by
// design — the save re-stamps exactly /Info /ModDate (1 Metadata item), and
// hidden layers stay PRESENT but forced OFF (the redaction-safe G4 policy:
// hidden stays hidden, never revealed — checked via the OFF policy, not by
// absence).
QString expectSanitizedShape(const SanitizePlan &plan)
{
    for (const auto &c : plan.categories) {
        if (c.category == SanitizeCategory::Metadata) {
            if (c.count > 1)
                return QStringLiteral("user metadata survived: %1 item(s)")
                    .arg(QString::number(c.count));
            for (const auto &item : c.items)
                if (!item.contains(QStringLiteral("ModDate")))
                    return QStringLiteral("unexpected metadata item: %1").arg(item);
            continue;
        }
        if (c.category == SanitizeCategory::HiddenLayers) {
            // Layers stay PRESENT by design (forced OFF — the redaction-safe
            // G4 policy); the OFF policy is verified separately per document.
            continue;
        }
        return QStringLiteral("%1 not clean: %2 item(s)")
            .arg(sanitizeCategoryLabel(c.category), QString::number(c.count));
    }
    return QString();
}

// Every layer must sit in /D /OFF after a sanitize (hidden stays hidden).
bool layersAllOffAfterFreshLoad(const QString &path, QString *why)
{
    PdfMemDocument d;
    d.Load(path.toUtf8().constData());
    auto *ocp = d.GetCatalog().GetDictionary().FindKey("OCProperties");
    if (!ocp) return true; // no layers at all — trivially hidden
    if (ocp->IsReference())
        ocp = &d.GetObjects().MustGetObject(ocp->GetReference());
    if (!ocp || !ocp->IsDictionary()) { *why = QStringLiteral("/OCProperties not a dict"); return false; }
    auto *dObj = ocp->GetDictionary().FindKey("D");
    if (dObj && dObj->IsReference())
        dObj = &d.GetObjects().MustGetObject(dObj->GetReference());
    if (!dObj || !dObj->IsDictionary()) { *why = QStringLiteral("no /D default config"); return false; }
    auto *on = dObj->GetDictionary().FindKey("ON");
    if (on && on->IsArray() && on->GetArray().GetSize() > 0) {
        *why = QStringLiteral("a layer is ON after sanitize");
        return false;
    }
    return true;
}

bool outlinesPreservedAfterFreshLoad(const QString &path, QString *why)
{
    PdfMemDocument d;
    d.Load(path.toUtf8().constData());
    auto *outlines = d.GetCatalog().GetDictionary().FindKey("Outlines");
    if (!outlines) { *why = QStringLiteral("/Outlines gone"); return false; }
    if (outlines->IsReference())
        outlines = &d.GetObjects().MustGetObject(outlines->GetReference());
    if (!outlines->IsDictionary()) { *why = QStringLiteral("/Outlines not a dict"); return false; }
    auto *first = outlines->GetDictionary().FindKey("First");
    if (!first) { *why = QStringLiteral("/Outlines /First gone"); return false; }
    if (first->IsReference())
        first = &d.GetObjects().MustGetObject(first->GetReference());
    if (!first->IsDictionary()) { *why = QStringLiteral("/First not a dict"); return false; }
    auto *title = first->GetDictionary().FindKey("Title");
    if (!title || !title->IsString()
        || std::string_view(title->GetString().GetString()) != "Chapter 1") {
        *why = QStringLiteral("first bookmark title lost");
        return false;
    }
    return true;
}

QCheckBox *findCategoryBox(const QDialog &dialog, const QString &labelPart)
{
    for (QCheckBox *box : dialog.findChildren<QCheckBox *>())
        if (box->text().contains(labelPart))
            return box;
    return nullptr;
}

} // namespace

class TestSanitizeSelective : public QObject {
    Q_OBJECT

    QTemporaryDir m_tmpDir;

    // Returns an empty string when the fixture could not be built (callers
    // must QVERIFY the result — QtTest macros cannot return from a helper).
    QString newFixture(const QString &name)
    {
        QString pdf = m_tmpDir.filePath(name);
        const QString err = makeDirtyFixture(pdf);
        if (!err.isEmpty())
            qWarning() << "fixture build failed:" << err;
        return err.isEmpty() ? pdf : QString();
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tmpDir.isValid());
    }

    // ── pin (a): summary — exact per-category counts, no mutation ──
    void classifyReportsExactPerCategoryCountsOnAllNineDirtyCategories()
    {
        QString pdf = newFixture("dirty.pdf");
        QVERIFY2(!pdf.isEmpty(), "fixture build failed");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        SanitizePlan plan = engine.sanitizeClassify();
        QVERIFY2(!plan.empty(), "classify must find the nine dirty categories");

        const ExpectedCounts expected;
        const QString mismatch = expectPlanCounts(plan, expected);
        QVERIFY2(mismatch.isEmpty(),
                 qPrintable(QStringLiteral("per-category counts wrong: ") + mismatch));

        // Item summaries the dialog shows: attachment file names, bookmark
        // titles, the scrubbed /Info keys.
        const auto *att = plan.find(SanitizeCategory::Attachments);
        QVERIFY(att != nullptr);
        QVERIFY2(att->items.contains(QStringLiteral("legacy.doc"))
                 && att->items.contains(QStringLiteral("notes.xlsx")),
                 "attachment summary must name the embedded files");
        const auto *bm = plan.find(SanitizeCategory::Bookmarks);
        QVERIFY(bm != nullptr);
        QVERIFY2(bm->items.contains(QStringLiteral("Chapter 1")),
                 "bookmark summary must list titles");
        QVERIFY2(!plan.describe().isEmpty(),
                 "plan must carry a one-line human summary");
    }

    void classifyWalkDoesNotMutateTheDocument()
    {
        QString pdf = newFixture("dirty_nomut.pdf");
        QVERIFY2(!pdf.isEmpty(), "fixture build failed");
        const QByteArray before = fileHash(pdf);
        QVERIFY(!before.isEmpty());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        SanitizePlan first = engine.sanitizeClassify();
        SanitizePlan second = engine.sanitizeClassify();

        // Repeated classification is stable (a mutating walk would drift).
        QCOMPARE(expectPlanCounts(first, ExpectedCounts()), QString());
        QCOMPARE(expectPlanCounts(second, ExpectedCounts()), QString());
        QCOMPARE(first.describe(), second.describe());

        // The source file on disk is byte-identical.
        QCOMPARE(fileHash(pdf), before);
    }

    // ── pin (b): selective — uncheck one category, others removed, it stays ──
    void uncheckingAttachmentsRemovesEverythingElseAndPreservesTheFiles()
    {
        QString pdf = newFixture("sel_att.pdf");
        QVERIFY2(!pdf.isEmpty(), "fixture build failed");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        SanitizePlan promise = engine.sanitizeClassify();
        QCOMPARE(expectPlanCounts(promise, ExpectedCounts()), QString());

        SanitizeCategories selection = sanitizeAllCategories()
                                     & ~SanitizeCategories(SanitizeCategory::Attachments);
        QString out = m_tmpDir.filePath("sel_att_out.pdf");
        SanitizePlan removed;
        QVERIFY2(engine.sanitizeDocument(out, selection, &removed),
                 "selective sanitize must succeed");

        // The result proves what was actually removed: no Attachments record.
        QVERIFY2(removed.find(SanitizeCategory::Attachments) == nullptr,
                 "proof must not claim the preserved category was removed");
        const ExpectedCounts withoutAttachments;
        // Everything except attachments must match its promise…
        SanitizePlan expectedPlan;
        auto addRow = [&expectedPlan](SanitizeCategory c, int n) {
            if (n > 0) expectedPlan.categories.append({ c, n, {} });
        };
        addRow(SanitizeCategory::Metadata, withoutAttachments.metadata);
        addRow(SanitizeCategory::Annotations, withoutAttachments.annotations);
        addRow(SanitizeCategory::FormFields, withoutAttachments.formFields);
        addRow(SanitizeCategory::JavaScriptActions, withoutAttachments.jsActions);
        addRow(SanitizeCategory::HiddenLayers, withoutAttachments.hiddenLayers);
        addRow(SanitizeCategory::Bookmarks, withoutAttachments.bookmarks);
        addRow(SanitizeCategory::PrivateData, withoutAttachments.privateData);
        addRow(SanitizeCategory::StructureAltText, withoutAttachments.structureAltText);
        for (const auto &row : expectedPlan.categories) {
            const auto *p = removed.find(row.category);
            QVERIFY2(p != nullptr,
                     qPrintable(QStringLiteral("removed result missing %1")
                                    .arg(sanitizeCategoryLabel(row.category))));
            QCOMPARE(p->count, row.count);
        }

        // Read-back on a fresh load: the two files are still embedded, byte for
        // byte, and still listed under their names.
        QVERIFY2(anyStreamContains(out, kAttachmentDocPayload),
                 "legacy.doc payload must survive an unchecked Attachments run");
        QVERIFY2(anyStreamContains(out, kAttachmentXlsxPayload),
                 "notes.xlsx payload must survive an unchecked Attachments run");
        {
            PdfMemDocument d;
            d.Load(out.toUtf8().constData());
            QCOMPARE(nameTreeEntryCount(d), 2);
        }

        // …and the checked categories are really gone from the saved copy.
        QVERIFY2(catalogKeyAbsentAfterFreshLoad(out, "OpenAction"),
                 "OpenAction must be removed");
        QVERIFY2(catalogKeyAbsentAfterFreshLoad(out, "PieceInfo"),
                 "PieceInfo must be removed");
        QVERIFY2(anyStreamContains(out, "app.alert('pwned')") == false,
                 "the JavaScript action must be gone");
        QVERIFY2(anyStreamContains(out, "FieldSecretValue") == false,
                 "the form field value must be gone");
        QVERIFY2(anyStreamContains(out, "secret reviewer note") == false,
                 "the annotation body must be gone");
    }

    void uncheckingBookmarksRemovesEverythingElseAndPreservesTheOutline()
    {
        QString pdf = newFixture("sel_bm.pdf");
        QVERIFY2(!pdf.isEmpty(), "fixture build failed");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        SanitizeCategories selection = sanitizeAllCategories()
                                     & ~SanitizeCategories(SanitizeCategory::Bookmarks);
        QString out = m_tmpDir.filePath("sel_bm_out.pdf");
        SanitizePlan removed;
        QVERIFY(engine.sanitizeDocument(out, selection, &removed));

        QVERIFY2(removed.find(SanitizeCategory::Bookmarks) == nullptr,
                 "proof must not claim the preserved bookmarks were removed");
        QVERIFY2(removed.find(SanitizeCategory::Attachments) != nullptr,
                 "attachments were checked — they must be reported removed");

        // Bookmarks survive with their content…
        QString why;
        QVERIFY2(outlinesPreservedAfterFreshLoad(out, &why),
                 qPrintable(QStringLiteral("bookmarks must survive: ") + why));
        // …while the checked categories are gone.
        QVERIFY2(catalogKeyAbsentAfterFreshLoad(out, "OpenAction"),
                 "OpenAction must be removed");
        QVERIFY2(anyStreamContains(out, kAttachmentDocPayload) == false,
                 "checked Attachments must be removed (GC must drop the payload)");
        QVERIFY2(anyStreamContains(out, "secret alt text") == false,
                 "checked structure replacement text must be removed");
    }

    // ── pin (c): all-checked selective == legacy full sanitize outcome ──
    void allCheckedSelectiveRunMatchesLegacyFullSanitizeOutcome()
    {
        QString pdf = newFixture("equiv.pdf");
        QVERIFY2(!pdf.isEmpty(), "fixture build failed");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        SanitizePlan promise = engine.sanitizeClassify();
        QCOMPARE(expectPlanCounts(promise, ExpectedCounts()), QString());

        // Legacy full sanitize (the one-argument default path).
        QString legacyOut = m_tmpDir.filePath("equiv_legacy.pdf");
        QVERIFY(engine.sanitizeDocument(legacyOut));

        // All-checked selective run on a fresh load of the same source.
        QVERIFY(engine.loadDocumentForEditing(pdf));
        QString selOut = m_tmpDir.filePath("equiv_selective.pdf");
        SanitizePlan removed;
        QVERIFY(engine.sanitizeDocument(selOut, sanitizeAllCategories(), &removed));

        // Proof-carrying: the selective run removed exactly what the summary
        // promised, per category.
        QCOMPARE(expectPlanCounts(removed, ExpectedCounts()), QString());

        // Category-count equivalence: both outputs classify to the same
        // honest sanitized shape — zero user data anywhere (the only legal
        // remainder is the save-re-stamped /Info /ModDate). (Byte equality is
        // not the contract: the trailer /ID second element is randomized on
        // every sanitize save by design.)
        PdfEditorEngine verify;
        QVERIFY(verify.loadDocumentForEditing(legacyOut));
        SanitizePlan legacyPlan = verify.sanitizeClassify();
        QVERIFY2(expectSanitizedShape(legacyPlan).isEmpty(),
                 qPrintable(QStringLiteral("legacy output still dirty: ")
                                + expectSanitizedShape(legacyPlan)));
        QVERIFY(verify.loadDocumentForEditing(selOut));
        SanitizePlan selPlan = verify.sanitizeClassify();
        QVERIFY2(expectSanitizedShape(selPlan).isEmpty(),
                 qPrintable(QStringLiteral("selective output still dirty: ")
                                + expectSanitizedShape(selPlan)));

        // Hidden layers: both outputs keep the layer but force it OFF.
        QString why;
        QVERIFY2(layersAllOffAfterFreshLoad(legacyOut, &why),
                 qPrintable(QStringLiteral("legacy output revealed a layer: ") + why));
        QVERIFY2(layersAllOffAfterFreshLoad(selOut, &why),
                 qPrintable(QStringLiteral("selective output revealed a layer: ") + why));

        // Spot equivalence on the documents themselves.
        QVERIFY2(catalogKeyAbsentAfterFreshLoad(legacyOut, "OpenAction")
                 && catalogKeyAbsentAfterFreshLoad(selOut, "OpenAction"),
                 "both outputs must lose /OpenAction");
        {
            PdfMemDocument a, b;
            a.Load(legacyOut.toUtf8().constData());
            b.Load(selOut.toUtf8().constData());
            QCOMPARE(nameTreeEntryCount(a), 0);
            QCOMPARE(nameTreeEntryCount(b), 0);
        }
    }

    // ── pin (d): zero-checked refuses — engine boundary ──
    void emptySelectionRefusedAtEngineBoundaryWithoutWritingOutput()
    {
        QString pdf = newFixture("refuse.pdf");
        QVERIFY2(!pdf.isEmpty(), "fixture build failed");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QString out = m_tmpDir.filePath("refuse_out.pdf");
        SanitizePlan removed;
        QVERIFY2(!engine.sanitizeDocument(out, SanitizeCategories(), &removed),
                 "an empty selection must refuse — removing nothing is not a sanitize");
        QVERIFY2(!QFileInfo::exists(out),
                 "a refused commit must not write an output file");
        QVERIFY2(removed.empty(),
                 "a refused commit must not report removals");
        const ErrorInfo err = engine.lastError();
        QVERIFY2(!err.isOk(),
                 "the refusal must be reported as an error condition");
        QVERIFY2(err.userMessage.contains(QStringLiteral("nothing"),
                                          Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("refusal must say why, got: ")
                                + err.userMessage));

        // The legacy one-argument path is untouched by the refusal: it is the
        // all-categories default and still commits.
        QString legacyOut = m_tmpDir.filePath("refuse_legacy_out.pdf");
        QVERIFY2(engine.sanitizeDocument(legacyOut),
                 "the all-or-nothing default must keep working");
        QVERIFY2(QFileInfo::exists(legacyOut),
                 "legacy sanitize must still write its output");
    }

private:
    // Hand-built plan for the dialog pins (the dialog consumes a plan, not an
    // engine) — same shape the classify walk produces.
    static SanitizePlan samplePlan()
    {
        SanitizePlan plan;
        plan.categories.append({ SanitizeCategory::Metadata, 4,
            { QStringLiteral("Title"), QStringLiteral("Author"),
              QStringLiteral("Producer"),
              QStringLiteral("XMP /Metadata (catalog)") } });
        plan.categories.append({ SanitizeCategory::Attachments, 2,
            { QStringLiteral("legacy.doc"), QStringLiteral("notes.xlsx") } });
        plan.categories.append({ SanitizeCategory::HiddenLayers, 1,
            { QStringLiteral("HiddenReviewers") } });
        plan.categories.append({ SanitizeCategory::Bookmarks, 2,
            { QStringLiteral("Chapter 1"), QStringLiteral("Chapter 2") } });
        return plan;
    }

private slots:

    // ── pin (summary UI): the pre-commit summary reflects the plan, all
    // checked by default (= today's all-or-nothing behavior) ──
    void summaryDialogDefaultsToAllCheckedAndReflectsThePlan()
    {
        const SanitizePlan plan = samplePlan();
        SanitizeSummaryDialog dialog(plan);

        // One checkbox per found category, labeled with the category, its
        // count, and the item names.
        auto boxes = dialog.findChildren<QCheckBox *>();
        QCOMPARE(boxes.size(), 4);
        QVERIFY(findCategoryBox(dialog, QStringLiteral("Metadata")) != nullptr);
        QVERIFY(findCategoryBox(dialog, QStringLiteral("File attachments")) != nullptr);
        QVERIFY(findCategoryBox(dialog, QStringLiteral("Hidden content layers")) != nullptr);
        QVERIFY(findCategoryBox(dialog, QStringLiteral("Bookmarks")) != nullptr);
        QVERIFY2(findCategoryBox(dialog, QStringLiteral("legacy.doc")) != nullptr,
                 "the summary must name the attachment files it will remove");

        for (QCheckBox *box : boxes)
            QVERIFY2(box->isChecked(),
                     "every category must default to checked (all-or-nothing default)");

        // Default selection = every category the plan found.
        QCOMPARE(dialog.selectedCategories().toInt(),
                 int(SanitizeCategory::Metadata | SanitizeCategory::Attachments
                     | SanitizeCategory::HiddenLayers | SanitizeCategory::Bookmarks));
        QVERIFY2(dialog.canCommit(), "all-checked must be committable");

        // The dialog states plainly what will remain if a category is kept.
        auto *attBox = findCategoryBox(dialog, QStringLiteral("File attachments"));
        QVERIFY(attBox != nullptr);
        attBox->setChecked(false);
        const auto remaining = dialog.findChildren<QLabel *>();
        bool statesRemaining = false;
        for (const QLabel *l : remaining)
            if (l->text().contains(QStringLiteral("Will remain unchanged"))
                && l->text().contains(QStringLiteral("File attachments (2)")))
                statesRemaining = true;
        QVERIFY2(statesRemaining,
                 "the dialog must state which categories stay unchanged");
    }

    // ── pin (selective UI): the checkboxes drive the commit selection ──
    void summaryDialogSelectedCategoriesFollowTheCheckboxes()
    {
        SanitizeSummaryDialog dialog(samplePlan());

        auto *attBox = findCategoryBox(dialog, QStringLiteral("File attachments"));
        QVERIFY(attBox != nullptr);
        attBox->setChecked(false);
        QCOMPARE(dialog.selectedCategories().toInt(),
                 int((sanitizeAllCategories()
                      & ~SanitizeCategories(SanitizeCategory::Attachments))
                     & (SanitizeCategory::Metadata | SanitizeCategory::HiddenLayers
                        | SanitizeCategory::Bookmarks)));
        QVERIFY2(dialog.canCommit(), "unchecking one category must stay committable");

        attBox->setChecked(true);
        auto *bmBox = findCategoryBox(dialog, QStringLiteral("Bookmarks"));
        QVERIFY(bmBox != nullptr);
        bmBox->setChecked(false);
        QVERIFY(dialog.selectedCategories().testFlag(SanitizeCategory::Attachments));
        QVERIFY(!dialog.selectedCategories().testFlag(SanitizeCategory::Bookmarks));
    }

    // ── pin (refusal UI): zero-checked refuses with the honest message; a
    // clean document refuses too ──
    void summaryDialogRefusesZeroCheckedCommitWithHonestMessage()
    {
        SanitizeSummaryDialog dialog(samplePlan());

        for (QCheckBox *box : dialog.findChildren<QCheckBox *>())
            box->setChecked(false);

        QString reason;
        QVERIFY2(!dialog.canCommit(&reason),
                 "a zero-checked commit must be refused");
        QVERIFY2(!reason.isEmpty(), "the refusal must carry a visible reason");
        QVERIFY2(reason.contains(QStringLiteral("nothing"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("refusal must say what would (not) "
                                          "happen, got: ")
                                + reason));
        QVERIFY2(reason.contains(QStringLiteral("Cancel"), Qt::CaseInsensitive),
                 "the refusal must tell the user the way out");

        // A plan with nothing found refuses just as honestly.
        SanitizePlan emptyPlan;
        SanitizeSummaryDialog emptyDialog(emptyPlan);
        QString emptyReason;
        QVERIFY2(!emptyDialog.canCommit(&emptyReason),
                 "nothing found — nothing to sanitize");
        QVERIFY2(emptyReason.contains(QStringLiteral("Nothing removable"),
                                      Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("clean-document refusal must be "
                                          "honest, got: ")
                                + emptyReason));
    }
};

// GUI main: the summary-dialog pins construct real QWidgets.
QTEST_MAIN(TestSanitizeSelective)
#include "TestSanitizeSelective.moc"
