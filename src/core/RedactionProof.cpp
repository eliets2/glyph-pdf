// SPDX-License-Identifier: Apache-2.0
#include "core/RedactionProof.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <podofo/podofo.h>

#include "core/PageSpaceTransform.h"
#include "engines/pdfium/PdfiumBackend.h"

// Windows headers pulled in transitively define `#define DrawText DrawTextW`;
// keep the guard consistent with the other PoDoFo TUs.
#ifdef DrawText
#undef DrawText
#endif
// Same hazard for `GetObject` (→ GetObjectW): PdfAnnotation::GetObject() is
// needed for the SEP13 L7 annotation/form attribution walk.
#ifdef GetObject
#undef GetObject
#endif

#include <functional>
#include <optional>

namespace gp {
namespace RedactionProof {

namespace {

constexpr int kMaxWalkDepth = 32;

QString sha256Hex(const QString& path, qint64* sizeOut = nullptr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    if (sizeOut) *sizeOut = f.size();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buf[65536];
    while (!f.atEnd()) {
        const qint64 n = f.read(buf, sizeof(buf));
        if (n <= 0) return QString();
        hash.addData(buf, int(n));
    }
    return QString::fromLatin1(hash.result().toHex());
}

QByteArray utf16Bytes(const QString& text, bool bigEndian, bool withBom)
{
    QByteArray bytes;
    if (withBom) bytes += bigEndian ? QByteArrayLiteral("\xFE\xFF") : QByteArrayLiteral("\xFF\xFE");
    for (QChar ch : text) {
        const ushort u = ch.unicode();
        if (bigEndian) {
            bytes += char((u >> 8) & 0xFF);
            bytes += char(u & 0xFF);
        } else {
            bytes += char(u & 0xFF);
            bytes += char((u >> 8) & 0xFF);
        }
    }
    return bytes;
}

// Case-sensitive byte search used on every raw/decoded surface.
bool containsAny(const QByteArray& haystack, const QList<QByteArray>& needles)
{
    for (const QByteArray& n : needles)
        if (!n.isEmpty() && haystack.contains(n)) return true;
    return false;
}

// ── content-stream operator counting ────────────────────────────────────────

// Lexical scan of a decoded content stream: literal strings (with full
// \-escapes and balanced nesting), hex strings, dicts and comments are
// skipped; the GLYPH-CARRYING text-showing operators are counted.
//
// "Glyph-carrying" matters because the excision engine replaces a removed
// `(...) Tj` with a numeric-only `[ N ] TJ` (the Edact-Ray advance-gap
// defense: the cursor shift survives, the glyphs do not). A raw Tj/TJ count
// would then NOT decrease across a real excision; a count of text-showing
// operators that actually carry glyph strings does.
int countTextOperatorsImpl(const QByteArray& s)
{
    int count = 0;
    int i = 0;
    const int n = s.size();
    bool inArray = false;
    bool arrayHasString = false;
    auto isWhite = [](char c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\0' || c == '\f';
    };
    while (i < n) {
        const char c = s[i];
        if (isWhite(c)) { ++i; continue; }
        if (c == '%') { // comment to end of line
            while (i < n && s[i] != '\n' && s[i] != '\r') ++i;
            continue;
        }
        if (c == '(') { // literal string: nesting + escapes
            if (inArray) arrayHasString = true;
            int depth = 1;
            ++i;
            while (i < n && depth > 0) {
                if (s[i] == '\\') { i += 2; continue; }
                if (s[i] == '(') ++depth;
                else if (s[i] == ')') --depth;
                ++i;
            }
            continue;
        }
        if (c == '<') {
            if (i + 1 < n && s[i + 1] == '<') { i += 2; continue; } // dict
            if (inArray) arrayHasString = true;                     // hex string
            ++i;
            while (i < n && s[i] != '>') ++i;
            ++i;
            continue;
        }
        if (c == '[') { inArray = true; arrayHasString = false; ++i; continue; }
        if (c == ']') { inArray = false; ++i; continue; }
        // Operand / operator / name token: read to the next delimiter.
        const int b = i;
        while (i < n && !isWhite(s[i]) && s[i] != '/' && s[i] != '(' && s[i] != '<'
               && s[i] != '[' && s[i] != ']' && s[i] != '{' && s[i] != '}' && s[i] != '%')
            ++i;
        if (i == b) { ++i; continue; } // lone delimiter — do not stall
        const QByteArray tok = s.mid(b, i - b);
        if (tok == "Tj" || tok == "'" || tok == "\"")
            ++count;                       // always glyph-carrying
        else if (tok == "TJ" && arrayHasString)
            ++count;                       // numeric-only [ N ] TJ is NOT glyph-carrying
    }
    return count;
}

// ── PDF model helpers (PoDoFo, read-only) ───────────────────────────────────

struct PageMechanics {
    bool ok = false;
    QString sha256;
    int textOps = 0;
};

// A page's decoded content stream + mechanical counts. `PdfContents::CopyTo`
// returns the FILTERED bytes (PoDoFoBackend relies on the same contract).
PageMechanics pageMechanics(PoDoFo::PdfPage& page)
{
    PageMechanics m;
    try {
        PoDoFo::PdfContents* contents = page.GetContents();
        if (!contents) return m;
        PoDoFo::charbuff buf;
        contents->CopyTo(buf);
        const QByteArray decoded(buf.data(), int(buf.size()));
        m.sha256 = QString::fromLatin1(
            QCryptographicHash::hash(decoded, QCryptographicHash::Sha256).toHex());
        m.textOps = countTextOperatorsImpl(decoded);
        m.ok = true;
    } catch (const std::exception&) {
        m.ok = false;
    }
    return m;
}

// Raw bytes of every string value reachable inside one object's variant tree
// (dictionaries, arrays; depth-capped; references are NOT followed — the
// caller walks the whole indirect-object list). Raw bytes keep the sweep
// exact: no QString conversion round trip on the evidence path.
void collectStringBytes(const PoDoFo::PdfObject* obj, QList<QByteArray>* out, int depth)
{
    if (!obj || depth > kMaxWalkDepth) return;
    if (obj->IsString()) {
        const std::string_view raw = obj->GetString().GetString();
        out->append(QByteArray(raw.data(), int(raw.size())));
        return;
    }
    if (obj->IsDictionary()) {
        for (auto& kv : obj->GetDictionary())
            collectStringBytes(&kv.second, out, depth + 1);
        return;
    }
    if (obj->IsArray()) {
        for (const auto& el : obj->GetArray())
            collectStringBytes(&el, out, depth + 1);
    }
}

bool isImageStream(const PoDoFo::PdfObject* obj)
{
    if (!obj || !obj->IsDictionary()) return false;
    const auto* sub = obj->GetDictionary().FindKey("Subtype");
    return sub && sub->IsName() && sub->GetName() == PoDoFo::PdfName("Image");
}

bool hasMediaFilter(const PoDoFo::PdfObject* obj)
{
    if (!obj || !obj->IsDictionary()) return false;
    const auto* filter = obj->GetDictionary().FindKey("Filter");
    if (!filter) return false;
    QList<PoDoFo::PdfName> names;
    if (filter->IsName()) {
        names.append(filter->GetName());
    } else if (filter->IsArray()) {
        for (const auto& el : filter->GetArray())
            if (el.IsName()) names.append(el.GetName());
    }
    for (const auto& n : names) {
        const std::string_view s = n.GetString();
        if (s == "DCTDecode" || s == "JPXDecode" || s == "JBIG2Decode"
            || s == "CCITTFaxDecode" || s == "RunLengthDecode")
            return true;
    }
    return false;
}

// Walks the catalog /Names /EmbeddedFiles name tree, returning decoded
// payload bytes for every embedded file. Unwalkable entries are reported via
// `problems` (honest failure) — a tree we cannot walk may hide survivors.
struct EmbeddedPayload { QString name; QByteArray bytes; };
QList<EmbeddedPayload> embeddedFilePayloads(PoDoFo::PdfMemDocument& doc, QStringList* problems)
{
    QList<EmbeddedPayload> out;
    auto resolve = [&doc](const PoDoFo::PdfObject* o) -> const PoDoFo::PdfObject* {
        int hops = 0;
        while (o && o->IsReference() && hops++ < kMaxWalkDepth)
            o = &doc.GetObjects().MustGetObject(o->GetReference());
        return o;
    };
    std::function<void(const PoDoFo::PdfObject*, int)> walkTree =
        [&](const PoDoFo::PdfObject* node, int depth) {
            if (!node || depth > kMaxWalkDepth) return;
            node = resolve(node);
            if (!node || !node->IsDictionary()) return;
            const auto& dict = node->GetDictionary();
            if (dict.HasKey("Names")) {
                const auto* names = resolve(dict.FindKey("Names"));
                if (names && names->IsArray()) {
                    const auto& arr = names->GetArray();
                    // Name-tree /Names is a flat [key value key value ...] array.
                    for (unsigned i = 0; i + 1 < arr.GetSize(); i += 2) {
                        if (!arr[i].IsString()) continue;
                        const std::string_view rawName = arr[i].GetString().GetString();
                        const QString name = QString::fromLatin1(rawName.data(), int(rawName.size()));
                        const PoDoFo::PdfObject* spec = resolve(&arr[i + 1]);
                        if (!spec || !spec->IsDictionary()) {
                            problems->append(QStringLiteral("embedded file \"%1\": unreadable file specification")
                                                 .arg(name));
                            continue;
                        }
                        const auto* ef = resolve(spec->GetDictionary().FindKey("EF"));
                        const PoDoFo::PdfObject* streamObj = nullptr;
                        if (ef && ef->IsDictionary()) {
                            streamObj = resolve(ef->GetDictionary().FindKey("F"));
                            if (!streamObj)
                                streamObj = resolve(ef->GetDictionary().FindKey("UF"));
                        }
                        if (!streamObj || !streamObj->HasStream()) {
                            problems->append(QStringLiteral("embedded file \"%1\": payload stream missing or unreadable")
                                                 .arg(name));
                            continue;
                        }
                        EmbeddedPayload p;
                        p.name = name;
                        try {
                            PoDoFo::charbuff buf;
                            streamObj->GetStream()->CopyTo(buf);
                            p.bytes = QByteArray(buf.data(), int(buf.size()));
                        } catch (const std::exception& e) {
                            problems->append(QStringLiteral("embedded file \"%1\": payload could not be decoded (%2)")
                                                 .arg(name, QString::fromLatin1(e.what())));
                            continue;
                        }
                        out.append(p);
                    }
                }
            }
            if (dict.HasKey("Kids")) {
                const auto* kids = resolve(dict.FindKey("Kids"));
                if (kids && kids->IsArray())
                    for (const auto& kid : kids->GetArray())
                        walkTree(&kid, depth + 1);
            }
        };
    try {
        const auto* namesRoot = resolve(doc.GetCatalog().GetDictionary().FindKey("Names"));
        if (namesRoot && namesRoot->IsDictionary())
            walkTree(resolve(namesRoot->GetDictionary().FindKey("EmbeddedFiles")), 0);
    } catch (const std::exception& e) {
        problems->append(QStringLiteral("embedded-file tree could not be walked: %1")
                             .arg(QString::fromLatin1(e.what())));
    }
    return out;
}

void inspectRevisions(const QByteArray& raw, QStringList* locations, int* eofCount)
{
    int count = 0;
    int pos = 0;
    while ((pos = raw.indexOf("%%EOF", pos)) >= 0) { ++count; pos += 5; }
    *eofCount = count;
    if (count > 1)
        locations->append(QStringLiteral("%1 revision markers (%%EOF) — incremental-update "
                                         "sections are present").arg(count));
}

struct SweepTargets {
    QStringList strings;                 // de-duplicated removed strings
    QList<QList<QByteArray>> needles;    // per-string byte encodings
};

SweepTargets buildTargets(const QStringList& derived, const QStringList& extra)
{
    SweepTargets t;
    auto add = [&t](const QString& s) {
        if (s.isEmpty()) return;
        for (const QString& existing : t.strings)
            if (existing == s) return;
        t.strings.append(s);
        t.needles.append(survivorEncodings(s));
    };
    for (const QString& s : derived) add(s);
    for (const QString& s : extra) add(s);
    return t;
}

// One sweep pass over one document. Survivor/unswept findings land in
// `reports`; fatal problems additionally land in `fatalProblems`. `role` names
// the document in locations ("redacted output" / "sanitized copy").
void sweepDocument(PoDoFo::PdfMemDocument& doc,
                   const QString& filePath,
                   const QString& role,
                   const SweepTargets& targets,
                   QList<SurfaceReport>* reports,
                   QStringList* fatalProblems)
{
    // 1) RawBytes — literal byte scan of the whole file.
    {
        SurfaceReport r;
        r.surface = Surface::RawBytes;
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray raw = f.readAll();
            f.close();
            r.itemsScanned = 1;
            for (int i = 0; i < targets.strings.size(); ++i) {
                if (containsAny(raw, targets.needles[i])) {
                    r.survivors.append(targets.strings[i]);
                    r.locations.append(QStringLiteral("%1: raw file bytes").arg(role));
                }
            }
            r.verdict = r.survivors.isEmpty() ? SurfaceVerdict::Clean : SurfaceVerdict::Survivor;
        } else {
            r.verdict = SurfaceVerdict::Unswept;
            r.problems.append(QStringLiteral("raw bytes of the %1 could not be read").arg(role));
            *fatalProblems << r.problems;
        }
        reports->append(r);
    }

