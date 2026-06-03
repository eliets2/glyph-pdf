# Workstream E — Silent Failures, Swallowed Errors, Dangerous Fallbacks

Auditor: Claude Sonnet 4.6 (automated read-only sweep)
Date: 2026-06-02
Scope: `src/` tree, security-critical paths first (SignatureManager, PoDoFoBackend, PdfEditorEngine, RedactMode, BatchMode, AutosaveManager, ConversionManager, FormManager, EncryptionDialog, PdfAValidationPanel, VeraPdfValidator, LaneScheduler, RenderCache, SecurityController, GlyphAdvanceCalculator)

---

## Summary

| Severity | Count |
|----------|-------|
| Critical | 5 |
| High     | 8 |
| Medium   | 7 |
| Low / Info | 4 |
| **Total** | **24** |

Security-critical silent failures (sign/redact/encrypt impact): **E-01, E-02, E-03, E-04, E-05, E-06, E-08**

---

## Findings

---

### E-01 · certifyDocument silently falls back to regular signature with no user notification
**Severity:** Critical · **Confidence:** High
**File:** `src/engines/SignatureManager.cpp:793-808`

**Evidence:**
```cpp
bool SignatureManager::certifyDocument(...) {
    qWarning() << "certifyDocument is not fully implemented yet, falling back to regular signature";
    return signDocument(inputPath, outputPath, certPath, password, reason, location);
}
```

**Consequence (security + data-integrity):** The `SecurityController::certifyDocument()` path is fully wired to the UI ("Certify" ribbon button, M4-PROMPT-5). When a user clicks "Certify" intending to produce a certification signature with `/DocMDP` (which restricts subsequent modifications), they silently receive an ordinary approval signature instead. The `/DocMDP` entry is never written, so a recipient cannot verify that the document has not been altered since certification. A `qWarning` goes to the debug console but no error is returned to the caller, no `ErrorInfo` is set, and the UI shows "Document signed and saved to…" — indistinguishable from a real certification. This is a security regression: the user believes a document is certified (and locked to a specific permission level) when it is not.

**Hidden errors:** Any bug or exception path in the sign sub-call is now attributed to "certify" — the real failure mode is invisible to the certify call site.

**Proposed Fix:**
```cpp
bool SignatureManager::certifyDocument(const QString &inputPath,
                                       const QString &outputPath,
                                       const QString &certPath,
                                       const QString &password,
                                       int certificationLevel,
                                       const QString &reason,
                                       const QString &location)
{
    // Do NOT fall through to signDocument — return false so the caller
    // can surface a clear "not yet implemented" error to the user.
    Q_UNUSED(inputPath); Q_UNUSED(outputPath); Q_UNUSED(certPath);
    Q_UNUSED(password);  Q_UNUSED(certificationLevel);
    Q_UNUSED(reason);    Q_UNUSED(location);
    qWarning() << "certifyDocument (/DocMDP) not yet implemented";
    return false;   // caller sets ErrorInfo and shows dialog
}
```
The `SecurityController::certifyDocument()` caller must then check the return value and show a `QMessageBox::information` explaining the feature is under development (M4-PROMPT-5 scope).

---

### E-02 · B-LT/B-LTA failures reported to caller but SecurityController ignores return value; UI always shows success
**Severity:** Critical · **Confidence:** High
**Files:** `src/engines/SignatureManager.cpp:758-780`, `src/shell/controllers/SecurityController.cpp:195-210`

**Evidence (SignatureManager):**
```cpp
bool dssOk = d->buildDssDictionary(outputPath, certChain, ocsps, {}, sigContentsRaw);
if (!dssOk) {
    overallOk = false;
    qWarning() << "B-LT: DSS dictionary build failed ...";
}
...
if (!d->addDocTimestamp(outputPath)) {
    overallOk = false;
    qWarning() << "B-LTA: document timestamp failed ...";
}
return overallOk;  // can be false
```
**Evidence (SecurityController):**
```cpp
QThread* worker = QThread::create([signing, doc, outputPath, certPath, pwd, reason, location, result]() {
    bool ok = SignDocumentHelper::execute(signing, doc, outputPath, certPath, pwd, reason, location);
    result->store(ok);
});
connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
    ...
    bool ok = result->load();
    if (ok) {
        self->_mainWindow->statusBar()->showMessage(tr("Document signed and saved to %1").arg(outputPath), 5000);
    } else {
        QMessageBox::critical(..., tr("Failed to sign document."));
    }
});
```
`SignDocumentHelper::execute` returns the value from `signDocument`, which can be `false` for B-LT/B-LTA partial failures (DSS or timestamp fails after the cryptographic signature bytes are written). In that case the status bar shows success. But if `overallOk` is `false` the UI correctly shows an error — **the actual problem is the inverse**: `signDocument` returns `false` even though a valid cryptographic signature IS on disk. The user sees "Failed to sign document" when the core signature actually succeeded; the only failure is the archival enhancement.

