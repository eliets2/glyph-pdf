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
#include <set>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QRandomGenerator>


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

bool PoDoFoBackend::editTextInline(int pageIndex, const QRectF &rect, const QString &newText,
                                   const QString &fontFamily, int fontSize,
                                   const QColor &color, bool bold,
                                   bool italic, int alignment) {
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
        if (!fontFamily.isEmpty()) {
            extractedFontName = fontFamily.toStdString();
        }
        if (fontSize > 0) {
            extractedFontSize = fontSize;
        }

        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        
        painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(1.0, 1.0, 1.0));
        PoDoFo::Rect pageRect = page.GetMediaBox();
        double pdfY = pageRect.Height - (rect.y() + rect.height());
        painter.DrawRectangle(rect.x(), pdfY, rect.width(), rect.height(), PoDoFo::PdfPathDrawMode::Fill);
        
        painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(color.redF(), color.greenF(), color.blueF()));
        
        // Find standard font corresponding to bold/italic if it's a standard one
        const PoDoFo::PdfFont* font = nullptr;
        if (extractedFontName == "Helvetica" || extractedFontName == "Arial" || fontFamily.isEmpty()) {
            if (bold && italic) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::HelveticaBoldOblique);
            else if (bold) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::HelveticaBold);
            else if (italic) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::HelveticaOblique);
            else font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
        } else if (extractedFontName == "Times" || extractedFontName == "Times New Roman") {
            if (bold && italic) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::TimesBoldItalic);
            else if (bold) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::TimesBold);
            else if (italic) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::TimesItalic);
            else font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::TimesRoman);
        } else if (extractedFontName == "Courier") {
            if (bold && italic) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::CourierBoldOblique);
            else if (bold) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::CourierBold);
            else if (italic) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::CourierOblique);
            else font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Courier);
        }

        if (!font) font = d->document->GetFonts().SearchFont(extractedFontName);
        if (!font) font = &d->document->GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
        
        painter.TextState.SetFont(*font, extractedFontSize);
        
        QStringList lines = newText.split('\n');
        double currentY = pdfY + rect.height() - extractedFontSize;
        for (const QString& line : lines) {
            double x = rect.x();
            if (alignment == 1 || alignment == 2) { // Center or Right
                PoDoFo::PdfTextState textState;
                textState.Font = font;
                textState.FontSize = extractedFontSize;
                double textWidth = font->GetStringLength(line.toUtf8().constData(), textState);
                if (alignment == 1) { // Center
                    x += (rect.width() - textWidth) / 2.0;
                } else if (alignment == 2) { // Right
                    x += rect.width() - textWidth;
                }
            }
            painter.DrawText(line.toUtf8().constData(), x, currentY);
            currentY -= (extractedFontSize * 1.2);
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

double getEncodedStringWidth(const PoDoFo::PdfFont* font, const PoDoFo::PdfString& str, const PoDoFo::PdfTextState& state) {
    if (str.IsStringEvaluated()) {
        return font->GetStringLength(str.GetString(), state);
    }
    try {
        double len = 0.0;
        if (font->TryGetEncodedStringLength(str, state, len)) {
            return len;
        }
    } catch (...) {
        // Fallback
    }
    return font->GetStringLength(str.GetString(), state);
}

void cleanStructElement(PoDoFo::PdfObject* elem, 
                        PoDoFo::PdfReference pageRef, 
                        const std::set<int64_t>& redactedMcids, 
                        PoDoFo::PdfMemDocument* doc) {
    if (!elem) return;
    if (elem->IsReference()) {
        elem = &doc->GetObjects().MustGetObject(elem->GetReference());
    }
    if (!elem->IsDictionary()) return;
    
    auto& dict = elem->GetDictionary();
    bool belongsToPage = false;
    auto* pgKey = dict.FindKey("Pg");
    if (pgKey && pgKey->IsReference() && pgKey->GetReference() == pageRef) {
        belongsToPage = true;
    }
    
    bool childRedacted = false;
    auto* kKey = dict.FindKey("K");
    if (kKey) {
        if (kKey->IsArray()) {
            auto& arr = kKey->GetArray();
            for (unsigned i = 0; i < arr.GetSize(); ) {
                bool removeKid = false;
                auto& kid = arr[i];
                
                if (kid.IsNumber()) {
                    int64_t mcid = kid.GetNumber();
                    if (belongsToPage && redactedMcids.count(mcid)) {
                        removeKid = true;
                        childRedacted = true;
                    }
                } else if (kid.IsDictionary() || kid.IsReference()) {
                    PoDoFo::PdfObject* kidObj = &kid;
                    if (kid.IsReference()) {
                        kidObj = &doc->GetObjects().MustGetObject(kid.GetReference());
                    }
                    if (kidObj->IsDictionary()) {
                        auto& kidDict = kidObj->GetDictionary();
                        auto* typeKey = kidDict.FindKey("Type");
                        if (typeKey && typeKey->IsName() && typeKey->GetName().GetString() == "MCR") {
                            auto* mcrPg = kidDict.FindKey("Pg");
                            auto* mcrMcid = kidDict.FindKey("MCID");
                            if (mcrPg && mcrPg->IsReference() && mcrPg->GetReference() == pageRef &&
                                mcrMcid && mcrMcid->IsNumber() && redactedMcids.count(mcrMcid->GetNumber())) {
                                removeKid = true;
                                childRedacted = true;
                            }
                        } else {
                            cleanStructElement(kidObj, pageRef, redactedMcids, doc);
                            auto* subK = kidDict.FindKey("K");
                            if (subK && subK->IsArray() && subK->GetArray().GetSize() == 0) {
                                removeKid = true;
                                childRedacted = true;
                            }
                        }
                    }
                }
                
                if (removeKid) {
                    arr.RemoveAt(i);
                } else {
                    ++i;
                }
            }
        } else if (kKey->IsNumber()) {
            int64_t mcid = kKey->GetNumber();
            if (belongsToPage && redactedMcids.count(mcid)) {
                dict.RemoveKey("K");
                childRedacted = true;
            }
        } else if (kKey->IsDictionary() || kKey->IsReference()) {
            PoDoFo::PdfObject* kidObj = kKey;
            if (kKey->IsReference()) {
                kidObj = &doc->GetObjects().MustGetObject(kKey->GetReference());
            }
            if (kidObj->IsDictionary()) {
                auto& kidDict = kidObj->GetDictionary();
                auto* typeKey = kidDict.FindKey("Type");
                if (typeKey && typeKey->IsName() && typeKey->GetName().GetString() == "MCR") {
                    auto* mcrPg = kidDict.FindKey("Pg");
                    auto* mcrMcid = kidDict.FindKey("MCID");
                    if (mcrPg && mcrPg->IsReference() && mcrPg->GetReference() == pageRef &&
                        mcrMcid && mcrMcid->IsNumber() && redactedMcids.count(mcrMcid->GetNumber())) {
                        dict.RemoveKey("K");
                        childRedacted = true;
                    }
                } else {
                    cleanStructElement(kidObj, pageRef, redactedMcids, doc);
                    auto* subK = kidDict.FindKey("K");
                    if (subK && subK->IsArray() && subK->GetArray().GetSize() == 0) {
                        dict.RemoveKey("K");
                        childRedacted = true;
                    }
                }
            }
        }
    }
    
    bool hasRedactedContent = childRedacted || (belongsToPage && !redactedMcids.empty());
    if (hasRedactedContent) {
        if (dict.HasKey("ActualText")) dict.RemoveKey("ActualText");
        if (dict.HasKey("Alt")) dict.RemoveKey("Alt");
        if (dict.HasKey("E")) dict.RemoveKey("E");
    }
}

} // anonymous namespace