    // 2) ObjectStrings — every string value of every indirect object
    //    (annotations, outlines, form fields, structure alt text, ...).
    {
        SurfaceReport r;
        r.surface = Surface::ObjectStrings;
        try {
            int stringsScanned = 0;
            auto& objects = doc.GetObjects();
            for (auto* obj : objects) {
                if (!obj) continue;
                QList<QByteArray> values;
                collectStringBytes(obj, &values, 0);
                for (const QByteArray& value : values) {
                    ++stringsScanned;
                    for (int i = 0; i < targets.strings.size(); ++i) {
                        if (containsAny(value, targets.needles[i])) {
                            r.survivors.append(targets.strings[i]);
                            // SEP13 L7: name the object WITHOUT going through
                            // PdfObject::GetReference() — it is VARIANT-typed
                            // in PoDoFo 1.1 and raises InvalidDataType for an
                            // indirect object whose variant carries a string
                            // (annotation strings living in object streams —
                            // the common compressed case), which aborted the
                            // whole object walk mid-scan. TryGetReference is
                            // the non-throwing form; a direct object is named
                            // as such.
                            PoDoFo::PdfReference ref;
                            r.locations.append(
                                obj->TryGetReference(ref)
                                    ? QStringLiteral("%1: string value in object %2 %3 R")
                                          .arg(role)
                                          .arg(ref.ObjectNumber())
                                          .arg(ref.GenerationNumber())
                                    : QStringLiteral("%1: string value in a direct object "
                                                     "(object-stream resident)").arg(role));
                        }
                    }
                }
            }
            r.itemsScanned = stringsScanned;
            r.verdict = r.survivors.isEmpty() ? SurfaceVerdict::Clean : SurfaceVerdict::Survivor;
        } catch (const std::exception& e) {
            r.verdict = SurfaceVerdict::Unswept;
            r.problems.append(QStringLiteral("object table of the %1 could not be walked: %2")
                                  .arg(role, QString::fromLatin1(e.what())));
            *fatalProblems << r.problems;
        }
        reports->append(r);
    }

