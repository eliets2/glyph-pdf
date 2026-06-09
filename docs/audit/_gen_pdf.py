#!/usr/bin/env python3
"""Render all GlyphPDF audit reports into one consolidated PDF (reportlab, no external deps)."""
import os, re, html, datetime
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
                                PageBreak, Preformatted, HRFlowable)
from reportlab.lib.enums import TA_LEFT, TA_CENTER

AUDIT = r"C:\Users\User\Projects\pdf\docs\audit"
OUT   = os.path.join(AUDIT, "GlyphPDF-Audit-Findings-Report.pdf")

SECTIONS = [
    ("Consolidated Audit & Remediation Plan", os.path.join(AUDIT, "AUDIT-2026-06-02.md")),
    ("Round-2 Verification Summary",          os.path.join(AUDIT, "round2", "SUMMARY.md")),
    ("Workstream A - Security: Crypto / Redaction / Parser", os.path.join(AUDIT, "_workstreams", "A-security-crypto-pdf.md")),
    ("Workstream B - Supply Chain / Secrets / Build",        os.path.join(AUDIT, "_workstreams", "B-supplychain-secrets-build.md")),
    ("Workstream C - Claims vs Reality",                     os.path.join(AUDIT, "_workstreams", "C-claims-vs-reality.md")),
    ("Workstream D - Architecture & Code Quality",           os.path.join(AUDIT, "_workstreams", "D-architecture-quality.md")),
    ("Workstream E - Silent Failures / Error Handling",      os.path.join(AUDIT, "_workstreams", "E-silent-failures.md")),
    ("Workstream F - Performance & Concurrency",             os.path.join(AUDIT, "_workstreams", "F-performance-concurrency.md")),
    ("Workstream G - Tests & Release Readiness",             os.path.join(AUDIT, "_workstreams", "G-tests-release-readiness.md")),
    ("Round-2 - Security Re-verification", os.path.join(AUDIT, "round2", "SEC-reverify.md")),
    ("Round-2 - UX Audit",                 os.path.join(AUDIT, "round2", "UX.md")),
    ("Round-2 - UI Audit",                 os.path.join(AUDIT, "round2", "UI.md")),
    ("Round-2 - Code Completeness",        os.path.join(AUDIT, "round2", "CODE.md")),
    ("Round-2 - Repository Audit",         os.path.join(AUDIT, "round2", "REPO.md")),
    ("Round-2 - Vault Audit",              os.path.join(AUDIT, "round2", "VAULT.md")),
]

EMOJI = {"✅":"[OK] ","❌":"[X] ","⚠":"[!] ","️":"","\U0001F534":"[CRIT] ",
 "\U0001F7E0":"[HIGH] ","\U0001F7E1":"[MED] ","\U0001F7E2":"[LOW] ","⛔":"[BLOCK] ",
 "☑":"[x] ","☐":"[ ] ","✓":"[x]","✗":"[ ]","→":"->","←":"<-",
 "↔":"<->","↤":"<-","‖":"||","·":" - ","…":"...","—":" - ",
 "–":"-","‘":"'","’":"'","“":'"',"”":'"',"×":"x","≡":"=",
 "§":"section ","↑":"^","↓":"v"}
def deemoji(s):
    for k,v in EMOJI.items(): s = s.replace(k,v)
    return s.encode("latin-1","ignore").decode("latin-1")

def inline(t):
    t = deemoji(t)
    t = html.escape(t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"__(.+?)__", r"<b>\1</b>", t)
    t = re.sub(r"`([^`]+?)`", r'<font face="Courier" size="8">\1</font>', t)
    t = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", t)
    t = re.sub(r"(?<![\*\w])\*([^*]+?)\*(?![\*\w])", r"<i>\1</i>", t)
    return t

