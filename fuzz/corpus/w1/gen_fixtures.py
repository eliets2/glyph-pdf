#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# gen_fixtures.py — W1 sweep seed corpora (feat/sweep-w1-fuzz).
#
# Deterministically regenerates every seed under fuzz/corpus/{signreq,
# batchpreset, policy, a11y, reviewsummary}. The PDFs are written with exact
# xref offsets (PoDoFo loads them without reconstruction, so mutations are the
# only source of xref damage).
#
#   python fuzz/corpus/w1/gen_fixtures.py
from __future__ import annotations
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.dirname(HERE)  # fuzz/corpus


def w(rel: str, data) -> None:
    path = os.path.join(BASE, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if isinstance(data, str):
        data = data.encode("utf-8")
    with open(path, "wb") as f:
        f.write(data)
    print(f"  {rel} ({len(data)} B)")


# ── minimal PDF assembly with exact xref ────────────────────────────────────

class Pdf:
    """Byte-exact minimal PDF builder (1..N objects, one xref, trailer)."""

    def __init__(self) -> None:
        self.objs: dict[int, bytes] = {}
        self.trailer_extra = b""

    def add(self, num: int, body: bytes) -> int:
        self.objs[num] = body
        return num

    def build(self, root: int) -> bytes:
        out = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
        offsets: dict[int, int] = {}
        for num in sorted(self.objs):
            offsets[num] = len(out)
            out += f"{num} 0 obj\n".encode()
            out += self.objs[num]
            out += b"\nendobj\n"
        maxnum = max(self.objs)
        xref_at = len(out)
        out += f"xref\n0 {maxnum + 1}\n".encode()
        out += b"0000000000 65535 f \n"
        for i in range(1, maxnum + 1):
            if i in offsets:
                out += f"{offsets[i]:010d} 00000 n \n".encode()
            else:
                out += b"0000000000 65535 f \n"
        out += (
            f"trailer\n<< /Size {maxnum + 1} /Root {root} 0 R "
            + self.trailer_extra.decode() + " >>\nstartxref\n"
            + f"{xref_at}\n%%EOF\n"
        ).encode()
        return bytes(out)


def name_obj(s: str) -> bytes:
    return b"<< " + s.encode("utf-8") + b" >>"


def page(res: str = "", extra: str = "") -> bytes:
    parts = ["/Type /Page", "/Parent 2 0 R", "/MediaBox [0 0 200 200]"]
    if res:
        parts.append(f"/Resources {res}")
    if extra:
        parts.append(extra)
    return name_obj(" ".join(parts))


# ═══════════════════════════════════ S1: signreq ═══════════════════════════

def gen_signreq() -> None:
    print("[corpus/signreq]")
    valid = {
        "glyphpdf-signrequest": 1,
        "schemaVersion": 1,
        "createdUtc": "2026-09-19T10:00:00Z",
        "preparedUtc": "2026-09-19T10:05:00Z",
        "preparedSha256": "a" * 64,
        "reconfirmedSha256": "",
        "sourcePdf": "contract.pdf",
        "signers": [
            {
                "order": 1, "name": "A. Buyer", "fieldName": "Sig1",
                "anchorPage": 0,
                "anchorRect": {"x": 10, "y": 20, "w": 120, "h": 40},
                "createdField": True, "signed": False, "signedAtUtc": "",
                "signedFieldName": "", "fieldMatch": False,
                "attainedLevel": "", "signatureSummary": "",
            },
            {
                "order": 2, "name": "B. Seller", "fieldName": "Sig2",
                "anchorPage": 1,
                "anchorRect": {"x": 5, "y": 5, "w": 90, "h": 30},
                "createdField": False, "signed": True,
                "signedAtUtc": "2026-09-19T11:00:00Z",
                "signedFieldName": "Sig2", "fieldMatch": True,
                "attainedLevel": "PAdES-B-B", "signatureSummary": "valid",
            },
        ],
    }
    w("signreq/valid.json", json.dumps(valid, indent=2))
    w("signreq/missing_magic.json", json.dumps({k: v for k, v in valid.items()
                                                if k != "glyphpdf-signrequest"}))
    # Magic PRESENT but value not 1 — P1 accepts-invalid probe (value is
    # never verified by the decoder, key presence only).
    w("signreq/magic_int_999.json", json.dumps({**valid, "glyphpdf-signrequest": 999}))
    w("signreq/magic_string.json", json.dumps({**valid, "glyphpdf-signrequest": "one"}))
    w("signreq/magic_bool.json", json.dumps({**valid, "glyphpdf-signrequest": True}))
    w("signreq/magic_null.json", json.dumps({**valid, "glyphpdf-signrequest": None}))
    w("signreq/magic_float.json", json.dumps({**valid, "glyphpdf-signrequest": 1.5}))
    # Duplicate magic keys: Qt's QJsonObject keeps the LAST occurrence.
    w("signreq/magic_dup_last_bad.json",
      '{"glyphpdf-signrequest": 1, "schemaVersion": 1, "signers": [],'
      ' "glyphpdf-signrequest": 2}')
    w("signreq/magic_dup_last_good.json",
      '{"glyphpdf-signrequest": 2, "schemaVersion": 1, "signers": [],'
      ' "glyphpdf-signrequest": 1}')
    for v in (0, -1, 2, 999999):
        w(f"signreq/version_{v}.json", json.dumps({**valid, "schemaVersion": v}))
    w("signreq/version_string.json", json.dumps({**valid, "schemaVersion": "1"}))
    w("signreq/version_float.json", json.dumps({**valid, "schemaVersion": 1.0}))
    w("signreq/version_null.json", json.dumps({**valid, "schemaVersion": None}))
    w("signreq/signers_not_array.json", json.dumps({**valid, "signers": {}}))
    w("signreq/signers_string.json", json.dumps({**valid, "signers": "two"}))
    w("signreq/signer_not_object.json",
      json.dumps({**valid, "signers": ["Alice"]}))
    w("signreq/signer_missing_name.json",
      json.dumps({**valid, "signers": [{"fieldName": "Sig1"}]}))
    w("signreq/signer_empty_fieldname.json",
      json.dumps({**valid, "signers": [{"name": "A", "fieldName": ""}]}))
    w("signreq/signer_name_number.json",
      json.dumps({**valid, "signers": [{"name": 7, "fieldName": "Sig1"}]}))
    # Wrong types at every signer field, one file per probe cluster.
    w("signreq/anchorrect_types.json",
      json.dumps({**valid, "signers": [
          {"name": "A", "fieldName": "S", "anchorRect": "not-a-rect"},
          {"name": "B", "fieldName": "S", "anchorRect": [1, 2, 3, 4]},
          {"name": "C", "fieldName": "S", "anchorRect": {"x": 0, "y": 0, "w": 0, "h": 5}},
          {"name": "D", "fieldName": "S", "anchorRect": {"x": -1, "y": 0, "w": 5, "h": 5}},
          {"name": "E", "fieldName": "S", "anchorRect": {"x": 0, "y": 0, "w": -3, "h": 5}},
          {"name": "F", "fieldName": "S", "anchorRect": {"x": 1e308, "y": 1e308, "w": 1e308, "h": 1e308}},
          {"name": "G", "fieldName": "S", "anchorRect": {"x": "0", "y": 0, "w": 1, "h": 1}},
      ]}))
    w("signreq/bools_as_strings.json",
      json.dumps({**valid, "signers": [
          {"name": "A", "fieldName": "S", "createdField": "yes",
           "signed": "true", "fieldMatch": 1}]}))
    w("signreq/preparedsha_number.json", json.dumps({**valid, "preparedSha256": 12345}))
    w("signreq/preparedsha_empty.json", json.dumps({**valid, "preparedSha256": ""}))
    # Unicode keys/values, path-shaped field names.
    w("signreq/unicode_signer.json",
      json.dumps({**valid, "signers": [
          {"name": "\u00dcn\u00efcode \U0001f600", "fieldName": "\u0421\u0438\u0433/\u043f\u043e\u043b\u0435",
           "anchorPage": 0}]}))
    w("signreq/path_shaped_fields.json",
      json.dumps({**valid, "signers": [
          {"name": "../../etc", "fieldName": "..\\..\\Sig1"},
          {"name": "CON", "fieldName": "NUL"}]}))
    # Massive signer count + huge strings + deep nesting.
    w("signreq/many_signers.json",
      json.dumps({**valid, "signers": [
          {"name": f"S{i}", "fieldName": f"F{i}"} for i in range(5000)]}))
    w("signreq/huge_string.json",
      json.dumps({**valid, "signers": [{"name": "A" * 1000000, "fieldName": "F"}]}))
    w("signreq/deep_nesting.json",
      json.dumps({**valid, "signers": [{"name": "A", "fieldName": "F"}]})
      .replace('"A"', '[' * 2000 + '"' + "A" + '"' + ']' * 2000, 1))
    w("signreq/bom_prefix.json", b"\xef\xbb\xbf" + json.dumps(valid).encode())
    w("signreq/trailing_garbage.json", (json.dumps(valid) + "}}}}").encode())
    w("signreq/not_object.json", b"[1,2,3]")
    w("signreq/empty.json", b"")
    w("signreq/truncated_valid.json",
      json.dumps(valid, indent=2)[: len(json.dumps(valid, indent=2)) // 2].encode())


# ═════════════════════════════════ S2: batchpreset ═════════════════════════

def preset(steps=None, **kw) -> dict:
    p = {
        "glyphpreset": {"schemaVersion": 1, "kind": "batch-preset"},
        "id": "my-preset",
        "name": "My Preset",
        "created": "2026-09-19T10:00:00Z",
        "modified": "2026-09-19T10:00:00Z",
        "steps": steps if steps is not None
        else [{"op": "compress", "params": {"quality": 80}}],
    }
    p.update(kw)
    return p


def gen_batchpreset() -> None:
    print("[corpus/batchpreset]")
    w("batchpreset/valid_minimal.json", json.dumps(preset()))
    w("batchpreset/valid_full.json", json.dumps(preset(
        steps=[
            {"op": "compress", "label": "Shrink", "params": {"quality": 10, "targetDpi": 600}},
            {"op": "strip-metadata", "params": {"sanitize": True, "clearInfoDict": False}},
            {"op": "pdfa-export", "params": {"level": "2b"}},
            {"op": "pdfa-check", "params": {"level": "3u"}},
            {"op": "watermark", "params": {"text": "DRAFT", "opacity": 50}},
            {"op": "redact", "params": {"presets": ["emails"], "patterns": ["X\\d+"],
                                        }},
        ],
        description="d" * 300, authorApp="GlyphPDF 1.3",
        output={"naming": "{basename}_{preset}_{n}_{date}.pdf", "onConflict": "overwrite"},
        onFileFailure="continue",
    )))
    # Range edges — each boundary as its own file.
    for q in (9, 10, 100, 101):
        w(f"batchpreset/quality_{q}.json",
          json.dumps(preset([{"op": "compress", "params": {"quality": q}}])))
    for d in (35, 36, 600, 601):
        w(f"batchpreset/dpi_{d}.json",
          json.dumps(preset([{"op": "compress", "params": {"targetDpi": d}}])))
    for o in (0, 1, 100, 101):
        w(f"batchpreset/opacity_{o}.json",
          json.dumps(preset([{"op": "watermark", "params": {"opacity": o}}])))
    w("batchpreset/watermark_121.json",
      json.dumps(preset([{"op": "watermark", "params": {"text": "W" * 121}}])))
    w("batchpreset/watermark_120.json",
      json.dumps(preset([{"op": "watermark", "params": {"text": "W" * 120}}])))
    w("batchpreset/watermark_blank.json",
      json.dumps(preset([{"op": "watermark", "params": {"text": "   "}}])))
    w("batchpreset/strip_none_true.json",
      json.dumps(preset([{"op": "strip-metadata",
                          "params": {"sanitize": False, "clearInfoDict": False}}])))
    # Unknown keys at every level.
    w("batchpreset/unknown_root_key.json", json.dumps({**preset(), "zzz": 1}))
    w("batchpreset/unknown_envelope_key.json",
      json.dumps({**preset(), "glyphpreset": {"schemaVersion": 1, "kind": "batch-preset",
                                              "extra": 1}}))
    w("batchpreset/unknown_step_key.json",
      json.dumps(preset([{"op": "compress", "zzz": 1, "params": {}}])))
    w("batchpreset/unknown_param_key.json",
      json.dumps(preset([{"op": "compress", "params": {"zzz": 1}}])))
    w("batchpreset/unknown_output_key.json",
      json.dumps(preset(output={"zzz": 1})))
    # Version handshake abuse.
    for v, tag in ((2, "v2"), (0, "v0"), (-1, "vneg"), (1.5, "vfrac")):
        w(f"batchpreset/schema_{tag}.json",
          json.dumps({**preset(), "glyphpreset": {"schemaVersion": v,
                                                  "kind": "batch-preset"}}))
    w("batchpreset/schema_string.json",
      json.dumps({**preset(), "glyphpreset": {"schemaVersion": "1",
                                              "kind": "batch-preset"}}))
    w("batchpreset/kind_wrong.json",
      json.dumps({**preset(), "glyphpreset": {"schemaVersion": 1, "kind": "other"}}))
    w("batchpreset/kind_missing.json",
      json.dumps({"glyphpreset": {"schemaVersion": 1}, **{k: v for k, v in preset().items()
                                                          if k != "glyphpreset"}}))
    # Wrong types at every field.
    w("batchpreset/id_uppercase.json", json.dumps({**preset(), "id": "MyPreset"}))
    w("batchpreset/id_65chars.json", json.dumps({**preset(), "id": "a" * 65}))
    w("batchpreset/name_81.json", json.dumps({**preset(), "name": "N" * 81}))
    w("batchpreset/label_121.json",
      json.dumps(preset([{"op": "compress", "label": "L" * 121, "params": {}}])))
    w("batchpreset/params_not_object.json",
      json.dumps(preset([{"op": "compress", "params": []}])))
    w("batchpreset/params_number_type.json",
      json.dumps(preset([{"op": "compress", "params": {"quality": "80"}}])))
    w("batchpreset/params_float.json",
      json.dumps(preset([{"op": "compress", "params": {"quality": 80.5}}])))
    w("batchpreset/params_bool_for_int.json",
      json.dumps(preset([{"op": "compress", "params": {"quality": True}}])))
    w("batchpreset/params_nested_array.json",
      json.dumps(preset([{"op": "redact", "params": {"patterns": [["a"]], "presets": ["emails"]}}])))
    w("batchpreset/params_huge_string.json",
      json.dumps(preset([{"op": "watermark", "params": {"text": "H" * 1048577}}])))
    w("batchpreset/huge_int.json",
      json.dumps(preset([{"op": "compress", "params": {"quality": 10**20}}])))
    w("batchpreset/negative_steps.json",
      json.dumps(preset([{"op": "unknown-op", "params": {}}])))
    w("batchpreset/steps_empty.json", json.dumps(preset(steps=[])))
    steps17 = [{"op": "compress", "params": {}} for _ in range(17)]
    w("batchpreset/steps_17.json", json.dumps(preset(steps=steps17)))
    w("batchpreset/step_not_object.json", json.dumps(preset(steps=["compress"])))
    w("batchpreset/dates_invalid.json",
      json.dumps({**preset(), "created": "yesterday", "modified": ""}))
    _no_dates = {k: v for k, v in preset().items() if k not in ("created", "modified")}
    w("batchpreset/dates_missing.json", json.dumps(_no_dates))
    # Unimplemented-but-grammar-valid values must be REFUSED (V-diagnostics).
    w("batchpreset/onconflict_rename.json",
      json.dumps(preset(output={"onConflict": "rename"})))
    w("batchpreset/onfilefailure_stop.json",
      json.dumps({**preset(), "onFileFailure": "stop"}))
    # Naming templates — the RENDER path probes.
    for tag, naming in (
        ("default", "{basename}.pdf"),
        ("dots", "..\\..\\{basename}.pdf"),
        ("dots_fwd", "../../{basename}.pdf"),
        ("drive", "C:\\{basename}.pdf"),
        ("sep", "{basename}/{n}.pdf"),
        ("unknown_token", "{hostile}.pdf"),
        ("unclosed", "{basename.pdf"),
        ("empty_token", "{}.pdf"),
        ("no_ext", "{basename}"),
        ("only_ext", ".pdf"),
        ("nested_token", "{{basename}}.pdf"),
        ("huge", "{basename}" + "A" * 60000 + ".pdf"),
        ("date_long", "{date}" * 4000 + ".pdf"),
    ):
        w(f"batchpreset/naming_{tag}.json", json.dumps(preset(output={"naming": naming})))
    # Regex edge cases (redact) — validity only, parse never matches.
    w("batchpreset/regex_invalid.json",
      json.dumps(preset([{"op": "redact", "params": {"patterns": ["( [ "]}}])))
    w("batchpreset/regex_unknown_preset.json",
      json.dumps(preset([{"op": "redact", "params": {"presets": ["not-a-key"]}}])))
    w("batchpreset/regex_empty_all.json",
      json.dumps(preset([{"op": "redact", "params": {"patterns": [""]}}])))
    w("batchpreset/regex_huge.json",
      json.dumps(preset([{"op": "redact", "params": {"patterns": ["(a+)+$"]}}])))
    # 256 KiB file cap.
    w("batchpreset/over_file_cap.json",
      json.dumps(preset()).encode() + b" " * (256 * 1024 + 1))
    w("batchpreset/not_object.json", b"[[]]")
    w("batchpreset/empty.json", b"")


# ═══════════════════════════════════ S5: policy ════════════════════════════

def gen_policy() -> None:
    print("[corpus/policy]")
    valid = {"schemaVersion": 1, "settings": {"signing/tsaUrl": "https://tsa.example/rfc3161",
                                              "signing/padesLevel": "B-B"}}
    w("policy/valid.json", json.dumps(valid))
    w("policy/valid_all_keys.json", json.dumps({"schemaVersion": 1, "settings": {
        "signing/tsaUrl": "https://t", "signing/padesLevel": "B-LT",
        "update/checkOnStartup": True, "update/channel": "stable",
        "ai/ollamaEndpoint": "http://127.0.0.1:11434",
        "ocr/allowNetworkDownload": False}}))
    for v, tag in ((0, "0"), (2, "2"), (-1, "neg")):
        w(f"policy/version_{tag}.json", json.dumps({**valid, "schemaVersion": v}))
    w("policy/version_string.json", json.dumps({**valid, "schemaVersion": "1"}))
    w("policy/version_float.json", json.dumps({**valid, "schemaVersion": 1.9}))
    w("policy/settings_not_object.json", json.dumps({**valid, "settings": []}))
    w("policy/settings_missing.json", json.dumps({"schemaVersion": 1}))
    # Unknown keys are ignored AND disclosed — never enter the allowlist.
    w("policy/unknown_keys.json", json.dumps({"schemaVersion": 1, "settings": {
        "signing/tsaUrl": "https://t", "ui/theme": "dark",
        "files/autoSave": True, "x/y/z": None}}))
    # Wrong value types for KNOWN keys — ignored + disclosed, never managed.
    w("policy/object_value.json", json.dumps({"schemaVersion": 1, "settings": {
        "signing/tsaUrl": {"url": "https://t"}}}))
    w("policy/array_value.json", json.dumps({"schemaVersion": 1, "settings": {
        "signing/padesLevel": ["B-B"]}}))
    w("policy/null_value.json", json.dumps({"schemaVersion": 1, "settings": {
        "update/channel": None}}))
    # Duplicate keys (last wins in Qt) + case variants of known keys.
    w("policy/dup_keys.json",
      b'{"schemaVersion":1,"schemaVersion":1,"settings":{"signing/tsaUrl":"a",'
      b'"signing/tsaUrl":"https://real"}}')
    w("policy/case_variant.json", json.dumps({"schemaVersion": 1, "settings": {
        "Signing/TSAUrl": "https://t"}}))
    # Hostile strings as policy values.
    w("policy/hostile_values.json", json.dumps({"schemaVersion": 1, "settings": {
        "signing/tsaUrl": "javascript:alert(1)",
        "signing/padesLevel": "A" * 100000,
        "update/channel": "\x00\x01\x02"}}))
    w("policy/unicode_keys.json", json.dumps({"schemaVersion": 1, "settings": {
        "signing/\u0442\u0441\u0430Url": "x", "\U0001f600": 1}}))
    w("policy/path_shaped_keys.json", json.dumps({"schemaVersion": 1, "settings": {
        "../signing/tsaUrl": "x", "signing/../../tsaUrl": "y"}}))
    # Structural corruption + size.
    w("policy/not_object.json", b"[]")
    w("policy/truncated.json", json.dumps(valid)[:-4] .encode())
    w("policy/nested_bomb.json",
      ("[" * 2000 + "]" * 2000).encode())
    w("policy/huge_string.json",
      json.dumps({"schemaVersion": 1, "settings": {"signing/tsaUrl": "S" * 5000000}}).encode())
    w("policy/empty.json", b"")
    w("policy/bom.json", b"\xef\xbb\xbf" + json.dumps(valid).encode())


# ═══════════════════════════════════ S3: a11y PDFs ═════════════════════════

def gen_a11y() -> None:
    print("[corpus/a11y]")
    IMG = (b"<< /Subtype /Image /Width 1 /Height 1 /ColorSpace /DeviceGray "
           b"/BitsPerComponent 8 /Length 2 >>\nstream\n\x00\xff\nendstream")

    # Baseline: untagged, no fields.
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    w("a11y/untagged_baseline.pdf", p.build(1))

    # Tagged + /MarkInfo /Marked false + no /Lang + image without /Alt.
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R /StructTreeRoot 4 0 R "
                      "/MarkInfo << /Marked false >>"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page(res="<< /XObject << /Im0 5 0 R >> >>"))
    p.add(4, name_obj("/Type /StructTreeRoot"))
    p.add(5, IMG)
    w("a11y/tagged_markinfo_false_img.pdf", p.build(1))

    # THE CRASH CANDIDATE: /Fields cycle without /FT (pure container cycle).
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R "
                      "/AcroForm << /Fields [5 0 R] >>"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    p.add(5, name_obj("/T (a) /Kids [6 0 R]"))
    p.add(6, name_obj("/T (b) /Kids [5 0 R]"))
    w("a11y/fields_cycle_no_ft.pdf", p.build(1))

    # Cycle WITH /FT and /TU (both set — the else branch still recurses).
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R "
                      "/AcroForm << /Fields [5 0 R] >>"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    p.add(5, name_obj("/T (a) /FT /Tx /TU (a-label) /Kids [6 0 R]"))
    p.add(6, name_obj("/T (b) /FT /Tx /TU (b-label) /Kids [5 0 R]"))
    w("a11y/fields_cycle_with_tu.pdf", p.build(1))

    # Self-referencing field (A.Kids → A).
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R "
                      "/AcroForm << /Fields [5 0 R] >>"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    p.add(5, name_obj("/T (self) /Kids [5 0 R]"))
    w("a11y/fields_self_cycle.pdf", p.build(1))

    # Extreme DEPTH chain (no cycle): 2000 nested containers.
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R "
                      "/AcroForm << /Fields [5 0 R] >>"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    depth = 2000
    for i in range(depth):
        kid = f"{6 + i} 0 R" if i + 1 < depth else "[]"
        p.add(5 + i, name_obj(f"/T (n{i}) /Kids [{kid}]"))
    w("a11y/fields_deep_2000.pdf", p.build(1))

    # Form XObject cycle A ↔ B (the depth-8 cap MUST hold) + an image.
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page(res="<< /XObject << /FormA 5 0 R /Im0 7 0 R >> >>"))
    p.add(5, name_obj("/Type /XObject /Subtype /Form /BBox [0 0 10 10] "
                      "/Resources << /XObject << /FormB 6 0 R >> >> /Length 0"))
    p.add(6, name_obj("/Type /XObject /Subtype /Form /BBox [0 0 10 10] "
                      "/Resources << /XObject << /FormA 5 0 R >> >> /Length 0"))
    p.add(7, IMG)
    w("a11y/formxobject_cycle.pdf", p.build(1))

    # Non-dict /Lang, /Info /Title as number, /ViewerPreferences as number.
    p = Pdf()
    p.trailer_extra = b"/Info 5 0 R"
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R /Lang [not a string] "
                      "/ViewerPreferences 42"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    p.add(5, name_obj("/Title 7 /Author (x)"))
    w("a11y/lang_viewerinfo_types.pdf", p.build(1))

    # /ViewerPreferences /DisplayDocTitle true + /Info /Title present (clean
    # doc-level except /Lang missing).
    p = Pdf()
    p.trailer_extra = b"/Info 5 0 R"
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R "
                      "/ViewerPreferences << /DisplayDocTitle true >>"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    p.add(5, name_obj("/Title (Titled document)"))
    w("a11y/doc_title_ok.pdf", p.build(1))

    # Fields: acyclic /TU-missing + unnamed (empty /T) + number /T.
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R "
                      "/AcroForm << /Fields [5 0 R 6 0 R 7 0 R] >>"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    p.add(5, name_obj("/T (f1) /FT /Tx"))                       # missing /TU
    p.add(6, name_obj("/FT /Tx /TU (has-label)"))               # unnamed but has TU
    p.add(7, name_obj("/T (f3) /FT /Tx /TU ()"))                # EMPTY string TU
    w("a11y/fields_tu_gaps.pdf", p.build(1))

    # /Fields as a dict (not array), /AcroForm as a number — type confusion.
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R "
                      "/AcroForm << /Fields 5 0 R >>"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page())
    p.add(5, name_obj("/T (not-an-array)"))
    w("a11y/fields_wrong_type.pdf", p.build(1))

    # Broken xref (startxref garbage) — reconstruction path.
    good = Pdf()
    good.add(1, name_obj("/Type /Catalog /Pages 2 0 R"))
    good.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    good.add(3, page())
    blob = bytearray(good.build(1))
    idx = blob.rfind(b"startxref")
    blob[idx:idx + 20] = b"startxref\n99999999\n%%EOF\n"
    w("a11y/broken_xref.pdf", bytes(blob))