bool PoDoFoBackend::cropPage(const QString &path, int pageIndex, const QRectF &cropRect) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);
        auto& pages = doc.GetPages();
        if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= pages.GetCount()) return false;
        
        auto& page = pages.GetPageAt(pageIndex);
        PoDoFo::Rect podofoRect(cropRect.x(), cropRect.y(), cropRect.width(), cropRect.height());
        page.GetDictionary().AddKey("CropBox", podofoRect.ToArray());
        
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool PoDoFoBackend::resizePage(const QString &path, int pageIndex, const QSizeF &size) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);
        auto& pages = doc.GetPages();
        if (pageIndex < 0 || static_cast<size_t>(pageIndex) >= pages.GetCount()) return false;
        
        auto& page = pages.GetPageAt(pageIndex);
        PoDoFo::Rect oldMedia = page.GetMediaBox();
        PoDoFo::Rect newMedia(oldMedia.X, oldMedia.Y, size.width(), size.height());
        page.GetDictionary().AddKey("MediaBox", newMedia.ToArray());
        
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool PoDoFoBackend::reorderPages(const QString &path, int fromIndex, int toIndex) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);
        auto& pages = doc.GetPages();
        if (fromIndex < 0 || static_cast<size_t>(fromIndex) >= pages.GetCount()) return false;
        if (toIndex < 0 || static_cast<size_t>(toIndex) >= pages.GetCount()) return false;
        if (fromIndex == toIndex) return true;
        
        // Use a temporary document to extract and reinsert the page
        PoDoFo::PdfMemDocument tempDoc;
        tempDoc.GetPages().InsertDocumentPageAt(0, doc, fromIndex);
        
        pages.RemovePageAt(fromIndex);
        
        // Adjust toIndex after removal if necessary
        if (fromIndex < toIndex) {
            toIndex--;
        }
        
        pages.InsertDocumentPageAt(toIndex, tempDoc, 0);
        
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool PoDoFoBackend::addHeaderFooter(const QString &path, const HeaderFooterOptions &options) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);
        auto& pages = doc.GetPages();
        int totalPages = static_cast<int>(pages.GetCount());
        
        std::string fontName = options.fontFamily.isEmpty() ? "Helvetica" : options.fontFamily.toStdString();
        const PoDoFo::PdfFont* font = doc.GetFonts().SearchFont(fontName);
        if (!font) {
            font = &doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
        }
        
        for (int i = 0; i < totalPages; ++i) {
            auto& page = pages.GetPageAt(i);
            QString text = options.textTemplate;
            text.replace("{page}", QString::number(i + 1));
            text.replace("{total}", QString::number(totalPages));
            
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(*font, options.fontSize);
            
            PoDoFo::PdfTextState textState;
            textState.Font = font;
            textState.FontSize = options.fontSize;
            double textWidth = font->GetStringLength(text.toUtf8().constData(), textState);
            
            PoDoFo::Rect mediaBox = page.GetMediaBox();
            double x = 0, y = 0;
            double margin = 36.0; // 0.5 inch margin
            
            switch (options.position) {
                case HeaderFooterOptions::Position::TopLeft:
                    x = mediaBox.X + margin;
                    y = mediaBox.Y + mediaBox.Height - margin - options.fontSize;
                    break;
                case HeaderFooterOptions::Position::TopCenter:
                    x = mediaBox.X + (mediaBox.Width - textWidth) / 2.0;
                    y = mediaBox.Y + mediaBox.Height - margin - options.fontSize;
                    break;
                case HeaderFooterOptions::Position::TopRight:
                    x = mediaBox.X + mediaBox.Width - margin - textWidth;
                    y = mediaBox.Y + mediaBox.Height - margin - options.fontSize;
                    break;
                case HeaderFooterOptions::Position::BottomLeft:
                    x = mediaBox.X + margin;
                    y = mediaBox.Y + margin;
                    break;
                case HeaderFooterOptions::Position::BottomCenter:
                    x = mediaBox.X + (mediaBox.Width - textWidth) / 2.0;
                    y = mediaBox.Y + margin;
                    break;
                case HeaderFooterOptions::Position::BottomRight:
                    x = mediaBox.X + mediaBox.Width - margin - textWidth;
                    y = mediaBox.Y + margin;
                    break;
            }
            
            painter.DrawText(text.toUtf8().constData(), x, y);
            painter.FinishDrawing();
        }
        
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}

