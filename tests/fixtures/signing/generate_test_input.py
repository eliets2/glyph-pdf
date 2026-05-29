"""
generate_test_input.py — generate a minimal but structurally valid PDF-1.4
with correct cross-reference table offsets.

The batch-echo approach in generate.bat produces hardcoded xref offsets that
are only correct if the Windows console uses exactly the line ending and spacing
expected at the time of creation. This Python script generates the file with
byte-accurate xref offsets regardless of the calling environment.
"""
import os
import struct

def make_pdf(path):
    # Build each object body — note: PoDoFo 1.1 is more strict about PDF structure.
    # Use PDF-1.4 header with binary marker to indicate binary content.
    # Include /MediaBox directly to satisfy PoDoFo's page validation.
    catalog = b"1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
    pages   = b"2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
    page    = b"3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>\nendobj\n"

    # PDF header with binary hint (4 bytes with high bits set per PDF spec section 7.5.2)
    header  = b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n"
    body    = catalog + pages + page

    # Compute offsets
    off_catalog = len(header)
    off_pages   = off_catalog + len(catalog)
    off_page    = off_pages   + len(pages)

    xref_pos = len(header) + len(body)

    xref = b"xref\n"
    xref += b"0 4\n"
    xref += b"0000000000 65535 f \n"           # free entry 0
    xref += f"{off_catalog:010d} 00000 n \n".encode()
    xref += f"{off_pages:010d} 00000 n \n".encode()
    xref += f"{off_page:010d} 00000 n \n".encode()

    trailer = (
        b"trailer\n"
        b"<< /Size 4 /Root 1 0 R >>\n"
        b"startxref\n"
        + str(xref_pos).encode()
        + b"\n%%EOF\n"
    )

    pdf_bytes = header + body + xref + trailer
    with open(path, "wb") as f:
        f.write(pdf_bytes)

    print(f"Written {len(pdf_bytes)} bytes to {path}")
    return len(pdf_bytes)

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_input.pdf")
    make_pdf(out)