    // 3) DecodedStreams — every stream, filters applied. Media-image streams
    //    are scanned in their ENCODED bytes only (pixel data is not text);
    //    that quality limitation is stated in the note, never hidden.
    {
        SurfaceReport r;
        r.surface = Surface::DecodedStreams;
        QStringList unsweptStreams;
        try {
            auto& objects = doc.GetObjects();
            for (auto* obj : objects) {
                if (!obj || !obj->HasStream()) continue;
                PoDoFo::PdfObjectStream* stream = obj->GetStream();
                if (!stream) continue;
                ++r.itemsScanned;
                const bool media = isImageStream(obj) || hasMediaFilter(obj);
                QByteArray bytes;
                bool scanned = false;
                bool rawOnly = media;
                if (!media) {
                    try {
                        PoDoFo::charbuff buf;
                        stream->CopyTo(buf);
                        bytes = QByteArray(buf.data(), int(buf.size()));
                        scanned = true;
                    } catch (const std::exception&) {
                        // Fall back to the encoded bytes before declaring the
                        // stream unsweepable: a literal survivor is still
                        // visible in encoded form.
                        try {
                            PoDoFo::charbuff buf;
                            stream->CopyTo(buf, /*raw=*/true);
                            bytes = QByteArray(buf.data(), int(buf.size()));
                            scanned = true;
                            rawOnly = true;
                        } catch (const std::exception&) {
                            scanned = false;
                        }
                    }
                } else {
                    try {
                        PoDoFo::charbuff buf;
                        stream->CopyTo(buf, /*raw=*/true);
                        bytes = QByteArray(buf.data(), int(buf.size()));
                        scanned = true;
                    } catch (const std::exception&) {
                        scanned = false;
                    }
                }
                if (!scanned) {
                    // TryGetReference: GetReference() is variant-typed and
                    // throws for non-reference variants (SEP13 L7 sweep fix).
                    PoDoFo::PdfReference ref;
                    unsweptStreams.append(
                        obj->TryGetReference(ref)
                            ? QStringLiteral("object %1 %2 R")
                                  .arg(ref.ObjectNumber()).arg(ref.GenerationNumber())
                            : QStringLiteral("a direct stream object"));
                    continue;
                }
                if (rawOnly) ++r.itemsRawOnly;
                for (int i = 0; i < targets.strings.size(); ++i) {
                    if (containsAny(bytes, targets.needles[i])) {
                        r.survivors.append(targets.strings[i]);
                        PoDoFo::PdfReference ref;
                        r.locations.append(QStringLiteral("%1: %2 stream%3")
                            .arg(role,
                                 rawOnly ? QStringLiteral("media/raw scan")
                                         : QStringLiteral("decoded"),
                             obj->TryGetReference(ref)
                                 ? QStringLiteral(" in object %1 %2 R")
                                       .arg(ref.ObjectNumber())
                                       .arg(ref.GenerationNumber())
                                 : QString()));
                    }
                }
            }
            if (!unsweptStreams.isEmpty()) {
                r.verdict = SurfaceVerdict::Unswept;
                r.problems.append(QStringLiteral("%1 stream(s) of the %2 could not be read: %3")
                    .arg(unsweptStreams.size()).arg(role, unsweptStreams.join(QStringLiteral(", "))));
                *fatalProblems << r.problems;
            } else {
                r.verdict = r.survivors.isEmpty() ? SurfaceVerdict::Clean : SurfaceVerdict::Survivor;
            }
        } catch (const std::exception& e) {
            r.verdict = SurfaceVerdict::Unswept;
            r.problems.append(QStringLiteral("stream table of the %1 could not be walked: %2")
                                  .arg(role, QString::fromLatin1(e.what())));
            *fatalProblems << r.problems;
        }
        r.note = QStringLiteral(
            "Image/media streams are byte-scanned in their encoded form; pixel "
            "data is not decoded, so text rendered as raster images is out of "
            "scope for string-level proof (see the pack limitations).");
        reports->append(r);
    }

    // 4) ExtractedText — decode-level extraction through the font machinery
    //    on every page (PDFium). Catches survivors whose stream encoding is
    //    non-literal (subset fonts, custom CMaps).
    {
        SurfaceReport r;
        r.surface = Surface::ExtractedText;
        PdfiumBackend backend;
        if (!backend.loadDocument(filePath)) {
            r.verdict = SurfaceVerdict::Unswept;
            r.problems.append(QStringLiteral("the %1 could not be opened for text extraction").arg(role));
            *fatalProblems << r.problems;
        } else {
            const int pages = backend.pageCount();
            r.itemsScanned = pages;
            QStringList pageText;
            pageText.reserve(pages);
            for (int p = 0; p < pages; ++p)
                pageText.append(backend.extractText(p));
            for (int i = 0; i < targets.strings.size(); ++i) {
                const QString& needle = targets.strings[i];
                if (needle.isEmpty()) continue;
                for (int p = 0; p < pages; ++p) {
                    if (pageText[p].contains(needle)) {
                        r.survivors.append(needle);
                        r.locations.append(QStringLiteral("%1: text layer of page %2")
                                               .arg(role).arg(p + 1));
                    }
                }
            }
            r.verdict = r.survivors.isEmpty() ? SurfaceVerdict::Clean : SurfaceVerdict::Survivor;
        }
        reports->append(r);
    }

    // 5) InfoDictionary — document info values.
    {
        SurfaceReport r;
        r.surface = Surface::InfoDictionary;
        try {
            QStringList values;
            auto& meta = doc.GetMetadata();
            // GetTitle() & co. return PoDoFo::nullable<const PdfString&>;
            // its accessors are not const-qualified, so take it by value.
            auto grab = [&values](auto v) {
                if (v.has_value()) {
                    const std::string_view raw = v.value().GetString();
                    values.append(QString::fromLatin1(raw.data(), int(raw.size())));
                }
            };
            grab(meta.GetTitle());
            grab(meta.GetAuthor());
            grab(meta.GetSubject());
            grab(meta.GetCreator());
            grab(meta.GetProducer());
            for (const auto& kw : meta.GetKeywords())
                values.append(QString::fromLatin1(kw.data(), int(kw.size())));
            r.itemsScanned = values.size();
            for (const QString& v : values) {
                for (int i = 0; i < targets.strings.size(); ++i) {
                    if (!targets.strings[i].isEmpty() && v.contains(targets.strings[i])) {
                        r.survivors.append(targets.strings[i]);
                        r.locations.append(QStringLiteral("%1: document info dictionary").arg(role));
                    }
                }
            }
            r.verdict = r.itemsScanned == 0 ? SurfaceVerdict::Absent
                        : (r.survivors.isEmpty() ? SurfaceVerdict::Clean : SurfaceVerdict::Survivor);
        } catch (const std::exception& e) {
            r.verdict = SurfaceVerdict::Unswept;
            r.problems.append(QStringLiteral("info dictionary of the %1 could not be read: %2")
                                  .arg(role, QString::fromLatin1(e.what())));
            *fatalProblems << r.problems;
        }
        reports->append(r);
    }

