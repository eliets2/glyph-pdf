#include "core/ToolId.h"

#include <QHash>

// ── Canonical toString ───────────────────────────────────────────────────

QString toolIdToString(ToolId id) {
    // Static lookup table (canonical name for each enum value)
    static const QHash<ToolId, QString> map = {
        // Home
        { ToolId::Open,           QStringLiteral("open") },
        { ToolId::Save,           QStringLiteral("save") },
        { ToolId::SaveAs,         QStringLiteral("saveAs") },
        { ToolId::Print,          QStringLiteral("print") },
        { ToolId::PrintPreview,   QStringLiteral("printPreview") },
        { ToolId::PageSetup,      QStringLiteral("pageSetup") },
        { ToolId::ExportPresets,  QStringLiteral("exportPresets") },
        { ToolId::Share,          QStringLiteral("share") },
        { ToolId::Properties,     QStringLiteral("properties") },
        // View
        { ToolId::ZoomIn,         QStringLiteral("zoomIn") },
        { ToolId::ZoomOut,        QStringLiteral("zoomOut") },
        { ToolId::ActualSize,     QStringLiteral("actual") },
        { ToolId::FitWidth,       QStringLiteral("fitWidth") },
        { ToolId::FitPage,        QStringLiteral("fitPage") },
        { ToolId::SinglePage,     QStringLiteral("single") },
        { ToolId::Continuous,     QStringLiteral("continuous") },
        { ToolId::TwoPage,        QStringLiteral("two") },
        { ToolId::Presentation,   QStringLiteral("presentation") },
        { ToolId::Fullscreen,     QStringLiteral("fullscreen") },
        { ToolId::DarkMode,       QStringLiteral("darkMode") },
        { ToolId::EyeCare,        QStringLiteral("eyeCare") },
        // Edit
        { ToolId::Hand,           QStringLiteral("hand") },
        { ToolId::Select,         QStringLiteral("select") },
        { ToolId::SelectObject,   QStringLiteral("selectObj") },
        { ToolId::EditText,       QStringLiteral("editText") },
        { ToolId::EditObject,     QStringLiteral("editObject") },
        { ToolId::EditImage,      QStringLiteral("editImage") },
        { ToolId::Search,         QStringLiteral("search") },
        { ToolId::Ocr,            QStringLiteral("ocr") },
        { ToolId::Highlight,      QStringLiteral("highlight") },
        { ToolId::Underline,      QStringLiteral("underline") },
        { ToolId::Strikeout,      QStringLiteral("strikeout") },
        { ToolId::Squiggly,       QStringLiteral("squiggly") },
        { ToolId::Note,           QStringLiteral("note") },
        { ToolId::Comment,        QStringLiteral("comment") },
        { ToolId::Pencil,         QStringLiteral("pencil") },
        { ToolId::Freehand,       QStringLiteral("freehand") },
        { ToolId::TextBox,        QStringLiteral("textbox") },
        { ToolId::AddText,        QStringLiteral("addText") },
        { ToolId::Line,           QStringLiteral("line") },
        { ToolId::Arrow,          QStringLiteral("arrow") },
        { ToolId::Rectangle,      QStringLiteral("rect") },
        { ToolId::Oval,           QStringLiteral("oval") },
        { ToolId::Signature,      QStringLiteral("signature") },
        { ToolId::Image,          QStringLiteral("image") },
        { ToolId::MarkRedact,     QStringLiteral("markRedact") },
        // Pages
        { ToolId::RotateCW,       QStringLiteral("rotate") },
        { ToolId::RotateCCW,      QStringLiteral("rotL") },
        { ToolId::DeletePage,     QStringLiteral("deletePage") },
        { ToolId::InsertPage,     QStringLiteral("insertPage") },
        { ToolId::Extract,        QStringLiteral("extract") },
        { ToolId::Split,          QStringLiteral("split") },
        { ToolId::Reorder,        QStringLiteral("reorder") },
        { ToolId::Crop,           QStringLiteral("crop") },
        { ToolId::Resize,         QStringLiteral("resize") },
        { ToolId::AddHeader,      QStringLiteral("addHeader") },
        { ToolId::AddFooter,      QStringLiteral("addFooter") },
        { ToolId::AddPageNumbers, QStringLiteral("addPageNumbers") },
        { ToolId::BatesNumber,    QStringLiteral("batesNumber") },
        // Convert
        { ToolId::Combine,        QStringLiteral("combine") },
        { ToolId::ToWord,         QStringLiteral("toWord") },
        { ToolId::ToExcel,        QStringLiteral("toExcel") },
        { ToolId::ToCsv,          QStringLiteral("toCSV") },
        { ToolId::ToHtml,         QStringLiteral("toHTML") },
        { ToolId::ToText,         QStringLiteral("toText") },
        { ToolId::ToPPT,          QStringLiteral("toPPT") },
        { ToolId::Compress,       QStringLiteral("compress") },
        { ToolId::Linearize,      QStringLiteral("linearize") },
        { ToolId::PdfA,           QStringLiteral("pdfA") },
        // Forms
        { ToolId::TextField,      QStringLiteral("textField") },
        { ToolId::Checkbox,       QStringLiteral("checkbox") },
        { ToolId::Radio,          QStringLiteral("radio") },
        { ToolId::Dropdown,       QStringLiteral("dropdown") },
        { ToolId::CreateForm,     QStringLiteral("createForm") },
        { ToolId::ListBox,        QStringLiteral("listbox") },
        { ToolId::Button,         QStringLiteral("button") },
        { ToolId::DateField,      QStringLiteral("dateField") },
        { ToolId::NumField,       QStringLiteral("numField") },
        { ToolId::SigField,       QStringLiteral("sigField") },
        { ToolId::AutoDetect,     QStringLiteral("autoDetect") },
        { ToolId::Tabs,           QStringLiteral("tabs") },
        { ToolId::ImportData,     QStringLiteral("importData") },
        { ToolId::ExportData,     QStringLiteral("exportData") },
        // Security
        { ToolId::Encrypt,        QStringLiteral("encrypt") },
        { ToolId::Password,       QStringLiteral("password") },
        { ToolId::Sign,           QStringLiteral("sign") },
        { ToolId::ValidateSig,    QStringLiteral("validateSig") },
        { ToolId::Sanitize,       QStringLiteral("sanitize") },
        { ToolId::ApplyRedact,    QStringLiteral("applyRedact") },
        { ToolId::ExportAnno,     QStringLiteral("exportAnno") },
        { ToolId::ImportAnno,     QStringLiteral("importAnno") },
        { ToolId::Cloud,          QStringLiteral("cloud") },
        { ToolId::Permissions,    QStringLiteral("permissions") },
        { ToolId::RemoveSecurity, QStringLiteral("removeSec") },
        { ToolId::Certify,        QStringLiteral("certify") },
        { ToolId::Timestamp,      QStringLiteral("timestamp") },
        { ToolId::PatternRedact,  QStringLiteral("patternRedact") },
        { ToolId::RegexRedact,    QStringLiteral("regexRedact") },
    };
    return map.value(id, QStringLiteral("unknown"));
}

