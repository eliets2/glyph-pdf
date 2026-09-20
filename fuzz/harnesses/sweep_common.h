// SPDX-License-Identifier: Apache-2.0
// sweep_common.h — shared scaffolding for the W1 ponytail-sweep drivers
// (feat/sweep-w1-fuzz). Deterministic in-process mutation loop, per-input
// watchdog budget, single-seed repro + materialize modes.
//
// Reuse (rung 2): one copy of the campaign/watchdog scaffolding for all five
// W1 harnesses; each driver only supplies runOne(data) -> verdict string.
//
// Driver CLI (identical for every W1 harness):
//   <drv> campaign <corpusDir> [mutsPerSeed=32] [budgetMs=10000]
//       For every file (sorted) and every mutation index 0..K-1: mutate,
//       run, print "EXEC <file> <idx> <verdict>". A hard death (crash) or
//       watchdog TIMEOUT leaves the guilty input as the LAST printed line.
//   <drv> one <file> [mutIdx]
//       Run a single input (raw bytes, or the deterministic mutant) — the
//       single-seed repro mode. Exit 0 = no finding, 42 = finding verdict,
//       53 = watchdog timeout, else crash signal.
//   <drv> materialize <file> <mutIdx> <outPath>
//       Write the deterministic mutant bytes to <outPath> (seed preservation).
//
// No libFuzzer/AFL on this toolchain (MSYS2 ucrt64 g++ 16.x, no compiler-rt);
// this follows the G19 rig pattern: driver exes + seeded corpora + oracles
// (see fuzz/MANIFEST.md).
#ifndef SWEEP_COMMON_H
#define SWEEP_COMMON_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <direct.h>
#include <windows.h>

namespace sweep {

// ── deterministic xorshift64 PRNG (same sequence for the same seed) ─────────
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed)
        : s(seed ? seed : 0x9E3779B97F4A7C15ull) {}
    uint64_t next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s;
    }
};

inline uint64_t fnv1a(const std::vector<uint8_t>& b) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint8_t c : b) { h ^= c; h *= 0x100000001B3ull; }
    return h;
}

// ── deterministic byte-level mutator (havoc-lite) ───────────────────────────
inline std::vector<uint8_t> mutate(const std::vector<uint8_t>& in,
                                   uint64_t seedHash, int idx) {
    std::vector<uint8_t> b = in;
    if (b.empty()) b = {'{', '}'};
    Rng rng(seedHash + (uint64_t)idx * 0xBF58476D1CE4E5B9ull + 1);
    const int nOps = (int)(rng.next() % 6u) + 1;   // 1..6 operations
    for (int op = 0; op < nOps && !b.empty(); ++op) {
        switch (rng.next() % 7) {
        case 0:  // single-bit flip
            b[rng.next() % b.size()] ^= (uint8_t)(1u << (rng.next() % 8));
            break;
        case 1:  // byte overwrite
            b[rng.next() % b.size()] = (uint8_t)rng.next();
            break;
        case 2: {  // ASCII chunk overwrite
            size_t p = rng.next() % b.size();
            size_t l = rng.next() % 64 + 1;
            for (size_t i = 0; i < l && p + i < b.size(); ++i)
                b[p + i] = (uint8_t)('A' + rng.next() % 64);
            break;
        }
        case 3: {  // truncate second half from a point
            size_t p = rng.next() % b.size();
            if (p + 1 < b.size())
                b.erase(b.begin() + (long)p + 1, b.end());
            break;
        }
        case 4: {  // duplicate a chunk in place (deepens nesting / dup keys)
            size_t p = rng.next() % b.size();
            size_t l = rng.next() % 128;
            size_t realL = l < (b.size() - p) ? l : (b.size() - p);
            std::vector<uint8_t> chunk(b.begin() + (long)p,
                                       b.begin() + (long)(p + realL));
            b.insert(b.begin() + (long)p, chunk.begin(), chunk.end());
            break;
        }
        case 5: {  // insert a run of bytes
            size_t run = rng.next() % 4096;
            uint8_t v = (uint8_t)rng.next();
            b.insert(b.begin() + (long)(rng.next() % b.size()), run, v);
            break;
        }
        case 6:  // hard truncate
            b.resize(rng.next() % b.size());
            break;
        }
        if (b.size() > (4u << 20)) b.resize(4u << 20);  // mutant cap: 4 MiB
    }
    return b;
}

