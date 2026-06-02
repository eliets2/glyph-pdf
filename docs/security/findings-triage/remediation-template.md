# Remediation Record Template

> **Deliverable:** M7-PROMPT-1 / D5
> **Date:** 2026-06-02
> **Instructions:** Copy this template once per audit finding. Replace all
> `[PLACEHOLDER]` fields. File each completed record as
> `GLPH-SEC-NNN-<short-slug>.md` in this directory.
> Do NOT commit remediation records to a public repo until after disclosure.

---

## Finding header

| Field | Value |
| :-- | :-- |
| **GlyphPDF finding ID** | GLPH-SEC-[NNN] |
| **Auditor finding ID** | [Auditor's internal ID, e.g. TOB-GLPH-001] |
| **Severity** | [Critical / High / Medium / Low / Informational] |
| **CVSS v3.1 base score** | [e.g. 8.1 — include vector string: AV:L/AC:H/PR:N/UI:R/S:U/C:H/I:H/A:H] |
| **Tags** | [KNOWN / BLOCKER / UPSTREAM / ASPIRATIONAL-CLAIM — select all that apply] |
| **Threat model ID** | [T-RED-1 / T-API-2 / etc. — or "NEW" if not in threat model] |
| **Engineering owner** | [Name] |
| **Date assigned** | [YYYY-MM-DD] |
| **SLA deadline** | [YYYY-MM-DD — per triage-protocol.md §2] |

---

## Finding description

**Title:** [One-line title matching the auditor's report]

**Affected file(s) and line(s):**

```
src/engines/podofo/PoDoFoBackend.cpp:1233-1260  (example)
src/engines/ai/AnthropicProvider.cpp:42          (example)
```

**Description:**

[Describe the vulnerability in 3–5 sentences. What is the attacker's input?
What code path does it traverse? What is the security impact? Copy from or
quote the auditor's report — do not paraphrase in a way that understates the
severity.]

---

## Reproduction

**Environment:** [Windows 10/11 version; GlyphPDF version; commit SHA]

**Steps to reproduce:**

1. [Step 1]
2. [Step 2]
3. [Step 3 — e.g.: Open the attached PoC PDF in GlyphPDF. Apply redaction to
   the region containing the image. Save the file. Open the saved PDF in
   iText RUPS and navigate to the image XObject — original pixel data is
   present.]

**PoC file / script location:** [Link or path to auditor-provided PoC]

**Expected behavior:** [What should happen — e.g.: Image pixel data should be
excised from the PDF object graph; only the black-rectangle overlay should
remain.]

**Actual behavior:** [What actually happens — e.g.: Original image XObject
remains intact and is recoverable by removing the overlay operator.]

---

## Root cause analysis

[2–5 sentences identifying the precise root cause. Reference the specific
function, code path, and design decision that produces the vulnerability.
Example:

"In `redactCanvasRecursively` (`PoDoFoBackend.cpp`, lines 1233–1260), when the
`Do` operator is encountered for an image XObject and `isIntersectingSpan`
returns true, the code skips the operator (preventing it from invoking the
XObject a second time) but does not remove the XObject definition from the
`/XObject` resources dictionary. The XObject remains referenced and its pixel
data is fully accessible via direct object extraction. The final `applyRedactions`
step then draws an opaque black rectangle over the region, creating a visual-only
overlay rather than a data excision."]

---

## Fix approach

**Proposed fix:** [Describe the fix strategy concisely. If multiple approaches
are viable, list them with a recommendation. Example:

"Option A (recommended): In `applyRedactions`, after `redactCanvasRecursively`
completes, enumerate all image XObjects from the page's `/Resources/XObject`
dictionary whose bounding box intersects any redaction rectangle. For each
intersecting image XObject, replace its stream data with a single-pixel
black placeholder and zero its width/height, then remove the original pixel
data. This ensures the pixel data is genuinely excised.

Option B: Fail-closed. When any image XObject intersects the redaction
rectangle, abort the operation (as the inline-image path already does) and
return an error to the UI informing the user that image redaction is not yet
supported. This is the minimum safe behavior before Option A is implemented."]

**Implementation notes:**

[Any caveats, edge cases, or constraints on the fix. Example: "Option A must
also handle the case where the image XObject is referenced from multiple
pages — the XObject should only be replaced if ALL referencing pages have
the same redaction applied, or a per-page copy must be made first."]

---

## Fix record

| Field | Value |
| :-- | :-- |
| **Fix committed** | [YYYY-MM-DD] |
| **Fix branch** | [branch name] |
| **Fix commit SHA** | [full 40-char SHA] |
| **Fix PR / MR** | [link or number] |
| **Fix merged to** | [target branch, e.g. main] |
| **Merge date** | [YYYY-MM-DD] |
| **Release version** | [version that ships the fix, e.g. 1.0.1] |
| **Release date** | [YYYY-MM-DD] |

---

## Re-test and verification

**Re-test requested from auditor:** [YES / NO / IN-PROGRESS]
**Re-test date:** [YYYY-MM-DD]
**Re-test performed by:** [Auditor name / internal QA]

**Re-test steps:**

1. [Same reproduction steps as above, applied to the fixed version]
2. [Confirm the original PoC no longer reproduces the vulnerability]
3. [Confirm no regression in related security tests: ctest --output-on-failure]

**Re-test result:** [PASS — vulnerability no longer reproducible / FAIL —
further fix needed / PARTIAL — partially mitigated]

**Notes:**

[Any observations from the re-test. Example: "The image XObject path is now
fail-closed (Option B implemented). A user attempting to redact a page
containing image XObjects receives the error: 'Image redaction requires
content excision, which is not yet available. The operation has been cancelled.'
The original PoC PDF now returns false from applyRedactions and no black
rectangle overlay is drawn."]

---

## Auditor sign-off

| Field | Value |
| :-- | :-- |
| **Auditor** | [Name, firm] |
| **Sign-off date** | [YYYY-MM-DD] |
| **Sign-off status** | [CONFIRMED FIXED / PARTIALLY MITIGATED (with rationale) / NOT YET REVIEWED] |
| **Auditor notes** | [Any caveats or conditions on the sign-off] |

---

## Disclosure

| Field | Value |
| :-- | :-- |
| **CVE requested** | [YES / NO / N/A] |
| **CVE ID** | [CVE-YYYY-NNNNN or PENDING or N/A] |
| **NVD publication date** | [YYYY-MM-DD or PENDING] |
| **Public disclosure date** | [YYYY-MM-DD] |
| **Disclosure method** | [SECURITY.md update / GitHub Security Advisory / blog post] |
| **Acknowledgement** | [Auditor firm name for the acknowledgements section] |

---

## Status summary

| Phase | Status | Date |
| :-- | :-- | :-- |
| Finding received | [ ] | |
| Triaged (severity confirmed, owner assigned) | [ ] | |
| Root cause confirmed | [ ] | |
| Fix approach agreed | [ ] | |
| Fix committed | [ ] | |
| Fix merged | [ ] | |
| Re-test requested | [ ] | |
| Re-test passed | [ ] | |
| Auditor sign-off | [ ] | |
| Public disclosure | [ ] | |

---

*Use one file per finding. Filename convention: `GLPH-SEC-NNN-short-slug.md`*
*Example: `GLPH-SEC-001-image-redaction-bypass.md`*
