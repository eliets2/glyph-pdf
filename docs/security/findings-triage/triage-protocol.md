# Findings Triage Protocol

> **Deliverable:** M7-PROMPT-1 / D5
> **Date:** 2026-06-02
> **Owner:** Security lead (GlyphPDF)
> **Use:** Activate this protocol when the auditor's report is received.
> Each finding from the report gets a remediation record in
> `remediation-template.md`.

---

## 1. Severity classification

GlyphPDF uses **CVSS v3.1** as the primary scoring system. Auditor-assigned
CVSS scores are the canonical severity; if the auditor does not assign a score,
the engineering triage team assigns one before the finding is classified.

| Severity | CVSS v3.1 base score | Description |
| :-- | :-- | :-- |
| **Critical** | 9.0–10.0 | Attacker-controlled RCE, reliable data exfiltration of redacted content, signature forgery without key material, or plaintext credential exfiltration from an installed product. **Must be fixed before public launch.** |
| **High** | 7.0–8.9 | Significant data leakage, partial redaction bypass, signature bypass with limited attacker capability, or exploitable memory corruption requiring user-supplied PDF. |
| **Medium** | 4.0–6.9 | Information disclosure not directly actionable, improper privilege/scope (e.g., `CRED_PERSIST_LOCAL_MACHINE` vs current-user), subprocess argument-injection with constrained impact, or sanitization gaps for uncommon metadata vectors. |
| **Low** | 0.1–3.9 | Cosmetic or theoretical risks with no practical exploitation path in the current threat model: unauthenticated audit log (T-TMP-3), timeout absence for a rarely-used subprocess. |
| **Informational** | n/a | No CVSS score. Design observations, code-quality notes, hardening recommendations without an exploitable condition. |

**Additional classification tags (apply alongside severity):**

| Tag | Meaning |
| :-- | :-- |
| `KNOWN` | Finding was pre-disclosed in the threat model (T-RED-1, T-API-2). Severity is confirmed or revised by auditor; remediation still required. |
| `BLOCKER` | Must be resolved before GlyphPDF public launch regardless of CVSS score (e.g., any Critical; T-RED-1 image-redaction gap). |
| `UPSTREAM` | Root cause is in a third-party library; upstream coordination required alongside local mitigation. |
| `ASPIRATIONAL-CLAIM` | Corresponds to a security claim in `security-claims.md` rated ASPIRATIONAL — finding confirms the claim is unproven. |

---

## 2. Response SLA

SLA starts from the date the **final audit report** is delivered (not the
preliminary draft).

| Severity | SLA | Pre-launch requirement |
| :-- | :-- | :-- |
| **Critical** | **Fix deployed within 7 days.** Public disclosure (coordinated with auditor) only after fix is confirmed deployed. | **Launch is BLOCKED** until all Critical findings are resolved and auditor confirms. |
| **High** | Fix or accepted mitigation within **14 days**. | Launch is **strongly recommended to block** on High findings related to redaction (T-RED-1 if confirmed High+) or key storage. |
| **Medium** | Fix or accepted mitigation within **30 days** (may land in next scheduled release). | Does not block launch, but must be on the public roadmap with a committed release date. |
| **Low** | Next scheduled release cycle (no hard SLA, target within **90 days**). | Does not block launch. |
| **Informational** | No SLA. Track as enhancement backlog items. | Does not block launch. |

**SLA exceptions:**
- If a finding is marked `UPSTREAM` and the upstream fix is not yet available,
  document a local mitigation (workaround, feature gate, or fail-closed
  behavior) and communicate the upstream status publicly.
- If fixing a Critical finding requires a major architectural change, agree a
  mutually acceptable interim mitigation with the auditor before the 7-day
  deadline — do not let the SLA expire without a documented interim control.

---

## 3. Owner assignment by area

When a finding arrives, immediately assign an engineering owner based on the
affected file/module:

| Affected area | Primary path(s) | Engineering owner |
| :-- | :-- | :-- |
| **Redaction** | `src/engines/podofo/PoDoFoBackend.cpp` (`applyRedactions`, `redactCanvasRecursively`) | [INSERT redaction owner] |
| **Signatures** | `src/engines/SignatureManager.cpp` | [INSERT signature owner] |
| **PDF parsing / encryption / sanitization** | `src/engines/podofo/PoDoFoBackend.cpp` (all other areas) | [INSERT parsing owner] |
| **AI providers / credential storage** | `src/engines/ai/`, `src/core/CredentialManager.cpp` | [INSERT AI/secrets owner] |
| **Subprocess invocation** | Subprocess call sites (soffice, veraPDF invocations) | [INSERT subprocess owner] |
| **Auto-update** | `UpdateChecker` | [INSERT update owner] |
| **Build / packaging** | `CMakeLists.txt`, `packaging/` | [INSERT build owner] |
| **Cross-cutting / architecture** | Multiple files | Security lead (directly) |

