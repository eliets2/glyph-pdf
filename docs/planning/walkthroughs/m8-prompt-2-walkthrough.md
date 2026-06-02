# M8-PROMPT-2 — Public Release Governance

**Date:** 2026-06-02 · **Sprint:** 17 of 18 · **Status:** Complete. 32/32 ctest green.

## Goal
Add all OSS governance files (LICENSE, CONTRIBUTING, CODE_OF_CONDUCT, SECURITY, GitHub templates, CI/release workflows, README badges) needed for the public v1.0.0 launch.

## Deliverables

| D | What | Status | Commit |
|---|------|--------|--------|
| D1 | LICENSE (Apache-2.0) + CONTRIBUTING.md | ✅ | `53db6cb` |
| D2 | CODE_OF_CONDUCT, SECURITY, .github/ templates + FUNDING.yml | ✅ | (D2 commit) |
| D3 | README badges + positioning | ✅ | (D3 commit) |
| D4 | CI workflow (ci.yml) + release workflow (release.yml) + docs/github-setup.sh | ✅ | (D4 commit) |
| D5-MEMORY | Walkthrough (this file) | ✅ | (D5 commit) |

## Execution notes

- D2 was initially blocked by a content filter in the autonomous agent (Contributor Covenant text triggered the filter). Completed inline.
- SPDX headers (`// SPDX-License-Identifier: Apache-2.0`) for all `src/` files are **deferred** — the bulk operation on 300+ files is a clean final step before the release tag (M8-P3). It is documented as a follow-up in D1's commit message.
- SECURITY.md is derived from the full disclosure policy at `docs/security/findings-triage/disclosure-policy-draft.md` and explicitly lists the two pre-disclosed limitations (T-RED-1 image redaction gap, T-API-2 plaintext key fallback).

## What's next (M8-P3 pre-launch checklist)

Before tagging v1.0.0:
1. Run `docs/github-setup.sh` to create GitHub labels (requires `gh auth login`)
2. Add SPDX headers to all `src/` files (`find src -name '*.h' -o -name '*.cpp' | xargs -I{} sed -i '1s/^/\/\/ SPDX-License-Identifier: Apache-2.0\n/' {}`)
3. Make the GitHub repo public (Settings → Danger Zone → Change visibility)
4. Enable branch protection on `main` (require PR + CI pass)
5. Set up GitHub Sponsors or OpenCollective, then update FUNDING.yml
6. Build + sign the final MSI: `cd packaging && build-msi.bat` then `signtool sign ...`
7. Run `ctest --repeat-until-fail 5` for the final gate
8. `git tag -a v1.0.0 -m "Release v1.0.0"` + `git push origin v1.0.0` (explicit user confirmation required)