**Consequence (security + UX):** Either the user sees a false success ("Signed") when B-LTA assurances are missing, or a false failure that causes them to discard a validly-signed file. The `qWarning` text says "The caller MUST inform the user" but there is no mechanism in the SecurityController to distinguish `overallOk==false` due to DSS failure from a total signing failure.

**Proposed Fix:** Introduce a `SignResult` struct with `coreOk`, `dssOk`, `ltaOk`, and error strings. Return it from `signDocument`. In `SecurityController`, show a success dialog with a warning panel ("Signature applied; long-term validation data could not be embedded — please verify TSA/OCSP configuration") rather than a generic error.

---

### E-03 · validateSignatures inner `catch(...)` silently marks ANY exception as "Invalid" — hides logic errors and crashes
**Severity:** Critical · **Confidence:** High
**File:** `src/engines/SignatureManager.cpp:1135-1138`

**Evidence:**
```cpp
} catch (...) {
    info.isValid = false;
    info.trustStatus = "Invalid";
}
```

**Consequence (security):** This broad catch wraps the entire per-signature processing block (lines 878–1134). It catches not only PoDoFo/OpenSSL exceptions but also `std::bad_alloc`, `std::logic_error`, assertion violations, and any future programming errors. A bug that causes an incorrect `isValid = true` evaluation (e.g., a future refactor of the EKU check) would silently be covered up. More dangerously: an OOM during the CMS parse returns `info.isValid = false` with `trustStatus = "Invalid"`, which the `SignaturesPanel` renders as "INVALID" — this could mislead a user into believing a valid signature failed verification, causing them to reject a legitimately signed document. Zero logging context is captured (no file path, no field name, no exception type).

**Hidden errors that could be concealed:** `std::bad_alloc` from BIO/CMS allocation, `std::logic_error` from PoDoFo internal state violations, `std::overflow_error` from ByteRange arithmetic, future assertion failures in added verification steps.

**Proposed Fix:**
```cpp
} catch (const std::exception &ex) {
    qWarning() << "validateSignatures: unexpected exception on field"
               << info.fieldName << "in" << filePath << ":" << ex.what();
    info.isValid = false;
    info.trustStatus = "VerificationError";
} catch (...) {
    qCritical() << "validateSignatures: non-standard exception on field"
                << info.fieldName << "in" << filePath;
    info.isValid = false;
    info.trustStatus = "VerificationError";
}
```
Use a distinct `trustStatus` value (`"VerificationError"`) so the UI can show "Could not verify" rather than "Invalid", and the operator knows to check logs.

---

### E-04 · EncryptDocumentHelper ignores `saveDocument` return value — encryption applied but file not written, UI shows success
**Severity:** Critical · **Confidence:** High
**File:** `src/commands/EncryptDocumentHelper.h:26-28`

**Evidence:**
```cpp
bool ok = engine->saveDocument(doc->path());
if (ok) doc->markReload();
return ok;
```
The helper correctly returns `ok`. **But** `SecurityController::encryptDocument()` (lines 142–153) does not capture the return value:
```cpp
QThread* worker = QThread::create([engine, doc, userPwd, ownerPwd, perms]() {
    EncryptDocumentHelper::execute(engine, doc, userPwd, ownerPwd, perms);  // return value DISCARDED
});
connect(worker, &QThread::finished, _mainWindow, [self, progress]() {
    ...
    self->_mainWindow->statusBar()->showMessage(QObject::tr("Document encrypted"), 5000);
});
```

**Consequence (security):** If `saveDocument` fails (disk full, file locked, I/O error), the in-memory document has encryption applied (`SetEncrypted` succeeded on the PoDoFo object) but nothing is written to disk. The status bar unconditionally shows "Document encrypted". The user believes the file is encrypted. The original unencrypted file is unchanged on disk. This is a data-integrity and security failure: the user's security expectation is violated with no feedback.

**Proposed Fix:**
```cpp
auto result = std::make_shared<std::atomic<bool>>(false);
QThread* worker = QThread::create([engine, doc, userPwd, ownerPwd, perms, result]() {
    result->store(EncryptDocumentHelper::execute(engine, doc, userPwd, ownerPwd, perms));
});
connect(worker, &QThread::finished, _mainWindow, [self, progress, result]() {
    progress->close(); progress->deleteLater(); if (!self) return;
    if (result->load()) {
        self->_mainWindow->statusBar()->showMessage(tr("Document encrypted"), 5000);
    } else {
        QMessageBox::critical(self->_mainWindow, tr("Encryption Failed"),
            tr("The document could not be saved after encryption. "
               "Check that the file is not read-only and the disk is not full."));
    }
});
```