// ── Alias-aware fromString ───────────────────────────────────────────────

std::optional<ToolId> toolIdFromString(const QString& str) {
    // Build once on first call: canonical + alias strings → ToolId.
    // All keys stored lowercase for case-insensitive comparison.
    static const QHash<QString, ToolId> map = []() {
        QHash<QString, ToolId> m;

        // Helper: insert a canonical key plus arbitrary aliases
        auto add = [&](ToolId id, std::initializer_list<const char*> keys) {
            for (auto k : keys) m.insert(QString::fromLatin1(k).toLower(), id);
        };

        // ── Home ──
        add(ToolId::Open,           {"open"});
        add(ToolId::Save,           {"save"});
        add(ToolId::SaveAs,         {"saveAs", "save-as", "saveas"});
        add(ToolId::Print,          {"print"});
        add(ToolId::PrintPreview,   {"printPreview", "print-preview", "printpreview"});
        add(ToolId::PageSetup,      {"pageSetup", "page-setup", "pagesetup"});
        add(ToolId::ExportPresets,  {"exportPresets", "export-presets", "exportpresets"});
        add(ToolId::Share,          {"share"});
        add(ToolId::Properties,     {"properties"});

        // ── View ──
        add(ToolId::ZoomIn,         {"zoomIn", "zoom-in", "zoomin"});
        add(ToolId::ZoomOut,        {"zoomOut", "zoom-out", "zoomout"});
        add(ToolId::ActualSize,     {"actual", "actual-size", "actualsize"});
        add(ToolId::FitWidth,       {"fitWidth", "fit-width", "fitwidth"});
        add(ToolId::FitPage,        {"fitPage", "fit-page", "fitpage"});
        add(ToolId::SinglePage,     {"single", "single-page", "singlepage"});
        add(ToolId::Continuous,     {"continuous"});
        add(ToolId::TwoPage,        {"two", "two-page", "twopage"});
        add(ToolId::Presentation,   {"presentation"});
        add(ToolId::Fullscreen,     {"fullscreen"});
        add(ToolId::DarkMode,       {"darkMode", "darkmode"});
        add(ToolId::EyeCare,        {"eyeCare", "eyecare"});

        // ── Edit ──
        add(ToolId::Hand,           {"hand"});
        add(ToolId::Select,         {"select"});
        add(ToolId::SelectObject,   {"selectObj", "selectobj"});
        add(ToolId::EditText,       {"editText", "edit-text", "edittext"});
        add(ToolId::EditObject,     {"editObject", "editobject"});
        add(ToolId::EditImage,      {"editImage", "editimage"});
        add(ToolId::Search,         {"search", "find"});
        add(ToolId::Ocr,            {"ocr"});
        add(ToolId::Highlight,      {"highlight"});
        add(ToolId::Underline,      {"underline"});
        add(ToolId::Strikeout,      {"strikeout", "strike"});
        add(ToolId::Squiggly,       {"squiggly"});
        add(ToolId::Note,           {"note"});
        add(ToolId::Comment,        {"comment"});
        add(ToolId::Pencil,         {"pencil"});
        add(ToolId::Freehand,       {"freehand"});
        add(ToolId::TextBox,        {"textbox"});
        add(ToolId::AddText,        {"addText", "addtext"});
        add(ToolId::Line,           {"line"});
        add(ToolId::Arrow,          {"arrow"});
        add(ToolId::Rectangle,      {"rect", "rectangle"});
        add(ToolId::Oval,           {"oval", "ellipse"});
        add(ToolId::Signature,      {"signature"});
        add(ToolId::Image,          {"image"});
        add(ToolId::MarkRedact,     {"markRedact", "markredact"});

        // ── Pages ──
        add(ToolId::RotateCW,       {"rotate", "rotR", "rotate-cw", "rotr", "rotatecw"});
        add(ToolId::RotateCCW,      {"rotL", "rotate-ccw", "rotl", "rotateccw"});
        add(ToolId::DeletePage,     {"deletePage", "delete-page", "deletepage"});
        add(ToolId::InsertPage,     {"insertPage", "insert-page", "insertpage"});
        add(ToolId::Extract,        {"extract"});
        add(ToolId::Split,          {"split"});
        add(ToolId::Reorder,        {"reorder"});
        add(ToolId::Crop,           {"crop"});
        add(ToolId::Resize,         {"resize"});
        add(ToolId::AddHeader,      {"addHeader", "addheader", "header"});
        add(ToolId::AddFooter,      {"addFooter", "addfooter", "footer"});
        add(ToolId::AddPageNumbers, {"addPageNumbers", "addpagenumbers", "page-numbers"});
        add(ToolId::BatesNumber,    {"batesNumber", "batesnumber", "bates"});

        // ── Convert ──
        add(ToolId::Combine,        {"combine", "combinePDF", "combinepdf"});
        add(ToolId::ToWord,         {"toWord", "to-word", "toword"});
        add(ToolId::ToExcel,        {"toExcel", "to-excel", "toexcel"});
        add(ToolId::ToCsv,          {"toCSV", "exportCSV", "tocsv", "exportcsv"});
        add(ToolId::ToHtml,         {"toHTML", "to-h-t-m-l", "tohtml"});
        add(ToolId::ToText,         {"toText", "to-text", "totext"});
        add(ToolId::ToPPT,          {"toPPT", "to-ppt", "toppt", "powerpoint"});
        add(ToolId::Compress,       {"compress"});
        add(ToolId::Linearize,      {"linearize"});
        add(ToolId::PdfA,           {"pdfA", "pdfa"});

        // ── Forms ──
        add(ToolId::TextField,      {"textField", "text-field", "textfield"});
        add(ToolId::Checkbox,       {"checkbox"});
        add(ToolId::Radio,          {"radio"});
        add(ToolId::Dropdown,       {"dropdown"});
        add(ToolId::CreateForm,     {"createForm", "createform"});
        add(ToolId::ListBox,        {"listbox"});
        add(ToolId::Button,         {"button"});
        add(ToolId::DateField,      {"dateField", "datefield"});
        add(ToolId::NumField,       {"numField", "numfield"});
        add(ToolId::SigField,       {"sigField", "sigfield"});
        add(ToolId::AutoDetect,     {"autoDetect", "autodetect"});
        add(ToolId::Tabs,           {"tabs", "tab-order", "taborder"});
        add(ToolId::ImportData,     {"importData", "importdata", "import-data"});
        add(ToolId::ExportData,     {"exportData", "exportdata", "export-data"});

        // ── Security ──
        add(ToolId::Encrypt,        {"encrypt"});
        add(ToolId::Password,       {"password"});
        add(ToolId::Sign,           {"sign"});
        add(ToolId::ValidateSig,    {"validateSig", "validate", "validatesig"});
        add(ToolId::Sanitize,       {"sanitize"});
        add(ToolId::ApplyRedact,    {"applyRedact", "applyredact"});
        add(ToolId::ExportAnno,     {"exportAnno", "exportanno"});
        add(ToolId::ImportAnno,     {"importAnno", "importanno"});
        add(ToolId::Cloud,          {"cloud"});
        add(ToolId::Permissions,    {"permissions"});
        add(ToolId::RemoveSecurity, {"removeSec", "removesec"});
        add(ToolId::Certify,        {"certify"});
        add(ToolId::Timestamp,      {"timestamp"});
        add(ToolId::PatternRedact,  {"patternRedact", "patternredact"});
        add(ToolId::RegexRedact,    {"regexRedact", "regexredact"});

        return m;
    }();

    auto it = map.find(str.toLower());
    if (it != map.end())
        return it.value();
    return std::nullopt;
}

bool isValidToolIdString(const QString& str) {
    return toolIdFromString(str).has_value();
}
