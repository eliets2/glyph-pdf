#pragma once

#include <QString>
#include <QList>
#include <QDateTime>

/**
 * Structured error container used throughout GlyphPDF engines.
 *
 * Every engine method that can fail populates an ErrorInfo instead of
 * returning a bare bool.  The UI layer (ErrorDialog, BatchMode) uses
 * the severity + suggestedAction to decide which buttons to show.
 */
struct ErrorInfo
{
    enum Severity { Info, Warning, Error, Critical };

    enum SuggestedAction {
        None        = 0x00,
        Retry       = 0x01,
        Skip        = 0x02,
        ExportLog   = 0x04,
        Repair      = 0x08
    };
    Q_DECLARE_FLAGS(SuggestedActions, SuggestedAction)

    Severity          severity        = Error;
    QString           userMessage;          // human-readable, shown prominently
    QString           technicalDetails;     // stack context, kept in expandable section
    SuggestedActions  suggestedActions = None;
    QString           sourceFile;           // originating file path (for batch)
    int               sourcePage       = -1;
    QDateTime         timestamp        = QDateTime::currentDateTime();

    // Convenience constructors
    static ErrorInfo info(const QString& msg) {
        ErrorInfo e; e.severity = Info; e.userMessage = msg; return e;
    }
    static ErrorInfo warning(const QString& msg, const QString& tech = {}) {
        ErrorInfo e; e.severity = Warning; e.userMessage = msg;
        e.technicalDetails = tech; return e;
    }
    static ErrorInfo error(const QString& msg, const QString& tech = {},
                           SuggestedActions actions = None) {
        ErrorInfo e; e.severity = Error; e.userMessage = msg;
        e.technicalDetails = tech; e.suggestedActions = actions; return e;
    }
    static ErrorInfo critical(const QString& msg, const QString& tech = {}) {
        ErrorInfo e; e.severity = Critical; e.userMessage = msg;
        e.technicalDetails = tech;
        e.suggestedActions = ExportLog; return e;
    }

    bool isOk() const { return userMessage.isEmpty(); }
    explicit operator bool() const { return !isOk(); }

    QString severityString() const {
        switch (severity) {
            case Info:     return QStringLiteral("INFO");
            case Warning:  return QStringLiteral("WARNING");
            case Error:    return QStringLiteral("ERROR");
            case Critical: return QStringLiteral("CRITICAL");
        }
        return QStringLiteral("UNKNOWN");
    }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ErrorInfo::SuggestedActions)

/**
 * Collects errors for batch operations.
 * BatchMode and engines append to this during multi-file processing.
 */
struct ErrorLog
{
    QList<ErrorInfo> entries;

    void append(const ErrorInfo& e)       { entries.append(e); }
    void append(ErrorInfo&& e)            { entries.append(std::move(e)); }
    int  count() const                    { return entries.size(); }
    int  errorCount() const;
    int  warningCount() const;
    bool hasErrors() const;
    bool exportCsv(const QString& path) const;
    bool exportJson(const QString& path) const;
    void clear()                          { entries.clear(); }
};