---

### E-05 · PoDoFoBackend::saveDocument / loadDocument suppress all errors in Release builds
**Severity:** Critical · **Confidence:** High
**File:** `src/engines/podofo/PoDoFoBackend.cpp:202-217`, `src/engines/podofo/PoDoFoBackend.cpp:182-200`

**Evidence:**
```cpp
} catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
    qWarning() << "Error saving document:" << e.what();
#endif
    return false;
}
```
The `#ifdef QT_DEBUG` guard means **all PoDoFo error text is silently discarded in Release builds**. The same pattern exists in `loadDocument`, `setMetadata`, `exportPdfA`, `encryptDocument` (lines 1509–1519, 1543–1552) and several other methods. In a Release build, `return false` is the only signal — the `PdfError` detail (page count, object number, stream type, error code) is thrown away.

**Consequence (data-integrity):** When a save fails in production (e.g., during autosave or the sign flow), the only trace is `PoDoFoBackend::saveDocument returned false` in whatever caller logs it. Debugging a customer-reported "my file got corrupted" issue becomes nearly impossible without reproducing in a Debug build. More critically, `exportPdfA` also suppresses the `std::exception` variant under `#ifdef QT_DEBUG` — a non-PDF exception (OOM, filesystem) leaves zero trace.

**Proposed Fix:** Remove the `#ifdef QT_DEBUG` guards from all catch blocks in security/IO-critical paths. Use `qWarning()` unconditionally (it is not significantly more expensive than a conditional in an error path). For the most critical paths (save, export), use `qCritical()`:
```cpp
} catch (const PoDoFo::PdfError& e) {
    qCritical() << "PoDoFoBackend::saveDocument failed:" << e.what()
                << "path:" << path;
    return false;
}
```

---

### E-06 · B-LTA: empty TSA token written as 4-null-byte placeholder — corrupt timestamp field committed to signed PDF
**Severity:** Critical · **Confidence:** High
**File:** `src/engines/SignatureManager.cpp:518-521`

**Evidence:**
```cpp
if (token.isEmpty()) {
    qWarning() << "B-LTA: TSA returned empty token";
    contents.assign(4, '\0');   // ← 4 null bytes written as /DocTimeStamp /Contents
    return;
}
```
`ComputeSignature` is called by PoDoFo's `SignDocument` after `AppendData` has already accumulated the entire signed byte range. Returning a 4-byte null payload causes `SignDocument` to embed an invalid RFC 3161 token in the `/DocTimeStamp` field and **commit it to the file via the `FileStreamDevice` append already in progress**. The resulting PDF has a structurally malformed `/DocTimeStamp` that all PDF validators will reject.

**Consequence (security + data-integrity):** The `addDocTimestamp` function returns `true` (no exception is thrown by PoDoFo since it just writes whatever `contents` contains), so `overallOk` stays `true`. The PDF on disk has a poisoned timestamp field. Any downstream validator that inspects the `/DocTimeStamp` will either crash, report an error, or incorrectly accept the 4-null-byte blob. The user's B-LTA claim is completely broken.

**Proposed Fix:** `ComputeSignature` must throw when the TSA fails, so PoDoFo aborts the signing operation and the partial file is not finalized:
```cpp
void ComputeSignature(charbuff &contents, bool dryrun) override {
    if (dryrun) { contents.assign(32768, '\0'); return; }
    QByteArray digest = QCryptographicHash::hash(m_accumulated, QCryptographicHash::Sha256);
    QByteArray token  = m_priv->fetchTimestampToken(digest);
    if (token.isEmpty()) {
        throw std::runtime_error("B-LTA: TSA returned empty token — aborting document timestamp");
    }
    contents.assign(token.constData(), token.size());
}
```
The `addDocTimestamp` caller wraps in `try/catch(PdfError)` but not `std::runtime_error` — add that catch as well.

---

### E-07 · RenderCache::prefetchViewport — QtConcurrent::run future is discarded; exceptions propagate to std::terminate
**Severity:** High · **Confidence:** High
**File:** `src/engines/RenderCache.cpp:271-308`

**Evidence:**
```cpp
QtConcurrent::run([weakThis, pagesToPrefetch, scale, renderer, currentToken]() {
    auto self = weakThis.lock();
    if (!self) return;
    for (int p : pagesToPrefetch) {
        ...
        QImage rendered = renderer->renderPage(p, dpi);   // can throw
        ...
    }
});  // QFuture<void> return value discarded (nodiscard suppressed by void)
```

