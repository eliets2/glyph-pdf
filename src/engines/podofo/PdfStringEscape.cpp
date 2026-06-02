// SPDX-License-Identifier: Apache-2.0
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

std::string pdfUnescapeLiteralString(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c != '\\') {
            result += c;
            continue;
        }
        if (i + 1 >= input.size()) {        // trailing backslash — keep literal
            result += c;
            break;
        }
        char next = input[i + 1];
        switch (next) {
        case '(': result += '(';  i += 1; break;
        case ')': result += ')';  i += 1; break;
        case '\\':result += '\\'; i += 1; break;
        case 'n': result += '\n'; i += 1; break;
        case 'r': result += '\r'; i += 1; break;
        case 't': result += '\t'; i += 1; break;
        case 'b': result += '\b'; i += 1; break;
        case 'f': result += '\f'; i += 1; break;
        default:
            if (next >= '0' && next <= '7') {
                // Octal escape: up to 3 octal digits.
                int value = 0, digits = 0;
                size_t j = i + 1;
                while (j < input.size() && digits < 3
                       && input[j] >= '0' && input[j] <= '7') {
                    value = value * 8 + (input[j] - '0');
                    ++j;
                    ++digits;
                }
                result += static_cast<char>(value & 0xFF);
                i = j - 1;
            } else {
                // Unknown escape — drop the backslash, keep the char (matches
                // PDF reader leniency).
                result += next;
                i += 1;
            }
            break;
        }
    }
    return result;
}