    // 6) XmpMetadata — the catalog /Metadata stream (also covered by the
    //    generic stream sweep; named explicitly because the pack must say
    //    what was swept).
    {
        SurfaceReport r;
        r.surface = Surface::XmpMetadata;
        try {
            PoDoFo::PdfObject* meta = doc.GetCatalog().GetMetadataObject();
            if (!meta || !meta->HasStream()) {
                r.verdict = SurfaceVerdict::Absent;
            } else {
                PoDoFo::charbuff buf;
                meta->GetStream()->CopyTo(buf);
                const QByteArray xmp(buf.data(), int(buf.size()));
                r.itemsScanned = 1;
                for (int i = 0; i < targets.strings.size(); ++i) {
                    if (containsAny(xmp, targets.needles[i])) {
                        r.survivors.append(targets.strings[i]);
                        r.locations.append(QStringLiteral("%1: XMP metadata stream").arg(role));
                    }
                }
                r.verdict = r.survivors.isEmpty() ? SurfaceVerdict::Clean : SurfaceVerdict::Survivor;
            }
        } catch (const std::exception& e) {
            r.verdict = SurfaceVerdict::Unswept;
            r.problems.append(QStringLiteral("the XMP metadata stream of the %1 could not be read: %2")
                                  .arg(role, QString::fromLatin1(e.what())));
            *fatalProblems << r.problems;
        }
        reports->append(r);
    }

    // 7) EmbeddedFiles — decoded payloads.
    {
        SurfaceReport r;
        r.surface = Surface::EmbeddedFiles;
        QStringList problems;
        const QList<EmbeddedPayload> payloads = embeddedFilePayloads(doc, &problems);
        r.itemsScanned = payloads.size();
        for (const EmbeddedPayload& p : payloads) {
            for (int i = 0; i < targets.strings.size(); ++i) {
                if (containsAny(p.bytes, targets.needles[i])) {
                    r.survivors.append(targets.strings[i]);
                    r.locations.append(QStringLiteral("%1: embedded file \"%2\"")
                                           .arg(role, p.name));
                }
            }
        }
        if (!problems.isEmpty()) {
            r.verdict = SurfaceVerdict::Unswept;
            r.problems = problems;
            *fatalProblems << problems;
        } else {
            r.verdict = r.itemsScanned == 0 ? SurfaceVerdict::Absent
                        : (r.survivors.isEmpty() ? SurfaceVerdict::Clean : SurfaceVerdict::Survivor);
        }
        reports->append(r);
    }

