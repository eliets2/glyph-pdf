// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

namespace pdfws_djot {

// ---------------------------------------------------------------------------
// DjotToRichTextXhtml — annotation rich-text transcoder (M6-PROMPT-4 D3).
//
// PURPOSE
//   PDF annotations carry rich text in the /RC entry, which ISO 32000-1
//   §12.5.6.4 ("Rich text strings") restricts to a small XHTML 1.0 + CSS2
//   subset wrapped in <body xmlns=...>. Storing raw Djot in /RC violates the
//   spec and breaks Acrobat/Foxit interop (see non-negotiables: "Raw Djot in
//   PDF /RC NEVER"). This helper transcodes the *annotation-subset* of Djot
//   into that XHTML subset, and also produces the plain-text projection used
//   for the spec-required /Contents fallback.
//
// SCOPE — this is NOT a Djot grammar reimplementation
//   The full-document Djot interchange path uses the vendored Lua 5.4 reference
//   parser (pdfws::LuaDjotCodec) — see non-negotiable "Djot grammar
//   reimplementation NEVER". This helper deliberately handles only the bounded
//   inline + simple-block subset that the InspectorWidget rich-text toolbar can
//   author (D2): strong (*..* / **..**), emphasis (_.._), inline code (`..`),
//   links ([text](url)), unordered list items (- ..), and ATX headings (# ..).
//   That subset maps 1:1 to the XHTML elements §12.5.6.4 permits, so a small
//   dedicated transcoder is the correct tool here — not the document parser.
//
//   Anything outside the subset degrades gracefully to its literal text (the
//   markup characters are emitted verbatim / HTML-escaped), never crashing and
//   never silently dropping content.
// ---------------------------------------------------------------------------

// Convert annotation-subset Djot into the §12.5.6.4 XHTML rich-text string
// (full <?xml ...?><body xmlns=...>...</body> document) suitable for /RC.
QString djotToXhtml(const QString& djot);

// Convert annotation-subset Djot into a plain-text projection: all inline and
// block markup is stripped, leaving readable text. Used for the PDF /Contents
// fallback and for AnnotationItem::text (so non-Djot-aware readers and the
// rest of the app always have a faithful plain-text view).
QString djotToPlainText(const QString& djot);

// Produce just the <body>...</body> fragment (no XML prolog). Exposed for the
// live HTML preview pane in InspectorWidget, which renders into a QTextEdit
// (Qt rich text understands the fragment directly).
QString djotToHtmlFragment(const QString& djot);

} // namespace pdfws_djot