ss = getSampleStyleSheet()
P  = ParagraphStyle("body", parent=ss["BodyText"], fontSize=8.5, leading=11, spaceAfter=3)
H1 = ParagraphStyle("h1", parent=ss["Heading1"], fontSize=15, spaceBefore=8, spaceAfter=5, textColor=colors.HexColor("#1a2a4a"))
H2 = ParagraphStyle("h2", parent=ss["Heading2"], fontSize=12, spaceBefore=7, spaceAfter=4, textColor=colors.HexColor("#23406e"))
H3 = ParagraphStyle("h3", parent=ss["Heading3"], fontSize=10, spaceBefore=5, spaceAfter=3, textColor=colors.HexColor("#2e5090"))
H4 = ParagraphStyle("h4", parent=ss["Heading4"], fontSize=9, spaceBefore=4, spaceAfter=2, textColor=colors.HexColor("#444"))
CELL = ParagraphStyle("cell", parent=P, fontSize=7, leading=8.5, spaceAfter=0)
CELLH= ParagraphStyle("cellh", parent=CELL, textColor=colors.white, fontName="Helvetica-Bold")
BQ = ParagraphStyle("bq", parent=P, leftIndent=8, textColor=colors.HexColor("#555"), fontName="Helvetica-Oblique")
CODE = ParagraphStyle("code", parent=P, fontName="Courier", fontSize=7, leading=8.5, backColor=colors.HexColor("#f2f2f2"), borderPadding=3)

def make_table(rows):
    rows = [r for r in rows if not (len(r)>0 and all(re.fullmatch(r"\s*:?-{2,}:?\s*", c or "") for c in r))]
    if not rows: return None
    ncol = max(len(r) for r in rows)
    data = []
    for ri, r in enumerate(rows):
        r = (r + [""]*ncol)[:ncol]
        st = CELLH if ri == 0 else CELL
        data.append([Paragraph(inline(c), st) for c in r])
    total = 180*mm
    w = [total/ncol]*ncol
    t = Table(data, colWidths=w, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND",(0,0),(-1,0),colors.HexColor("#23406e")),
        ("GRID",(0,0),(-1,-1),0.4,colors.HexColor("#bcbcbc")),
        ("ROWBACKGROUNDS",(0,1),(-1,-1),[colors.white,colors.HexColor("#f4f6fa")]),
        ("VALIGN",(0,0),(-1,-1),"TOP"),
        ("LEFTPADDING",(0,0),(-1,-1),3),("RIGHTPADDING",(0,0),(-1,-1),3),
        ("TOPPADDING",(0,0),(-1,-1),2),("BOTTOMPADDING",(0,0),(-1,-1),2),
    ]))
    return t

def render(md, story):
    lines = md.split("\n")
    i = 0
    while i < len(lines):
        ln = lines[i]
        s = ln.strip()
        if not s:
            i += 1; continue
        # code fence
        if s.startswith("```"):
            buf=[]; i+=1
            while i < len(lines) and not lines[i].strip().startswith("```"):
                buf.append(lines[i]); i+=1
            i+=1
            txt = deemoji("\n".join(buf)) or " "
            try: story.append(Preformatted(txt, CODE))
            except Exception: story.append(Paragraph(html.escape(txt).replace("\n","<br/>"), CODE))
            story.append(Spacer(1,3)); continue
        # table
        if s.startswith("|") and s.endswith("|") and i+1 < len(lines) and re.search(r"\|\s*:?-{2,}", lines[i+1]):
            block=[]
            while i < len(lines) and lines[i].strip().startswith("|"):
                block.append([c.strip() for c in lines[i].strip().strip("|").split("|")]); i+=1
            try:
                t = make_table(block)
                if t: story.append(t); story.append(Spacer(1,5))
            except Exception:
                for r in block: story.append(Paragraph(inline(" | ".join(r)), P))
            continue
        # headings
        m = re.match(r"(#{1,6})\s+(.*)", s)
        if m:
            lvl=len(m.group(1)); txt=inline(m.group(2))
            story.append(Paragraph(txt, {1:H1,2:H2,3:H3}.get(lvl,H4))); i+=1; continue
        # hr
        if re.fullmatch(r"(-{3,}|\*{3,}|_{3,})", s):
            story.append(HRFlowable(width="100%", thickness=0.6, color=colors.HexColor("#aaaaaa"), spaceBefore=4, spaceAfter=4)); i+=1; continue
        # blockquote
        if s.startswith(">"):
            story.append(Paragraph(inline(s.lstrip("> ").strip()), BQ)); i+=1; continue
        # bullet / numbered
        mb = re.match(r"[-*+]\s+(.*)", s); mn = re.match(r"(\d+)[.)]\s+(.*)", s)
        if mb:
            story.append(Paragraph("&bull;&nbsp; "+inline(mb.group(1)), ParagraphStyle("li",parent=P,leftIndent=10))); i+=1; continue
        if mn:
            story.append(Paragraph(mn.group(1)+".&nbsp; "+inline(mn.group(2)), ParagraphStyle("ol",parent=P,leftIndent=10))); i+=1; continue
        story.append(Paragraph(inline(s), P)); i+=1

