#!/usr/bin/env python3
"""
triage_dedup.py -- CASR-style crash dedup/bucketing WITHOUT CASR.

CASR (github.com/ispras/casr) is the preferred triage suite, but it is not
installed in this MSYS2/ucrt64 environment and needs SYS_PTRACE (Linux). This
is a portable stand-in: it buckets crash logs by a normalized top-N stack hash
and emits a deterministic repro line per bucket. When CASR is available, prefer:
    casr-libfuzzer -i fuzz/findings -o fuzz/casr_out -- ./fuzz/bin/djot_fuzzer
    casr-cluster -d fuzz/casr_out         # dedup
    casr-cluster -c fuzz/casr_out clusters

Usage:
    python triage_dedup.py <crash_log_dir>     # logs = *.txt sanitizer output
Emits buckets.json: {stack_hash: {count, first_user_frame, sample, severity_hint}}
"""
import hashlib, json, os, re, sys

FRAME_RE = re.compile(r'#\d+\s+0x[0-9a-f]+\s+in\s+([^\s(]+)')
PROJ = re.compile(r'(PoDoFoBackend|LuaDjotCodec|PdfEditorEngine|PatternRedactor|djot)')

def normalize_frames(text):
    frames = FRAME_RE.findall(text)
    # strip libc/runtime frames; keep top 5
    keep = [f for f in frames if not f.startswith(('__', 'operator new', 'malloc'))]
    return keep[:5]

def severity_hint(text):
    t = text.lower()
    if 'heap-buffer-overflow' in t and 'write' in t: return 'HIGH (heap write OOB)'
    if 'use-after-free' in t: return 'HIGH (UAF)'
    if 'double-free' in t: return 'HIGH (double-free)'
    if 'heap-buffer-overflow' in t: return 'MED (heap read OOB)'
    if 'null' in t or 'segv on unknown address 0x000' in t: return 'LOW (null-deref/DoS)'
    if 'out-of-memory' in t or 'allocation-size-too-big' in t: return 'LOW (alloc DoS)'
    return 'UNKNOWN'

def main():
    if len(sys.argv) < 2:
        print("usage: triage_dedup.py <dir>"); sys.exit(2)
    d = sys.argv[1]
    buckets = {}
    for fn in os.listdir(d):
        if not fn.endswith('.txt'): continue
        text = open(os.path.join(d, fn), 'r', errors='replace').read()
        frames = normalize_frames(text)
        h = hashlib.sha1('|'.join(frames).encode()).hexdigest()[:12]
        first_user = next((f for f in frames if PROJ.search(f)), frames[0] if frames else '?')
        b = buckets.setdefault(h, {"count":0, "first_user_frame":first_user,
                                   "sample":fn, "severity_hint":severity_hint(text),
                                   "frames":frames})
        b["count"] += 1
    print(json.dumps(buckets, indent=2))
    json.dump(buckets, open(os.path.join(d,"buckets.json"),"w"), indent=2)

if __name__ == "__main__":
    main()
