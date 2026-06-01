# Third-Party Security Audit — Vendor Shortlist + Quotes

> **Deliverable:** M7-PROMPT-1 / D1
> **Status:** Draft for review (no firm has been contacted yet)
> **Date:** 2026-06-01
> **Owner:** Security coordination
>
> **IMPORTANT — all dollar figures below are ESTIMATES.** They are derived from
> published industry pricing guides and the firms' public engagement reports
> (see *Sources*), **not** from quotes issued to GlyphPDF. Real numbers require a
> scoping call. Treat the ranges as planning figures only. Each firm prices per
> consultant-day against an agreed scope; the totals below assume a focused
> **1–2 week (5–10 consultant-day) white-box source-code audit** of the security
> surface defined in `auditor-kit/scope.md`.

---

## What we are buying

A **white-box (source-available) security assessment** of a C++17 / Qt 6 desktop
PDF workstation. The audit must cover, in priority order:

1. **Memory safety of untrusted-PDF parsing** — GlyphPDF parses attacker-supplied
   PDFs through PoDoFo / PDFium / qpdf. Native-code memory corruption (heap/stack
   overflow, UAF, integer overflow, type confusion) is the dominant risk class.
   Requires C/C++ auditing + **fuzzing harness** capability.
2. **Redaction correctness** — validate that "secure redaction" actually excises
   content. See the known image/vector limitation flagged in
   `auditor-kit/threat-model.md` (T-RED-1).
3. **Cryptography review** — AES-256 (PoDoFo `AESV3R6`), PAdES B-LT/B-LTA
   signatures (SHA-256, DSS/VRI, OCSP, TSA), signature byte-range validation.
4. **Secrets handling** — AI-provider API keys stored in Windows Credential
   Manager and transmitted over TLS to third-party endpoints.
5. **Sanitization vectors** — JavaScript / OpenAction / embedded-file / metadata
   stripping completeness.

The ideal vendor has **all three** of: (a) native C/C++ memory-safety auditing,
(b) parser fuzzing, (c) applied cryptography review — under one roof.

---

## Shortlist (ranked by fit)

### 1. Trail of Bits — *recommended primary*

| | |
| :-- | :-- |
| HQ | New York, USA (global, remote-first) |
| Best fit for | Memory-safety + fuzzing of the native PDF parsing surface |
| Crypto review | Yes — dedicated cryptography practice |
| Engagement model | Per consultant-day; scoped statement of work |
| **Estimated cost (1–2 wk)** | **~$25K–$45K+** (estimate) |

**Why them.** Trail of Bits publicly documents exactly the capability mix this
audit needs: comprehensive **C/C++ security reviews for memory corruption,
integer overflows, race conditions, and platform-specific vulnerabilities**, and
they **use fuzzing on most of their audits** (AddressSanitizer-driven harnesses,
fuzzing dictionaries, harness authoring). They publish a C/C++ chapter of their
Testing Handbook covering Windows specifically — directly relevant to our
ucrt64 / MinGW target. This is the strongest single match for risk #1 (untrusted
PDF parsing) plus risk #3 (crypto).

**Estimate basis.** Trail of Bits sits at the premium end of the boutique appsec
market. A focused 1–2 week native-code + crypto engagement typically lands in the
mid-five-figure range; budget toward the top of the band if a custom fuzzing
harness is in scope. **Estimate — confirm via scoping call.**

**Caveats.** Premium pricing; lead times can be multiple weeks/months. Fuzzing
harness construction may push effort to the upper end.

---

### 2. NCC Group — *recommended alternative / second bid*

| | |
| :-- | :-- |
| HQ | Manchester, UK (large global consultancy, US offices) |
| Best fit for | Broad C/C++ code review + **dedicated cryptography services** |
| Crypto review | Yes — standalone Cryptography Services team |
| Engagement model | Per consultant-day; scoped SOW |
| **Estimated cost (1–2 wk)** | **~$20K–$40K+** (estimate) |

**Why them.** NCC Group runs a **dedicated Cryptography Services** team focused
"exclusively on the most challenging projects involving cryptographic primitives,
protocols, implementations, systems, and applications" — ideal for validating the
PAdES/AES claims. Their **code review** practice targets "vulnerabilities that
can occur in unmanaged languages," and their principal consultants literally
wrote the reference book on **secure coding in C and C++** (Robert C. Seacord,
*Secure Coding in C and C++*). Strong fit for risks #1 and #3, with deeper crypto
bench than most boutiques. They publish customer audit letters (e.g. the Padloc
assessment), which matters for D4 (marketing letter).

**Estimate basis.** Comparable per-day rates to other tier-1 consultancies; a
1–2 week focused engagement is typically low-to-mid five figures. As a larger
firm, scoping/contracting overhead is higher but capacity/availability is better.
**Estimate — confirm via scoping call.**

**Caveats.** Larger-firm process overhead; may bundle into a broader (more
expensive) assessment unless scope is held tight.

---

### 3. Cure53 — *recommended for tight, report-focused scope*

| | |
| :-- | :-- |
| HQ | Berlin, Germany |
| Best fit for | Time-boxed white-box code audit with a strong public report |
| Crypto review | Yes (general appsec + crypto in code audits) |
| Engagement model | Fixed team × fixed days (publishes the day-count) |
| **Estimated cost (1–2 wk)** | **~$15K–$35K** (estimate) |

