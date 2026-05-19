#include "engines/podofo/PoDoFoBackend.h"
#include <memory>
#include <QDebug>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QRecursiveMutex>
#include <QMutexLocker>
#include <podofo/podofo.h>
#include <podofo/auxiliary/StreamDevice.h>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <QMap>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef HAS_QPDF
#include <qpdf/qpdf-c.h>
#endif

class PoDoFoBackend::Private {
public:
    std::unique_ptr<PoDoFo::PdfMemDocument> document;
    std::unique_ptr<PoDoFo::PdfMemDocument> tempDocument;
    QString currentFile;
    mutable QRecursiveMutex mutex;
    QList<PdfImageInfo> lastListedImages;

    PoDoFo::PdfMemDocument& resolveDocument(const QString& path) {
        if (document && currentFile == path) {
            return *document;
        }
        tempDocument = std::make_unique<PoDoFo::PdfMemDocument>();
        tempDocument->Load(path.toUtf8().constData());
        return *tempDocument;
    }

    PdfImageInfo* findImageByName(int pageIndex, const QString& xobjectName, PoDoFoBackend* parent) {
        lastListedImages = parent->listImages(pageIndex);
        for (auto& img : lastListedImages) {
            if (img.xobjectName == xobjectName) {
                return &img;
            }
        }
        return nullptr;
    }
};

PoDoFoBackend::PoDoFoBackend() : d(std::make_unique<Private>()) {}

PoDoFoBackend::~PoDoFoBackend() = default;

bool PoDoFoBackend::loadDocument(const QString &path) {
    QMutexLocker locker(&d->mutex);
    auto newDoc = std::make_unique<PoDoFo::PdfMemDocument>();
    try {
        newDoc->Load(path.toUtf8().constData());
        if (newDoc->GetPages().GetCount() > 10000) {
            qCritical() << "SECURITY: Rejected document exceeding max page limit of 10,000 pages.";
            return false;
        }
        d->document = std::move(newDoc);
        d->currentFile = path;
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "Error loading document:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::saveDocument(const QString &path) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;
    try {
        d->document->Save(path.toUtf8().constData());
#ifdef QT_DEBUG
        qDebug() << "PoDoFo Engine structurally saved document to:" << path;
#endif
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "Error saving document:" << e.what();
#endif
        return false;
    }
}

int PoDoFoBackend::pageCount() const {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return 0;
    return static_cast<int>(d->document->GetPages().GetCount());
}

PdfMetadata PoDoFoBackend::metadata() const {
    QMutexLocker locker(&d->mutex);
    PdfMetadata outMetadata;
    if (!d->document) return outMetadata;
    try {
        auto& metadata = d->document->GetMetadata();
        
        auto t = metadata.GetTitle();
        if (t.has_value()) outMetadata.title = QString::fromStdString(std::string(t.value().GetString()));
        
        auto a = metadata.GetAuthor();
        if (a.has_value()) outMetadata.author = QString::fromStdString(std::string(a.value().GetString()));
        
        auto s = metadata.GetSubject();
        if (s.has_value()) outMetadata.subject = QString::fromStdString(std::string(s.value().GetString()));
        
        auto c = metadata.GetCreator();
        if (c.has_value()) outMetadata.creator = QString::fromStdString(std::string(c.value().GetString()));
        
        auto p = metadata.GetProducer();
        if (p.has_value()) outMetadata.producer = QString::fromStdString(std::string(p.value().GetString()));
        
        QStringList kwList;
        for (const auto& kw : metadata.GetKeywords()) {
            kwList << QString::fromStdString(kw);
        }
        outMetadata.keywords = kwList.join(", ");
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "PoDoFo error during getMetadata:" << e.what();
#endif
    }
    return outMetadata;
}

bool PoDoFoBackend::setMetadata(const PdfMetadata &metadata) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;
    try {
        auto& meta = d->document->GetMetadata();
        meta.SetTitle(PoDoFo::PdfString(metadata.title.toUtf8().constData()));
        meta.SetAuthor(PoDoFo::PdfString(metadata.author.toUtf8().constData()));
        meta.SetSubject(PoDoFo::PdfString(metadata.subject.toUtf8().constData()));
        meta.SetCreator(PoDoFo::PdfString(metadata.creator.toUtf8().constData()));
        meta.SetProducer(PoDoFo::PdfString(metadata.producer.toUtf8().constData()));
        
        std::vector<std::string> kwVec;
        QStringList kwList = metadata.keywords.split(',', Qt::SkipEmptyParts);
        for (auto kw : kwList) {
            kwVec.push_back(kw.trimmed().toStdString());
        }
        meta.SetKeywords(kwVec);
        
        meta.SyncXMPMetadata(true);
#ifdef QT_DEBUG
        qDebug() << "Successfully updated document metadata.";
#endif
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "PoDoFo error during setMetadata:" << e.what();
#endif
        return false;
    } catch (...) {
        return false;
    }
}

bool PoDoFoBackend::writeDocument(const QString &path) {
    return saveDocument(path);
}

bool PoDoFoBackend::writeUpdate(const QString &path) {
    return saveDocument(path);
}

QString PoDoFoBackend::currentFile() const {
    QMutexLocker locker(&d->mutex);
    return d->currentFile;
}

void PoDoFoBackend::setCurrentFile(const QString &path) {
    QMutexLocker locker(&d->mutex);
    d->currentFile = path;
}

