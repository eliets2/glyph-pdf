# CHANGELOG Correction — 2026-07-01

## §9.3 P0: false "file attachments" claim in v1.3.0

**CHANGELOG.md line 149 currently reads:**

```
- Annotation **Stamp, Callout, Erase** tools and comment **file attachments**.
```

**Finding (docs/audit/COMPARISON-TABLES-2026-07-01.md §9.3):** comment file
attachments were never implemented. `AnnotationItem::attachmentPath`
(`src/core/AnnotationTypes.h`) is a dead field — no code sets or reads it. The
ribbon's own attach tool is explicitly disabled/planned.

**Required replacement line (apply by hand — CHANGELOG.md is outside the
automated-fix sandbox):**

```
- Annotation **Stamp, Callout** tools and an **Erase** placeholder (Erase is not
  yet functional; see Known Issues). Comment **file attachments are NOT
  implemented** — the `attachmentPath` model field exists but no UI sets or
  reads it yet.
```

**Code change shipped with this correction:** `AnnotationItem::attachmentPath`
is now marked as a dead field in-source (`src/core/AnnotationTypes.h`) so the
gap is visible at the implementation site, not only in audit docs.