**Why them.** Cure53 explicitly offers **white-box tests and code audits** (not
just black-box pentests) and has **15+ years** of software-test/code-audit
history. Their published reports show the engagement shape we want: e.g. a
**4–5 person team over ~12–12.5 days** for a focused product. They are known for
crisp, citable public reports — valuable for D4 and for the README security
section. Good fit when scope is held tight to the PDF security surface.

**Estimate basis.** Cure53 prices as *team-size × days*. A small, focused
engagement (e.g. 2 consultants × 5–8 days, or a compact ~10–12 person-day audit)
maps to the low-to-mid five figures. Their reports disclose person-day counts,
which makes their effort estimates unusually transparent — but they do **not**
publish rate cards. **Estimate — confirm via scoping call.**

**Caveats.** EU-based (timezone/contracting); fuzzing-harness depth is less
foregrounded than Trail of Bits — confirm fuzzing is in scope if risk #1 is the
priority.

---

## Comparison matrix

| Criterion (weight) | Trail of Bits | NCC Group | Cure53 |
| :-- | :-- | :-- | :-- |
| C/C++ memory-safety audit (30%) | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| Parser fuzzing capability (25%) | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ |
| Cryptography review (20%) | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| Public/citable report for D4 (15%) | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| Cost efficiency for 1–2 wk (10%) | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| **Est. total (1–2 wk)** | ~$25–45K | ~$20–40K | ~$15–35K |

*Stars reflect fit to GlyphPDF's specific surface based on each firm's public
capability statements and reports — not an objective ranking of the firms overall.*

---

## Recommendation

- **Primary bid: Trail of Bits.** Best single-vendor coverage of the dominant
  risk (memory safety of untrusted PDF parsing) plus fuzzing plus crypto. Expect
  the highest quote.
- **Second bid: NCC Group.** Near-equal C/C++ coverage and the deepest dedicated
  crypto bench; better availability; useful published-letter track record for D4.
- **Budget / tight-scope option: Cure53.** Best price-for-report if scope is held
  to the core PDF security surface; transparent person-day estimates.

**Suggested action:** request scoping calls from **Trail of Bits and NCC Group**
in parallel (primary + alternate), with Cure53 as a fallback if both exceed the
$40K ceiling. Hold scope to `auditor-kit/scope.md` to keep all three inside the
$15–40K planning envelope. Get the redaction limitation (T-RED-1) explicitly into
the SOW so the auditor validates it rather than re-discovering it.

## Budget envelope (planning)

| Scenario | Estimated cost |
| :-- | :-- |
| Tight scope, 1 wk, 1–2 consultants | ~$15K–$25K |
| Standard scope, 1–2 wk, fuzzing included | ~$25K–$40K |
| Premium vendor + custom fuzzing harness | $40K+ |

The prompt's stated planning target is **~$15–40K for a typical 1–2 week PDF-tool
audit**, which aligns with the standard-scope row. Anything materially above $40K
indicates scope creep beyond the defined surface.

> Industry context for these ranges: general IT/security audit guides put
> source-code-inclusive engagements broadly in the **$3K–$50K+** band, and
> penetration-test pricing guides cite **$4K to $100K+** depending on scope. A
> focused boutique white-box code audit of a single desktop app's security
> surface sits in the middle of those bands.

---

## What I could NOT ground

- **No firm has issued GlyphPDF a quote.** Every dollar figure is an estimate
  from public pricing guides + the firms' published engagement reports. None of
  the three firms publishes a public rate card, so exact day rates are not
  verifiable without contacting them. Numbers are planning placeholders, not
  commitments.
- **Lead times / current availability** for each firm are not verifiable from
  public sources and must come from a scoping call.

## Sources

- [Trail of Bits — Memory Safety (blog category)](https://blog.trailofbits.com/categories/memory-safety/)
- [Trail of Bits — Fuzzing (blog category)](https://blog.trailofbits.com/categories/fuzzing/)
- [Trail of Bits — Audits (blog category)](https://blog.trailofbits.com/categories/audits/)
- [Trail of Bits — Master fuzzing with our new Testing Handbook chapter](https://blog.trailofbits.com/2024/02/09/master-fuzzing-with-our-new-testing-handbook-chapter/)
- [NCC Group — Code Review service](https://www.nccgroup.com/us/technical-assurance/application-security/code-review/)
- [NCC Group — Cryptography & Encryption Services](https://www.nccgroup.com/us/assessment-advisory/cryptography/)
- [NCC Group — Secure Coding in C and C++ (Robert C. Seacord)](https://www.nccgroup.com/research/secure-coding-in-c-and-cplusplus/)
- [NCC Group — Application Security](https://www.nccgroup.com/us/assessment-advisory/application-software/)
- [Padloc — Security Audit by NCC Group (example public letter)](https://padloc.app/blog/security-audit-ncc/)
- [Cure53 — Security assessments for software that matters](https://cure53.de/)
- [Cure53 — OpenPGP.js security audit (example white-box report)](https://github.com/openpgpjs/openpgpjs/wiki/Cure53-security-audit)
- [BSG — Penetration Testing Cost in 2026: $4K–$100K+ Guide](https://bsg.tech/blog/what-can-you-expect-to-pay-for-penetration-testing/)
- [Qualysec — How Much Does an IT Security Audit Cost](https://qualysec.com/it-security-audit-cost/)
- [Astra — How Much Does an IT Cybersecurity Audit Cost in 2026](https://www.getastra.com/blog/security-audit/it-security-audit-cost/)
