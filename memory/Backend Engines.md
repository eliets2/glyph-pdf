# Backend Engines

All engines implement interfaces from `core/interfaces/` and are instantiated in `main.cpp`.

## PdfEditorEngine

**Interface:** `IPdfEditorEngine` | **Backend:** PoDoFo | **File:** `engines/PdfEditorEngine.cpp`

Core operations:
- `loadDocumentForEditing` / `saveDocument` - Document I/O via PoDoFo
- `editTextInline` - In-place text editing on a page
- `deleteObjectAt` - Remove object at coordinates
- `applyRedactions` - Black-box redact regions
- `linearizeDocument` - Web-optimized PDF (requires `HAS_QPDF`)
- `exportPdfA` - PDF/A-1b, 2b, 3b conformance export
- `encryptDocument` - AES-256 encryption with permission flags
- `sanitizeDocument` - Strip metadata, XMP, embedded files, JavaScript, auto-actions
- `getMetadata` / `setMetadata` - Document info dictionary
- Page operations: `rotatePage`, `deletePage`, `insertBlankPage`, `extractPageAsBytes`, `insertPageFromBytes`
- Image operations: `listImages`, `moveImage`, `resizeImage`, `rotateImage`, `replaceImage`, `deleteImage`

Implementation uses PoDoFo's `PdfMemDocument` internally via pimpl (`Private` struct).

## OcrEngine

**Interface:** `IOcrEngine` | **Backend:** Tesseract + Leptonica | **File:** `engines/OcrEngine.cpp`

- `initialize(language, dataPath)` - Init Tesseract API
- `processImage(image)` - Returns `QList<OcrResult>` with bounding boxes
- `getRawText(image)` - Plain text extraction
- All wrapped with `#ifdef HAS_TESSERACT` guards; returns empty/false without it

## FormManager

**Interface:** `IFormManager` | **Backend:** PoDoFo | **File:** `engines/FormManager.cpp`

- `extractFormFields(filePath)` - Parse AcroForm fields
- `fillFormFields(filePath, outputPath, fieldValues)` - Populate form data

## SignatureManager

**Interface:** `ISignatureManager` | **Backend:** OpenSSL + PoDoFo | **File:** `engines/SignatureManager.cpp`

- `signDocument(input, output, certPath, password, reason, location)` - PKCS#12 digital signing
- `validateSignatures(filePath)` - Returns `QList<SignatureInfo>` with validity/trust status

## ConversionManager

**Interface:** `IConversionEngine` | **File:** `engines/ConversionManager.cpp`

- `convertTo(pdfPath, outputPath, format)` - PDF to Word/Excel
- Word export requires `HAS_DUCKX`, Excel requires `HAS_OPENXLSX`

## CollaborationManager

**Interface:** `ICollaboration` | **File:** `engines/CollaborationManager.cpp`

- Real-time collaboration support (WebSocket-based)

## DocumentSession

**File:** `engines/DocumentSession.h/.cpp` | **Not interface-based**

- Tracks current document path, dirty state, reload signals
- Used by commands to know which file is being operated on