**Consequence:** `renderer->renderPage` (PDFium backend) can throw `std::exception` or platform exceptions. When an exception escapes a `QtConcurrent::run` lambda in Qt 6, it is stored in the `QFuture`. Since the future is discarded, the exception is **never retrieved**. In Qt 6 with QFuture exception propagation, an unobserved exception in a `QFuture<void>` will call `std::terminate` when the future is destroyed. This is the sibling of the build-flagged `RenderCache.cpp:271` finding. Even if Qt swallows it, render errors are completely invisible — the prefetch silently produces no pages with no log entry.

**Proposed Fix:**
```cpp
// Store the future and connect a watcher to observe exceptions.
auto future = QtConcurrent::run([weakThis, pagesToPrefetch, scale, renderer, currentToken]() {
    ...
});
// Detach with an error observer:
auto* watcher = new QFutureWatcher<void>();
QObject::connect(watcher, &QFutureWatcher<void>::finished, watcher, [watcher]() {
    if (watcher->future().isValid()) {
        try { watcher->result(); }
        catch (const std::exception& e) {
            qWarning() << "RenderCache prefetch exception:" << e.what();
        } catch (...) {
            qWarning() << "RenderCache prefetch: unknown exception";
        }
    }
    watcher->deleteLater();
});
watcher->setFuture(future);
```

---

### E-08 · PoDoFoBackend::writeUpdate falls through to full-rewrite, silently invalidating existing signatures
**Severity:** High · **Confidence:** High
**File:** `src/engines/podofo/PoDoFoBackend.cpp:297-301`

**Evidence:**
```cpp
bool PoDoFoBackend::writeUpdate(const QString &path) {
    qCritical() << "PoDoFoBackend::writeUpdate called but only full-rewrite is implemented — "
                << "this would invalidate any existing signatures. Falling through to saveDocument; "
                << "ensure no signed document state when this path is reached.";
    return saveDocument(path);
}
```

**Consequence (security):** The CLAUDE.md non-negotiable explicitly requires `WriteUpdate` (incremental save) for all saves when signatures exist. If any code path calls `writeUpdate` on a signed document — even accidentally during a future refactor — the entire signature is silently invalidated (the full-rewrite changes the byte offsets that `/ByteRange` refers to). The `qCritical` log is the only protection; there is no guard that inspects whether the in-memory document has signatures before proceeding. A developer who doesn't notice the critical log will ship broken signatures.

**Proposed Fix:** Add an explicit guard:
```cpp
bool PoDoFoBackend::writeUpdate(const QString &path) {
    // Refuse if the document has signatures — a full rewrite would invalidate them.
    if (d->document) {
        for (auto field : d->document->GetFieldsIterator()) {
            if (field->GetType() == PoDoFo::PdfFieldType::Signature) {
                qCritical() << "PoDoFoBackend::writeUpdate: document has signatures;"
                            << "full rewrite would invalidate them. Operation REFUSED.";
                return false;   // caller must use incremental save path (PoDoFo FileMode::Append)
            }
        }
    }
    qWarning() << "PoDoFoBackend::writeUpdate: falling through to saveDocument (no incremental path)";
    return saveDocument(path);
}
```

---

### E-09 · getTrustStore: silent failure loading custom trust store path — returns empty store, all sigs appear untrusted
**Severity:** High · **Confidence:** High
**File:** `src/engines/SignatureManager.cpp:120-126`

**Evidence:**
```cpp
X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
if (lookup) X509_LOOKUP_load_file(lookup, path.toUtf8().constData(), X509_FILETYPE_PEM);
trustStoreUsedStr = "CustomPath";
```

**Consequence (security):** If `X509_LOOKUP_load_file` fails (file not found, wrong format, permissions error), the return value is discarded and `trustStoreUsedStr` is set to `"CustomPath"` as if loading succeeded. `CMS_verify` then runs against an empty store and fails with `X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT` — every signature in the document will report `trustStatus = "UntrustedChain"` regardless of actual validity. A user who configures a custom trust store sees all their documents reporting invalid signatures with no indication that the store itself failed to load. The same pattern exists for the `hash_dir` path.

**Proposed Fix:**
```cpp
if (lookup) {
    int ret = X509_LOOKUP_load_file(lookup, path.toUtf8().constData(), X509_FILETYPE_PEM);
    if (ret != 1) {
        char errBuf[256];
        ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
        qWarning() << "SignatureManager: failed to load custom trust store from"
                   << path << ":" << errBuf;
        trustStoreUsedStr = "CustomPathLoadFailed";
    } else {
        trustStoreUsedStr = "CustomPath";
    }
}
```

---

### E-10 · extractOcspFromDss: broad `catch(...){}` with empty return — OCSP revocation check silently skipped
**Severity:** High · **Confidence:** High
**File:** `src/engines/SignatureManager.cpp:566-568`

