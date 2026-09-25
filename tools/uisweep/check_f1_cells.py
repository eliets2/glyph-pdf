#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# ui-fix lane helper: F1 PASS check over the run_matrix audit JSONs.
# Prints per-cell task-screen grab sizes and the F1 verdict; exits 1 if any
# 1366x768 cell has a screen that grew past the requested size.
import glob
import json
import os
import sys

TASK_IDS = {"compare", "batch", "ocr", "redact", "signature", "measure",
            "accessibility", "form", "pdfa", "pages"}


def main():
    sweep = sys.argv[1] if len(sys.argv) > 1 else ".context/ui-sweep"
    cells = sorted(glob.glob(os.path.join(sweep, "*", "audit-*.json")))
    if not cells:
        print("no audit-*.json found under", sweep)
        return 2
    f1_fail = False
    for path in cells:
        tag = os.path.basename(path)[len("audit-"):-len(".json")]
        with open(path, encoding="utf-8") as f:
            d = json.load(f)
        req = d.get("requestedSize", {})
        # grabs are DEVICE pixels; offscreen grabs scale by QT_SCALE_FACTOR, so
        # the honest overflow test is grab > requested*scale (logical min size).
        scale = float(d.get("qtScaleFactorEnv", 1.0) or 1.0)
        rows = []
        worst = 0
        scr = d.get("screens", {})
        screen_list = scr.get("screens", []) if isinstance(scr, dict) else scr
        for s in screen_list:
            if not isinstance(s, dict):
                continue
            sid = s.get("id", s.get("currentScreen", "?"))
            g = s.get("grabSize", {})
            gw, gh = g.get("w") or 0, g.get("h") or 0
            grew = gw > req.get("w", 0) * scale + 1 or gh > req.get("h", 0) * scale + 1
            rows.append((sid, gw, gh, grew))
            if grew and sid in TASK_IDS:
                worst += 1
        f1 = "PASS" if worst == 0 else f"FAIL({worst})"
        if worst and req.get("w") == 1366:
            f1_fail = True
        print(f"\n== cell {tag}  requested={req.get('w')}x{req.get('h')} "
              f"scale={scale}  task-screens overflow (device-px vs req*scale): {worst}  F1({tag}): {f1}")
        for sid, w, h, grew in rows:
            mark = "GREW" if grew else "ok"
            print(f"   {sid:>14}  {w}x{h}  {mark}")
    print("\nF1 verdict (1366 cells):", "FAIL" if f1_fail else "PASS")
    return 1 if f1_fail else 0


if __name__ == "__main__":
    sys.exit(main())
