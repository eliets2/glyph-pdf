#!/usr/bin/env bash
# Create recommended GitHub labels for GlyphPDF.
# Run once after the repo goes public: bash docs/github-setup.sh
# Requires: gh auth login

set -e

REPO="eliets2/glyph-pdf"

gh label create "release-blocker"  --repo "$REPO" --color "d93f0b" --description "Must be fixed before v1.0.0 ships" --force
gh label create "bug"              --repo "$REPO" --color "d73a4a" --description "Something isn't working" --force
gh label create "enhancement"      --repo "$REPO" --color "a2eeef" --description "New feature or improvement" --force
gh label create "good first issue" --repo "$REPO" --color "7057ff" --description "Good for newcomers" --force
gh label create "help wanted"      --repo "$REPO" --color "008672" --description "Extra attention needed" --force
gh label create "documentation"    --repo "$REPO" --color "0075ca" --description "Documentation improvements" --force
gh label create "security"         --repo "$REPO" --color "e4e669" --description "Security finding or hardening" --force
gh label create "v1.1-roadmap"     --repo "$REPO" --color "c5def5" --description "Planned for v1.1 (Cloud Sync / macOS / mobile)" --force

echo "Labels created for $REPO"
