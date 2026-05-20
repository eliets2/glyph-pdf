#pragma once
#include "core/interfaces/IPdfEditorEngine.h"
#include <QMap>

class MockPdfEditorEngine : public IPdfEditorEngine {
public:
    bool loadDocumentForEditing(const QString &) override { m_loaded = true; return true; }
    bool saveDocument(const QString &path) override { ++m_saveCalls; m_lastSavedPath = path; return m_loaded; }
    bool editTextInline(int, const QRectF &, const QString &) override { return m_loaded; }
    bool deleteObjectAt(int, const QPointF &) override { return m_loaded; }
    bool linearizeDocument(const QString &) override { return m_loaded; }
    bool exportPdfA(const QString &, int) override { return m_loaded; }
    bool encryptDocument(const QString &, const QString &, bool, bool, bool) override { return m_loaded; }
    bool encryptWithCertificate(const QString &, const QString &, const QStringList &) override { return m_loaded; }
    bool sanitizeDocument(const QString &path) override { ++m_sanitizeCalls; m_lastSanitizedPath = path; return m_sanitizeResult && m_loaded; }
    bool getMetadata(PdfMetadata &out) override { out = m_meta; return true; }
    bool setMetadata(const PdfMetadata &meta) override { m_meta = meta; return true; }
    QString currentFile() const override { return m_file; }
    QStringList getEmbeddedFiles() override { return {}; }
    QStringList getLayers() override { return {}; }
    
    bool rotatePage(const QString &path, int pageIndex, int degrees) override { return true; }
    QByteArray extractPageAsBytes(const QString &path, int pageIndex) override { return QByteArray(); }
    bool insertPageFromBytes(const QString &path, int atIndex, const QByteArray &pageData) override { return true; }
    bool deletePage(const QString &path, int pageIndex) override { return true; }
    bool insertBlankPage(const QString &path, int atIndex) override { return true; }

    QList<PdfImageInfo> listImages(int) override { return {}; }
    bool moveImage(int, const QString &, double, double) override { return true; }
    bool resizeImage(int, const QString &, double, double) override { return true; }
    bool rotateImage(int, const QString &, double) override { return true; }
    bool replaceImage(int, const QString &, const QString &) override { return true; }
    bool deleteImage(int, const QString &) override { return true; }
    bool applyRedactions(int, const QList<QRectF> &) override { return m_loaded; }

    // Test helpers
    bool m_loaded = false;
    bool m_sanitizeResult = true;
    int m_sanitizeCalls = 0;
    int m_saveCalls = 0;
    QString m_lastSanitizedPath;
    QString m_lastSavedPath;
    QString m_file;
    PdfMetadata m_meta;
};
