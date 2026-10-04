# 7-Zip 26.03 bump — download cross-verification evidence

Date: 2026-10-04. Worktree `D:/pdf/pdf-7z26`, branch `feat/7z-2603`.

Two independent downloads of the official x64 installer, then byte-comparison:

| Source | URL | Bytes | SHA-256 |
|---|---|---|---|
| www.7-zip.org | `https://www.7-zip.org/a/7z2603-x64.exe` | 1,661,239 | `0859c524b8a63551848f0c246abddcb1d0b7b656b0fbfe879f8d85e61a9e6edd` |
| GitHub (ip7z/7zip) | `https://github.com/ip7z/7zip/releases/download/26.03/7z2603-x64.exe` | 1,661,239 | `0859c524b8a63551848f0c246abddcb1d0b7b656b0fbfe879f8d85e61a9e6edd` |

**BYTE-IDENTICAL** — `sha256sum` on both files agrees; the two downloads match
exactly. (Files themselves are not committed — reproducible from the URLs
above, integrity given by the hash; consistent with the 26.02 vendoring,
which recorded hashes only.)

## Extraction (SFX member extraction only — installer NOT executed)

Performed by the hash-verified vendored 26.02 binary
(`third_party/7zip/bin/7z.exe`, pin `83967f1b…` at time of extraction):

```
7z x 7z2603-x64.exe 7z.exe 7z.dll License.txt
```

Both copies (www + GitHub download) extracted separately; members
byte-identical across the two extractions:

- `7z.exe` 577,536 bytes — SHA-256 `6ee3c0ed0b27663c1b948ae85a7c0bb073aed1498983182f3f0df1f6a8c30b2f`
- `7z.dll` 1,906,688 bytes — SHA-256 `65e4c1f855f9ef6e8f0f5df8e3f27d9eb5f07311408639da0a1ca0b8f4871b0d`
- `License.txt` 6,031 bytes — SHA-256 `519ac0a4bded9c18ea02e0afb71f663d8c47373bd9facd3ac96a79f51d77765d`
  — **differs in no byte from the committed 26.02 `License.txt`** (license unchanged in 26.03)

Version banner of the extracted `7z.exe`:
`7-Zip 26.03 (x64) : Copyright (c) 1999-2026 Igor Pavlov : 2026-09-03`

## 26.03 release-notes check (a/t CLI surface)

From both `https://github.com/ip7z/7zip/releases/tag/26.03` and
`https://www.7-zip.org/history.txt` (verbatim changelog, 2026-09-03):

- "Improved support for Joliet ISO images and Compound archives."
- "Some bugs and vulnerabilities were fixed."
- "CVE-2026-58052 : 7-Zip failed to preserve the Mark-of-the-Web when
  extracting a crafted archive."

**No CLI changes**: nothing on the `a` (create) / `t` (test) commands, no
switch changes, no encryption/password behavior changes. The CVE fix is
extraction-side only — a surface GlyphPDF never invokes (`7z a` + `7z t`
only), so this bump is version hygiene, not a behavioral fix for the app.

## M-1 password-stdin contract — empirical probe of the NEW 26.03 binary

(do-not-guess clause; `m1-probe-2603/` holds the raw logs)

| Probe | Result |
|---|---|
| `a -tzip -mem=AES256 -p pkg.zip doc.txt` + password on stdin | RC=0, archive created |
| `t pkg.zip` (NO `-p`) + password on stdin | RC=0 (prompt-stdin read-back works) |
| `t pkg.zip` + WRONG password on stdin | RC=2 (fail-closed) |
| `t -p pkg.zip` (bare `-p` trap) + correct password on stdin | RC=2 — bare `-p` on `t` parses as EMPTY password, stdin ignored (unchanged trap; read-back must omit `-p`) |
| `7z l -slt pkg.zip` | `Encrypted = +`, `Method = AES-256 Store` — genuinely AES-256 |

Contract identical to 26.02 on every leg.
