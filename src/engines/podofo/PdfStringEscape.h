#pragma once

#include <string>
#include <QString>

std::string pdfEscapeLiteralString(const std::string& input);
std::string pdfEscapeLiteralString(const QString& input);

// Exact inverse of pdfEscapeLiteralString. Reverses \( \) \\ \n \r \t \b \f and
// octal \ooo escapes back to raw bytes, so escape→unescape is lossless for
// arbitrary UTF-8 content (e.g. the /PieceInfo Djot sidecar, M6-P4 D4).
std::string pdfUnescapeLiteralString(const std::string& input);
