// SPDX-License-Identifier: Apache-2.0
#include "RibbonModel.h"
#include <QHash>

namespace gp {

// ── R15 (PP06 / UI01): the honest planned-entry table ────────────────────────
// Every planned ribbon entry stays VISIBLE and DISABLED with a truthful reason
// and a supported alternative (never a hidden row, never an enabled no-op).
// An entry leaves this table only in the same commit that wires its real
// route. Promoted in this wave (real routes exist): findRep, regex (T2-2
// dialog), measure/distance/area (T1 Measure panel), ocrVerify/ocrLang (OCR
// Verify screen), thumbs/bookmarks/comments/layers (sidebar panes), batchConv/
// watch (Batch workspace incl. hot folder), reduce (alias of Compress),
// calc (alias of CalcField — Form Builder calculated fields).
const QVector<RibbonModel::PlannedToolSpec>& RibbonModel::plannedToolSpecs() {
    static const QVector<PlannedToolSpec> specs = {
        // Home
        { "snapshot", "Page snapshots are not available yet.",
          "Convert the page to an image (Convert \xE2\x96\xB8 Image) or select and copy text." },
        { "history", "A history browser is not available yet.",
          "Undo and Redo (Ctrl+Z / Ctrl+Y) step through recent changes." },
        // View — window management
        { "splitWin", "Split view is not available yet.",
          "Two-Page view (View \xE2\x96\xB8 Layout) shows two pages side by side." },
        { "newWin", "Multiple windows are not supported yet.",
          "Open one document at a time in this window." },
        // Edit — advanced text/object operations
        { "insertText", "Inserting text into existing content is not available yet.",
          "Add Text (Edit \xE2\x96\xB8 Text) places a new text box you can position and edit." },
        { "deleteText", "Deleting text objects is not available yet.",
          "Redaction (Protect \xE2\x96\xB8 Mark) permanently removes content." },
        { "link", "Editing links is not available yet.",
          "Existing links stay clickable and are preserved on save." },
        { "attach", "Attaching files is not available yet.",
          "Existing attachments can be exported from the Files panel (left sidebar)." },
        { "alignL", "Object alignment is not available yet.",
          "Move objects precisely with the Select tool." },
        { "alignC", "Object alignment is not available yet.",
          "Move objects precisely with the Select tool." },
        { "alignR", "Object alignment is not available yet.",
          "Move objects precisely with the Select tool." },
        { "distribute", "Object distribution is not available yet.",
          "Move objects precisely with the Select tool." },
        { "group", "Grouping objects is not available yet.",
          "Move or delete objects individually with the Select tool." },
        { "layerOrder", "Object stacking order is not available yet.",
          "Move or delete objects individually with the Select tool." },
        { "ocrSettings", "OCR has no separate settings dialog yet.",
          "Choose the recognition language on the OCR Verify screen (Tools \xE2\x96\xB8 OCR)." },
        // Organize — page decoration and advanced ops
        { "replace", "Page replacement is not available yet.",
          "Delete the pages, then insert the replacement file's pages (Organize \xE2\x96\xB8 Pages)." },
        { "reverse", "One-click page reversal is not available yet.",
          "Reorder pages by drag in the Pages task (Organize \xE2\x96\xB8 Pages)." },
        { "background", "Page backgrounds are not available yet.",
          "Watermark (Organize \xE2\x96\xB8 Decorate) adds a visible overlay to every page." },
        // Comment — advanced annotation and review
        { "poly", "Polyline drawing is not available yet.",
          "Use the Pencil for freehand shapes or Line for straight segments." },
        { "summary", "The review summary lives in the Comments panel.",
          "Open the Comments panel (left sidebar) \xE2\x80\x94 it shows the summary and exports CSV." },
        { "filterComm", "Comment filters live in the Comments panel.",
          "Open the Comments panel (left sidebar) to filter the review list." },
        { "statusComm", "Comment review statuses live in the Comments panel.",
          "Open the Comments panel (left sidebar) to set review statuses." },
        { "trackChanges", "Change tracking is not available yet.",
          "The Comments panel lists every annotation with its author and date." },
        { "reply", "Replying works on a selected annotation.",
          "Select an annotation, then reply in the Inspector's reply thread (right sidebar)." },
        // Convert — additional format targets
        { "toMD", "Markdown export is not available yet.",
          "Text (Convert \xE2\x96\xB8 Text) or HTML export covers plain-text and web use." },
        { "toEPUB", "EPUB export is not available yet.",
          "Text (Convert \xE2\x96\xB8 Text) or HTML export covers plain-text and web use." },
        { "fromScan", "Scanner capture is not available yet.",
          "Import scanned images with Images to PDF, then run OCR (Edit \xE2\x96\xB8 Run OCR)." },
        { "fromWeb", "Web-page capture is not available yet.",
          "Print the page to PDF from your browser and open it here." },
        { "extractTables", "Dedicated table extraction is not available yet.",
          "CSV export (Convert \xE2\x96\xB8 CSV) keeps the column layout for spreadsheet use." },
        { "detectTables", "Automatic table detection is not available yet.",
          "CSV export (Convert \xE2\x96\xB8 CSV) captures text with column structure." },
        { "preset", "Saved conversion presets are not available yet.",
          "File \xE2\x96\xB8 Export Presets\xE2\x80\xA6 stores reusable export settings." },
        // Forms — validation, reset, flatten
        { "rules", "Field validation rules are not editable yet.",
          "The Form Builder (Forms tab) creates fields and edits their properties." },
        { "required", "Marking fields as required is not available yet.",
          "The Form Builder (Forms tab) creates fields and edits their properties." },
        { "reset", "Resetting filled form values is not available yet.",
          "Undo recent edits (Ctrl+Z), or reopen the original form file." },
        { "flatten", "Flattening form fields is not available yet.",
          "No in-app alternative yet \xE2\x80\x94 saved copies keep their fields editable." },
        // Protect — trust store management
        { "trust", "Certificate trust management is not available yet.",
          "Signature badges on the page show each signature's validity state "
          "(right sidebar \xE2\x96\xB8 Signatures)." },
    };
    return specs;
}

const QSet<QString>& RibbonModel::plannedTools() {
    static const QSet<QString> planned = []() {
        QSet<QString> ids;
        for (const auto& spec : plannedToolSpecs())
            ids.insert(spec.id);
        return ids;
    }();
    return planned;
}

const RibbonModel::PlannedToolSpec* RibbonModel::plannedSpecFor(const QString& id) {
    static const QHash<QString, const PlannedToolSpec*> byId = []() {
        QHash<QString, const PlannedToolSpec*> m;
        for (const auto& spec : plannedToolSpecs())
            m.insert(spec.id, &spec);
        return m;
    }();
    return byId.value(id, nullptr);
}


// Build tabs one at a time to keep compiler memory usage down.
// (A single massive aggregate initializer can OOM g++ with PCH.)

static RibbonTabDef makeHome() {
    return { "Home", {
        { "Selection", {{ "select","Select","cursor",true },{ "hand","Hand","hand",false },{ "snapshot","Snapshot","share",false }}},
        { "Find", {{ "search","Find","search",true },{ "findRep","Find & Replace","search",false },{ "regex","Regex","search",false }}},
        { "History", {{ "undo","Undo","undo",true },{ "redo","Redo","redo",false },{ "history","History","more",false }}},
        { "Share", {{ "share","Share","share",true }}},
        { "File", {{ "save","Save","save",true },{ "saveAs","Save As","save",false },{ "print","Print","print",false }}},
    }};
}
static RibbonTabDef makeView() {
    return { "View", {
        { "Zoom", {{ "zoomIn","Zoom In","zoomIn",true },{ "zoomOut","Zoom Out","zoomOut",false },{ "actual","Actual Size","zoomIn",false },{ "fitWidth","Fit Width","zoomIn",false },{ "fitPage","Fit Page","zoomIn",false }}},
        { "Layout", {{ "single","Single Page","form",true },{ "continuous","Continuous","form",false },{ "two","Two Up","form",false },{ "presentation","Presentation","form",false }}},
        { "Panes", {{ "thumbs","Thumbnails","reorder",true },{ "bookmarks","Bookmarks","note",false },{ "comments","Comments","comment",false },{ "layers","Layers","reorder",false }}},
        { "Reading", {{ "darkMode","Dark Mode","rotate",true },{ "eyeCare","Eye Care","rotate",false },{ "nightMode","Night Mode","rotate",false },{ "rtl","RTL","rotate",false }}},
        { "Window", {{ "splitWin","Split","compare",true },{ "compare","Compare","compare",true },{ "newWin","New","form",false }}},
    }};
}
static RibbonTabDef makeEdit() {
    return { "Edit", {
        { "Text", {{ "editText","Edit Text","editText",true },{ "addText","Add Text","textbox",false },{ "insertText","Insert","textbox",false },{ "deleteText","Delete","redact",false }}},
        { "Objects", {{ "image","Image","form",true },{ "link","Link","form",false },{ "attach","Attach","comment",false },{ "delete","Delete","redact",false }}},
        { "Arrange", {{ "alignL","Align L","form",false },{ "alignC","Align C","form",false },{ "alignR","Align R","form",false },{ "distribute","Distribute","form",false },{ "group","Group","form",false },{ "layerOrder","Order","form",false }}},
        { "OCR", {{ "ocr","Run OCR","ocr",true },{ "ocrVerify","Verify Text","ocr",false },{ "ocrLang","Language","ocr",false },{ "ocrSettings","Settings","more",false }}},
        { "Measure", {{ "measure","Measure","form",true },{ "distance","Distance","form",false },{ "area","Area","form",false }}},
    }};
}
static RibbonTabDef makeOrganize() {
    return { "Organize", {
        { "Pages", {{ "insertPage","Insert","insertPage",true },{ "deletePage","Delete","deletePage",false },{ "rotate","Rotate","rotate",false },{ "replace","Replace","rotate",false },{ "reorder","Reorder","reorder",false },{ "reverse","Reverse","rotate",false }}},
        { "Document", {{ "split","Split","compare",true },{ "merge","Merge","merge",true },{ "extract","Extract","merge",false },{ "compareDocs","Compare","compare",false }}},
        { "Numbering", {{ "pageNums","Page Numbers","form",true },{ "header","Header/Footer","textbox",false },{ "bates","Bates","form",false }}},
        { "Decorate", {{ "watermark","Watermark","form",true },{ "background","Background","form",false }}},
    }};
}
static RibbonTabDef makeComment() {
    return { "Comment", {
        { "Markup", {{ "highlight","Highlight","highlight",true },{ "underline","Underline","underline",false },{ "strike","Strikeout","strike",false },{ "squiggly","Squiggly","underline",false }}},
        { "Notes", {{ "note","Sticky Note","note",true },{ "callout","Callout","comment",false },{ "textbox","Text Box","textbox",false }}},
        { "Drawing", {{ "line","Line","editText",false },{ "arrow","Arrow","editText",false },{ "rect","Rectangle","form",false },{ "oval","Oval","form",false },{ "poly","Polyline","editText",false },{ "pencil","Pencil","editText",false },{ "eraser","Eraser","redact",false }}},
        { "Stamps", {{ "stamp","Stamp","form",true },{ "customStamp","Custom","form",true }}},
        { "Review", {{ "summary","Summary","form",true },{ "filterComm","Filter","form",false },{ "statusComm","Status","form",false },{ "trackChanges","Track Changes","form",false },{ "reply","Reply","comment",false }}},
    }};
}
static RibbonTabDef makeConvert() {
    return { "Convert", {
        { "From PDF", {{ "toWord","Word","form",true },{ "toExcel","Excel","form",true },{ "toPPT","PowerPoint","form",false },{ "toHTML","HTML","form",false },{ "toMD","Markdown","form",false },{ "toEPUB","EPUB","form",false },{ "toText","Text","form",false },{ "toImage","Image","form",false },{ "toCSV","CSV","form",false }}},
        { "To PDF", {{ "fromFile","From File","form",true },{ "fromScan","Scanner","form",false },{ "fromWeb","Web Page","form",false },{ "combinePDF","Combine","merge",false }}},
        { "Tables", {{ "extractTables","Extract Tables","form",true },{ "detectTables","Auto-detect","form",false },{ "exportCSV","Export CSV","form",false }}},
        { "Optimize", {{ "compress","Compress","form",true },{ "reduce","Reduce Size","form",false },{ "pdfA","PDF/A","form",false }}},
        { "Batch", {{ "batchConv","Batch","merge",true },{ "preset","Preset","form",false },{ "watch","Watch Folder","form",false }}},
    }};
}
static RibbonTabDef makeForms() {
    return { "Forms", {
        { "Design", {{ "textField","Text Field","textbox",true },{ "checkbox","Checkbox","form",false },{ "radio","Radio","form",false },{ "dropdown","Dropdown","form",false },{ "listbox","List Box","form",false },{ "button","Button","form",false },{ "dateField","Date","form",false },{ "numField","Numeric","form",false },{ "sigField","Signature","signature",false }}},
        { "Build", {{ "createForm","Create Form","form",true },{ "autoDetect","Auto-detect","form",false },{ "tabs","Tab Order","form",false }}},
        { "Validate", {{ "rules","Validation","form",true },{ "required","Required","form",false },{ "calc","Calculation","form",false }}},
        { "Data", {{ "export","Export Data","merge",false },{ "import","Import Data","insertPage",false },{ "reset","Reset","redact",false },{ "flatten","Flatten","form",false }}},
    }};
}
static RibbonTabDef makeProtect() {
    return { "Protect", {
        { "Security", {{ "password","Password","lock",true },{ "permissions","Permissions","lock",false },{ "encrypt","Encrypt","lock",false },{ "certEncrypt","Encrypt (Certs)","lock",false },{ "removeSec","Remove Sec.","lock",false }}},
        { "Redact", {{ "markRedact","Mark","redact",true },{ "applyRedact","Apply","redact",false },{ "patternRedact","Pattern","redact",false },{ "regexRedact","Regex","redact",false },{ "sanitize","Sanitize","redact",false }}},
        { "Sign", {{ "certify","Certify","signature",true },{ "sign","Sign","signature",false },{ "prepareSigningReq","Prepare Request","signature",false },{ "timestamp","Timestamp","signature",false },{ "validateSig","Validate","signature",false },{ "trust","Trust Store","lock",false }}},
    }};
}

const QVector<RibbonTabDef>& RibbonModel::tabs() {
    static const QVector<RibbonTabDef> defs = {
        makeHome(), makeView(), makeEdit(), makeOrganize(),
        makeComment(), makeConvert(), makeForms(), makeProtect()
    };
    return defs;
}

} // namespace gp
