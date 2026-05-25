from __future__ import annotations

import io
import sys
from pathlib import Path

import cv2
import fitz
import numpy as np
from PIL import Image


def render_page(doc: fitz.Document, page_index: int, dpi: int) -> np.ndarray:
    zoom = dpi / 72
    pix = doc[page_index].get_pixmap(matrix=fitz.Matrix(zoom, zoom), alpha=False)
    arr = np.frombuffer(pix.samples, dtype=np.uint8).reshape(pix.height, pix.width, 3)
    return arr.copy()


def remove_colored_marks(rgb: np.ndarray) -> np.ndarray:
    img = rgb.copy()
    r, g, b = img[:, :, 0], img[:, :, 1], img[:, :, 2]
    hsv = cv2.cvtColor(img, cv2.COLOR_RGB2HSV)
    h, s, v = hsv[:, :, 0], hsv[:, :, 1], hsv[:, :, 2]

    red_like = (
        (((h <= 14) | (h >= 160)))
        & (s > 22)
        & (v > 70)
        & (r.astype(np.int16) > g.astype(np.int16) + 8)
    )
    pink_faint = (
        (r > 135)
        & (r.astype(np.int16) > g.astype(np.int16) + 5)
        & (r.astype(np.int16) > b.astype(np.int16) + 8)
        & (s > 12)
    )
    saturated_pen_or_stamp = (s > 70) & (v > 65)

    mask = (red_like | pink_faint | saturated_pen_or_stamp).astype(np.uint8) * 255
    kernel = np.ones((3, 3), np.uint8)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel, iterations=1)
    mask = cv2.dilate(mask, kernel, iterations=1)
    img[mask > 0] = 255
    return img


def estimate_skew(binary: np.ndarray) -> float:
    h, w = binary.shape
    inv = 255 - binary
    inv[: int(h * 0.04), :] = 0
    inv[int(h * 0.96) :, :] = 0
    inv[:, : int(w * 0.04)] = 0
    inv[:, int(w * 0.96) :] = 0

    coords = np.column_stack(np.where(inv > 0))
    if len(coords) < 1500:
        return 0.0

    rect = cv2.minAreaRect(coords)
    angle = rect[-1]
    if angle < -45:
        angle = 90 + angle
    elif angle > 45:
        angle = angle - 90
    if abs(angle) > 4.0:
        return 0.0
    return float(angle)


def rotate_keep_white(img: np.ndarray, angle: float) -> np.ndarray:
    if abs(angle) < 0.08:
        return img
    h, w = img.shape[:2]
    matrix = cv2.getRotationMatrix2D((w / 2, h / 2), angle, 1.0)
    return cv2.warpAffine(
        img,
        matrix,
        (w, h),
        flags=cv2.INTER_CUBIC,
        borderMode=cv2.BORDER_CONSTANT,
        borderValue=255,
    )


def remove_edge_artifacts(binary: np.ndarray) -> np.ndarray:
    h, w = binary.shape
    inv = 255 - binary
    num, labels, stats, _ = cv2.connectedComponentsWithStats(inv, 8)
    remove = np.zeros_like(inv)
    edge_x = int(w * 0.045)
    edge_y = int(h * 0.025)

    for lab in range(1, num):
        x, y, cw, ch, area = stats[lab]
        touches_side = x <= edge_x or x + cw >= w - edge_x
        touches_top_bottom = y <= edge_y or y + ch >= h - edge_y
        long_artifact = ch > h * 0.045 or cw > w * 0.045
        narrow_edge = (touches_side and cw < w * 0.12) or (touches_top_bottom and ch < h * 0.08)
        if (touches_side or touches_top_bottom) and long_artifact and narrow_edge:
            remove[labels == lab] = 255

    cleaned = binary.copy()
    cleaned[remove > 0] = 255
    return cleaned


def remove_separable_hand_marks(binary: np.ndarray) -> np.ndarray:
    """Remove large diagonal marks only when they are separable components.

    This intentionally avoids horizontal/vertical components because those are
    often tables, underlines, or printed rules.
    """
    inv = 255 - binary
    num, labels, stats, _ = cv2.connectedComponentsWithStats(inv, 8)
    h, w = binary.shape
    remove = np.zeros_like(inv)
    min_span = min(h, w) * 0.11
    min_area = max(350, int(binary.size * 0.00008))

    for lab in range(1, num):
        x, y, cw, ch, area = stats[lab]
        if area < min_area or max(cw, ch) < min_span or min(cw, ch) < 10:
            continue
        if cw > w * 0.75 and ch < h * 0.08:
            continue
        if ch > h * 0.75 and cw < w * 0.08:
            continue

        pts = np.column_stack(np.where(labels == lab))
        if len(pts) < 20:
            continue
        rect = cv2.minAreaRect(pts)
        angle = rect[-1]
        if angle < -45:
            angle = 90 + angle
        elif angle > 45:
            angle = angle - 90

        fill_ratio = area / max(1, cw * ch)
        is_diagonal = 12 <= abs(angle) <= 42
        is_sparse = fill_ratio < 0.38
        if is_diagonal and is_sparse:
            remove[labels == lab] = 255

    cleaned = binary.copy()
    cleaned[remove > 0] = 255
    return cleaned