# ══════════════════════════════ S4: reviewsummary PDFs ═════════════════════

def gen_reviewsummary() -> None:
    print("[corpus/reviewsummary]")

    def with_annots(annots: list[str], res: str = "") -> bytes:
        p = Pdf()
        p.add(1, name_obj("/Type /Catalog /Pages 2 0 R"))
        p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
        refs = " ".join(f"{i} 0 R" for i in range(4, 4 + len(annots)))
        p.add(3, page(res=res, extra=f"/Annots [{refs}]"))
        for i, body in enumerate(annots):
            p.add(4 + i, name_obj(body))
        return p.build(1)

    w("reviewsummary/basic_text.pdf", with_annots([
        "/Subtype /Text /Rect [10 10 110 40] /Contents (hello world) "
        "/T (alice) /M (D:20260919120000Z)"]))

    # Empty /Rect, absent /Rect, mismatched /Rect arrays.
    w("reviewsummary/rect_variants.pdf", with_annots([
        "/Subtype /Text /Rect [] /Contents (empty rect)",
        "/Subtype /Text /Contents (no rect)",
        "/Subtype /Text /Rect [1 2 3] /Contents (three elems)",
        "/Subtype /Text /Rect [1 2 3 4 5 6] /Contents (six elems)",
        "/Subtype /Text /Rect [1e400 2 3 4] /Contents (huge coord)",
        "/Subtype /Text /Rect [10 -900 -110 -940] /Contents (inverted)",
    ]))

    # /Contents as hex string (UTF-16BE with BOM) vs literal, plus escapes.
    w("reviewsummary/contents_hex.pdf", with_annots([
        "/Subtype /Text /Rect [0 0 10 10] /Contents <FEFF00480065006C006C006F>",
        "/Subtype /Text /Rect [0 0 10 10] /Contents <0048656C6C6F>",
        "/Subtype /FreeText /Rect [0 0 10 10] /Contents (parens \\\\\\) escaped)",
    ]))

    # Huge author /T (100 KiB) and huge /Contents.
    w("reviewsummary/huge_strings.pdf", with_annots([
        "/Subtype /Text /Rect [0 0 10 10] /Contents (x) /T (" + "A" * 100000 + ")",
        "/Subtype /Text /Rect [0 0 10 10] /Contents (" + "B" * 100000 + ") /T (bob)",
    ]))

    # Nested popup: annot → popup → popup (chained), plus /Parent back-ref.
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page(extra="/Annots [4 0 R 6 0 R]"))
    p.add(4, name_obj("/Subtype /Text /Rect [0 0 10 10] /Contents (root) "
                      "/Popup 5 0 R"))
    p.add(5, name_obj("/Subtype /Popup /Parent 4 0 R /Popup 6 0 R "
                      "/Rect [0 0 10 10]"))
    p.add(6, name_obj("/Subtype /Popup /Parent 4 0 R /Popup 5 0 R "
                      "/Rect [0 0 10 10]"))
    w("reviewsummary/popup_chain_cycle.pdf", p.build(1))

    # /Measure extremes: huge /C factor, empty /U, odd /Vertices, odd /InkList.
    w("reviewsummary/measure_extremes.pdf", with_annots([
        "/Subtype /Line /Rect [0 0 100 100] /L [0 0 100 100] "
        "/Measure << /Type /Measure /R (1 m = 3 pt) "
        "/X [{1 0 R}] /A [{2 0 R}] >>",
        "/Subtype /PolyLine /Rect [0 0 100 100] /Vertices [1 2 3] "
        "/Measure << /X [{1 0 R}] >>",
        "/Subtype /Ink /Rect [0 0 100 100] /InkList [[1 2 3] []]",
        "/Subtype /Polygon /Rect [0 0 100 100] /Vertices [] "
        "/Measure << /X [{1 0 R}] /A [{2 0 R}] >>",
    ]))
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page(extra="/Annots [4 0 R]"))
    p.add(4, name_obj("/Subtype /Line /Rect [0 0 100 100] /L [0 0 100 100] "
                      "/Measure << /X [<< /C 1e308 /U >>] "
                      "/A [<< /U >>] /R (r) >>"))
    w("reviewsummary/measure_nanish.pdf", p.build(1))

    # /IT dimension intents without /Measure; /GlyphSigMode weirdness.
    w("reviewsummary/intents_and_modes.pdf", with_annots([
        "/Subtype /Line /Rect [0 0 100 100] /L [0 0 50 50] /IT /LineDimension",
        "/Subtype /PolyLine /Rect [0 0 100 100] /Vertices [0 0 1 1] "
        "/IT /PolyLineDimension",
        "/Subtype /Stamp /Rect [0 0 20 20] /GlyphSigMode /Typed",
        "/Subtype /Stamp /Rect [0 0 20 20] /GlyphSigMode /NotARealMode",
        "/Subtype /Stamp /Rect [0 0 20 20] /GlyphSigMode (not-a-name)",
    ]))

    # Review states: /State model names incl. bogus.
    w("reviewsummary/states.pdf", with_annots([
        "/Subtype /Text /Rect [0 0 10 10] /Contents (a) /State /Accepted "
        "/StateModel /Review",
        "/Subtype /Text /Rect [0 0 10 10] /Contents (b) /State /Bogus /StateModel /Review",
        "/Subtype /Text /Rect [0 0 10 10] /Contents (c) /State (string-not-name)",
    ]))

    # /RT reply type + /IRT in-reply-to refs (threaded comments).
    p = Pdf()
    p.add(1, name_obj("/Type /Catalog /Pages 2 0 R"))
    p.add(2, name_obj("/Type /Pages /Kids [3 0 R] /Count 1"))
    p.add(3, page(extra="/Annots [4 0 R 5 0 R]"))
    p.add(4, name_obj("/Subtype /Text /Rect [0 0 10 10] /Contents (root)"))
    p.add(5, name_obj("/Subtype /Text /Rect [0 0 10 10] /Contents (reply) "
                      "/IRT 4 0 R /RT /R"))
    w("reviewsummary/threaded_replies.pdf", p.build(1))


# ════════════════════════════════════════════════════════════════════════════

def main() -> int:
    gen_signreq()
    gen_batchpreset()
    gen_policy()
    gen_a11y()
    gen_reviewsummary()
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