    // 8) RevisionStructure — single-revision check.
    {
        SurfaceReport r;
        r.surface = Surface::RevisionStructure;
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray raw = f.readAll();
            f.close();
            QStringList locations;
            int eofCount = 0;
            inspectRevisions(raw, &locations, &eofCount);
            r.itemsScanned = 1;
            if (eofCount > 1) {
                r.verdict = SurfaceVerdict::Unswept;
                r.locations = locations;
                r.problems.append(QStringLiteral(
                    "the %1 contains %2 revision sections; superseded bytes from an "
                    "incremental update cannot be ruled out by the raw sweep")
                    .arg(role).arg(eofCount));
                *fatalProblems << r.problems;
            } else {
                r.verdict = SurfaceVerdict::Clean;
            }
        } else {
            r.verdict = SurfaceVerdict::Unswept;
            r.problems.append(QStringLiteral("raw bytes of the %1 could not be read").arg(role));
            *fatalProblems << r.problems;
        }
        reports->append(r);
    }
}

// Text runs of `pdfPath` (PDFium decode-level), with per-page run counts.
struct RunInventory {
    bool extractorUsable = false;
    QMap<int, int> runsPerPage;
    QMap<int, QList<PdfiumBackend::TextRun>> runs;
};
RunInventory inventoryRuns(const QString& pdfPath)
{
    RunInventory inv;
    PdfiumBackend backend;
    if (!backend.loadDocument(pdfPath)) return inv;
    inv.extractorUsable = true;
    const int pages = backend.pageCount();
    for (int p = 0; p < pages; ++p) {
        inv.runs[p] = backend.extractPageTextRuns(p);
        inv.runsPerPage[p] = inv.runs[p].size();
    }
    return inv;
}

QString rectText(const QRectF& r)
{
    return QStringLiteral("(%1, %2, %3x%4)")
        .arg(QString::number(r.x(), 'f', 1), QString::number(r.y(), 'f', 1),
             QString::number(r.width(), 'f', 1), QString::number(r.height(), 'f', 1));
}

// ── SEP13 L7: annotation / form-field strings are attribution targets ───────
//
// PDFium page-content extraction never sees text that lives only in an
// annotation (e.g. a FreeText /Contents) or a form-field value (/V) —
// attribution that derives targets from page runs alone certified clean
// outputs while such a secret survived verbatim. Annotation /Rect values and
// field values live in raw user space (a page's /Rotate never applies to
// annotations), so they intersect the mark's user rect directly.
struct AnnotString {
    QRectF rect;    // raw user space, stored y-up (y() = lower edge)
    QString text;
};

QString pdfStringToText(const PoDoFo::PdfString& s)
{
    const std::string_view raw = s.GetString();
    const char* d = raw.data();
    const size_t n = raw.size();
    if (n >= 2 && static_cast<unsigned char>(d[0]) == 0xFE
               && static_cast<unsigned char>(d[1]) == 0xFF) {
        // UTF-16BE with BOM (the PDF text-string form).
        QString out;
        out.reserve(int(n / 2));
        for (size_t i = 2; i + 1 < n; i += 2)
            out.append(QChar(int(static_cast<unsigned char>(d[i]) << 8)
                             | static_cast<unsigned char>(d[i + 1])));
        return out;
    }
    return QString::fromUtf8(d, static_cast<int>(n));
}

void collectAnnotStrings(PoDoFo::PdfAnnotation& annot, QList<AnnotString>* out)
{
    // F1 (independent review 2026-09-14): read the RAW /Rect from the
    // annotation dictionary — PoDoFo's GetRect() pipes the dictionary value
    // through TransformCornersPage, which folds the PAGE's /Rotate into the
    // rect at read time (measured: raw /Rect [100 650 300 680] on a
    // MediaBox [0 200 612 1042] + /Rotate 90 page came back as
    // (450, 512, 30x200)). ISO 32000-1 §12.5.2 puts /Rect in DEFAULT USER
    // space — exactly the space PageSpace::viewerToUser maps the viewer mark
    // into (the L5/L8 shared law), so attribution must intersect the raw
    // /Rect against the transformed mark. GetRect()'s pre-adjusted rect made
    // rotated-page annot attribution miss while the proof certified a clean
    // PASS over a surviving annot secret (data-loss-class false success).
    // GetRectRaw() is PoDoFo's own dictionary accessor (raises when /Rect is
    // absent — same failure shape GetRect() had, caught by the caller's
    // honest UNSWEPT handling below).
    const PoDoFo::Rect r = annot.GetRectRaw().GetNormalized();
    const QRectF rect(qMin(r.GetLeft(), r.GetRight()),
                      qMin(r.GetBottom(), r.GetTop()),
                      qAbs(r.GetRight() - r.GetLeft()),
                      qAbs(r.GetTop() - r.GetBottom()));

    auto add = [&](const QString& text) {
        if (!text.trimmed().isEmpty())
            out->append(AnnotString{ rect, text });
    };

    // FreeText / Text annotation content.
    auto contents = annot.GetContents(); // nullable<const PdfString&>
    if (contents.has_value())
        add(pdfStringToText(contents.value()));

    // Widget annotation /V, plus the field's inherited /V up the /Parent
    // chain (a kid widget often carries only the geometry, the value lives
    // on an ancestor field).
    const PoDoFo::PdfObject* obj = &annot.GetObject();
    for (int depth = 0; obj != nullptr && depth < 16; ++depth) {
        if (const PoDoFo::PdfObject* v = obj->GetDictionary().FindKey("V")) {
            if (v->IsString())
                add(pdfStringToText(v->GetString()));
        }
        obj = obj->GetDictionary().FindKey("Parent");
    }
}

// Overlap between a redaction mark and a PDFium text run (rect anchored at
// the baseline origin in PDF user space, Y up, width = glyph extent). The
// mark arrives ALREADY transformed into user space (PageSpace::viewerToUser —
// the shared transform that honors the MediaBox lower-left origin and /Rotate;
// SEP13 L5: the old Height-only flip certified regions on offset/rotated pages
// that still contained the secret).
//
// W2c residual (SWEEP-W2C 2026-09-20, probe-w2c-loose.txt): the vertical test
// used to add a blanket 3*font-size ascender headroom ("over-attribution can
// only tighten the proof"). That argument holds only for MISSED redactions:
// a neighbor line whose baseline sits within the multiplier's reach was
// attributed to the mark, swept for in the output, found alive, and FAILED a
// geometrically correct redaction — the excision itself (PoDoFoBackend
// isIntersectingSpan) fires on the run's BASELINE segment only, so the
// over-attributed string was provably never removed.
//
// The honest vertical margin is the run's REAL glyph extent — the font's
// ascender..descender band, carried from the PDFium char boxes (the N08
// metric-derived pattern; no guessed multiplier). Band-OVERLAP semantics keep
// the safe direction: the band always contains the baseline, so every run the
// excision can remove (baseline inside the mark band) is also attributed —
// a mark that clips any part of a run's ink (an edge-straddling secret, an
// under-excising sloppy mark) still attributes and still fails loudly, while
// a mark resting between lines no longer grabs a neighbor's text.
bool runIntersects(const PdfiumBackend::TextRun& run, const QRectF& userMark)
{
    const double x0 = run.rect.x();
    const double x1 = run.rect.x() + run.rect.width();
    const double baselineY = run.rect.y();            // PDF space (Y up)
    const double fs = run.fontSize > 0 ? run.fontSize : 12.0;
    double runTop;
    double runBottom;
    if (run.hasInkBox) {
        runTop = run.inkTop;                          // font ascender extent
        runBottom = run.inkBottom;                    // font descender extent
    } else {
        // No char box survived extraction (degenerate run): fall back to the
        // PDF spec's default font-bbox scale (±1em is the declared outer
        // bound of a font's glyph space) — still baseline-covering, never
        // the old blanket 3*fs reach.
        runTop = baselineY + fs;
        runBottom = baselineY - fs;
    }
    const double markLo = userMark.y();               // lower edge (y-up)
    const double markHi = userMark.y() + userMark.height();
    const bool horiz = (x1 >= userMark.x()) && (x0 <= userMark.x() + userMark.width());
    const bool vert = (runBottom <= markHi) && (runTop >= markLo);
    return horiz && vert;
}

} // namespace

// ── primitives (unit-testable) ───────────────────────────────────────────────

QList<QByteArray> survivorEncodings(const QString& text)
{
    QList<QByteArray> out;
    const QByteArray utf8 = text.toUtf8();
    out << utf8;
    out << utf16Bytes(text, /*bigEndian=*/true, /*withBom=*/true);   // PDF text string form
    out << utf16Bytes(text, /*bigEndian=*/false, /*withBom=*/false);
    out << utf8.toHex().toUpper();                                    // hex string forms
    out << utf8.toHex();
    QByteArray escaped;                                               // literal-string escapes
    escaped.reserve(utf8.size() + 8);
    for (char c : utf8) {
        if (c == '(' || c == ')' || c == '\\') escaped += '\\';
        escaped += c;
    }
    out << escaped;
    return out;
}

int countTextOperators(const QByteArray& decodedStream)
{
    return countTextOperatorsImpl(decodedStream);
}

// ── verify ───────────────────────────────────────────────────────────────────

