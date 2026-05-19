#include "RibbonModel.h"

namespace gp {

// Build tabs one at a time to keep compiler memory usage down.
// (A single massive aggregate initializer can OOM g++ with PCH.)

static RibbonTabDef makeHome() {
    return { "Home", {
        { "Selection", {{ "select","Select","cursor",true },{ "hand","Hand","hand",false },{ "snapshot","Snapshot","share",false }}},
        { "Find", {{ "search","Find","search",true },{ "findRep","Find & Replace","search",false },{ "regex","Regex","search",false }}},
        { "History", {{ "undo","Undo","undo",true },{ "redo","Redo","redo",false },{ "history","History","more",false }}},
        { "Share", {{ "share","Share","share",true },{ "cloud","Cloud Sync","cloudSync",false },{ "sendSign","Send for Sig.","share",false }}},
        { "File", {{ "save","Save","save",true },{ "saveAs","Save As","save",false },{ "print","Print","print",false }}},
    }};
}
static RibbonTabDef makeView() {
    return { "View", {
        { "Zoom", {{ "zoomIn","Zoom In","zoomIn",true },{ "zoomOut","Zoom Out","zoomOut",false },{ "actual","Actual Size","zoomIn",false },{ "fitWidth","Fit Width","zoomIn",false },{ "fitPage","Fit Page","zoomIn",false }}},
        { "Layout", {{ "single","Single Page","form",true },{ "continuous","Continuous","form",false },{ "two","Two Up","form",false },{ "presentation","Presentation","form",false }}},
        { "Panes", {{ "thumbs","Thumbnails","reorder",true },{ "bookmarks","Bookmarks","note",false },{ "comments","Comments","comment",false },{ "layers","Layers","reorder",false }}},
        { "Reading", {{ "darkMode","Dark Mode","rotate",true },{ "eyeCare","Eye Care","rotate",false },{ "rtl","RTL","rotate",false }}},
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
        { "Stamps", {{ "stamp","Stamp","form",true },{ "customStamp","Custom","form",false }}},
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
        { "Distribute", {{ "sendForm","Send Form","share",true },{ "collect","Collect","merge",false },{ "submit","Submit","share",false }}},
    }};
}
static RibbonTabDef makeProtect() {
    return { "Protect", {
        { "Security", {{ "password","Password","lock",true },{ "permissions","Permissions","lock",false },{ "encrypt","Encrypt","lock",false },{ "removeSec","Remove Sec.","lock",false }}},
        { "Redact", {{ "markRedact","Mark","redact",true },{ "applyRedact","Apply","redact",false },{ "patternRedact","Pattern","redact",false },{ "regexRedact","Regex","redact",false },{ "sanitize","Sanitize","redact",false }}},
        { "Sign", {{ "certify","Certify","signature",true },{ "sign","Sign","signature",false },{ "timestamp","Timestamp","signature",false },{ "validateSig","Validate","signature",false },{ "trust","Trust Store","lock",false }}},
        { "Compliance", {{ "auditLog","Audit Log","form",true },{ "dlp","DLP","lock",false },{ "policy","Policy","form",false }}},
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
