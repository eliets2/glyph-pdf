// SPDX-License-Identifier: Apache-2.0
#include "DjotToRichTextXhtml.h"

#include <QStringList>

// ---------------------------------------------------------------------------
// DjotToRichTextXhtml — implementation
//
// Two output sinks (XHTML and plain text) share a single inline tokenizer so
// the projections can never drift apart. The tokenizer recognises the bounded
// annotation subset documented in the header; unrecognised markup degrades to
// literal characters.
// ---------------------------------------------------------------------------

namespace pdfws_djot {

namespace {

// Escape the five XML predefined entities for text node / attribute content.
QString xmlEscape(const QString& in)
{
    QString out;
    out.reserve(in.size() + 8);
    for (QChar c : in) {
        switch (c.unicode()) {
        case '&':  out += QStringLiteral("&amp;");  break;
        case '<':  out += QStringLiteral("&lt;");   break;
        case '>':  out += QStringLiteral("&gt;");   break;
        case '"':  out += QStringLiteral("&quot;"); break;
        case '\'': out += QStringLiteral("&#39;");  break;
        default:   out += c;                        break;
        }
    }
    return out;
}

// Sink interface so the inline tokenizer can emit to either XHTML or plain
// text without duplicating the scanning logic.
struct InlineSink {
    virtual ~InlineSink() = default;
    virtual void text(const QString& literal) = 0;       // already-unescaped
    virtual void strongOpen()  = 0;
    virtual void strongClose() = 0;
    virtual void emphOpen()    = 0;
    virtual void emphClose()   = 0;
    virtual void codeOpen()    = 0;
    virtual void codeClose()   = 0;
    virtual void link(const QString& text, const QString& url) = 0;
};

struct XhtmlInlineSink : InlineSink {
    QString out;
    void text(const QString& s) override     { out += xmlEscape(s); }
    void strongOpen()  override              { out += QStringLiteral("<b>"); }
    void strongClose() override              { out += QStringLiteral("</b>"); }
    void emphOpen()    override              { out += QStringLiteral("<i>"); }
    void emphClose()   override              { out += QStringLiteral("</i>"); }
    // §12.5.6.4 permits <span> with a CSS2 style attribute; monospace is the
    // closest spec-legal rendering of an inline code span.
    void codeOpen()  override { out += QStringLiteral("<span style=\"font-family:Courier\">"); }
    void codeClose() override { out += QStringLiteral("</span>"); }
    void link(const QString& t, const QString& url) override {
        out += QStringLiteral("<a href=\"%1\">%2</a>")
                   .arg(xmlEscape(url), xmlEscape(t));
    }
};

struct PlainInlineSink : InlineSink {
    QString out;
    void text(const QString& s) override { out += s; }
    void strongOpen()  override {}
    void strongClose() override {}
    void emphOpen()    override {}
    void emphClose()   override {}
    void codeOpen()    override {}
    void codeClose()   override {}
    // Plain-text projection of a link keeps the visible text; the URL is
    // dropped (matches how readers show link bodies in a text-only view).
    void link(const QString& t, const QString& /*url*/) override { out += t; }
};

// Try to parse a link [text](url) starting at s[i] (s[i] == '['). On success,
// advances i past the closing ')' and returns true.
bool tryParseLink(const QString& s, int& i, InlineSink& sink)
{
    const int n = s.size();
    int j = i + 1;
    QString text;
    while (j < n && s[j] != ']') {
        if (s[j] == '\\' && j + 1 < n) { text += s[j + 1]; j += 2; continue; }
        text += s[j];
        ++j;
    }
    if (j >= n || s[j] != ']') return false;          // no closing ]
    ++j;
    if (j >= n || s[j] != '(') return false;          // not a link
    ++j;
    QString url;
    while (j < n && s[j] != ')') { url += s[j]; ++j; }
    if (j >= n || s[j] != ')') return false;          // no closing )
    ++j;
    sink.link(text, url);
    i = j;
    return true;
}

// Tokenise one line of inline Djot into the sink.
void emitInlineLine(const QString& line, InlineSink& sink)
{
    const int n = line.size();
    QString run;                  // buffered literal text
    auto flush = [&]() { if (!run.isEmpty()) { sink.text(run); run.clear(); } };

    for (int i = 0; i < n; ) {
        const QChar c = line[i];

        // Backslash escape: next char is literal.
        if (c == '\\' && i + 1 < n) {
            run += line[i + 1];
            i += 2;
            continue;
        }

        // Inline code: `...` (single backtick; literal content).
        if (c == '`') {
            int j = i + 1;
            QString code;
            while (j < n && line[j] != '`') { code += line[j]; ++j; }
            if (j < n) {                 // matched closing backtick
                flush();
                sink.codeOpen();
                sink.text(code);
                sink.codeClose();
                i = j + 1;
                continue;
            }
            // Unmatched — treat as literal.
            run += c; ++i; continue;
        }

        // Strong: ** ... ** (preferred) — check before single '*'.
        if (c == '*' && i + 1 < n && line[i + 1] == '*') {
            int close = line.indexOf(QStringLiteral("**"), i + 2);
            if (close > i + 1) {
                flush();
                sink.strongOpen();
                emitInlineLine(line.mid(i + 2, close - (i + 2)), sink);
                sink.strongClose();
                i = close + 2;
                continue;
            }
            run += c; ++i; continue;
        }

        // Strong: * ... * (single-asterisk form).
        if (c == '*') {
            int close = line.indexOf(QLatin1Char('*'), i + 1);
            if (close > i) {
                flush();
                sink.strongOpen();
                emitInlineLine(line.mid(i + 1, close - (i + 1)), sink);
                sink.strongClose();
                i = close + 1;
                continue;
            }
            run += c; ++i; continue;
        }

        // Emphasis: _ ... _
        if (c == '_') {
            int close = line.indexOf(QLatin1Char('_'), i + 1);
            if (close > i) {
                flush();
                sink.emphOpen();
                emitInlineLine(line.mid(i + 1, close - (i + 1)), sink);
                sink.emphClose();
                i = close + 1;
                continue;
            }
            run += c; ++i; continue;
        }

        // Link: [text](url)
        if (c == '[') {
            flush();
            int saved = i;
            if (tryParseLink(line, i, sink)) continue;
            i = saved;               // not a link — fall through to literal
            run += c; ++i; continue;
        }

        run += c;
        ++i;
    }
    flush();
}

// Strip a leading ATX heading marker ("# ", "## ", ...) from a line, returning
// the heading level (1..6) and the remaining text. Level 0 == not a heading.
int parseHeading(const QString& line, QString& rest)
{
    int level = 0;
    while (level < line.size() && line[level] == QLatin1Char('#')) ++level;
    if (level >= 1 && level <= 6 && level < line.size()
        && line[level] == QLatin1Char(' ')) {
        rest = line.mid(level + 1);
        return level;
    }
    return 0;
}

// True if the line is an unordered list item ("- " or "* " at column 0).
bool isListItem(const QString& line, QString& rest)
{
    if (line.size() >= 2
        && (line[0] == QLatin1Char('-') || line[0] == QLatin1Char('*'))
        && line[1] == QLatin1Char(' ')) {
        rest = line.mid(2);
        return true;
    }
    return false;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QString djotToHtmlFragment(const QString& djot)
{
    const QStringList lines = djot.split(QLatin1Char('\n'));
    QString body = QStringLiteral("<body xmlns=\"http://www.w3.org/1999/xhtml\" "
                                  "xmlns:xfa=\"http://www.xfa.org/schema/xfa-data/1.0/\" "
                                  "xfa:APIVersion=\"GlyphPDF:1.0\" xfa:spec=\"2.0.2\">");
    bool inList = false;

    for (const QString& raw : lines) {
        const QString line = raw;
        QString rest;

        // Blank line: paragraph break (close any open list).
        if (line.trimmed().isEmpty()) {
            if (inList) { body += QStringLiteral("</ul>"); inList = false; }
            continue;
        }

        int hLevel = parseHeading(line, rest);
        if (hLevel > 0) {
            if (inList) { body += QStringLiteral("</ul>"); inList = false; }
            // §12.5.6.4 has no <h1..h6>; emit a bold paragraph as the closest
            // spec-legal heading rendering.
            XhtmlInlineSink sink;
            emitInlineLine(rest, sink);
            body += QStringLiteral("<p style=\"font-weight:bold\">%1</p>").arg(sink.out);
            continue;
        }

        if (isListItem(line, rest)) {
            if (!inList) { body += QStringLiteral("<ul>"); inList = true; }
            XhtmlInlineSink sink;
            emitInlineLine(rest, sink);
            body += QStringLiteral("<li>%1</li>").arg(sink.out);
            continue;
        }

        // Plain paragraph line.
        if (inList) { body += QStringLiteral("</ul>"); inList = false; }
        XhtmlInlineSink sink;
        emitInlineLine(line, sink);
        body += QStringLiteral("<p>%1</p>").arg(sink.out);
    }

    if (inList) body += QStringLiteral("</ul>");
    body += QStringLiteral("</body>");
    return body;
}

QString djotToXhtml(const QString& djot)
{
    // §12.5.6.4 rich-text strings are well-formed XML beginning with an XML
    // declaration; the body element carries the XHTML namespace.
    return QStringLiteral("<?xml version=\"1.0\"?>") + djotToHtmlFragment(djot);
}

QString djotToPlainText(const QString& djot)
{
    const QStringList lines = djot.split(QLatin1Char('\n'));
    QStringList outLines;

    for (const QString& line : lines) {
        QString rest;

        if (line.trimmed().isEmpty()) {
            outLines += QString();         // preserve paragraph breaks
            continue;
        }

        int hLevel = parseHeading(line, rest);
        if (hLevel > 0) {
            PlainInlineSink sink;
            emitInlineLine(rest, sink);
            outLines += sink.out;
            continue;
        }

        if (isListItem(line, rest)) {
            PlainInlineSink sink;
            emitInlineLine(rest, sink);
            // Keep a bullet so the text reads as a list in /Contents.
            outLines += QStringLiteral("• ") + sink.out;
            continue;
        }

        PlainInlineSink sink;
        emitInlineLine(line, sink);
        outLines += sink.out;
    }

    // Collapse trailing blank lines, then join.
    while (!outLines.isEmpty() && outLines.last().isEmpty())
        outLines.removeLast();
    return outLines.join(QLatin1Char('\n'));
}

} // namespace pdfws_djot