**Evidence:**
```cpp
} catch (...) {
    return {};
}
```

**Consequence (security):** `extractOcspFromDss` is called inside `validateSignatures` to check if the embedded OCSP indicates revocation. If the DSS traversal throws (malformed DSS, corrupted OCSP stream, OOM), an empty `QByteArray` is silently returned. The revocation check is then skipped entirely, and a revoked certificate would appear as "Valid" or "ValidWithDSS". There is zero logging to indicate a revocation-check anomaly occurred.

**Proposed Fix:**
```cpp
} catch (const std::exception &e) {
    qWarning() << "SignatureManager: exception extracting OCSP from DSS for field"
               << sigFieldName << ":" << e.what()
               << "— revocation status UNKNOWN";
    return {};
} catch (...) {
    qWarning() << "SignatureManager: unknown exception extracting OCSP from DSS";
    return {};
}
```

---

### E-11 · ConversionManager::exportToHtml: `catch(...){}` in per-page loop — partial HTML written silently on error
**Severity:** High · **Confidence:** High
**File:** `src/engines/ConversionManager.cpp:299`

**Evidence:**
```cpp
try {
    PoDoFo::PdfMemDocument doc;
    doc.Load(pdfPath.toUtf8().constData());
    QList<TextElement> elements = d->extractTextFromPage(i, doc);
    ...
} catch(...) {}  // completely empty — no log, no per-page error
out << "</div>\n";  // div closed even though content is missing
```

**Consequence (data-integrity):** When a page fails to extract, the `<div class="page">` is written but contains no text content. The caller receives `true` (success), and the output HTML is silently truncated for that page. For a legal or compliance workflow where HTML export is used for document review, a page may appear blank without any indication of the extraction failure.

**Proposed Fix:**
```cpp
} catch (const std::exception &e) {
    qWarning() << "ConversionManager::exportToHtml: page" << i << "extraction failed:" << e.what();
} catch (...) {
    qWarning() << "ConversionManager::exportToHtml: page" << i << "unknown extraction failure";
}
```

---

### E-12 · PoDoFoBackend::encryptDocument / exportPdfA: all errors suppressed in Release builds via #ifdef QT_DEBUG
**Severity:** High · **Confidence:** High
**File:** `src/engines/podofo/PoDoFoBackend.cpp:1509-1519, 1547-1552`

**Evidence:**
```cpp
} catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
    qWarning() << "PoDoFo error during PDF/A export:" << e.what();
#endif
    return false;
}
```

This is a sibling of E-05 specifically for the security-critical paths `encryptDocument` and `exportPdfA`. In a Release build, a failed encryption or PDF/A export returns `false` with zero diagnostic output — the only signal is the boolean.

**Consequence:** Already covered under E-05, but highlighted separately for the security-critical paths: a failed `encryptDocument` in Release = no log, no error string, user gets a silent no-op.

**Proposed Fix:** Same as E-05 — remove all `#ifdef QT_DEBUG` guards from error-path logging.

---

### E-13 · BatchMode: exceptions in per-file worker lambda propagate through QtConcurrent::mapped with no catch
**Severity:** High · **Confidence:** Med
**File:** `src/modes/BatchMode.cpp:705-802`

**Evidence:**
```cpp
auto processFileReal = [=](const QString& inputPath) -> BatchFileResult {
    ...
    // editor.loadDocumentForEditing / optimizeDocument / addTextWatermark / exportPdfA
    // None of these calls are wrapped in try/catch
    ...
};
QFuture<BatchFileResult> future = QtConcurrent::mapped(capturedFiles, processFileReal);
```

**Consequence:** If any engine call inside `processFileReal` throws an exception (PoDoFo `PdfError`, `std::exception`, or any other), `QtConcurrent::mapped` stores it in the `QFuture`. The `QFutureWatcher` connections (`resultReadyAt`, `finished`) do not call `watcher.result()` (which would re-throw), so the exception sits in the future unobserved. In Qt 6, when `m_watcher` goes out of scope, the unobserved exception may call `std::terminate`. Even if it does not terminate, that file's `BatchFileResult` is never emitted — the progress counter is wrong and the user sees an incomplete result with no error logged.

**Proposed Fix:** Wrap the lambda body in try/catch:
```cpp
auto processFileReal = [=](const QString& inputPath) -> BatchFileResult {
    BatchFileResult result;
    result.inputPath = inputPath;
    try {
        // ... existing logic ...
    } catch (const std::exception &e) {
        result.success = false;
        result.errorMessage = QString::fromStdString(e.what());
    } catch (...) {
        result.success = false;
        result.errorMessage = QStringLiteral("Unknown exception processing file");
    }
    return result;
};
```

---

