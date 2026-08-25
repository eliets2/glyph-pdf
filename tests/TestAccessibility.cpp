#include <QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "../src/modes/PdfAValidationPanel.h"

using namespace gp;

class TestAccessibility : public QObject {
    Q_OBJECT

private:
    QString m_docPath;
    QTemporaryDir m_tmpDir;

    QString createTaggedPdf() {
        QString path = m_tmpDir.filePath("tagged.pdf");
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        
        auto& objects = doc.GetObjects();
        auto& root = objects.CreateObject(PoDoFo::PdfDictionary());
        root.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructTreeRoot"));
        doc.GetCatalog().GetDictionary().AddKey("StructTreeRoot", root.GetIndirectReference());

        auto& parent = objects.CreateObject(PoDoFo::PdfDictionary());
        parent.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructElem"));
        parent.GetDictionary().AddKey("S", PoDoFo::PdfName("Document"));
        parent.GetDictionary().AddKey("P", root.GetIndirectReference());
        parent.GetDictionary().AddKey("Pg", page.GetObject().GetIndirectReference());

        PoDoFo::PdfArray kArray;

        // Child 1: Inherits Pg, y=100
        auto& child1 = objects.CreateObject(PoDoFo::PdfDictionary());
        child1.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructElem"));
        child1.GetDictionary().AddKey("S", PoDoFo::PdfName("P"));
        child1.GetDictionary().AddKey("P", parent.GetIndirectReference());
        PoDoFo::PdfArray bbox1;
        bbox1.Add(0.0); bbox1.Add(100.0); bbox1.Add(10.0); bbox1.Add(110.0);
        PoDoFo::PdfDictionary aDict1;
        aDict1.AddKey("BBox", bbox1);
        child1.GetDictionary().AddKey("A", aDict1);
        kArray.Add(child1.GetIndirectReference());

        // Child 2: Inherits Pg, y=80 (visually below Child 1)
        auto& child2 = objects.CreateObject(PoDoFo::PdfDictionary());
        child2.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructElem"));
        child2.GetDictionary().AddKey("S", PoDoFo::PdfName("P"));
        child2.GetDictionary().AddKey("P", parent.GetIndirectReference());
        PoDoFo::PdfArray bbox2;
        bbox2.Add(0.0); bbox2.Add(80.0); bbox2.Add(10.0); bbox2.Add(90.0);
        PoDoFo::PdfDictionary aDict2;
        aDict2.AddKey("BBox", bbox2);
        child2.GetDictionary().AddKey("A", aDict2);
        kArray.Add(child2.GetIndirectReference());

        // Child 3: Inherits Pg, y=60 (visually below Child 2)
        auto& child3 = objects.CreateObject(PoDoFo::PdfDictionary());
        child3.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructElem"));
        child3.GetDictionary().AddKey("S", PoDoFo::PdfName("P"));
        child3.GetDictionary().AddKey("P", parent.GetIndirectReference());
        PoDoFo::PdfArray bbox3;
        bbox3.Add(0.0); bbox3.Add(60.0); bbox3.Add(10.0); bbox3.Add(70.0);
        PoDoFo::PdfDictionary aDict3;
        aDict3.AddKey("BBox", bbox3);
        child3.GetDictionary().AddKey("A", aDict3);
        kArray.Add(child3.GetIndirectReference());

        parent.GetDictionary().AddKey("K", kArray);

        PoDoFo::PdfArray rootKArray;
        rootKArray.Add(parent.GetIndirectReference());
        root.GetDictionary().AddKey("K", rootKArray);

        doc.Save(path.toUtf8().constData());
        return path;
    }