def clean_page(rgb: np.ndarray) -> tuple[np.ndarray, bool, float, float]:
    no_color = remove_colored_marks(rgb)
    gray = cv2.cvtColor(no_color, cv2.COLOR_RGB2GRAY)

    bg = cv2.medianBlur(gray, 51)
    flat = cv2.divide(gray, bg, scale=255)
    flat = cv2.GaussianBlur(flat, (3, 3), 0)

    _, otsu = cv2.threshold(flat, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    adapt = cv2.adaptiveThreshold(
        flat, 255, cv2.ADAPTIVE_THRESH_GAUSSIAN_C, cv2.THRESH_BINARY, 41, 14
    )
    binary = cv2.min(otsu, adapt)
    binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, np.ones((2, 2), np.uint8), iterations=1)

    inv = 255 - binary
    num, labels, stats, _ = cv2.connectedComponentsWithStats(inv, 8)
    kept = np.zeros_like(inv)
    area_min = max(3, int(binary.size * 0.0000008))
    for lab in range(1, num):
        x, y, cw, ch, area = stats[lab]
        if area >= area_min or (ch >= 3 and cw >= 1):
            kept[labels == lab] = 255
    binary = 255 - kept

    binary = remove_edge_artifacts(binary)
    binary = remove_separable_hand_marks(binary)

    angle = estimate_skew(binary)
    binary = rotate_keep_white(binary, -angle)
    binary = remove_edge_artifacts(binary)

    inv = 255 - binary
    h, w = binary.shape
    inv[: int(h * 0.02), :] = 0
    inv[int(h * 0.98) :, :] = 0
    inv[:, : int(w * 0.02)] = 0
    inv[:, int(w * 0.98) :] = 0
    black_ratio = float(np.count_nonzero(inv)) / inv.size
    blank = black_ratio < 0.0035
    return binary, blank, black_ratio, angle


def save_png(binary: np.ndarray, path: Path, dpi: int) -> None:
    image = Image.fromarray(binary).convert("1")
    image.save(path, dpi=(dpi, dpi), optimize=True)


def insert_png_page(pdf_out: fitz.Document, binary: np.ndarray, source_rect: fitz.Rect, dpi: int) -> None:
    image = Image.fromarray(binary).convert("1")
    data = io.BytesIO()
    image.save(data, format="PNG", dpi=(dpi, dpi), optimize=True)
    page = pdf_out.new_page(width=source_rect.width, height=source_rect.height)
    page.insert_image(page.rect, stream=data.getvalue(), keep_proportion=False)


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: clean_scanned_pdf.py input.pdf output_dir output.pdf", file=sys.stderr)
        return 2

    input_pdf = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    output_pdf = Path(sys.argv[3])
    png_dir = output_dir / "png"
    png_dir.mkdir(parents=True, exist_ok=True)

    for old in png_dir.glob("page_*_cleaned.png"):
        old.unlink()
    if output_pdf.exists():
        output_pdf.unlink()

    dpi = 250
    doc = fitz.open(input_pdf)
    cleaned_pdf = fitz.open()
    skipped: list[int] = []
    written: list[int] = []

    total = max(0, doc.page_count - 1)
    print(f"input={input_pdf}")
    print(f"pages={doc.page_count}; processing page 2 through {doc.page_count}; dpi={dpi}")
    print(f"output_dir={output_dir}")
    print("", flush=True)

    for page_index in range(1, doc.page_count):
        page_number = page_index + 1
        rgb = render_page(doc, page_index, dpi)
        binary, blank, black_ratio, angle = clean_page(rgb)

        if blank:
            skipped.append(page_number)
            status = "skip_blank"
        else:
            png_path = png_dir / f"page_{page_number:03d}_cleaned.png"
            save_png(binary, png_path, dpi)
            insert_png_page(cleaned_pdf, binary, doc[page_index].rect, dpi)
            written.append(page_number)
            status = "written"

        if page_number == 2 or page_number == doc.page_count or page_number % 10 == 0:
            done = page_number - 1
            print(
                f"{done:03d}/{total:03d} page={page_number:03d} {status} "
                f"ink={black_ratio:.4f} skew={angle:+.2f}",
                flush=True,
            )

    cleaned_pdf.save(output_pdf, garbage=4, deflate=True, clean=True)
    cleaned_pdf.close()
    doc.close()

    report_path = output_dir / "cleanup_report.txt"
    report_path.write_text(
        "\n".join(
            [
                f"Input PDF: {input_pdf}",
                "Skipped first page: 1",
                f"Cleaned non-blank pages written: {len(written)}",
                f"Blank pages skipped after cleanup: {len(skipped)}",
                f"Skipped blank page numbers: {', '.join(map(str, skipped)) if skipped else 'none'}",
                f"PNG directory: {png_dir}",
                f"Cleaned PDF: {output_pdf}",
                "",
            ]
        ),
        encoding="utf-8",
    )

    print("")
    print(f"done: wrote {len(written)} PNG files")
    print(f"blank pages skipped: {len(skipped)}")
    print(f"pdf={output_pdf}")
    print(f"report={report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