---

## 4. Triage workflow — step by step

### Step 1: Receive and log the report

- [ ] Final report received: `Date: _______________`
- [ ] Assign each finding a GlyphPDF-internal ID: `GLPH-SEC-001`, `GLPH-SEC-002`, …
- [ ] Record findings in the tracking sheet (GitHub Security Advisories /
      private issue tracker / [INSERT tool]).
- [ ] Map each finding to a threat-model ID where applicable (T-RED-1, T-API-2,
      etc.).

### Step 2: Severity review

- [ ] For each finding: confirm or adjust the CVSS v3.1 base score.
  - If the auditor and internal team disagree on severity, escalate to the
    security lead. The auditor's score is the starting position; adjustments
    require written justification.
- [ ] Apply tags (KNOWN, BLOCKER, UPSTREAM, ASPIRATIONAL-CLAIM).
- [ ] Identify all BLOCKER findings. If any BLOCKER is unresolved, launch is
      blocked.

### Step 3: Assign owners and schedule

- [ ] Assign engineering owner per Section 3 for each finding.
- [ ] Schedule fix completion based on SLA in Section 2.
- [ ] For Critical/High findings: owner must acknowledge assignment within
      **24 hours** of report receipt.

### Step 4: Communicate with the auditor

- [ ] Send auditor a triage summary within **3 business days** of report receipt:
  - Acknowledgment of each finding.
  - Agreed severity (or note of disagreement with rationale).
  - Owner assignment and planned fix date.
  - Questions on reproduction / root cause for any unclear findings.
- [ ] Schedule a 30-minute call with the auditor if any finding needs live
      clarification (especially for Critical findings and T-RED-1 PoC
      walkthrough).

### Step 5: Remediation

- [ ] For each finding: create a remediation record using
      `remediation-template.md`.
- [ ] Fix is committed to a private branch; branch is reviewed and merged.
- [ ] Fix commit SHA recorded in the remediation record.
- [ ] For Critical/High: request auditor re-test confirmation after fix.
      (Include re-test in SOW if pre-arranged; otherwise negotiate.)

### Step 6: Auditor sign-off and letter

- [ ] Auditor reviews fixes and updates the remediation status in the report.
- [ ] Auditor issues the signed audit letter (`GlyphPDF — Security Audit
      Summary`) once all BLOCKER findings are resolved and confirmed.
- [ ] Signed letter received: `Date: _______________`
- [ ] Letter added to website and README per the disclosure policy.

### Step 7: Public disclosure

- [ ] Coordinate with auditor on the disclosure date.
- [ ] Update `SECURITY.md` with the disclosure per `disclosure-policy-draft.md`.
- [ ] Publish release notes crediting the audit.
- [ ] For any finding that constitutes a CVE-eligible vulnerability:
      request CVE from MITRE / NVD per the disclosure policy.

---

## 5. Communication protocol during remediation

| Situation | Protocol |
| :-- | :-- |
| Critical finding fix ready | Notify auditor immediately; request re-test within 48 hours |
| High finding fix ready | Notify auditor at next scheduled check-in; request re-test at auditor's convenience within the SLA window |
| Fix requires architecture change (beyond SLA) | Communicate to auditor in writing within SLA; document interim mitigation; agree revised date |
| Auditor identifies new finding during remediation review | Treated as a new finding; triaged from scratch |
| Upstream library fix needed | Open upstream issue; document in remediation record; local mitigation required within SLA; track upstream separately |

**Primary communication channel with auditor during remediation:**
[INSERT — e.g., encrypted email to audit lead; private GitHub repo issue;
Slack shared channel]

---

## 6. Known pre-disclosed findings — initial triage note

The following findings are **expected** from the threat model and should be
triaged at BLOCKER severity regardless of the auditor's CVSS score:

| Expected finding | Threat ID | Pre-audit severity | Expected auditor action |
| :-- | :-- | :-- | :-- |
| Image/vector content recoverable under redaction box | T-RED-1 | Critical (KNOWN) | PoC + fix recommendation required |
| Plaintext QSettings key fallback in AnthropicProvider | T-API-2 | High (KNOWN) | Confirm reachability; recommend removal or encryption of fallback |
| Auto-update SHA-256 provenance unverified | T-SPF-2 | High (ASPIRATIONAL) | Confirm manifest channel trustworthiness |
| AI provider data egress without explicit opt-in | T-API-1 | High | Confirm reachability in shipped UI |

If the auditor does NOT flag these, explicitly ask them to re-examine the
relevant code paths — absence of a finding for a known issue requires a written
explanation in the report.