Result verify(const Request& request)
{
    Result out;
    out.application = QStringLiteral("GlyphPDF Redaction Proof Mode (%1)").arg(
        QString::fromLatin1(kProofFormatVersion));
    out.extraStringsSwept = request.extraSurvivorStrings;
    out.generatedAtUtc = QDateTime::currentDateTimeUtc();

    if (request.sourcePath.isEmpty() || request.outputPath.isEmpty()
        || request.redactionsByPage.isEmpty()) {
        out.error = QStringLiteral("proof request is incomplete: source, output and at least "
                                   "one redaction region are required");
        return out;
    }

    out.sourceSha256 = sha256Hex(request.sourcePath);
    out.outputSha256 = sha256Hex(request.outputPath, &out.outputBytes);
    if (!request.sanitizedPath.isEmpty())
        out.sanitizedSha256 = sha256Hex(request.sanitizedPath);
    if (out.sourceSha256.isEmpty()) {
        out.error = QStringLiteral("the original document could not be read: %1")
                        .arg(request.sourcePath);
        return out;
    }
    if (out.outputSha256.isEmpty()) {
        out.error = QStringLiteral("the redacted output could not be read: %1")
                        .arg(request.outputPath);
        return out;
    }

    // ── SOURCE: parse + per-page mechanics + run inventory ──────────────────
    PoDoFo::PdfMemDocument srcDoc;
    try {
        srcDoc.Load(request.sourcePath.toUtf8().constData());
    } catch (const PoDoFo::PdfError& e) {
        out.error = QStringLiteral("the original document could not be parsed: %1")
                        .arg(QString::fromLatin1(e.what()));
        return out;
    }
    out.pagesBefore = int(srcDoc.GetPages().GetCount());

    QMap<int, PageMechanics> srcMechanics;
    QStringList srcStreamProblems;
    for (auto it = request.redactionsByPage.constBegin();
         it != request.redactionsByPage.constEnd(); ++it) {
        if (it.key() < 0 || it.key() >= out.pagesBefore) {
            out.error = QStringLiteral("a redaction mark targets page %1, but the source has "
                                       "%2 page(s)").arg(it.key() + 1).arg(out.pagesBefore);
            return out;
        }
        PageMechanics m = pageMechanics(srcDoc.GetPages().GetPageAt(it.key()));
        if (!m.ok)
            srcStreamProblems.append(QStringLiteral("page %1 content stream could not be decoded")
                                         .arg(it.key() + 1));
        srcMechanics[it.key()] = m;
    }

    const RunInventory srcRuns = inventoryRuns(request.sourcePath);
    if (!srcRuns.extractorUsable) {
        out.error = QStringLiteral("the original document could not be opened for text "
                                   "extraction — string attribution is impossible");
        return out;
    }

    // ── OUTPUT: parse + per-page mechanics ──────────────────────────────────
    PoDoFo::PdfMemDocument outDoc;
    try {
        outDoc.Load(request.outputPath.toUtf8().constData());
    } catch (const PoDoFo::PdfError& e) {
        out.error = QStringLiteral("the redacted output could not be parsed: %1")
                        .arg(QString::fromLatin1(e.what()));
        return out;
    }
    out.pagesAfter = int(outDoc.GetPages().GetCount());

    // Per-mark attributed strings, derived from the source's decoded runs
    // and (SEP13 L7) from the marked pages' annotation/form strings.
    QMap<int, QList<AnnotString>> annotStrings;
    for (auto it = request.redactionsByPage.constBegin();
         it != request.redactionsByPage.constEnd(); ++it) {
        const int page = it.key();
        QList<AnnotString> pageAnnots;
        try {
            auto& annos = srcDoc.GetPages().GetPageAt(page).GetAnnotations();
            const unsigned annotCount = annos.GetCount();
            for (unsigned i = 0; i < annotCount; ++i)
                collectAnnotStrings(annos.GetAnnotAt(i), &pageAnnots);
        } catch (const PoDoFo::PdfError&) {
            // Honest failure: this page's annotation strings cannot become
            // attribution targets, so the derived sweep is blind to them.
            srcStreamProblems.append(QStringLiteral(
                "page %1 annotations could not be read for string attribution")
                .arg(page + 1));
        }
        annotStrings[page] = pageAnnots;
    }

    struct EntryWork { ExcisionEntry entry; };
    QList<EntryWork> works;
    for (auto it = request.redactionsByPage.constBegin();
         it != request.redactionsByPage.constEnd(); ++it) {
        const int page = it.key();
        // SEP13 L5: the mark→region mapping MUST honor the MediaBox lower-left
        // origin AND /Rotate (the shared PageSpace transform). The old
        // Height-only flip certified regions on offset/rotated pages that
        // still contained the secret.
        const PageSpace::PageGeometry pageGeo =
            PageSpace::pageGeometry(srcDoc.GetPages().GetPageAt(page));
        for (const QRectF& mark : it.value()) {
            const QRectF userMark = PageSpace::viewerToUser(mark, pageGeo);
            EntryWork w;
            w.entry.pageIndex = page;
            w.entry.region = mark;
            w.entry.method = Method::Excision;
            for (const auto& run : srcRuns.runs.value(page)) {
                if (runIntersects(run, userMark) && !run.text.trimmed().isEmpty())
                    w.entry.removedStrings.append(run.text.trimmed());
            }
            for (const AnnotString& as : annotStrings.value(page)) {
                const bool hit = as.rect.right() >= userMark.left()
                              && as.rect.left() <= userMark.right()
                              && as.rect.bottom() >= userMark.top()
                              && as.rect.top() <= userMark.bottom();
                if (hit)
                    w.entry.removedStrings.append(as.text.trimmed());
            }
            works.append(w);
        }
    }

    QMap<int, PageMechanics> outMechanics;
    QStringList outStreamProblems;
    for (auto it = request.redactionsByPage.constBegin();
         it != request.redactionsByPage.constEnd(); ++it) {
        if (it.key() >= out.pagesAfter) {
            outStreamProblems.append(QStringLiteral("page %1 of the source has no counterpart "
                                                    "in the output (page count changed)")
                                         .arg(it.key() + 1));
            continue;
        }
        PageMechanics m = pageMechanics(outDoc.GetPages().GetPageAt(it.key()));
        if (!m.ok)
            outStreamProblems.append(QStringLiteral("output page %1 content stream could not be decoded")
                                         .arg(it.key() + 1));
        outMechanics[it.key()] = m;
    }

    const RunInventory outRuns = inventoryRuns(request.outputPath);

    // ── Survival sweep over the output (and the sanitized copy, if any) ─────
    QStringList derived;
    for (const EntryWork& w : works)
        derived.append(w.entry.removedStrings);
    const SweepTargets targets = buildTargets(derived, request.extraSurvivorStrings);

    QList<SurfaceReport> surfaces;
    QStringList fatalProblems;
    sweepDocument(outDoc, request.outputPath, QStringLiteral("redacted output"),
                  targets, &surfaces, &fatalProblems);
    if (!request.sanitizedPath.isEmpty()) {
        PoDoFo::PdfMemDocument sanDoc;
        bool sanLoaded = false;
        try {
            sanDoc.Load(request.sanitizedPath.toUtf8().constData());
            sanLoaded = true;
        } catch (const PoDoFo::PdfError& e) {
            fatalProblems.append(QStringLiteral("the sanitized copy could not be parsed: %1")
                                     .arg(QString::fromLatin1(e.what())));
        }
        if (sanLoaded)
            sweepDocument(sanDoc, request.sanitizedPath, QStringLiteral("sanitized copy"),
                          targets, &surfaces, &fatalProblems);
    }

    // ── Entry adjudication ───────────────────────────────────────────────────
    for (EntryWork& w : works) {
        ExcisionEntry& e = w.entry;
        const PageMechanics& before = srcMechanics.value(e.pageIndex);
        const PageMechanics& after = outMechanics.value(e.pageIndex);
        e.pageStreamSha256Before = before.sha256;
        e.pageStreamSha256After = after.sha256;
        e.pageStreamDigestable = before.ok && after.ok;
        e.textOpsBefore = before.ok ? before.textOps : -1;
        e.textOpsAfter = after.ok ? after.textOps : -1;
        e.textRunsBefore = srcRuns.runsPerPage.value(e.pageIndex, -1);
        e.textRunsAfter = outRuns.runsPerPage.value(e.pageIndex, -1);

        // Survivors attributed to this entry (name surface + location).
        QStringList entryFailures;
        for (const QString& s : e.removedStrings) {
            for (const SurfaceReport& rep : surfaces) {
                if (rep.verdict != SurfaceVerdict::Survivor) continue;
                for (int k = 0; k < rep.survivors.size(); ++k) {
                    if (rep.survivors[k] == s)
                        entryFailures.append(QStringLiteral(
                            "removed string \"%1\" still present — %2")
                            .arg(s, rep.locations.value(k, QStringLiteral("unknown location"))));
                }
            }
        }

        if (!e.pageStreamDigestable) {
            e.status = EntryStatus::Failed;
            e.detail = QStringLiteral("the page content stream could not be digested on both "
                                      "sides — the manifest's mechanical claim cannot be made");
        } else if (!entryFailures.isEmpty()) {
            e.status = EntryStatus::Failed;
            e.detail = entryFailures.join(QStringLiteral("; "));
        } else if (e.removedStrings.isEmpty()) {
            e.status = EntryStatus::VerifiedNoTextInRegion;
            e.detail = QStringLiteral("the mark covered no extractable text (nothing was removed "
                                      "by this mark); verify this matches your intent");
        } else {
            e.status = EntryStatus::Verified;
            e.detail = QStringLiteral("%1 attributed string(s); none survive on any swept surface")
                           .arg(e.removedStrings.size());
        }
        out.entries.append(e);
    }

    // ── Doc-level failure assembly (loud, located) ──────────────────────────
    QStringList failures;
    bool sweepFullyReachable = true;
    for (const SurfaceReport& rep : surfaces) {
        for (int k = 0; k < rep.survivors.size(); ++k) {
            failures.append(QStringLiteral("SURVIVOR [%1] %2 — %3")
                .arg(surfaceName(rep.surface),
                     rep.survivors[k],
                     rep.locations.value(k, QStringLiteral("unknown location"))));
        }
        for (const QString& p : rep.problems) {
            failures.append(QStringLiteral("UNSWEPT [%1] %2").arg(surfaceName(rep.surface), p));
            sweepFullyReachable = false;
        }
    }
    if (out.pagesAfter != out.pagesBefore) {
        failures.append(QStringLiteral("page count changed across the redaction (%1 -> %2)")
                            .arg(out.pagesBefore).arg(out.pagesAfter));
        sweepFullyReachable = false;
    }
    for (const QString& p : srcStreamProblems) {
        failures.append(QStringLiteral("UNSWEPT [PageStreams] source %1").arg(p));
        sweepFullyReachable = false;
    }
    for (const QString& p : outStreamProblems) {
        failures.append(QStringLiteral("UNSWEPT [PageStreams] %1").arg(p));
        sweepFullyReachable = false;
    }
    for (const QString& p : fatalProblems) {
        if (!failures.contains(p)) failures.append(p);
        sweepFullyReachable = false;
    }

    out.surfaces = surfaces;
    out.failureReasons = failures;
    out.proofRan = true;
    out.proofPassed = failures.isEmpty();

    // Honest downgrade: when ANY surface could not be swept, an entry whose
    // strings swept clean was only verified on the surfaces the sweep could
    // reach — it cannot be certified while part of the document is dark.
    if (out.proofPassed == false && !sweepFullyReachable) {
        for (ExcisionEntry& e : out.entries) {
            if (e.status == EntryStatus::Verified) {
                e.status = EntryStatus::Failed;
                e.detail = QStringLiteral(
                    "this entry's strings swept clean where the sweep could reach, "
                    "but at least one surface could not be swept — the entry cannot "
                    "be certified");
            }
        }
    }
    return out;
}