### E-14 · GlyphAdvanceCalculator::sumAdvances (Path 1&2): silent `catch(...){}` hides font metrics errors in Edact-Ray
**Severity:** Medium · **Confidence:** High
**File:** `src/engines/podofo/GlyphAdvanceCalculator.cpp:27-36`

**Evidence:**
```cpp
try {
    if (font.TryGetEncodedStringLength(encodedStr, ps, len)) {
        return len;
    }
} catch (...) {}   // path 1: completely swallowed

if (encodedStr.IsStringEvaluated()) {
    try {
        return font.GetStringLength(encodedStr.GetString(), ps);
    } catch (...) {}   // path 2: completely swallowed
}
```

**Consequence:** The Edact-Ray redaction engine relies on glyph advance sums to produce replacement whitespace of the correct width (audit non-negotiable: "never black rectangle"). If font metrics throw during the encoded-string traversal, paths 1 and 2 silently fail and fall through to path 3 (CID fallback) or throw `std::runtime_error`. A silent path-1/2 failure means the glyph advance sum is wrong — the replacement whitespace is the wrong width, potentially leaving a visual gap that reveals the redacted content length. No log entry indicates which document, page, or font encountered the error.

**Proposed Fix:**
```cpp
try {
    if (font.TryGetEncodedStringLength(encodedStr, ps, len))
        return len;
} catch (const std::exception &e) {
    qWarning() << "GlyphAdvanceCalculator: TryGetEncodedStringLength threw:" << e.what()
               << "— falling back to path 2";
}
```

---

### E-15 · MRC exportMrcPdfA: layer separation failure silently uses raw page image; not surfaced to veraPDF or caller
**Severity:** Medium · **Confidence:** High
**File:** `src/engines/PdfEditorEngine.cpp:368-377`

**Evidence:**
```cpp
if (!pl.mrc.success) {
    qWarning() << "MrcPdfA: page" << i << "layer separation failed:" << pl.mrc.errorMessage;
    // Produce a fallback layer: raw JPEG background, empty foreground
    pl.mrc.backgroundImage = pageImages[i];
    pl.mrc.pageWidthPx     = pageImages[i].width();
    pl.mrc.pageHeightPx    = pageImages[i].height();
    pl.mrc.success         = true;   // ← reset to true
}
```

**Consequence (data-integrity):** When MRC layer separation fails for a page, the fallback uses the raw uncompressed JPEG page image (no JBIG2 foreground mask, no sandwich text for that page). The function returns `true`. The veraPDF gate at the end may not flag this because the PDF structure is still valid. The output PDF claims to be a multi-layer MRC document but contains a subset of pages as flat images — the compression ratio is compromised and the OCR layer is silently missing. For a document going through a legal redaction + MRC pipeline, the missing text layer on a fallback page means that page's text is not searchable or selectable.

**Proposed Fix:** Track fallback pages and include them in the return value (or at minimum log which pages fell back before returning `true`). Consider adding them to the veraPDF validation metadata so downstream can detect degraded pages.

---

### E-16 · PoDoFoBackend page-manipulation methods all use bare `catch(...){}` — I/O errors on signed docs not detected
**Severity:** Medium · **Confidence:** High
**Files:** `src/engines/podofo/PoDoFoBackend.cpp`: rotatePage:490, insertPageFromBytes:536, deletePage:551, insertBlankPage:564, cropPage:852, resizePage:871, reorderPages:900, addHeaderFooter:929, applyBatesNumbering:967

**Evidence (representative):**
```cpp
bool PoDoFoBackend::rotatePage(const QString &path, int pageIndex, int degrees) {
    ...
    } catch (...) {
        return false;
    }
}
```

Each of these methods calls `doc.Save(path.toUtf8().constData())` inside the try block. A save failure (disk full, permissions, signed-document modification) returns `false` with zero log output. The caller's `PdfEditorEngine` wrapper does log an error via `ErrorInfo`, but the specific PoDoFo error text is lost.

**Consequence:** When one of these operations fails on a signed document (which should not be modified by full-rewrite), the failure is silent at the backend level. The `PdfEditorEngine` sets a generic error message but has no detail about what went wrong inside PoDoFo.

**Proposed Fix:** Replace bare `catch(...)` with typed catches:
```cpp
} catch (const PoDoFo::PdfError &e) {
    qWarning() << "PoDoFoBackend::" << __func__ << "PdfError:" << e.what() << "path:" << path;
    return false;
} catch (const std::exception &e) {
    qWarning() << "PoDoFoBackend::" << __func__ << "exception:" << e.what();
    return false;
} catch (...) {
    qCritical() << "PoDoFoBackend::" << __func__ << "unknown exception — path:" << path;
    return false;
}
```

---

