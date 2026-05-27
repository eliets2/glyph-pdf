#include "PdfStringEscape.h"
#include <QByteArray>
#include <iomanip>
#include <sstream>

std::string pdfEscapeLiteralString(const std::string& input) {
    std::string result;
    result.reserve(input.size() + 16);
    
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        
        // Idempotency: if we see '\' followed by a character we escape, treat it as already escaped
        if (c == '\\' && i + 1 < input.size()) {
            char next = input[i + 1];
            if (next == '(' || next == ')' || next == '\\' || next == 'n' || next == 'r' || next == 't' || next == 'b' || next == 'f') {
                result += c;
                result += next;
                ++i;
                continue;
            }
        }
        
        if (c == '(' || c == ')' || c == '\\') {
            result += '\\';
            result += c;
        } else if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126) {
            if (c == '\n') {
                result += "\\n";
            } else if (c == '\r') {
                result += "\\r";
            } else if (c == '\t') {
                result += "\\t";
            } else if (c == '\b') {
                result += "\\b";
            } else if (c == '\f') {
                result += "\\f";
            } else {
                std::ostringstream octal;
                octal << "\\" << std::setfill('0') << std::setw(3) << std::oct << static_cast<unsigned int>(static_cast<unsigned char>(c));
                result += octal.str();
            }
        } else {
            result += c;
        }
    }
    return result;
}

std::string pdfEscapeLiteralString(const QString& input) {
    const QByteArray utf8 = input.toUtf8();
    return pdfEscapeLiteralString(std::string(utf8.constData(), static_cast<size_t>(utf8.size())));
}
