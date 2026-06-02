# GitHub Milestone and Label Setup for v1.0.0 Beta Bash

This document is a step-by-step guide for the bash lead to configure the GitHub repository
before distributing the beta to testers. Complete all steps in order.

---

## Step 1 — Create the Beta Bash Milestone

1. Navigate to the repository on GitHub.
2. Click the "Issues" tab, then click "Milestones" (top right area near the Issues list).
3. Click "New milestone".
4. Fill in:
   - **Title:** `v1.0.0 Beta Bash`
   - **Due date:** Set to Day 5 of the bash period (e.g., 2026-06-07 if bash starts
     2026-06-02).
   - **Description:**
     ```
     Five-day internal beta bash for GlyphPDF v1.0.0. Ship gate: zero Critical and zero
     High severity bugs open at close. All release-blocker issues must be resolved or
     explicitly deferred with a written rationale before M8 public launch.
     ```
5. Click "Create milestone".

You will assign every bug filed during the bash to this milestone so the milestone
progress bar tracks remaining open issues against the ship gate.

---

## Step 2 — Create GitHub Labels

Navigate to Issues -> Labels -> New label. Create each label below.

| Label name | Color (hex) | Description |
|------------|-------------|-------------|
| `beta-feedback` | `#0075ca` (blue) | Bug or feedback filed during the v1.0.0 beta bash |
| `release-blocker` | `#d93f0b` (red) | Must be fixed before v1.0.0 ships (Critical or High severity) |
| `regression` | `#e4e669` (yellow) | Worked in a prior internal build; broken in beta build |
| `ui-polish` | `#cfd3d7` (light gray) | Cosmetic / UX improvement; does not block ship |
| `needs-repro` | `#f9d0c4` (light orange) | Cannot reproduce — awaiting additional info from reporter |
| `duplicate` | `#cfd3d7` (gray) | Duplicate of an existing issue; link to canonical in comment |
| `deferred-v1.1` | `#bfd4f2` (light blue) | Acknowledged; deferred to v1.1 backlog |
| `wont-fix` | `#eeeeee` (light gray) | Not a bug or out of scope; closed with explanation |

If labels from this list already exist in the repository, update their descriptions to
match the table above for consistency during the bash.

---

## Step 3 — Configure the Bug Report Template

Ensure `bug-report-template.md` (this repository, `docs/testing/beta-bash/`) is copied
into `.github/ISSUE_TEMPLATE/bug_report.md`. This makes the template auto-load when a
tester clicks "New Issue -> Bug Report".

You do NOT need to do this if testers will copy-paste the template manually from the
onboarding guide. Either workflow is acceptable; the auto-template approach reduces
friction.

---

## Step 4 — Issue Triage Process

Run triage once per day during the bash (ideally at the same time each day).

### Daily Triage Checklist

1. Open the milestone view: Issues -> Milestones -> v1.0.0 Beta Bash.
2. For each new issue filed since the last triage:
   a. Read the reproduction steps. If incomplete, add the `needs-repro` label and
      comment asking for more detail. Do not assign severity until reproducible.
   b. Confirm or adjust the severity the tester assigned.
   c. Apply the correct label set: always `beta-feedback`; add `release-blocker` for
      Critical or High; add `regression` if applicable.
   d. Assign the issue to the milestone: `v1.0.0 Beta Bash`.
   e. Assign to a developer if the bug is Critical or High.
3. For issues assigned to a developer: ensure they have acknowledged the issue with a
   comment (even just "Reproduced, investigating") within 24 hours of filing.
4. Close issues marked `needs-repro` if no response from reporter after 48 hours
   (comment explaining closure; testers can re-open with more info).

### Escalation

- Critical bugs must be acknowledged by a developer within 4 hours of filing.
- If a Critical bug is filed after business hours, ping the on-call developer
  directly (do not wait for the daily triage).

---

## Step 5 — Ship Gate Criteria

The v1.0.0 Beta Bash milestone is passed and the project may proceed to M8 (public
launch) when ALL of the following are true:

### Hard gates (no exceptions)

| Gate | Criterion |
|------|-----------|
| Release blockers | Zero open issues with label `release-blocker` |
| Critical bugs | Zero open issues with severity = Critical |
| High severity bugs | Zero open issues with severity = High (OR: each remaining High has an explicit `deferred-v1.1` label with a written rationale explaining why it is safe to defer) |

### Soft gates (documented exceptions allowed)

| Gate | Criterion |
|------|-----------|
| Medium bugs | All triaged; each either fixed, assigned to v1.0 patch, or labeled `deferred-v1.1` |
| Low bugs | All logged; no fix required for ship; labeled `ui-polish` or `deferred-v1.1` |
| Tester sign-off | At least 2 of 3 tester cohorts confirm via written message that the product is "usable for professional workflows" |
| ctest | All 12 ctest targets pass on the release candidate build (run `ctest --output-on-failure` on the final MSI source) |

### Formal sign-off

At the close of Day 5, the bash lead produces a one-paragraph "Bash Closed" comment on
the milestone and tags it with the date. This comment becomes the formal record that the
ship gate was passed (or documents explicitly why any open High bugs were accepted as
known limitations).

---

## Step 6 — Post-Bash Backlog Migration

After the bash is closed:

1. All remaining open issues in the milestone that are labeled `deferred-v1.1`:
   - Remove from `v1.0.0 Beta Bash` milestone.
   - Add to `v1.1.0` milestone (create it if it does not exist).
   - Leave labels intact for tracking.
2. Close the `v1.0.0 Beta Bash` milestone on GitHub once all remaining issues are
   migrated or closed.
3. Export the milestone issue list (Issues -> Milestones -> v1.0.0 Beta Bash -> closed)
   as a reference for the M8 launch retrospective.