QStringList PoDoFoBackend::getEmbeddedFiles() {
    QMutexLocker locker(&d->mutex);
    QStringList files;
    if (!d->document) return files;
    try {
        using namespace PoDoFo;
        auto& catalog = d->document->GetCatalog();
        if (catalog.GetDictionary().HasKey(PdfName("Names"))) {
            auto* namesObj = catalog.GetDictionary().GetKey(PdfName("Names"));
            if (namesObj && namesObj->IsDictionary()) {
                auto& namesDict = namesObj->GetDictionary();
                if (namesDict.HasKey(PdfName("EmbeddedFiles"))) {
                    auto* efObj = namesDict.GetKey(PdfName("EmbeddedFiles"));
                    if (efObj && efObj->IsDictionary()) {
                        auto& efDict = efObj->GetDictionary();
                        if (efDict.HasKey(PdfName("Names"))) {
                            auto* namesArrayObj = efDict.GetKey(PdfName("Names"));
                            if (namesArrayObj && namesArrayObj->IsArray()) {
                                const auto& arr = namesArrayObj->GetArray();
                                for (size_t i = 0; i < arr.size(); i += 2) {
                                    if (i < arr.size()) {
                                        const auto& nameVal = arr[i];
                                        if (nameVal.IsString()) {
                                            files.append(QString::fromStdString(std::string(nameVal.GetString().GetString())));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
        // Fallback gracefully
    }
    return files;
}

QStringList PoDoFoBackend::getLayers() {
    QMutexLocker locker(&d->mutex);
    QStringList layers;
    if (!d->document) return layers;
    try {
        using namespace PoDoFo;
        auto& catalog = d->document->GetCatalog();
        if (catalog.GetDictionary().HasKey(PdfName("OCProperties"))) {
            auto* ocgObj = catalog.GetDictionary().GetKey(PdfName("OCProperties"));
            if (ocgObj && ocgObj->IsDictionary()) {
                auto* ocgsObj = ocgObj->GetDictionary().GetKey(PdfName("OCGs"));
                if (ocgsObj && ocgsObj->IsArray()) {
                    const auto& arr = ocgsObj->GetArray();
                    for (size_t i = 0; i < arr.size(); ++i) {
                        const auto& item = arr[i];
                        const PdfObject* OCG = &item;
                        if (OCG->IsReference()) {
                            OCG = &d->document->GetObjects().MustGetObject(OCG->GetReference());
                        }
                        if (OCG && OCG->IsDictionary()) {
                            auto* nameVal = OCG->GetDictionary().GetKey(PdfName("Name"));
                            if (nameVal && nameVal->IsString()) {
                                layers.append(QString::fromStdString(std::string(nameVal->GetString().GetString())));
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {
        // Fallback gracefully
    }
    return layers;
}

bool PoDoFoBackend::rotatePage(const QString &path, int pageIndex, int degrees) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);
        auto& pages = doc.GetPages();
        if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= pages.GetCount()) return false;

        auto& page = pages.GetPageAt(pageIndex);
        int current = page.GetRotation();
        page.SetRotation((current + degrees) % 360);
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

QByteArray PoDoFoBackend::extractPageAsBytes(const QString &path, int pageIndex) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);
        auto& pages = doc.GetPages();
        if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= pages.GetCount()) return QByteArray();

        PoDoFo::PdfMemDocument tempDoc;
        tempDoc.GetPages().InsertDocumentPageAt(0, doc, pageIndex);

        std::vector<char> buffer;
        PoDoFo::VectorStreamDevice device(buffer);
        tempDoc.Save(device);

        return QByteArray(buffer.data(), static_cast<int>(buffer.size()));
    } catch (...) {
        return QByteArray();
    }
}

bool PoDoFoBackend::insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData) {
    QMutexLocker locker(&d->mutex);
    if (pageData.size() > 10 * 1024 * 1024) {
        qCritical() << "SECURITY: Rejected page data exceeding maximum allowed buffer size (10MB).";
        return false;
    }
    try {
        PoDoFo::PdfMemDocument sourceDoc;
        sourceDoc.LoadFromBuffer(PoDoFo::bufferview(pageData.constData(), pageData.size()));
        if (sourceDoc.GetPages().GetCount() == 0) return false;

        auto& doc = d->resolveDocument(path);

        if (doc.GetPages().GetCount() + sourceDoc.GetPages().GetCount() > 10000) {
            qCritical() << "SECURITY: Operation rejected. Page insertion would exceed the 10,000 pages threshold.";
            return false;
        }

        doc.GetPages().InsertDocumentPageAt(atIndex, sourceDoc, 0);
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool PoDoFoBackend::deletePage(const QString &path, int pageIndex) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);
        auto& pages = doc.GetPages();
        if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= pages.GetCount()) return false;

        pages.RemovePageAt(pageIndex);
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool PoDoFoBackend::insertBlankPage(const QString &path, int atIndex) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);

        doc.GetPages().CreatePageAt(atIndex, PoDoFo::PdfPageSize::A4);
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool PoDoFoBackend::editTextInline(int pageIndex, const QRectF &rect, const QString &newText) {
    QMutexLocker locker(&d->mutex);
    if (!d->document || pageIndex < 0 || (unsigned)pageIndex >= d->document->GetPages().GetCount()) return false;
    
    try {
        PoDoFo::PdfPage& page = d->document->GetPages().GetPageAt(pageIndex);
        
        std::string extractedFontName = "Helvetica";
        double extractedFontSize = 12.0;
        
        try {
            PoDoFo::PdfContentStreamReader reader(page);
            PoDoFo::PdfContent content;
            while (reader.TryReadNext(content)) {
                if (content.GetType() == PoDoFo::PdfContentType::Operator && content.GetKeyword() == "Tf") {
                    const auto& stack = content.GetStack();
                    if (stack.size() >= 2) {
                        if (stack[0].IsName()) {
                            extractedFontName = std::string(stack[0].GetName().GetString());
                        }
                        if (stack[1].IsNumberOrReal()) {
                            extractedFontSize = stack[1].GetReal();
                        }
                    }
                }
            }
            
            if (!extractedFontName.empty() && extractedFontName != "Helvetica") {
                const PoDoFo::PdfObject* fontObj = page.GetResources().GetResource(PoDoFo::PdfResourceType::Font, extractedFontName);
                if (fontObj && fontObj->IsDictionary()) {
                    const auto& fontDict = fontObj->GetDictionary();
                    if (fontDict.HasKey("BaseFont")) {
                        const auto* baseFont = fontDict.GetKey("BaseFont");
                        if (baseFont && baseFont->IsName()) {
                            extractedFontName = std::string(baseFont->GetName().GetString());
                            size_t plusPos = extractedFontName.find('+');
                            if (plusPos != std::string::npos) extractedFontName = extractedFontName.substr(plusPos + 1);
                            if (!extractedFontName.empty() && extractedFontName[0] == '/') extractedFontName = extractedFontName.substr(1);
                            size_t commaPos = extractedFontName.find(',');
                            if (commaPos != std::string::npos) extractedFontName = extractedFontName.substr(0, commaPos);
                            size_t dashPos = extractedFontName.find('-');
                            if (dashPos != std::string::npos) extractedFontName = extractedFontName.substr(0, dashPos);
                        }
                    }
                }
            }

        } catch (...) {
            extractedFontName = "Helvetica";
            extractedFontSize = 12.0;
        }
        if (extractedFontName.empty()) extractedFontName = "Helvetica";
        if (extractedFontSize <= 0.0) extractedFontSize = 12.0;

        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        
        painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(1.0, 1.0, 1.0));
        PoDoFo::Rect pageRect = page.GetMediaBox();
        double pdfY = pageRect.Height - (rect.y() + rect.height());
        painter.DrawRectangle(rect.x(), pdfY, rect.width(), rect.height(), PoDoFo::PdfPathDrawMode::Fill);
        
        painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(0.0, 0.0, 0.0));
        auto font = d->document->GetFonts().SearchFont(extractedFontName);
        if (!font) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(*font, extractedFontSize);
        
        QStringList lines = newText.split('\n');
        if (lines.size() == 1) {
            painter.DrawText(lines[0].toUtf8().constData(), rect.x(), pdfY + 2.0);
        } else {
            double currentY = pdfY + rect.height() - extractedFontSize;
            for (const QString& line : lines) {
                painter.DrawText(line.toUtf8().constData(), rect.x(), currentY);
                currentY -= (extractedFontSize * 1.2);
            }
        }
        painter.FinishDrawing();
        
#ifdef QT_DEBUG
        qDebug() << "Editing text inline via PoDoFo on page" << pageIndex << "font:" << extractedFontName.c_str() << "size:" << extractedFontSize;
#endif
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "PoDoFo error editing text:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::deleteObjectAt(int pageIndex, const QPointF &pos) {
    QMutexLocker locker(&d->mutex);
    QRectF redactionRect(pos.x() - 5, pos.y() - 5, 10, 10);
    if (!applyRedactions(pageIndex, {redactionRect})) return false;
    if (d->currentFile.isEmpty()) return false;
    try {
        d->document->Save(d->currentFile.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "deleteObjectAt save error:" << e.what();
#endif
        return false;
    }
}

namespace {

void redactCanvasRecursively(PoDoFo::PdfObject& canvasObj, 
                             const std::vector<PoDoFo::Rect>& pdfRects, 
                             PoDoFo::PdfPage& page, 
                             PoDoFo::PdfMemDocument* document) 
{
    using namespace PoDoFo;
    
    std::string streamStr;
    if (canvasObj.HasStream()) {
        auto* streamObj = canvasObj.GetStream();
        if (streamObj) {
            PoDoFo::charbuff buf;
            streamObj->CopyTo(buf);
            streamStr.assign(buf.data(), buf.size());
        }
    } else {
        auto* contentsObj = page.GetContents();
        if (contentsObj) {
            PoDoFo::charbuff buf;
            contentsObj->CopyTo(buf);
            streamStr.assign(buf.data(), buf.size());
        }
    }

    auto device = std::make_shared<PoDoFo::SpanStreamDevice>(streamStr);
    PdfContentStreamReader reader(device);
    PdfContent content;
    std::ostringstream newStream;
    
    double textX = 0.0, textY = 0.0;
    bool inTextBlock = false;
    double leading = 0.0;

    auto isIntersecting = [&](double x, double y) -> bool {
        for (const auto& r : pdfRects) {
            if (x >= r.X && x <= (r.X + r.Width) && y >= r.Y && y <= (r.Y + r.Height))
                return true;
        }
        return false;
    };

    while (reader.TryReadNext(content)) {
        if (content.GetType() == PdfContentType::Operator) {
            std::string_view kw = content.GetKeyword();
            const auto& stack = content.GetStack();
            
            if (kw == "BT") {
                inTextBlock = true;
                textX = 0.0; textY = 0.0;
                newStream << "BT\n";
                continue;
            }
            if (kw == "ET") {
                inTextBlock = false;
                newStream << "ET\n";
                continue;
            }
            if (kw == "TL" && stack.size() > 0) {
                if (stack[0].IsNumberOrReal()) leading = stack[0].GetReal();
            }
            if (kw == "T*" && inTextBlock) {
                textY -= leading;
            }
            if (kw == "Tm" && stack.size() >= 6) {
                if (stack[4].IsNumberOrReal()) textX = stack[4].GetReal();
                if (stack[5].IsNumberOrReal()) textY = stack[5].GetReal();
            } else if ((kw == "Td" || kw == "TD") && stack.size() >= 2) {
                if (stack[0].IsNumberOrReal()) textX += stack[0].GetReal();
                if (stack[1].IsNumberOrReal()) {
                    double dy = stack[1].GetReal();
                    textY += dy;
                    if (kw == "TD") leading = -dy;
                }
            }
            
            if (inTextBlock && isIntersecting(textX, textY)) {
                if (kw == "Tj" || kw == "TJ" || kw == "'" || kw == "\"") {
                    continue;
                }
            }
            
            if (kw == "Do" && stack.size() > 0 && stack[0].IsName()) {
                std::string xobjName(stack[0].GetName().GetString());
                if (isIntersecting(textX, textY)) {
                    continue;
                }
                
                auto resources = page.GetResources();
                auto* xobjectDict = resources.GetDictionary().FindKey("XObject");
                if (xobjectDict && xobjectDict->IsDictionary()) {
                    auto* xobjRef = xobjectDict->GetDictionary().FindKey(xobjName);
                    if (xobjRef) {
                        PdfObject* xobj = xobjRef;
                        if (xobj->IsReference())
                            xobj = &document->GetObjects().MustGetObject(xobj->GetReference());
                        
                        if (xobj->IsDictionary()) {
                            auto& dict = xobj->GetDictionary();
                            auto* subtype = dict.FindKey("Subtype");
                            if (subtype && subtype->GetName().GetString() == "Form") {
                                redactCanvasRecursively(*xobj, pdfRects, page, document);
                            }
                        }
                    }
                }
            }
            
            for (const auto& op : stack) {
                std::string out;
                op.ToString(out);
                newStream << out << " ";
            }
            newStream << kw << "\n";
        }
    }
    
    if (canvasObj.HasStream()) {
        auto& stream = canvasObj.GetOrCreateStream();
        stream.SetData(newStream.str());
    } else {
        auto* contentsObj = page.GetContents();
        if (contentsObj) {
            contentsObj->Reset();
            if (contentsObj->GetObject().IsArray()) {
                auto& stream = contentsObj->CreateStreamForAppending();
                stream.SetData(newStream.str());
            } else {
                auto& stream = contentsObj->GetObject().GetOrCreateStream();
                stream.SetData(newStream.str());
            }
        }
    }
}

} // anonymous namespace

bool PoDoFoBackend::applyRedactions(int pageIndex, const QList<QRectF> &rects) {
    QMutexLocker locker(&d->mutex);
    if (!d->document || pageIndex < 0 || (unsigned)pageIndex >= d->document->GetPages().GetCount()) return false;

    try {
        PoDoFo::PdfPage& page = d->document->GetPages().GetPageAt(pageIndex);
        double pageHeight = page.GetMediaBox().Height;

        std::vector<PoDoFo::Rect> pdfRects;
        for (const auto& r : rects) {
            pdfRects.push_back(PoDoFo::Rect(r.x(), pageHeight - r.y() - r.height(), r.width(), r.height()));
        }

        auto* contentsObj = page.GetContents();
        bool streamFilterApplied = false;
        if (contentsObj) {
            PoDoFo::charbuff streamBuf;
            contentsObj->CopyTo(streamBuf);
            std::string streamStr(streamBuf.data(), streamBuf.size());

            bool hasInlineImage = (streamStr.find("\nID ") != std::string::npos
                                || streamStr.find("\nID\n") != std::string::npos
                                || streamStr.find(" ID ") != std::string::npos);
            bool hasBinaryContent = false;
            for (size_t i = 0; i < std::min(streamStr.size(), size_t(512)); ++i) {
                unsigned char c = static_cast<unsigned char>(streamStr[i]);
                if (c == 0 || (c < 9 && c != 0)) { hasBinaryContent = true; break; }
            }

            if (hasInlineImage || hasBinaryContent) {
#ifdef QT_DEBUG
                qWarning() << "Redaction: stream contains inline images or binary data on page"
                           << pageIndex << "— skipping content surgery, applying visual overlay only.";
#endif
            } else {
                redactCanvasRecursively(page.GetObject(), pdfRects, page, d->document.get());
                streamFilterApplied = true;
            }
        }

        if (!streamFilterApplied && contentsObj) {
            qCritical() << "SECURITY: Redaction on page" << pageIndex
                        << "failed to apply content stream surgery due to unparseable or binary content."
                           " Aborting operation to prevent insecure visual-only overlay.";
            return false;
        }

        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(0.0, 0.0, 0.0));
        for (const auto& r : pdfRects) {
            painter.DrawRectangle(r.X, r.Y, r.Width, r.Height, PoDoFo::PdfPathDrawMode::Fill);
        }
        painter.FinishDrawing();

        auto& annos = page.GetAnnotations();
        std::vector<unsigned> toRemove;
        for (unsigned i = 0; i < annos.GetCount(); ++i) {
            auto& anno = annos.GetAnnotAt(i);
            auto r = anno.GetRect();
            for (const auto& redRect : pdfRects) {
                if (r.X < (redRect.X + redRect.Width) && (r.X + r.Width) > redRect.X &&
                    r.Y < (redRect.Y + redRect.Height) && (r.Y + r.Height) > redRect.Y) {
                    toRemove.push_back(i);
                    break;
                }
            }
        }
        for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
            annos.RemoveAnnotAt(*it);
        }

        return true;
    } catch (const PoDoFo::PdfError& e) {
        qCritical() << "SECURITY: Redaction failed on page" << pageIndex << "-" << e.what()
                    << "— document state may be inconsistent, do NOT save.";
        return false;
    } catch (const std::exception& e) {
        qCritical() << "SECURITY: General exception during redaction on page" << pageIndex << "-" << e.what();
        return false;
    } catch (...) {
        qCritical() << "SECURITY: Unknown exception during redaction on page" << pageIndex;
        return false;
    }
}

bool PoDoFoBackend::exportPdfA(const QString &outputPath, int conformanceLevel) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;

    try {
        using namespace PoDoFo;

        PdfALevel pdfaLevel;
        PdfVersion pdfVersion;
        switch (conformanceLevel) {
            case 2:
                pdfaLevel = PdfALevel::L2B;
                pdfVersion = PdfVersion::V1_7;
                break;
            case 3:
                pdfaLevel = PdfALevel::L3B;
                pdfVersion = PdfVersion::V2_0;
                break;
            default:
                pdfaLevel = PdfALevel::L1B;
                pdfVersion = PdfVersion::V1_4;
                break;
        }

        auto& metadata = d->document->GetMetadata();
        metadata.SetPdfVersion(pdfVersion);
        metadata.SetPdfALevel(pdfaLevel);
        metadata.SetProducer(PdfString("PDF Workstation Pro (PoDoFo)"));
        metadata.SetCreator(PdfString("PDF Workstation Pro"));
        metadata.SyncXMPMetadata(true);

        auto& catalog = d->document->GetCatalog();
        auto& objects = d->document->GetObjects();
        auto& intentObj = objects.CreateDictionaryObject();
        intentObj.GetDictionary().AddKey(PdfName("Type"), PdfName("OutputIntent"));
        intentObj.GetDictionary().AddKey(PdfName("S"), PdfName("GTS_PDFA1"));
        intentObj.GetDictionary().AddKey(PdfName("OutputConditionIdentifier"), PdfString("sRGB IEC61966-2.1"));
        intentObj.GetDictionary().AddKey(PdfName("RegistryName"), PdfString("http://www.color.org"));
        intentObj.GetDictionary().AddKey(PdfName("Info"), PdfString("sRGB IEC61966-2.1"));

        auto& intentArrayObj = catalog.GetDictionary().AddKey(PdfName("OutputIntents"), PdfArray());
        intentArrayObj.GetArray().AddIndirect(intentObj);

        d->document->Save(outputPath.toUtf8().constData());
#ifdef QT_DEBUG
        qDebug() << "Successfully exported PDF/A-" << conformanceLevel << "b to:" << outputPath;
#endif
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "PoDoFo error during PDF/A export:" << e.what();
#endif
        return false;
    } catch (const std::exception& e) {
#ifdef QT_DEBUG
        qWarning() << "Error during PDF/A export:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::encryptDocument(const QString &userPassword, const QString &ownerPassword, bool canPrint, bool canCopy, bool canModify) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;
    try {
        auto perms = static_cast<PoDoFo::PdfPermissions>(0);
        if (canPrint) perms = perms | PoDoFo::PdfPermissions::Print;
        if (canCopy) perms = perms | PoDoFo::PdfPermissions::Copy;
        if (canModify) perms = perms | PoDoFo::PdfPermissions::Edit;

        d->document->SetEncrypted(
            userPassword.toUtf8().constData(), 
            ownerPassword.toUtf8().constData(),
            perms,
            PoDoFo::PdfEncryptionAlgorithm::AESV3R6
        );
#ifdef QT_DEBUG
        qDebug() << "Applied AES-256 encryption to document.";
#endif
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "Error encrypting document:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::sanitizeDocument(const QString &outputPath) {
    QMutexLocker locker(&d->mutex);
    if (!d->document || outputPath.isEmpty()) return false;

    const QFileInfo outputInfo(outputPath);
    if (!d->currentFile.isEmpty()) {
        const QFileInfo sourceInfo(d->currentFile);
        const QString sourcePath = sourceInfo.canonicalFilePath().isEmpty()
            ? sourceInfo.absoluteFilePath()
            : sourceInfo.canonicalFilePath();
        const QString targetPath = outputInfo.canonicalFilePath().isEmpty()
            ? outputInfo.absoluteFilePath()
            : outputInfo.canonicalFilePath();
        if (QString::compare(sourcePath, targetPath, Qt::CaseInsensitive) == 0) {
#ifdef QT_DEBUG
            qWarning() << "Refusing to sanitize in place:" << outputPath;
#endif
            return false;
        }
    }

    try {
        using namespace PoDoFo;
        
        auto& trailer = d->document->GetTrailer();
        if (trailer.GetDictionary().HasKey("Info")) {
            trailer.GetDictionary().RemoveKey("Info");
        }
        
        auto& catalog = d->document->GetCatalog();
        if (catalog.GetDictionary().HasKey(PdfName("Metadata"))) {
            catalog.GetDictionary().RemoveKey(PdfName("Metadata"));
        }
        if (catalog.GetDictionary().HasKey(PdfName("PieceInfo"))) {
            catalog.GetDictionary().RemoveKey(PdfName("PieceInfo"));
        }
        if (catalog.GetDictionary().HasKey(PdfName("MarkInfo"))) {
            catalog.GetDictionary().RemoveKey(PdfName("MarkInfo"));
        }
        if (catalog.GetDictionary().HasKey(PdfName("OutputIntents"))) {
            catalog.GetDictionary().RemoveKey(PdfName("OutputIntents"));
        }
        
        if (catalog.GetDictionary().HasKey(PdfName("Names"))) {
            auto* namesObj = catalog.GetDictionary().GetKey(PdfName("Names"));
            if (namesObj && namesObj->IsDictionary()) {
                auto& namesDict = namesObj->GetDictionary();
                if (namesDict.HasKey(PdfName("EmbeddedFiles"))) {
                    namesDict.RemoveKey(PdfName("EmbeddedFiles"));
                }
                if (namesDict.HasKey(PdfName("JavaScript"))) {
                    namesDict.RemoveKey(PdfName("JavaScript"));
                }
            }
        }

        if (catalog.GetDictionary().HasKey(PdfName("OpenAction"))) {
            catalog.GetDictionary().RemoveKey(PdfName("OpenAction"));
        }
        if (catalog.GetDictionary().HasKey(PdfName("AA"))) {
            catalog.GetDictionary().RemoveKey(PdfName("AA"));
        }

        auto& pages = d->document->GetPages();
        for (unsigned pi = 0; pi < pages.GetCount(); ++pi) {
            auto& pg = pages.GetPageAt(pi);
            if (pg.GetDictionary().HasKey(PdfName("AA"))) {
                pg.GetDictionary().RemoveKey(PdfName("AA"));
            }
            if (pg.GetDictionary().HasKey(PdfName("A"))) {
                pg.GetDictionary().RemoveKey(PdfName("A"));
            }
            if (pg.GetDictionary().HasKey(PdfName("PieceInfo"))) {
                pg.GetDictionary().RemoveKey(PdfName("PieceInfo"));
            }
            if (pg.GetDictionary().HasKey(PdfName("Thumb"))) {
                pg.GetDictionary().RemoveKey(PdfName("Thumb"));
            }
            if (pg.GetDictionary().HasKey(PdfName("Metadata"))) {
                pg.GetDictionary().RemoveKey(PdfName("Metadata"));
            }
        }

        auto* acroForm = d->document->GetAcroForm();
        if (acroForm && acroForm->GetDictionary().HasKey(PdfName("XFA"))) {
            acroForm->GetDictionary().RemoveKey(PdfName("XFA"));
        }
        
        const QString outputDir = outputInfo.absoluteDir().absolutePath();
        QString tempPath;
        {
            QTemporaryFile tempFile(outputDir + "/.sanitize_XXXXXX.pdf");
            tempFile.setAutoRemove(false);
            if (!tempFile.open()) {
#ifdef QT_DEBUG
                qWarning() << "Unable to create temporary sanitized output in:" << outputDir;
#endif
                return false;
            }
            tempPath = tempFile.fileName();
        }

        d->document->Save(tempPath.toUtf8().constData());

#ifdef HAS_QPDF
        try {
            qpdf_data pdf = qpdf_init();
            if (qpdf_read(pdf, tempPath.toUtf8().constData(), nullptr) == 0 &&
                qpdf_init_write(pdf, outputPath.toUtf8().constData()) == 0) {
                qpdf_set_static_ID(pdf, 1);
                qpdf_set_stream_data_mode(pdf, qpdf_s_compress);
                if (qpdf_write(pdf) == 0) {
                    QFile::remove(tempPath);
                    qpdf_cleanup(&pdf);
                } else {
                    qpdf_cleanup(&pdf);
                    throw std::runtime_error("qpdf_write failed");
                }
            } else {
                qpdf_cleanup(&pdf);
                throw std::runtime_error("qpdf_read or qpdf_init_write failed");
            }
        } catch (const std::exception& e) {
#ifdef QT_DEBUG
            qWarning() << "QPDF sanitization rebuild failed, falling back to basic save:" << e.what();
#endif
            if (QFileInfo::exists(outputPath) && !QFile::remove(outputPath)) {
                QFile::remove(tempPath);
                return false;
            }
            if (!QFile::rename(tempPath, outputPath)) {
                QFile::remove(tempPath);
                return false;
            }
        }
#else
        if (QFileInfo::exists(outputPath) && !QFile::remove(outputPath)) {
            QFile::remove(tempPath);
            return false;
        }
        if (!QFile::rename(tempPath, outputPath)) {
            QFile::remove(tempPath);
            return false;
        }
#endif

#ifdef QT_DEBUG
        qDebug() << "Successfully sanitized document to:" << outputPath;
#endif
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "PoDoFo error during sanitize:" << e.what();
#endif
        return false;
    } catch (const std::exception& e) {
#ifdef QT_DEBUG
        qWarning() << "Error during sanitize:" << e.what();
#endif
        return false;
    }
}

namespace {

bool rewriteImageMatrix(PoDoFo::PdfPage& page, const std::string& xobjName,
                        double a, double b, double c, double d, double e, double f)
{
    auto* contentsObj = page.GetContents();
    if (!contentsObj) return false;
    
    PoDoFo::charbuff streamBuf;
    contentsObj->CopyTo(streamBuf);
    std::string content(streamBuf.data(), streamBuf.size());

    std::string doTarget = "/" + xobjName + " Do";
    
    size_t doPos = content.find(doTarget);
    if (doPos == std::string::npos) return false;
    
    size_t searchFrom = (doPos > 200) ? doPos - 200 : 0;
    std::string region = content.substr(searchFrom, doPos - searchFrom);
    
    size_t cmPos = region.rfind(" cm");
    if (cmPos == std::string::npos) {
        cmPos = region.rfind("\ncm");
    }
    if (cmPos == std::string::npos) return false;
    
    size_t cmAbsPos = searchFrom + cmPos;
    
    size_t lineStart = content.rfind('\n', cmAbsPos);
    if (lineStart == std::string::npos) lineStart = 0;
    else lineStart++;
    
    size_t cmEndPos = searchFrom + cmPos + 3;
    
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%.6f %.6f %.6f %.6f %.6f %.6f cm",
                  a, b, c, d, e, f);
    std::string replacement(buf);
    
    content.replace(lineStart, cmEndPos - lineStart, replacement);
    
    contentsObj->Reset();
    auto& stream = contentsObj->GetObject().GetOrCreateStream();
    stream.SetData(content);
    return true;
}

} // anonymous namespace

QList<PdfImageInfo> PoDoFoBackend::listImages(int pageIndex) {
    QMutexLocker locker(&d->mutex);
    QList<PdfImageInfo> result;
    if (!d->document) return result;

    try {
        auto& pages = d->document->GetPages();
        if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= pages.GetCount())
            return result;

        auto& page = pages.GetPageAt(pageIndex);
        auto resources = page.GetResources();

        auto* xobjectDict = resources.GetDictionary().FindKey("XObject");
        if (!xobjectDict || !xobjectDict->IsDictionary()) return result;

        struct XObjEntry { QString name; int w; int h; };
        QMap<QString, XObjEntry> imageXObjects;

        for (auto& kv : xobjectDict->GetDictionary()) {
            auto* obj = &kv.second;
            if (obj->IsReference())
                obj = &d->document->GetObjects().MustGetObject(obj->GetReference());
            if (!obj->IsDictionary()) continue;
            auto& dict = obj->GetDictionary();
            auto* subtype = dict.FindKey("Subtype");
            if (!subtype || subtype->GetName().GetString() != "Image") continue;

            XObjEntry entry;
            entry.name = QString::fromStdString(std::string(kv.first.GetString()));
            auto* widthObj = dict.FindKey("Width");
            auto* heightObj = dict.FindKey("Height");
            entry.w = widthObj && widthObj->IsNumber() ? static_cast<int>(widthObj->GetNumber()) : 0;
            entry.h = heightObj && heightObj->IsNumber() ? static_cast<int>(heightObj->GetNumber()) : 0;
            imageXObjects[entry.name] = entry;
        }

        if (imageXObjects.isEmpty()) return result;

        PoDoFo::PdfContentStreamReader reader(page);
        PoDoFo::PdfContent content;

        struct Matrix { double a,b,c,d,e,f; };
        Matrix ctm = {1,0,0,1,0,0};
        QList<Matrix> matrixStack;

        auto multiply = [](const Matrix& m1, const Matrix& m2) -> Matrix {
            return {
                m1.a*m2.a + m1.b*m2.c,
                m1.a*m2.b + m1.b*m2.d,
                m1.c*m2.a + m1.d*m2.c,
                m1.c*m2.b + m1.d*m2.d,
                m1.e*m2.a + m1.f*m2.c + m2.e,
                m1.e*m2.b + m1.f*m2.d + m2.f
            };
        };

        while (reader.TryReadNext(content)) {
            if (content.GetType() == PoDoFo::PdfContentType::Operator) {
                auto kw = content.GetKeyword();
                const auto& stack = content.GetStack();

                if (kw == "q") {
                    matrixStack.append(ctm);
                } else if (kw == "Q" && !matrixStack.isEmpty()) {
                    ctm = matrixStack.takeLast();
                } else if (kw == "cm" && stack.size() >= 6) {
                    Matrix cm;
                    cm.a = stack[0].IsNumberOrReal() ? stack[0].GetReal() : 0;
                    cm.b = stack[1].IsNumberOrReal() ? stack[1].GetReal() : 0;
                    cm.c = stack[2].IsNumberOrReal() ? stack[2].GetReal() : 0;
                    cm.d = stack[3].IsNumberOrReal() ? stack[3].GetReal() : 0;
                    cm.e = stack[4].IsNumberOrReal() ? stack[4].GetReal() : 0;
                    cm.f = stack[5].IsNumberOrReal() ? stack[5].GetReal() : 0;
                    ctm = multiply(cm, ctm);
                } else if (kw == "Do" && stack.size() >= 1) {
                    QString name = QString::fromStdString(std::string(stack[0].GetName().GetString()));
                    if (imageXObjects.contains(name)) {
                        auto& entry = imageXObjects[name];
                        PdfImageInfo info;
                        info.pageIndex = pageIndex;
                        info.xobjectName = name;
                        double w = std::sqrt(ctm.a * ctm.a + ctm.b * ctm.b);
                        double h = std::sqrt(ctm.c * ctm.c + ctm.d * ctm.d);
                        double rot = std::atan2(ctm.b, ctm.a) * 180.0 / M_PI;
                        info.placement = QRectF(ctm.e, ctm.f, w, h);
                        info.rotation = rot;
                        info.widthPx = entry.w;
                        info.heightPx = entry.h;
                        result.append(info);
                    }
                }
            }
        }
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "listImages error:" << e.what();
#endif
    }
    return result;
}

bool PoDoFoBackend::moveImage(int pageIndex, const QString &xobjectName, double dx, double dy) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;
    try {
        auto& page = d->document->GetPages().GetPageAt(pageIndex);
        
        PdfImageInfo* target = d->findImageByName(pageIndex, xobjectName, this);
        if (!target) return false;
        
        double radians = target->rotation * M_PI / 180.0;
        double w = target->placement.width();
        double h = target->placement.height();
        double newE = target->placement.x() + dx;
        double newF = target->placement.y() + dy;
        double cosR = std::cos(radians), sinR = std::sin(radians);
        
        bool ok = rewriteImageMatrix(page,
            xobjectName.toStdString(),
            w * cosR, w * sinR,
            -h * sinR, h * cosR,
            newE, newF);
        
        if (ok) d->document->Save(d->currentFile.toUtf8().constData());
        return ok;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "moveImage error:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::resizeImage(int pageIndex, const QString &xobjectName, double newWidth, double newHeight) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;
    try {
        auto& page = d->document->GetPages().GetPageAt(pageIndex);
        
        PdfImageInfo* target = d->findImageByName(pageIndex, xobjectName, this);
        if (!target) return false;
        
        double radians = target->rotation * M_PI / 180.0;
        double cosR = std::cos(radians), sinR = std::sin(radians);
        
        bool ok = rewriteImageMatrix(page,
            xobjectName.toStdString(),
            newWidth * cosR, newWidth * sinR,
            -newHeight * sinR, newHeight * cosR,
            target->placement.x(), target->placement.y());
        
        if (ok) d->document->Save(d->currentFile.toUtf8().constData());
        return ok;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "resizeImage error:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::rotateImage(int pageIndex, const QString &xobjectName, double degrees) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;
    try {
        auto& page = d->document->GetPages().GetPageAt(pageIndex);
        
        PdfImageInfo* target = d->findImageByName(pageIndex, xobjectName, this);
        if (!target) return false;
        
        double newRot = (target->rotation + degrees) * M_PI / 180.0;
        double w = target->placement.width();
        double h = target->placement.height();
        double cosR = std::cos(newRot), sinR = std::sin(newRot);
        
        double cx = target->placement.x() + w * std::cos(target->rotation * M_PI / 180.0) / 2.0;
        double cy = target->placement.y() + w * std::sin(target->rotation * M_PI / 180.0) / 2.0;
        double newE = cx - (w * cosR + h * (-sinR)) / 2.0;
        double newF = cy - (w * sinR + h * cosR) / 2.0;
        
        bool ok = rewriteImageMatrix(page,
            xobjectName.toStdString(),
            w * cosR, w * sinR,
            -h * sinR, h * cosR,
            newE, newF);
        
        if (ok) d->document->Save(d->currentFile.toUtf8().constData());
        return ok;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "rotateImage error:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::replaceImage(int pageIndex, const QString &xobjectName, const QString &newImagePath) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;
    try {
        auto& page = d->document->GetPages().GetPageAt(pageIndex);
        auto resources = page.GetResources();
        
        auto* xobjectDict = resources.GetDictionary().FindKey("XObject");
        if (!xobjectDict || !xobjectDict->IsDictionary()) return false;
        
        std::string name = xobjectName.toStdString();
        auto* xobjRef = xobjectDict->GetDictionary().FindKey(name);
        if (!xobjRef) return false;
        
        PoDoFo::PdfObject* xobj = xobjRef;
        if (xobj->IsReference())
            xobj = &d->document->GetObjects().MustGetObject(xobj->GetReference());
        
        QImage newImg(newImagePath);
        if (newImg.isNull()) return false;
        if (newImg.width() > 10000 || newImg.height() > 10000) {
            qCritical() << "SECURITY: Rejected replacement image exceeding max dimensions (10,000 x 10,000).";
            return false;
        }
        newImg = newImg.convertToFormat(QImage::Format_RGB888);
        
        auto& dict = xobj->GetDictionary();
        dict.AddKey("Width", PoDoFo::PdfObject(static_cast<int64_t>(newImg.width())));
        dict.AddKey("Height", PoDoFo::PdfObject(static_cast<int64_t>(newImg.height())));
        dict.AddKey("BitsPerComponent", PoDoFo::PdfObject(static_cast<int64_t>(8)));
        dict.AddKey("ColorSpace", PoDoFo::PdfName("DeviceRGB"));
        
        std::vector<char> rgbData;
        rgbData.reserve(newImg.width() * newImg.height() * 3);
        for (int y = 0; y < newImg.height(); ++y) {
            const uchar* scanline = newImg.constScanLine(y);
            for (int x = 0; x < newImg.width(); ++x) {
                rgbData.push_back(static_cast<char>(scanline[x * 3 + 0]));
                rgbData.push_back(static_cast<char>(scanline[x * 3 + 1]));
                rgbData.push_back(static_cast<char>(scanline[x * 3 + 2]));
            }
        }
        
        auto& stream = xobj->GetOrCreateStream();
        stream.SetData(PoDoFo::bufferview(rgbData.data(), rgbData.size()));
        
        dict.RemoveKey("Filter");
        dict.RemoveKey("DecodeParms");
        
        d->document->Save(d->currentFile.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "replaceImage error:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::deleteImage(int pageIndex, const QString &xobjectName) {
    QMutexLocker locker(&d->mutex);
    if (!d->document) return false;
    try {
        auto& page = d->document->GetPages().GetPageAt(pageIndex);
        
        auto* contentsObj = page.GetContents();
        if (!contentsObj) return false;
        
        PoDoFo::charbuff streamBuf;
        contentsObj->CopyTo(streamBuf);
        std::string content(streamBuf.data(), streamBuf.size());
        
        std::string doTarget = "/" + xobjectName.toStdString() + " Do";
        size_t doPos = content.find(doTarget);
        if (doPos == std::string::npos) return false;
        
        size_t qStart = content.rfind("\nq\n", doPos);
        if (qStart == std::string::npos) qStart = content.rfind("\nq ", doPos);
        size_t qEnd = content.find("\nQ", doPos);
        
        if (qStart != std::string::npos && qEnd != std::string::npos) {
            content.erase(qStart, (qEnd + 2) - qStart);
        } else {
            size_t lineStart = content.rfind('\n', doPos);
            size_t lineEnd = content.find('\n', doPos);
            if (lineStart != std::string::npos && lineEnd != std::string::npos)
                content.erase(lineStart, lineEnd - lineStart);
        }
        
        contentsObj->Reset();
        auto& stream = contentsObj->GetObject().GetOrCreateStream();
        stream.SetData(content);
        
        d->document->Save(d->currentFile.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
        qWarning() << "deleteImage error:" << e.what();
#endif
        return false;
    }
}

bool PoDoFoBackend::linearizeDocument(const QString &outputPath)
{
    QMutexLocker locker(&d->mutex);
#ifdef HAS_QPDF
    try {
        std::vector<char> buffer;
        if (d->document) {
            PoDoFo::VectorStreamDevice device(buffer);
            d->document->Save(device);
        } else if (!d->currentFile.isEmpty()) {
            QFile file(d->currentFile);
            if (file.open(QIODevice::ReadOnly)) {
                QByteArray data = file.readAll();
                buffer.assign(data.begin(), data.end());
            }
        }

        if (buffer.empty()) return false;

        qpdf_data pdf = qpdf_init();
        if (qpdf_read_memory(pdf, "input.pdf", buffer.data(), buffer.size(), nullptr) == 0 &&
            qpdf_init_write(pdf, outputPath.toUtf8().constData()) == 0) {
            qpdf_set_linearization(pdf, 1);
            if (qpdf_write(pdf) == 0) {
                qpdf_cleanup(&pdf);
#ifdef QT_DEBUG
                qDebug() << "QPDF API successfully linearized document to:" << outputPath;
#endif
                return true;
            }
        }
        qpdf_cleanup(&pdf);
        return false;
    } catch (const std::exception& e) {
#ifdef QT_DEBUG
        qWarning() << "QPDF linearization error:" << e.what();
#endif
        return false;
    }
#else
    Q_UNUSED(outputPath)
#ifdef QT_DEBUG
    qWarning() << "Linearization not available — built without qpdf";
#endif
    return false;
#endif
}

