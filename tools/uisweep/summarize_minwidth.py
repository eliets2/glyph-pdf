#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# ui-fix lane helper: summarize a minwidth-<tag>.json per-screen width budget.
# Usage: python summarize_minwidth.py <out-dir> <tag>
import json
import sys


def main():
    out, tag = sys.argv[1], sys.argv[2]
    with open(f"{out}/minwidth-{tag}.json", encoding="utf-8") as f:
        d = json.load(f)
    print(f"windowMinW/H at open: {d['windowMinW']}x{d['windowMinH']}")
    hdr = f"{'id':>13}  {'winMinW':>7}  {'winMinH':>7}  {'actualW':>7}  {'rowMin':>6}  center"
    print(hdr)
    for s in d["screens"]:
        c = s["center"]
        r = s["rightPanel"]
        print(f"{s['id']:>13}  {s['windowMinW']:>7}  {s['windowMinH']:>7}  "
              f"{s['windowActualW']:>7}  {s['rowMinFresh']:>6}  "
              f"{c['class']} minW={c['minW']} | right={r['class']}/{r['name']} minW={r['minW']}")
    if len(sys.argv) > 3 and sys.argv[3] == "--leaves":
        for s in d["screens"]:
            print(f"\n== {s['id']} center leaves:")
            for leaf in s["center"]["leaves"]:
                print(f"   {leaf['minW']:>5}  {leaf['path']}")
            print(f"   right leaves:")
            for leaf in s["rightPanel"]["leaves"]:
                print(f"   {leaf['minW']:>5}  {leaf['path']}")
            tall = s["center"].get("tall", [])
            if tall:
                print(f"   tall:")
                for leaf in tall:
                    print(f"   {leaf['minH']:>5}  {leaf['path']}")


if __name__ == "__main__":
    main()