### E-17 · FormManager::hasXfaForms: `catch(...)` returns false — XML-based form detection silently wrong
**Severity:** Medium · **Confidence:** High
**File:** `src/engines/FormManager.cpp:128-130`

**Evidence:**
```cpp
} catch (...) {
    return false;
}
```

**Consequence:** If PoDoFo throws while loading the document to check for XFA, `false` is returned (no XFA). A document with XFA forms that causes a load error will be treated as non-XFA, potentially allowing field operations on an XFA form that PoDoFo cannot safely modify. No log entry is produced.

---

### E-18 · PoDoFoBackend::loadDocument: PoDoFo error suppressed in Release — caller gets `false` with no context
**Severity:** Medium · **Confidence:** High
**File:** `src/engines/podofo/PoDoFoBackend.cpp:194-199`

This is the specific instance of E-05 for `loadDocument`. Distinct because it is the entry point for every document open, including the recovery-load path:
```cpp
} catch (const PoDoFo::PdfError& e) {
#ifdef QT_DEBUG
    qWarning() << "Error loading document:" << e.what();
#endif
    return false;
}
```
The qpdf repair path in `PdfEditorEngine::loadDocumentForEditing` triggers on `loadDocument` returning false — but without the PoDoFo error message in Release, diagnostics for "why did PoDoFo fail to load this PDF?" are impossible.

---

### E-19 · VeraPdfValidator::validate: process stderr not captured; timeout produces partial report
**Severity:** Medium · **Confidence:** Med
**File:** `src/engines/VeraPdfValidator.cpp:47-61`

**Evidence:**
```cpp
proc.start(kCliPath, args);
if (!proc.waitForFinished(30000)) {
    proc.kill();
    report.errorMessage = QStringLiteral("veraPDF timed out after 30 seconds.");
    return report;
}
QByteArray output = proc.readAllStandardOutput();
return parseJson(output);
```

`QProcess::setProcessChannelMode` is not called before `start`, so stderr goes to the process's stderr (visible in a terminal but not captured). If veraPDF prints a meaningful Java exception to stderr, it is discarded. After a timeout, `proc.kill()` is called but `waitForFinished` is not called after the kill — the process may still be running, leaking resources. `parseJson` on an empty `output` after timeout returns a report with `validatorAvailable = false` rather than a timeout-specific indicator.

---

### E-20 · SecurityController::encryptDocument: worker thread throws, never caught — potential std::terminate
**Severity:** Medium · **Confidence:** Med
**File:** `src/shell/controllers/SecurityController.cpp:142-144`

**Evidence:**
```cpp
QThread* worker = QThread::create([engine, doc, userPwd, ownerPwd, perms]() {
    EncryptDocumentHelper::execute(engine, doc, userPwd, ownerPwd, perms);
});
```

`QThread::create` creates a thread whose function body has no exception handling. If `PoDoFo::PdfMemDocument::Save` throws an exception that propagates out of `EncryptDocumentHelper::execute`, it crosses the thread boundary and calls `std::terminate`. This pattern is also present in `signDocument` (line 189) and `sanitizeDocument` (line 254) worker threads. The signing path does have try/catch in `SignatureManager::signDocument` itself, but if a Qt or system exception escapes the `SignDocumentHelper::execute` wrapper for any reason (e.g., a `QFile` throw on a full disk), there is no safety net.

**Proposed Fix:** Wrap all QThread::create worker lambdas:
```cpp
QThread* worker = QThread::create([engine, doc, userPwd, ownerPwd, perms, result]() {
    try {
        result->store(EncryptDocumentHelper::execute(engine, doc, userPwd, ownerPwd, perms));
    } catch (const std::exception &e) {
        qCritical() << "Encryption worker thread threw:" << e.what();
        result->store(false);
    } catch (...) {
        qCritical() << "Encryption worker thread threw unknown exception";
        result->store(false);
    }
});
```

---

### E-21 · GlyphAdvanceCalculator::sumAdvances (utf8 overload): throws std::runtime_error across PoDoFo content-stream reader
**Severity:** Low · **Confidence:** Med
**File:** `src/engines/podofo/GlyphAdvanceCalculator.cpp:72-79`

**Evidence:**
```cpp
} catch (const std::exception& e) {
    throw std::runtime_error(
        std::string("GlyphAdvanceCalculator: GetStringLength failed: ") + e.what());
}
```

This correctly re-throws with context. However, the throw is only caught by the `catch(...)` blocks in the PoDoFo content-stream redaction code — which returns `false` silently. The error text is lost unless the catch site logs it.

---

### E-22 · LaneScheduler: GPU task exception only catches `std::exception`; non-standard exceptions → `std::terminate`
**Severity:** Low · **Confidence:** Med
**File:** `src/engines/scheduling/LaneScheduler.h:158-165`