// ── file IO ──────────────────────────────────────────────────────────────────
inline bool readFileBytes(const std::string& path, std::vector<uint8_t>* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out->assign(std::istreambuf_iterator<char>(f),
                std::istreambuf_iterator<char>());
    return true;
}
inline bool writeFileBytes(const std::string& path,
                           const std::vector<uint8_t>& b) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(b.data()), (std::streamsize)b.size());
    return (bool)f;
}
inline std::vector<std::string> listFiles(const std::string& dir) {
    std::vector<std::string> out;
    std::string pattern = dir + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            out.push_back(dir + "\\" + fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

// ── per-input watchdog: hard-exits the process on budget overrun ────────────
struct Watchdog {
    std::atomic<uint64_t> deadlineMs{0};
    std::atomic<bool> stop{false};
    std::string lastLabel;                 // guarded by the exit path only
    std::thread th;

    void start() {
        th = std::thread([this] {
            while (!stop.load(std::memory_order_relaxed)) {
                uint64_t d = deadlineMs.load(std::memory_order_relaxed);
                if (d != 0) {
                    uint64_t now = (uint64_t)GetTickCount64();
                    if (now > d) {
                        fprintf(stderr, "TIMEOUT budget exceeded at: %s\n",
                                lastLabel.c_str());
                        fflush(stderr);
                        _Exit(53);   // 53 = budget kill
                    }
                }
                Sleep(20);
            }
        });
    }
    void arm(uint64_t budgetMs, const std::string& label) {
        lastLabel = label;
        deadlineMs.store(GetTickCount64() + budgetMs, std::memory_order_relaxed);
    }
    void disarm() { deadlineMs.store(0, std::memory_order_relaxed); }
    ~Watchdog() { stop.store(true); if (th.joinable()) th.join(); }
};

inline Watchdog g_watch;

// ── driver main (shared shape; per-harness runOne supplied via macro/use) ───
// Returns process exit code.
template <typename RunOne>
int driverMain(int argc, char** argv, RunOne runOne) {
    if (argc < 2) {
        fprintf(stderr,
                "usage: %s campaign <corpusDir> [muts=32] [budgetMs=10000]\n"
                "       %s one <file> [mutIdx]\n"
                "       %s materialize <file> <mutIdx> <out>\n",
                argv[0], argv[0], argv[0]);
        return 64;
    }
    const std::string mode = argv[1];

    if (mode == "materialize" && argc == 5) {
        std::vector<uint8_t> seed;
        if (!readFileBytes(argv[2], &seed)) { perror("seed"); return 65; }
        auto m = mutate(seed, fnv1a(seed), atoi(argv[3]));
        if (!writeFileBytes(argv[4], m)) { perror("out"); return 65; }
        printf("MATERIALIZED %s (%zu bytes)\n", argv[4], m.size());
        return 0;
    }

    if (mode == "one" && (argc == 3 || argc == 4)) {
        std::vector<uint8_t> seed;
        if (!readFileBytes(argv[2], &seed)) { perror("seed"); return 65; }
        std::vector<uint8_t> data =
            argc == 4 ? mutate(seed, fnv1a(seed), atoi(argv[3])) : seed;
        g_watch.start();
        g_watch.arm(120000, argv[2]);   // generous single-input budget
        const char* v = runOne(data);
        g_watch.disarm();
        printf("VERDICT %s\n", v ? v : "OK");
        return (v && strncmp(v, "FINDING", 7) == 0) ? 42 : 0;
    }

    if (mode == "campaign" && (argc == 3 || argc == 4 || argc == 5)) {
        const int muts = argc >= 4 ? atoi(argv[3]) : 32;
        const uint64_t budgetMs =
            argc >= 5 ? (uint64_t)strtoull(argv[4], nullptr, 10) : 10000ull;
        auto files = listFiles(argv[2]);
        if (files.empty()) { fprintf(stderr, "no corpus files in %s\n", argv[2]); return 66; }
        g_watch.start();
        long execs = 0, findings = 0;
        for (const auto& f : files) {
            std::vector<uint8_t> seed;
            if (!readFileBytes(f, &seed) || seed.empty()) continue;
            const uint64_t h = fnv1a(seed);
            for (int i = 0; i < muts; ++i) {
                auto m = mutate(seed, h, i);
                const std::string label =
                    f + " #" + std::to_string(i) + " (" +
                    std::to_string(m.size()) + "B)";
                g_watch.arm(budgetMs, label);
                const char* v = runOne(m);
                g_watch.disarm();
                ++execs;
                if (v && strncmp(v, "FINDING", 7) == 0) {
                    ++findings;
                    printf("EXEC %s #%d %s\n", f.c_str(), i, v);
                } else {
                    printf("EXEC %s #%d %s\n", f.c_str(), i, v ? v : "OK");
                }
                fflush(stdout);
            }
        }
        printf("SUMMARY execs=%ld findings=%ld\n", execs, findings);
        return 0;
    }

    fprintf(stderr, "bad args\n");
    return 64;
}

}  // namespace sweep

#endif  // SWEEP_COMMON_H