def footer(canvas, doc):
    canvas.saveState(); canvas.setFont("Helvetica",7); canvas.setFillColor(colors.grey)
    canvas.drawString(15*mm, 8*mm, "GlyphPDF Audit Findings - CONFIDENTIAL")
    canvas.drawRightString(195*mm, 8*mm, "Page %d" % doc.page)
    canvas.restoreState()

def build():
    story=[]
    story.append(Spacer(1,60))
    story.append(Paragraph("GlyphPDF", ParagraphStyle("t",parent=ss["Title"],fontSize=34,textColor=colors.HexColor("#1a2a4a"))))
    story.append(Paragraph("Complete Security &amp; Quality Audit - All Findings", ParagraphStyle("st",parent=ss["Title"],fontSize=15,textColor=colors.HexColor("#23406e"))))
    story.append(Spacer(1,16))
    meta = ("Generated: %s  &middot;  Branch: audit-remediation  &middot;  ~150 findings across 13 workstreams<br/>"
            "Two-round audit (original 7-workstream swarm + round-2 verification) + remediation status.<br/>"
            "Severity legend: CRIT = Critical, HIGH = High, MED = Medium, LOW = Low/Info.") % datetime.date.today().isoformat()
    story.append(Paragraph(meta, ParagraphStyle("m",parent=P,alignment=TA_CENTER,fontSize=9,textColor=colors.HexColor("#555"))))
    story.append(PageBreak())
    # TOC
    story.append(Paragraph("Contents", H1))
    for n,(title,_) in enumerate(SECTIONS,1):
        story.append(Paragraph("%d.&nbsp;&nbsp;%s" % (n, deemoji(title)), ParagraphStyle("toc",parent=P,leftIndent=6,spaceAfter=2)))
    story.append(PageBreak())
    for n,(title,path) in enumerate(SECTIONS,1):
        story.append(Paragraph("%d. %s" % (n, deemoji(title)), H1))
        story.append(HRFlowable(width="100%", thickness=1, color=colors.HexColor("#23406e"), spaceAfter=6))
        try:
            with open(path, encoding="utf-8") as f: md=f.read()
            render(md, story)
        except FileNotFoundError:
            story.append(Paragraph("(source file not found: %s)" % path, P))
        except Exception as e:
            story.append(Paragraph("(error rendering %s: %s)" % (os.path.basename(path), html.escape(str(e))), P))
        story.append(PageBreak())
    doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=15*mm, rightMargin=15*mm, topMargin=15*mm, bottomMargin=14*mm,
                            title="GlyphPDF Audit Findings Report", author="Claude audit swarm")
    doc.build(story, onFirstPage=footer, onLaterPages=footer)
    print("WROTE", OUT, os.path.getsize(OUT), "bytes")

if __name__ == "__main__":
    build()