int Result::survivorCount() const
{
    int n = 0;
    for (const SurfaceReport& r : surfaces)
        n += r.survivors.size();
    return n;
}

bool Result::hasUnsweptSurfaces() const
{
    for (const SurfaceReport& r : surfaces)
        if (r.verdict == SurfaceVerdict::Unswept) return true;
    return false;
}

// ── naming ───────────────────────────────────────────────────────────────────

QString methodName(Method method)
{
    switch (method) {
    case Method::Excision: return QStringLiteral("excision");
    }
    return QStringLiteral("unknown");
}

QString entryStatusName(EntryStatus status)
{
    switch (status) {
    case EntryStatus::Verified:               return QStringLiteral("verified");
    case EntryStatus::VerifiedNoTextInRegion: return QStringLiteral("verified-no-text-in-region");
    case EntryStatus::Failed:                 return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

QString surfaceName(Surface surface)
{
    switch (surface) {
    case Surface::RawBytes:          return QStringLiteral("raw-bytes");
    case Surface::ObjectStrings:     return QStringLiteral("object-strings");
    case Surface::DecodedStreams:    return QStringLiteral("decoded-streams");
    case Surface::ExtractedText:     return QStringLiteral("extracted-text");
    case Surface::InfoDictionary:    return QStringLiteral("info-dictionary");
    case Surface::XmpMetadata:       return QStringLiteral("xmp-metadata");
    case Surface::EmbeddedFiles:     return QStringLiteral("embedded-files");
    case Surface::RevisionStructure: return QStringLiteral("revision-structure");
    }
    return QStringLiteral("unknown");
}

// ── proof pack export ────────────────────────────────────────────────────────

namespace {

const char* kDisclaimer =
    "Scope and limits of this report. This pack is generated EVIDENCE describing "
    "automated checks GlyphPDF performed on specific files at a specific time. It is "
    "not a cryptographic audit stamp, not a certification, and not a legal opinion. "
    "The SHA-256 digests below bind the checked bytes at generation time only; they "
    "assert no authority and are trivially recomputable by anyone holding the files. "
    "String-level sweeps cannot see text rendered as raster images (scans): for "
    "scanned regions use a redaction method that rasterizes, and treat those "
    "regions as out of this proof's scope. What a redaction is swept for is "
    "exactly what attribution can name: strings from PDFium's decode-level "
    "extraction of page content plus the annotation and form-field strings "
    "readable on the marked pages. Text whose glyph encoding defeats that "
    "extraction (for example a subset font without a usable /ToUnicode map) is "
    "invisible to attribution and to the extracted-text survivor sweep alike, "
    "and literal byte search cannot decode glyph identifiers — such text is "
    "NOT covered by a PASS. A PASS is the strongest statement this tool makes: "
    "every check it knows how to run found nothing.";

QJsonObject surfaceJson(const SurfaceReport& r)
{
    QJsonObject o;
    o["surface"] = surfaceName(r.surface);
    switch (r.verdict) {
    case SurfaceVerdict::Clean:    o["verdict"] = QStringLiteral("clean"); break;
    case SurfaceVerdict::Survivor: o["verdict"] = QStringLiteral("survivor"); break;
    case SurfaceVerdict::Unswept:  o["verdict"] = QStringLiteral("unswept"); break;
    case SurfaceVerdict::Absent:   o["verdict"] = QStringLiteral("absent"); break;
    }
    o["items_scanned"] = r.itemsScanned;
    o["items_raw_only"] = r.itemsRawOnly;
    QJsonArray surv, locs, probs;
    for (const QString& s : r.survivors) surv.append(s);
    for (const QString& s : r.locations) locs.append(s);
    for (const QString& s : r.problems) probs.append(s);
    o["survivors"] = surv;
    o["locations"] = locs;
    o["problems"] = probs;
    if (!r.note.isEmpty()) o["note"] = r.note;
    return o;
}

QJsonObject entryJson(const ExcisionEntry& e)
{
    QJsonObject o;
    o["page_1based"] = e.pageIndex + 1;
    o["region"] = QJsonArray({ e.region.x(), e.region.y(), e.region.width(), e.region.height() });
    o["method"] = methodName(e.method);
    o["status"] = entryStatusName(e.status);
    o["detail"] = e.detail;
    QJsonArray removed;
    for (const QString& s : e.removedStrings) removed.append(s);
    o["removed_strings"] = removed;
    o["page_stream_sha256_before"] = e.pageStreamSha256Before;
    o["page_stream_sha256_after"] = e.pageStreamSha256After;
    o["page_stream_digestable"] = e.pageStreamDigestable;
    o["text_operators_before"] = e.textOpsBefore;
    o["text_operators_after"] = e.textOpsAfter;
    o["text_runs_before"] = e.textRunsBefore;
    o["text_runs_after"] = e.textRunsAfter;
    return o;
}

} // namespace

QByteArray Result::toJson() const
{
    QJsonObject root;
    root["format"] = QString::fromLatin1(kProofFormatVersion);
    root["generated_at_utc"] = generatedAtUtc.toString(Qt::ISODate);
    root["application"] = application;
    root["disclaimer"] = QString::fromUtf8(kDisclaimer);
    root["verdict"] = proofRan ? (proofPassed ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
                               : QStringLiteral("NOT-RUN");
    root["error"] = error;

    QJsonObject files;
    files["source_sha256"] = sourceSha256;
    files["output_sha256"] = outputSha256;
    files["output_bytes"] = double(outputBytes);
    if (!sanitizedSha256.isEmpty()) files["sanitized_sha256"] = sanitizedSha256;
    root["files"] = files;

    root["pages_before"] = pagesBefore;
    root["pages_after"] = pagesAfter;

    QJsonArray entriesArr;
    for (const ExcisionEntry& e : entries) entriesArr.append(entryJson(e));
    root["excisions"] = entriesArr;

    QJsonArray surfacesArr;
    for (const SurfaceReport& r : surfaces) surfacesArr.append(surfaceJson(r));
    root["surfaces"] = surfacesArr;

    QJsonArray extras;
    for (const QString& s : extraStringsSwept) extras.append(s);
    root["extra_strings_swept"] = extras;

    QJsonArray failuresArr;
    for (const QString& f : failureReasons) failuresArr.append(f);
    root["failures"] = failuresArr;

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QString Result::toTextReport() const
{
    QString t;
    t += QStringLiteral("GLYPHPDF REDACTION PROOF REPORT\n");
    t += QStringLiteral("==================================\n\n");
    t += QStringLiteral("Generated: %1 (UTC)\n").arg(generatedAtUtc.toString(Qt::ISODate));
    t += QStringLiteral("Tool: %1\n\n").arg(application);

    t += QStringLiteral("VERDICT: %1\n\n").arg(
        proofRan ? (proofPassed
                        ? QStringLiteral("PASS — no removed string survives on any swept surface")
                        : QStringLiteral("FAIL — the redaction did NOT fully remove its content"))
                 : QStringLiteral("NOT-RUN — %1").arg(error));
    if (!failureReasons.isEmpty()) {
        t += QStringLiteral("Failures:\n");
        for (const QString& f : failureReasons)
            t += QStringLiteral("  - %1\n").arg(f);
        t += QLatin1Char('\n');
    }

    t += QStringLiteral("Files checked (SHA-256):\n");
    t += QStringLiteral("  original        %1\n").arg(sourceSha256);
    t += QStringLiteral("  redacted output %1  (%2 bytes)\n").arg(outputSha256).arg(outputBytes);
    if (!sanitizedSha256.isEmpty())
        t += QStringLiteral("  sanitized copy  %1\n").arg(sanitizedSha256);
    t += QStringLiteral("  pages: %1 -> %2\n\n").arg(pagesBefore).arg(pagesAfter);

    t += QStringLiteral("Per-excision manifest (%1 excision(s)):\n").arg(entries.size());
    for (const ExcisionEntry& e : entries) {
        t += QStringLiteral("  page %1, region %2, method %3 — %4\n")
                 .arg(e.pageIndex + 1).arg(rectText(e.region))
                 .arg(methodName(e.method), entryStatusName(e.status));
        t += QStringLiteral("    %1\n").arg(e.detail);
        t += QStringLiteral("    page stream SHA-256: %1 -> %2 (%3)\n")
                 .arg(e.pageStreamSha256Before.isEmpty() ? QStringLiteral("unreadable")
                                                         : e.pageStreamSha256Before,
                      e.pageStreamSha256After.isEmpty() ? QStringLiteral("unreadable")
                                                        : e.pageStreamSha256After,
                      e.pageStreamDigestable ? QStringLiteral("both sides readable")
                                             : QStringLiteral("NOT digestable"));
        t += QStringLiteral("    text operators: %1 -> %2; text runs: %3 -> %4\n")
                 .arg(e.textOpsBefore).arg(e.textOpsAfter)
                 .arg(e.textRunsBefore).arg(e.textRunsAfter);
        if (!e.removedStrings.isEmpty())
            t += QStringLiteral("    removed text: %1\n")
                     .arg(e.removedStrings.join(QStringLiteral(" | ")));
    }
    t += QLatin1Char('\n');

    t += QStringLiteral("Survival sweep:\n");
    for (const SurfaceReport& r : surfaces) {
        QString verdictWord;
        switch (r.verdict) {
        case SurfaceVerdict::Clean:    verdictWord = QStringLiteral("clean"); break;
        case SurfaceVerdict::Survivor: verdictWord = QStringLiteral("SURVIVOR FOUND"); break;
        case SurfaceVerdict::Unswept:  verdictWord = QStringLiteral("COULD NOT BE SWEPT"); break;
        case SurfaceVerdict::Absent:   verdictWord = QStringLiteral("absent (nothing to sweep)"); break;
        }
        t += QStringLiteral("  %1: %2").arg(surfaceName(r.surface), verdictWord);
        if (r.itemsScanned > 0)
            t += QStringLiteral(" (%1 item(s) scanned").arg(r.itemsScanned);
        if (r.itemsRawOnly > 0)
            t += QStringLiteral(", %1 raw-only").arg(r.itemsRawOnly);
        if (r.itemsScanned > 0)
            t += QLatin1Char(')');
        t += QLatin1Char('\n');
        for (int k = 0; k < r.survivors.size(); ++k)
            t += QStringLiteral("      \"%1\" — %2\n")
                     .arg(r.survivors[k], r.locations.value(k, QStringLiteral("unknown location")));
        for (const QString& p : r.problems)
            t += QStringLiteral("      problem: %1\n").arg(p);
    }
    if (!extraStringsSwept.isEmpty())
        t += QStringLiteral("\nCaller-supplied strings also swept for: %1\n")
                 .arg(extraStringsSwept.join(QStringLiteral(", ")));

    t += QStringLiteral("\n%1\n").arg(QString::fromUtf8(kDisclaimer));
    return t;
}

bool Result::exportPack(const QString& jsonPath, const QString& textPath, QString* err) const
{
    auto writeOne = [&err](const QString& path, const QByteArray& bytes) -> bool {
        if (path.isEmpty()) return true;
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            if (err) *err = QStringLiteral("could not open %1 for writing").arg(path);
            return false;
        }
        if (f.write(bytes) != bytes.size()) {
            if (err) *err = QStringLiteral("short write to %1").arg(path);
            f.cancelWriting();
            return false;
        }
        if (!f.commit()) {
            if (err) *err = QStringLiteral("could not commit %1").arg(path);
            return false;
        }
        return true;
    };
    if (!writeOne(jsonPath, toJson())) return false;
    if (!writeOne(textPath, toTextReport().toUtf8())) return false;
    return true;
}

} // namespace RedactionProof
} // namespace gp
