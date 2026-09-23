from __future__ import annotations

import io
import os
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
    # G02: the format is stated explicitly so the path may be a uniquely
    # named staging file whose "extension" is the run token.
    image.save(path, format="PNG", dpi=(dpi, dpi), optimize=True)


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

    import tempfile
    import uuid

    input_pdf = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    output_pdf = Path(sys.argv[3])

    # G02 (QUALITY-GATE-2026-09-09): an identity check against EVERY path this
    # run writes — the output PDF, the (now unique) candidate, the per-run PNG
    # staging area and the report. The old fixed candidate name
    # `output.pdf.cleaning-tmp.pdf` could BE the input (the alias check only
    # compared input vs output) and was then unlinked on failure, destroying
    # the source; a pre-existing unrelated candidate was likewise deleted.
    def same_path(a: Path, b: Path) -> bool:
        try:
            return a.resolve() == b.resolve()
        except OSError:
            return a == b

    report_pdf = output_dir / "cleanup_report.txt"
    png_dir_early = output_dir / "png"
    for label, other in (
        ("output", output_pdf),
        ("report", report_pdf),
        ("png dir", png_dir_early),
    ):
        if same_path(input_pdf, other):
            print(
                f"refusing: input and {label} are the same path "
                "(same-path cleanup is not supported)",
                file=sys.stderr,
            )
            return 2

    # INF01: open/validate the input FIRST. A missing or corrupt input must
    # fail without touching any pre-existing output or artifacts.
    if not input_pdf.is_file():
        print(f"input not found: {input_pdf}", file=sys.stderr)
        return 1
    try:
        doc = fitz.open(input_pdf)
    except Exception as exc:  # corrupt / unreadable input
        print(f"cannot open input {input_pdf}: {exc}", file=sys.stderr)
        return 1

    png_dir = output_dir / "png"
    png_dir.mkdir(parents=True, exist_ok=True)

    # INF01: snapshot pre-existing artifacts instead of deleting them up
    # front — stale ones are cleared only after a successful commit, so a
    # failure never destroys the previous run's artifacts.
    preexisting_pngs = sorted(png_dir.glob("page_*_cleaned.png"))

    # G02: uniquely OWNED candidate — mkstemp creates it exclusively in the
    # output's directory (same volume, so the final commit is a plain atomic
    # os.replace). The run owns this exact path and may only ever unlink THIS
    # file, never a pre-existing user file at some fixed name. The same run
    # token namespaces the PNG staging files.
    run_token = uuid.uuid4().hex[:12]
    try:
        fd, candidate_name = tempfile.mkstemp(
            prefix=output_pdf.name + ".cleaning-tmp-", suffix=".pdf",
            dir=str(output_pdf.parent),
        )
        os.close(fd)
    except OSError as exc:
        print(f"cannot create candidate next to {output_pdf}: {exc}", file=sys.stderr)
        doc.close()
        return 1
    candidate = Path(candidate_name)
    # Staged PNGs this run created: (staged path, final path). Only these are
    # ever removed on failure; the final artifact names are touched only by
    # the explicit success policy below.
    staged_pngs: list[tuple[Path, Path]] = []
    rewritten: list[int] = []

    def fail_cleanup() -> None:
        candidate.unlink(missing_ok=True)
        for staged, _final in staged_pngs:
            staged.unlink(missing_ok=True)

    dpi = 250
    cleaned_pdf = fitz.open()
    skipped: list[int] = []

    fail_after_env = os.environ.get("CLEAN_SCANNED_PDF_FAIL_AFTER_PAGE")
    fail_after = int(fail_after_env) if fail_after_env else None

    total = max(0, doc.page_count - 1)
    print(f"input={input_pdf}")
    print(f"pages={doc.page_count}; processing page 2 through {doc.page_count}; dpi={dpi}")
    print(f"output_dir={output_dir}")
    print("", flush=True)

    processed = 0
    try:
        for page_index in range(1, doc.page_count):
            page_number = page_index + 1
            rgb = render_page(doc, page_index, dpi)
            binary, blank, black_ratio, angle = clean_page(rgb)

            if blank:
                skipped.append(page_number)
                status = "skip_blank"
            else:
                png_path = png_dir / f"page_{page_number:03d}_cleaned.png"
                # G02: the identity guarantee covers every artifact path —
                # a document named like one of this run's outputs is refused
                # instead of being overwritten by its own cleanup.
                if same_path(input_pdf, png_path) or same_path(input_pdf, candidate):
                    raise RuntimeError(
                        f"input aliases an output artifact path: {png_path}"
                    )
                staged_path = png_dir / f"{png_path.name}.cleaning-tmp-{run_token}"
                save_png(binary, staged_path, dpi)
                staged_pngs.append((staged_path, png_path))
                insert_png_page(cleaned_pdf, binary, doc[page_index].rect, dpi)
                rewritten.append(page_number)
                status = "written"

            if page_number == 2 or page_number == doc.page_count or page_number % 10 == 0:
                done = page_number - 1
                print(
                    f"{done:03d}/{total:03d} page={page_number:03d} {status} "
                    f"ink={black_ratio:.4f} skew={angle:+.2f}",
                    flush=True,
                )

            # Test seam (INF01 acceptance: processing failure). Deterministic
            # mid-run abort after N processed pages; not part of the cleanup
            # algorithm.
            processed += 1
            if fail_after is not None and processed >= fail_after:
                raise RuntimeError(
                    f"injected processing failure after {fail_after} page(s)"
                )

        cleaned_pdf.save(candidate, garbage=4, deflate=True, clean=True)
    except BaseException:
        cleaned_pdf.close()
        doc.close()
        # G02: only paths THIS run created are removed — the unique candidate
        # and the staged PNGs. Pre-existing outputs and artifacts survive with
        # their original bytes.
        fail_cleanup()
        raise
    cleaned_pdf.close()
    doc.close()

    # INF01: validate the closed candidate, then replace the output
    # atomically. Any failure up to here leaves the previous output intact.
    try:
        check = fitz.open(candidate)
        candidate_ok = check.page_count == len(rewritten)
        check.close()
    except Exception:
        candidate_ok = False
    if not candidate_ok:
        fail_cleanup()
        print(
            "candidate output failed validation; previous output preserved",
            file=sys.stderr,
        )
        return 1
    os.replace(candidate, output_pdf)

    # G02 explicit success policy: the PDF commit succeeded, so the staged
    # PNGs become the artifacts now (each rename is a same-directory atomic
    # replace), and only after that are stale pre-existing artifacts this run
    # did not rewrite cleared.
    for staged, final in staged_pngs:
        os.replace(staged, final)
    final_names = {final.name for _staged, final in staged_pngs}
    for old in preexisting_pngs:
        if old.name not in final_names:
            old.unlink(missing_ok=True)

    report_path = report_pdf
    report_path.write_text(
        "\n".join(
            [
                f"Input PDF: {input_pdf}",
                "Skipped first page: 1",
                f"Cleaned non-blank pages written: {len(rewritten)}",
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
    print(f"done: wrote {len(rewritten)} PNG files")
    print(f"blank pages skipped: {len(skipped)}")
    print(f"pdf={output_pdf}")
    print(f"report={report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