bool PoDoFoBackend::applyBatesNumbering(const QString &path, const BatesNumberingOptions &options) {
    QMutexLocker locker(&d->mutex);
    try {
        auto& doc = d->resolveDocument(path);
        auto& pages = doc.GetPages();
        int totalPages = static_cast<int>(pages.GetCount());
        
        std::string fontName = options.fontFamily.isEmpty() ? "Helvetica" : options.fontFamily.toStdString();
        const PoDoFo::PdfFont* font = doc.GetFonts().SearchFont(fontName);
        if (!font) {
            font = &doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
        }
        
        int currentNumber = options.startNumber;
        for (int i = 0; i < totalPages; ++i) {
            auto& page = pages.GetPageAt(i);
            
            QString numStr = QString::number(currentNumber).rightJustified(options.digitCount, '0');
            QString text = options.prefix + numStr + options.suffix;
            
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.TextState.SetFont(*font, options.fontSize);
            
            PoDoFo::PdfTextState textState;
            textState.Font = font;
            textState.FontSize = options.fontSize;
            double textWidth = font->GetStringLength(text.toUtf8().constData(), textState);
            
            PoDoFo::Rect mediaBox = page.GetMediaBox();
            double x = 0, y = 0;
            double margin = 36.0;
            
            switch (options.position) {
                case HeaderFooterOptions::Position::TopLeft:
                    x = mediaBox.X + margin;
                    y = mediaBox.Y + mediaBox.Height - margin - options.fontSize;
                    break;
                case HeaderFooterOptions::Position::TopCenter:
                    x = mediaBox.X + (mediaBox.Width - textWidth) / 2.0;
                    y = mediaBox.Y + mediaBox.Height - margin - options.fontSize;
                    break;
                case HeaderFooterOptions::Position::TopRight:
                    x = mediaBox.X + mediaBox.Width - margin - textWidth;
                    y = mediaBox.Y + mediaBox.Height - margin - options.fontSize;
                    break;
                case HeaderFooterOptions::Position::BottomLeft:
                    x = mediaBox.X + margin;
                    y = mediaBox.Y + margin;
                    break;
                case HeaderFooterOptions::Position::BottomCenter:
                    x = mediaBox.X + (mediaBox.Width - textWidth) / 2.0;
                    y = mediaBox.Y + margin;
                    break;
                case HeaderFooterOptions::Position::BottomRight:
                    x = mediaBox.X + mediaBox.Width - margin - textWidth;
                    y = mediaBox.Y + margin;
                    break;
            }
            
            painter.DrawText(text.toUtf8().constData(), x, y);
            painter.FinishDrawing();
            
            currentNumber++;
        }
        
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (...) {
        return false;
    }
}


void redactCanvasRecursively(PoDoFo::PdfObject& canvasObj, 
                             const std::vector<PoDoFo::Rect>& pdfRects, 
                             PoDoFo::PdfPage& page, 
                             PoDoFo::PdfMemDocument* document,
                             std::set<int64_t>& redactedMcids) 
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
    
    // Text state parameters
    double currentFontSize = 12.0;
    double currentCharSpacing = 0.0;
    double currentWordSpacing = 0.0;
    double currentFontScale = 1.0;
    std::string currentFontName = "Helvetica";
    int currentRenderingMode = 0; // Tr
    
    int64_t currentMcid = -1;

    auto isIntersectingSpan = [&](double xStart, double xEnd, double y) -> bool {
        for (const auto& r : pdfRects) {
            if (y >= r.Y && y <= (r.Y + r.Height)) {
                if (xEnd >= r.X && xStart <= (r.X + r.Width))
                    return true;
            }
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
            
            // Track Text State Parameters
            if (kw == "Tf" && stack.size() >= 2) {
                if (stack[0].IsName()) {
                    currentFontName = stack[0].GetName().GetString();
                }
                if (stack[1].IsNumberOrReal()) {
                    currentFontSize = stack[1].GetReal();
                }
            } else if (kw == "Tc" && stack.size() >= 1) {
                if (stack[0].IsNumberOrReal()) currentCharSpacing = stack[0].GetReal();
            } else if (kw == "Tw" && stack.size() >= 1) {
                if (stack[0].IsNumberOrReal()) currentWordSpacing = stack[0].GetReal();
            } else if (kw == "Tz" && stack.size() >= 1) {
                if (stack[0].IsNumberOrReal()) currentFontScale = stack[0].GetReal() / 100.0;
            } else if (kw == "Tr" && stack.size() >= 1) {
                if (stack[0].IsNumberOrReal()) currentRenderingMode = static_cast<int>(stack[0].GetReal());
            }
            
            // Track MCID
            if (kw == "BDC" && stack.size() >= 2) {
                const auto& prop = stack[1];
                if (prop.IsDictionary()) {
                    auto* mcidObj = prop.GetDictionary().FindKey("MCID");
                    if (mcidObj && mcidObj->IsNumber()) {
                        currentMcid = mcidObj->GetNumber();
                    }
                } else if (prop.IsName()) {
                    auto resources = page.GetResources();
                    auto* propDictObj = resources.GetDictionary().FindKey("Properties");
                    if (propDictObj && propDictObj->IsDictionary()) {
                        auto* subDictObj = propDictObj->GetDictionary().FindKey(prop.GetName());
                        if (subDictObj) {
                            if (subDictObj->IsReference())
                                subDictObj = &document->GetObjects().MustGetObject(subDictObj->GetReference());
                            if (subDictObj->IsDictionary()) {
                                auto* mcidObj = subDictObj->GetDictionary().FindKey("MCID");
                                if (mcidObj && mcidObj->IsNumber()) {
                                    currentMcid = mcidObj->GetNumber();
                                }
                            }
                        }
                    }
                }
            } else if (kw == "EMC") {
                currentMcid = -1;
            }
            
            bool isTextOp = (kw == "Tj" || kw == "TJ" || kw == "'" || kw == "\"");
            if (isTextOp) {
                // Resolve font
                const PdfFont* resolvedFont = nullptr;
                auto resources = page.GetResources();
                auto* fontDict = resources.GetDictionary().FindKey("Font");
                if (fontDict && fontDict->IsDictionary()) {
                    auto* fontObj = fontDict->GetDictionary().FindKey(currentFontName);
                    if (fontObj) {
                        if (fontObj->IsReference())
                            fontObj = &document->GetObjects().MustGetObject(fontObj->GetReference());
                        if (fontObj->IsDictionary()) {
                            auto* baseFont = fontObj->GetDictionary().FindKey("BaseFont");
                            if (baseFont && baseFont->IsName()) {
                                std::string baseFontName = std::string(baseFont->GetName().GetString());
                                size_t plusPos = baseFontName.find('+');
                                if (plusPos != std::string::npos) baseFontName = baseFontName.substr(plusPos + 1);
                                if (!baseFontName.empty() && baseFontName[0] == '/') baseFontName = baseFontName.substr(1);
                                size_t commaPos = baseFontName.find(',');
                                if (commaPos != std::string::npos) baseFontName = baseFontName.substr(0, commaPos);
                                size_t dashPos = baseFontName.find('-');
                                if (dashPos != std::string::npos) baseFontName = baseFontName.substr(0, dashPos);
                                resolvedFont = document->GetFonts().SearchFont(baseFontName);
                            }
                        }
                    }
                }
                if (!resolvedFont) {
                    resolvedFont = &document->GetFonts().GetStandard14Font(PdfStandard14FontType::Helvetica);
                }
                
                PdfTextState textState;
                textState.Font = resolvedFont;
                textState.FontSize = currentFontSize;
                textState.FontScale = currentFontScale;
                textState.CharSpacing = currentCharSpacing;
                textState.WordSpacing = currentWordSpacing;
                
                double totalAdvance = 0.0;
                if (kw == "Tj" && stack.size() >= 1) {
                    if (stack[0].IsString()) {
                        totalAdvance = getEncodedStringWidth(resolvedFont, stack[0].GetString(), textState);
                    }
                } else if (kw == "TJ" && stack.size() >= 1) {
                    if (stack[0].IsArray()) {
                        for (const auto& item : stack[0].GetArray()) {
                            if (item.IsString()) {
                                totalAdvance += getEncodedStringWidth(resolvedFont, item.GetString(), textState);
                            } else if (item.IsNumberOrReal()) {
                                totalAdvance -= (item.GetReal() / 1000.0) * currentFontSize * currentFontScale;
                            }
                        }
                    }
                } else if (kw == "'" && stack.size() >= 1) {
                    if (stack[0].IsString()) {
                        totalAdvance = getEncodedStringWidth(resolvedFont, stack[0].GetString(), textState);
                    }
                } else if (kw == "\"" && stack.size() >= 3) {
                    if (stack[0].IsNumberOrReal()) currentWordSpacing = stack[0].GetReal();
                    if (stack[1].IsNumberOrReal()) currentCharSpacing = stack[1].GetReal();
                    textState.CharSpacing = currentCharSpacing;
                    textState.WordSpacing = currentWordSpacing;
                    if (stack[2].IsString()) {
                        totalAdvance = getEncodedStringWidth(resolvedFont, stack[2].GetString(), textState);
                    }
                }
                
                if (kw == "'" || kw == "\"") {
                    textY -= leading;
                }
                
                if (inTextBlock && isIntersectingSpan(textX, textX + totalAdvance, textY)) {
                    // Record redacted MCID if active
                    if (currentMcid != -1) {
                        redactedMcids.insert(currentMcid);
                    }
                    
                    // D1: Normalize glyph advances: replace with single space glyph and custom adj
                    double spaceWidth = resolvedFont->GetSpaceCharLength(textState);
                    double scale = currentFontSize * currentFontScale;
                    double adj = 0.0;
                    if (std::abs(scale) > 1e-5) {
                        adj = 1000.0 * (spaceWidth - totalAdvance) / scale;
                    }
                    
                    if (kw == "'") {
                        newStream << "T*\n";
                    } else if (kw == "\"") {
                        newStream << stack[0].GetReal() << " Tw\n";
                        newStream << stack[1].GetReal() << " Tc\n";
                        newStream << "T*\n";
                    }
                    
                    newStream << "[ ( ) " << adj << " ] TJ\n";
                    textX += totalAdvance;
                    continue;
                }
                
                textX += totalAdvance;
            }
            
            if (kw == "Do" && stack.size() > 0 && stack[0].IsName()) {
                std::string xobjName(stack[0].GetName().GetString());
                if (isIntersectingSpan(textX, textX, textY)) { // Simple check for image position
                    if (currentMcid != -1) {
                        redactedMcids.insert(currentMcid);
                    }
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
                                redactCanvasRecursively(*xobj, pdfRects, page, document, redactedMcids);
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

bool PoDoFoBackend::applyRedactions(int pageIndex, const QList<QRectF> &rects) {
    QMutexLocker locker(&d->mutex);
    if (!d->document || pageIndex < 0 || (unsigned)pageIndex >= d->document->GetPages().GetCount()) return false;

    try {
        QByteArray beforeHash;
        if (!d->currentFile.isEmpty() && QFile::exists(d->currentFile)) {
            QFile file(d->currentFile);
            if (file.open(QIODevice::ReadOnly)) {
                beforeHash = QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex();
                file.close();
            }
        }

        PoDoFo::PdfPage& page = d->document->GetPages().GetPageAt(pageIndex);
        double pageHeight = page.GetMediaBox().Height;

        std::vector<PoDoFo::Rect> pdfRects;
        for (const auto& r : rects) {
            pdfRects.push_back(PoDoFo::Rect(r.x(), pageHeight - r.y() - r.height(), r.width(), r.height()));
        }

        std::set<int64_t> redactedMcids;
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
                redactCanvasRecursively(page.GetObject(), pdfRects, page, d->document.get(), redactedMcids);
                streamFilterApplied = true;
            }
        }

        if (!streamFilterApplied && contentsObj) {
            qCritical() << "SECURITY: Redaction on page" << pageIndex
                        << "failed to apply content stream surgery due to unparseable or binary content."
                           " Aborting operation to prevent insecure visual-only overlay.";
            return false;
        }

        // D3: Tagged PDF structure tree sanitization
        auto& catalog = d->document->GetCatalog();
        auto* structTreeRootObj = catalog.GetDictionary().FindKey("StructTreeRoot");
        if (structTreeRootObj && !redactedMcids.empty()) {
            if (structTreeRootObj->IsReference()) {
                structTreeRootObj = &d->document->GetObjects().MustGetObject(structTreeRootObj->GetReference());
            }
            if (structTreeRootObj->IsDictionary()) {
                auto& rootDict = structTreeRootObj->GetDictionary();
                auto* kKey = rootDict.FindKey("K");
                if (kKey) {
                    PoDoFo::PdfReference pageRef = page.GetObject().GetReference();
                    if (kKey->IsArray()) {
                        for (auto& kid : kKey->GetArray()) {
                            cleanStructElement(&kid, pageRef, redactedMcids, d->document.get());
                        }
                    } else {
                        cleanStructElement(kKey, pageRef, redactedMcids, d->document.get());
                    }
                }
            }
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

        // D4: Generate JSON sidecar audit log
        if (!d->currentFile.isEmpty()) {
            std::vector<char> afterBuffer;
            PoDoFo::VectorStreamDevice afterDevice(afterBuffer);
            d->document->Save(afterDevice);
            QByteArray afterHash = QCryptographicHash::hash(
                QByteArray(afterBuffer.data(), static_cast<int>(afterBuffer.size())), 
                QCryptographicHash::Sha256).toHex();

            QString logPath = d->currentFile + ".redaction-log.json";
            QJsonArray logArray;
            QFile logFile(logPath);
            if (logFile.exists() && logFile.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(logFile.readAll());
                if (doc.isArray()) {
                    logArray = doc.array();
                }
                logFile.close();
            }

            QJsonObject entry;
            entry["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            entry["page"] = pageIndex;

            QJsonArray regionsJson;
            for (const auto& r : rects) {
                QJsonObject regObj;
                regObj["x"] = r.x();
                regObj["y"] = r.y();
                regObj["width"] = r.width();
                regObj["height"] = r.height();
                regionsJson.append(regObj);
            }
            entry["regions"] = regionsJson;

            QJsonArray ops;
            ops.append("excised_text_operators");
            if (!redactedMcids.empty()) {
                ops.append("scrubbed_structure_elements");
            }
            entry["operations"] = ops;
            entry["before_sha256"] = QString(beforeHash);
            entry["after_sha256"] = QString(afterHash);

            logArray.append(entry);

            if (logFile.open(QIODevice::WriteOnly)) {
                logFile.write(QJsonDocument(logArray).toJson(QJsonDocument::Indented));
                logFile.close();
            }
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

        // D5: Extended sanitization passes
        // 16. Structure Tree Root sanitization: recursively walk and remove Alt, ActualText, E from all elements
        auto* structTreeRootObj = catalog.GetDictionary().FindKey("StructTreeRoot");
        if (structTreeRootObj) {
            std::function<void(PoDoFo::PdfObject*)> sanitizeAllStructElements = [&](PoDoFo::PdfObject* elem) {
                if (!elem) return;
                if (elem->IsReference()) {
                    elem = &d->document->GetObjects().MustGetObject(elem->GetReference());
                }
                if (!elem->IsDictionary()) return;
                
                auto& dict = elem->GetDictionary();
                if (dict.HasKey("ActualText")) dict.RemoveKey("ActualText");
                if (dict.HasKey("Alt")) dict.RemoveKey("Alt");
                if (dict.HasKey("E")) dict.RemoveKey("E");
                
                auto* kKey = dict.FindKey("K");
                if (kKey) {
                    if (kKey->IsArray()) {
                        for (auto& kid : kKey->GetArray()) {
                            sanitizeAllStructElements(&kid);
                        }
                    } else {
                        sanitizeAllStructElements(kKey);
                    }
                }
            };
            sanitizeAllStructElements(structTreeRootObj);
        }

        // 17. Flatten optional content layers by removing OCProperties
        if (catalog.GetDictionary().HasKey(PdfName("OCProperties"))) {
            catalog.GetDictionary().RemoveKey(PdfName("OCProperties"));
        }

        // 18. Remove Outlines (bookmarks)
        if (catalog.GetDictionary().HasKey(PdfName("Outlines"))) {
            catalog.GetDictionary().RemoveKey(PdfName("Outlines"));
        }

        // 20. Remove Collection portfolio
        if (catalog.GetDictionary().HasKey(PdfName("Collection"))) {
            catalog.GetDictionary().RemoveKey(PdfName("Collection"));
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

            // Sanitize page annotations
            auto& annos = pg.GetAnnotations();
            std::vector<unsigned> toRemoveAnnos;
            for (unsigned ai = 0; ai < annos.GetCount(); ++ai) {
                auto& anno = annos.GetAnnotAt(ai);
                auto& dict = anno.GetDictionary();
                // 23. Remove annotation contents & rich text
                if (dict.HasKey("Contents")) dict.RemoveKey("Contents");
                if (dict.HasKey("RC")) dict.RemoveKey("RC");

                // 19. Remove RichMedia, Movie, Screen annotations
                auto* subtypeObj = dict.FindKey("Subtype");
                if (subtypeObj && subtypeObj->IsName()) {
                    std::string subtypeName = std::string(subtypeObj->GetName().GetString());
                    if (subtypeName == "RichMedia" || subtypeName == "Screen" || subtypeName == "Movie") {
                        toRemoveAnnos.push_back(ai);
                    }
                }
            }
            for (auto it = toRemoveAnnos.rbegin(); it != toRemoveAnnos.rend(); ++it) {
                annos.RemoveAnnotAt(*it);
            }
        }

        // 22. Clear AcroForm field values
        for (auto field : d->document->GetFieldsIterator()) {
            auto& fieldDict = field->GetDictionary();
            if (fieldDict.HasKey("V")) fieldDict.RemoveKey("V");
            if (fieldDict.HasKey("DV")) fieldDict.RemoveKey("DV");
        }

        // 21. Trailer ID second element randomization
        if (trailer.GetDictionary().HasKey("ID")) {
            auto* idObj = trailer.GetDictionary().FindKey("ID");
            if (idObj && idObj->IsArray()) {
                auto& idArr = idObj->GetArray();
                if (idArr.GetSize() >= 2) {
                    std::vector<char> randomBytes(16);
                    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(randomBytes.data()), 4);
                    idArr[1] = PdfObject(PdfString(std::string(randomBytes.data(), 16)));
                }
            }
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

bool PoDoFoBackend::embedAnnotations(const QString &inputPath, const QString &outputPath, const QList<AnnotationItem> &annotations)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(inputPath.toUtf8().constData());
        
        QMap<QString, PoDoFo::PdfObject*> idToObjectMap;
        QList<AnnotationItem> commentsToLink;
        
        // Group by page
        QMap<int, QList<AnnotationItem>> pageAnnos;
        for (const auto& anno : annotations) {
            pageAnnos[anno.pageIndex].append(anno);
        }
        
        for (auto it = pageAnnos.constBegin(); it != pageAnnos.constEnd(); ++it) {
            int pageIdx = it.key();
            if (pageIdx < 0 || static_cast<unsigned>(pageIdx) >= doc.GetPages().GetCount()) continue;
            
            auto& page = doc.GetPages().GetPageAt(pageIdx);
            double pageHeight = page.GetMediaBox().Height;
            
            for (const auto& anno : it.value()) {
                // Determine bounds
                QRectF bounds = anno.rect;
                if (anno.mode == ToolMode::DrawFreehand || anno.mode == ToolMode::AddSignature) {
                    if (!anno.points.isEmpty()) {
                        bounds = QRectF(anno.points.first(), anno.points.first());
                        for (const auto& p : anno.points) bounds = bounds.united(QRectF(p, p));
                    }
                }
                PoDoFo::Rect pdfRect(bounds.x(), pageHeight - bounds.y() - bounds.height(), bounds.width(), bounds.height());
                
                auto& annot = page.GetAnnotations().CreateAnnot(PoDoFo::PdfAnnotationType::Text, pdfRect);
                PoDoFo::PdfDictionary& dict = annot.GetDictionary();
                
                if (anno.mode == ToolMode::Strikeout) dict.AddKey("Subtype", PoDoFo::PdfName("StrikeOut"));
                else if (anno.mode == ToolMode::Squiggly) dict.AddKey("Subtype", PoDoFo::PdfName("Squiggly"));
                else if (anno.mode == ToolMode::Underline) dict.AddKey("Subtype", PoDoFo::PdfName("Underline"));
                else if (anno.mode == ToolMode::Highlight) dict.AddKey("Subtype", PoDoFo::PdfName("Highlight"));
                else if (anno.mode == ToolMode::Stamp) dict.AddKey("Subtype", PoDoFo::PdfName("Stamp"));
                else if (anno.mode == ToolMode::Callout) dict.AddKey("Subtype", PoDoFo::PdfName("FreeText"));
                
                // Common properties
                if (!anno.text.isEmpty()) {
                    dict.AddKey("Contents", PoDoFo::PdfString(anno.text.toStdString()));
                }
                if (!anno.author.isEmpty()) {
                    dict.AddKey("T", PoDoFo::PdfString(anno.author.toStdString()));
                }
                if (!anno.creationDate.isEmpty()) {
                    // Approximate Date format
                    QString dateStr = "D:" + anno.creationDate; // simplify
                    dict.AddKey("CreationDate", PoDoFo::PdfString(dateStr.toStdString()));
                }
                
                // Color array
                PoDoFo::PdfArray colorArr;
                colorArr.Add(anno.color.redF());
                colorArr.Add(anno.color.greenF());
                colorArr.Add(anno.color.blueF());
                dict.AddKey("C", colorArr);
                
                // Review state logic & Linking
                if (!anno.id.isEmpty()) {
                    dict.AddKey("NM", PoDoFo::PdfString(anno.id.toStdString()));
                    idToObjectMap[anno.id] = &annot.GetObject();
                    if (!anno.parentId.isEmpty()) {
                        commentsToLink.append(anno);
                    }
                    if (anno.reviewState != ReviewState::None && anno.reviewState != ReviewState::Open) {
                        dict.AddKey("Subj", PoDoFo::PdfString("State")); // Standard for state change
                    }
                }
                
                // Appearance Streams for specific types (simplified generation)
                if (anno.mode == ToolMode::Stamp || anno.mode == ToolMode::Callout || anno.mode == ToolMode::Strikeout || anno.mode == ToolMode::Squiggly) {
                    auto& apDict = doc.GetObjects().CreateDictionaryObject();
                    
                    auto& streamObj = doc.GetObjects().CreateDictionaryObject();
                    streamObj.GetDictionary().AddKey("Type", PoDoFo::PdfName("XObject"));
                    streamObj.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Form"));
                    
                    PoDoFo::PdfArray bboxArray;
                    bboxArray.Add(PoDoFo::PdfObject(static_cast<int64_t>(0)));
                    bboxArray.Add(PoDoFo::PdfObject(static_cast<int64_t>(0)));
                    bboxArray.Add(PoDoFo::PdfObject(static_cast<int64_t>(bounds.width())));
                    bboxArray.Add(PoDoFo::PdfObject(static_cast<int64_t>(bounds.height())));
                    streamObj.GetDictionary().AddKey("BBox", bboxArray);
                    
                    std::string streamContent;
                    
                    if (anno.mode == ToolMode::Strikeout) {
                        double midY = bounds.height() / 2.0;
                        streamContent = std::to_string(anno.color.redF()) + " " + std::to_string(anno.color.greenF()) + " " + std::to_string(anno.color.blueF()) + " RG\n";
                        streamContent += "1 w\n0 " + std::to_string(midY) + " m\n" + std::to_string(bounds.width()) + " " + std::to_string(midY) + " l\nS\n";
                    } else if (anno.mode == ToolMode::Squiggly) {
                        streamContent = std::to_string(anno.color.redF()) + " " + std::to_string(anno.color.greenF()) + " " + std::to_string(anno.color.blueF()) + " RG\n";
                        streamContent += "1 w\n0 2 m\n";
                        for (double x = 2; x < bounds.width(); x += 2) {
                            streamContent += std::to_string(x) + " " + (std::fmod(x, 4) == 0 ? "0" : "4") + " l\n";
                        }
                        streamContent += "S\n";
                    } else {
                        streamContent = "0.8 0.8 0.8 rg\n0 0 " + std::to_string(bounds.width()) + " " + std::to_string(bounds.height()) + " re\nf\n";
                        if (!anno.text.isEmpty()) {
                            streamContent += "0 0 0 rg\nBT\n/Helv 12 Tf\n2 " + std::to_string(bounds.height() - 14) + " Td\n(" + anno.text.toStdString() + ") Tj\nET\n";
                        }
                    }
                    
                    streamObj.GetOrCreateStream().SetData(PoDoFo::bufferview(streamContent.data(), streamContent.size()));
                    
                    apDict.GetDictionary().AddKey("N", streamObj.GetReference());
                    dict.AddKey("AP", apDict.GetReference());
                }
            }
        }
        
        // IRT (In Reply To) Linking
        for (const auto& anno : commentsToLink) {
            if (idToObjectMap.contains(anno.id) && idToObjectMap.contains(anno.parentId)) {
                PoDoFo::PdfObject* childObj = idToObjectMap[anno.id];
                PoDoFo::PdfObject* parentObj = idToObjectMap[anno.parentId];
                childObj->GetDictionary().AddKey("IRT", parentObj->GetReference());
                // For replies to be part of the thread correctly in standard PDF, RT is often set to 'R' (Reply) or 'Group'
                childObj->GetDictionary().AddKey("RT", PoDoFo::PdfName("R"));
                
                // State updates (Accepted/Rejected/etc) in PDF are technically replies to the original annotation 
                // with StateModel=Review and State=Accepted
                if (anno.reviewState == ReviewState::Accepted) childObj->GetDictionary().AddKey("State", PoDoFo::PdfString("Accepted"));
                else if (anno.reviewState == ReviewState::Rejected) childObj->GetDictionary().AddKey("State", PoDoFo::PdfString("Rejected"));
                else if (anno.reviewState == ReviewState::Completed) childObj->GetDictionary().AddKey("State", PoDoFo::PdfString("Completed"));
                else if (anno.reviewState == ReviewState::Cancelled) childObj->GetDictionary().AddKey("State", PoDoFo::PdfString("Cancelled"));
            }
        }
        
        doc.Save(outputPath.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "Error embedding annotations:" << e.what();
        return false;
    }
}