    QString createReorderedPdf() {
        QString path = m_tmpDir.filePath("reordered.pdf");
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        
        auto& objects = doc.GetObjects();
        auto& root = objects.CreateObject(PoDoFo::PdfDictionary());
        root.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructTreeRoot"));
        doc.GetCatalog().GetDictionary().AddKey("StructTreeRoot", root.GetIndirectReference());

        auto& parent = objects.CreateObject(PoDoFo::PdfDictionary());
        parent.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructElem"));
        parent.GetDictionary().AddKey("S", PoDoFo::PdfName("Document"));
        parent.GetDictionary().AddKey("P", root.GetIndirectReference());
        parent.GetDictionary().AddKey("Pg", page.GetObject().GetIndirectReference());

        PoDoFo::PdfArray kArray;

        // Visual order is bottom-up here, but structure order is top-down
        for (int i = 0; i < 5; ++i) {
            auto& child = objects.CreateObject(PoDoFo::PdfDictionary());
            child.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructElem"));
            child.GetDictionary().AddKey("S", PoDoFo::PdfName("P"));
            child.GetDictionary().AddKey("P", parent.GetIndirectReference());
            PoDoFo::PdfArray bbox;
            // Structure element i has visual position that is inverted
            double y = 100 - i * 20; // 100, 80, 60, 40, 20
            bbox.Add(0.0); bbox.Add(y); bbox.Add(10.0); bbox.Add(y + 10.0);
            PoDoFo::PdfDictionary aDict;
            aDict.AddKey("BBox", bbox);
            child.GetDictionary().AddKey("A", aDict);
            kArray.Add(child.GetIndirectReference());
        }

        // Add an element that is severely out of visual order (drift > 2)
        auto& childOut = objects.CreateObject(PoDoFo::PdfDictionary());
        childOut.GetDictionary().AddKey("Type", PoDoFo::PdfName("StructElem"));
        childOut.GetDictionary().AddKey("S", PoDoFo::PdfName("P"));
        childOut.GetDictionary().AddKey("P", parent.GetIndirectReference());
        PoDoFo::PdfArray bboxOut;
        bboxOut.Add(0.0); bboxOut.Add(900.0); bboxOut.Add(10.0); bboxOut.Add(910.0);
        PoDoFo::PdfDictionary aDictOut;
        aDictOut.AddKey("BBox", bboxOut);
        childOut.GetDictionary().AddKey("A", aDictOut);
        // Put it at the end of structure, but it's at top of page (visual order 0)
        // Its structure position will be 6 (since parent is 0, children 1-5, this is 6)
        // Visual position will be 1 (parent is 0, this is 1, then the others).
        kArray.Add(childOut.GetIndirectReference());

        parent.GetDictionary().AddKey("K", kArray);

        PoDoFo::PdfArray rootKArray;
        rootKArray.Add(parent.GetIndirectReference());
        root.GetDictionary().AddKey("K", rootKArray);

        doc.Save(path.toUtf8().constData());
        return path;
    }

private slots:
    void initTestCase() {
        QVERIFY(m_tmpDir.isValid());
    }

    void testAnalyzeReadingOrder_InheritedPageNoFalsePositives() {
        QString path = createTaggedPdf();
        ReadingOrderResult result = analyzeReadingOrder(path);

        QVERIFY(result.tagged);
        QCOMPARE(result.elementCount, 4); // 1 parent + 3 children
        QVERIFY2(result.issues.isEmpty(), "Correctly tagged PDF should have no reading order issues");
    }

    void testAnalyzeReadingOrder_ReorderedElementsCaught() {
        QString path = createReorderedPdf();
        ReadingOrderResult result = analyzeReadingOrder(path);

        QVERIFY(result.tagged);
        QCOMPARE(result.elementCount, 7); // 1 parent + 6 children
        QVERIFY2(!result.issues.isEmpty(), "Reordered elements exceeding heuristic drift must be caught");
        
        bool foundSpecificIssue = false;
        for (const QString& issue : result.issues) {
            if (issue.contains("maps to visual position")) {
                foundSpecificIssue = true;
            }
        }
        QVERIFY(foundSpecificIssue);
    }
};

QTEST_GUILESS_MAIN(TestAccessibility)
#include "TestAccessibility.moc"
