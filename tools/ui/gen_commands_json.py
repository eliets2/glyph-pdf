#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate resources/commands.json (the CommandRegistry source) from the
UI-redesign handoff's curated command seed.

Usage:
    python tools/ui/gen_commands_json.py <commands.seed.curated.json> [resources/commands.json]

The seed (UI-redesign handoff pack, 2026-09-28) keys every command by the
app's canonical ToolId string (plan 06 section 7.1: "commands.json ids ARE the
existing ToolId strings; no second id space"). It uses the prototype's short
keys; this script writes the schema-1 file the app loads, with readable keys,
review-only bookkeeping dropped (protoIds, decision, home, proposedHome) and a
deterministic layout (sorted keys, stable indentation) so regeneration diffs
stay reviewable.
"""
import json
import sys
from pathlib import Path

SCHEMA = 1

# seed short key -> commands.json key (only fields the Qt app consumes)
FIELD_MAP = {
    "l": "label",
    "n": "name",
    "d": "description",
    "i": "icon",
    "k": "shortcut",
    "a": "action",
    "t": "control",
    "g": "gallery",
    "target": "target",
    "status": "status",
}


def normalize_variant(v):
    out = {}
    for short, long in (("l", "label"), ("n", "name"), ("d", "description"),
                        ("i", "icon"), ("a", "action")):
        if v.get(short):
            out[long] = v[short]
    return out


def normalize(cid, entry):
    out = {}
    for short, long in FIELD_MAP.items():
        value = entry.get(short)
        if value not in (None, ""):
            out[long] = value
    tool_id = entry.get("toolId") or entry.get("proposedToolId")
    if tool_id:
        out["toolId"] = tool_id.replace("ToolId::", "")
    if isinstance(entry.get("m"), list) and entry["m"]:
        out["menu"] = entry["m"]
    if isinstance(entry.get("variants"), list) and entry["variants"]:
        out["variants"] = [normalize_variant(v) for v in entry["variants"]]
    planned = entry.get("planned")
    if isinstance(planned, dict):
        out["planned"] = {"reason": planned.get("r", ""),
                          "alternative": planned.get("alt", "")}
    if entry.get("placeholder"):
        out["placeholder"] = True
    if "label" not in out:
        raise SystemExit(f"{cid}: the seed entry has no label")
    return out


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    seed_path = Path(argv[1])
    out_path = Path(argv[2]) if len(argv) > 2 else Path("resources/commands.json")
    seed = json.loads(seed_path.read_text(encoding="utf-8"))
    commands = seed["commands"] if "commands" in seed else seed
    meta = seed.get("_meta", {})
    doc = {
        "_meta": {
            "schema": SCHEMA,
            "generator": "tools/ui/gen_commands_json.py",
            "source": "UI-redesign handoff: commands.seed.curated.json ("
                      + str(meta.get("date", "2026-09-28")) + ")",
            "rule": "Command ids ARE the canonical ToolId strings "
                    "(plan 06 7.1). Metadata only: no handlers.",
            "internalToolIds": meta.get("internalToolIds", []),
        },
        "commands": {cid: normalize(cid, e) for cid, e in commands.items()},
    }
    text = json.dumps(doc, ensure_ascii=False, indent=1, sort_keys=True) + "\n"
    out_path.write_text(text, encoding="utf-8", newline="\n")
    print(f"wrote {out_path}: {len(doc['commands'])} commands (schema {SCHEMA})")


if __name__ == "__main__":
    main(sys.argv)
