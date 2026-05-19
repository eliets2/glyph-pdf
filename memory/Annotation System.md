# Annotation System

## AnnotationItem Struct

Defined in `core/AnnotationTypes.h`:

```
AnnotationItem {
    int pageIndex         // target page
    ToolMode mode         // drawing mode used to create
    QList<QPointF> points // path/shape points
    QColor color          // stroke/fill color
    int thickness         // line width in px
    QString text          // text content (for text boxes, comments)
    QRectF rect           // bounding rectangle
}
```

## AnnotationLayer

**File:** `ui/AnnotationLayer.h/.cpp`

Transparent `QWidget` overlay on top of `QPdfView`. Handles:
- Mouse-based drawing (press/move/release)
- Shape rendering in `paintEvent`
- Selection and hit-testing of existing annotations
- Image overlay rendering and manipulation (move, resize via corner/edge handles)
- OCR result display
- Page-at-point callback for multi-page coordinate mapping

### Signals
- `annotationsChanged()` - annotation list modified
- `selectionChanged(int index)` - new annotation selected
- `textEditRequested(int pageIndex, QPointF pos)` - user double-clicked for text edit
- `imageSelected(QString xobjectName, QRectF placement)` - embedded image selected
- `imageMoved(QString xobjectName, double dx, double dy)` - image dragged
- `imageResized(QString xobjectName, double newW, double newH)` - image handle dragged

## Related Commands

- `EditAnnotationCommand` - Undo/redo for annotation property changes (uses `QPointer<PdfViewerWidget>` for safety)
- `RedactCommand` - Undo/redo for redaction regions (uses `QPointer<PdfViewerWidget>`)
- Image commands: `MoveImageCommand`, `ResizeImageCommand`, `RotateImageCommand`, `ReplaceImageCommand`, `DeleteImageCommand`

## PdfViewerWidget Integration

`PdfViewerWidget` owns the `AnnotationLayer` and provides:
- `saveAnnotations()` / `loadAnnotations()` - JSON persistence
- `setAnnotations()` / `annotations()` - bulk get/set
- `deleteSelectedAnnotation()`
- `searchDocument()` / `redactAllMatches()` - text search + redaction
- `setOcrResults()` - overlay OCR bounding boxes
- Save debounce via `m_saveDebounceTimer`
- Render cache: `m_pageCache` (mutable, max 20 pages)