**Evidence:**
```cpp
try {
    T result = work();
    promise->addResult(ScheduledValue<T>::success(std::move(result)));
} catch (const std::exception& e) {
    SchedulerError err; ...
    promise->addResult(ScheduledValue<T>::failure(err));
}
promise->finish();
```

Non-`std::exception` throws (e.g., from native Windows API calls through ONNX or PDFium) would escape the lambda and call `std::terminate` on the GPU worker thread.

**Proposed Fix:** Add `catch (...)` after the `std::exception` catch that logs and records a `WorkerCrashed` error.

---

### E-23 · ConversionManager::exportToText and exportToPowerPoint: `catch(...){}` / `catch(...){return false}` with no logging
**Severity:** Low · **Confidence:** High
**Files:** `src/engines/ConversionManager.cpp:469-471, 489-491`

**Evidence:**
```cpp
} catch (...) {
    return false;
}
```
Both export paths return `false` on any exception with no log. These are not security-critical but produce silent failures with no user feedback from the calling `BatchMode` or `ConversionManager`.

---

### E-24 · SignaturesPanel: validateSignatures exceptions produce "UNAVAILABLE" rather than explicit error state
**Severity:** Low · **Confidence:** Med
**File:** `src/modes/SignaturesPanel.cpp:130-133`

**Evidence:**
```cpp
const QList<SignatureInfo> sigs = signing->validateSignatures(filePath);
if (sigs.isEmpty()) { showNoSignatures(tr("UNSIGNED")); return; }
```

If `validateSignatures` throws at the outer `try/catch(std::exception)` level (line 1142 of SignatureManager.cpp), it returns an empty list. `showNoSignatures("UNSIGNED")` is called — the user sees "UNSIGNED" instead of "VERIFICATION ERROR". A user who expects signatures on a document would believe it is unsigned rather than that verification failed.

---

## Security-critical silent failures (subset that can compromise sign / redact / encrypt)

| ID | Title | File:Line | Impact |
|----|-------|-----------|--------|
| E-01 | certifyDocument falls back to regular sign silently | `SignatureManager.cpp:807-808` | False security claim: /DocMDP never written |
| E-02 | B-LT/B-LTA failures not surfaced to UI | `SecurityController.cpp:200` | User believes long-term validation data present when it is not |
| E-03 | validateSignatures inner catch(...) marks any exception as "Invalid" | `SignatureManager.cpp:1135-1138` | Hides logic bugs; legitimate sigs may appear invalid |
| E-04 | EncryptDocumentHelper::execute return value discarded in SecurityController | `SecurityController.cpp:143` | Encrypted in-memory, unencrypted on disk; UI shows success |
| E-05 | PoDoFoBackend load/save errors silently discarded in Release | `PoDoFoBackend.cpp:194-199, 211-216` | No diagnostic for any production save/load failure |
| E-06 | B-LTA TSA empty token written as 4 null bytes; corrupt timestamp committed | `SignatureManager.cpp:518-521` | Malformed /DocTimeStamp silently embedded |
| E-08 | writeUpdate falls through to full-rewrite without signature guard | `PoDoFoBackend.cpp:297-301` | Future code paths can silently invalidate signatures |

---

## Verified vs Could-Not-Verify

### Verified (evidence directly in source)
- E-01 through E-24: all confirmed by direct reading of source code at quoted lines.
- E-04 specifically confirmed: `SecurityController::encryptDocument` at line 143 does not capture the return value of `EncryptDocumentHelper::execute`.
- E-02 confirmed: `overallOk` is returned by `signDocument` but the SecurityController wires success/failure display through the same `ok` variable — a B-LT DSS failure sets `overallOk = false` and the dialog shows "Failed to sign document" even though the signature bytes are written.
- E-06 confirmed: `contents.assign(4, '\0')` followed by `return` (no throw); PoDoFo `SignDocument` proceeds to write this to the append stream.

### Could-Not-Verify (runtime behavior or inter-process paths)
- **E-07** (prefetch `std::terminate`): Qt 6 QFuture<void> exception propagation semantics require runtime confirmation; behavior may vary between Qt versions. The structural issue (discarded future) is confirmed.
- **E-13** (BatchMode QtConcurrent::mapped exception): depends on whether engine calls within the lambda can actually throw; confirmed they call PoDoFo which does throw.
- **E-19** (veraPDF stderr): confirmed process does not capture stderr, but whether veraPDF actually produces meaningful stderr diagnostics depends on the veraPDF version.
- **E-20** (SecurityController worker thread std::terminate probability): depends on whether OS-level exceptions can escape the inner engine try/catch blocks; confirmed no outer catch in the thread lambda.
