// vmp_solver.cpp
//
// Solves the .vmp format VM placement problem:
//   line 1: instance name
//   line 2: total PM count (upper bound, intentionally generous -- ignored,
//           see LowerBounds.txt instead)
//   line 3: CPU capacity per PM
//   line 4: RAM capacity per PM
//   line 5: total VM count
//   lines 6..: "<cpu_demand> <ram_demand> <ignored>"
//
// All PMs are IDENTICAL capacity here, so this is 2D Vector Bin Packing
// (NP-hard): minimize the number of PMs needed to fit all VMs under both
// CPU and RAM constraints simultaneously.
//
// Strategy: Variable Neighborhood Search (VNS) targeting a fixed bin count.
//   1. k starts at the known lower bound (from LowerBounds.txt).
//   2. attemptFeasiblePacking(vms, k, ..., 5.0) tries, within a 5-second
//      wall-clock budget, to fit every VM into exactly k bins:
//        a. Seed with a plain Best-Fit-Decreasing pack.
//        b. Any VM that doesn't fit goes on an "unplaced" queue, resolved
//           via ejection chains: try a direct move first (N1); if no bin
//           has room, try evicting some other VM from a bin to make room,
//           then recursively find a home for the evicted VM too (N2/N3,
//           depth-limited chain of relocations). This is what lets the
//           search escape local optima that simple whole-PM compaction
//           gets stuck in.
//   3. If the 5s budget runs out with VMs still unplaced, the search has
//      failed for this k -- increment k by 1 and restart FROM SCRATCH
//      (fresh bins, fresh 5s budget). Repeat until a k succeeds.
//
// Compile: g++ -std=c++17 -O2 vmp_solver.cpp -o vmp_solver
// Run:     ./vmp_solver VMP_A100.vmp LowerBounds.txt

#include <bits/stdc++.h>
#include <filesystem>
using namespace std;

// ============================================================================
// Bitmask representation of a PM's VM set.
//
// Convention: bit i (value 2^i) is set  <=>  the VM with id i is assigned to
// this PM. VM ids are 0-based (id 0 is the first VM read from the .vmp file).
// One uint64_t only covers 64 VM ids, so for instances with more VMs than
// that we use a vector of uint64_t "words": word 0 holds VM ids 0-63,
// word 1 holds VM ids 64-127, and so on.
// ============================================================================
using Bitmask = vector<uint64_t>;

// Number of 64-bit words needed to cover VM ids 0..totalVmCount-1.
int bitmaskWordCount(int totalVmCount) {
    return (totalVmCount + 63) / 64;
}

// Build a bitmask from a list of VM ids (used when importing/exporting).
Bitmask toBitmask(const vector<int>& vmIds, int totalVmCount) {
    Bitmask bm(bitmaskWordCount(totalVmCount), 0ULL);
    for (int id : vmIds) {
        bm[id / 64] |= (1ULL << (id % 64));
    }
    return bm;
}

// Inverse: recover the list of VM ids set in a bitmask.
vector<int> fromBitmask(const Bitmask& bm) {
    vector<int> ids;
    for (size_t w = 0; w < bm.size(); ++w) {
        uint64_t word = bm[w];
        while (word) {
            int bit = __builtin_ctzll(word);      // index of lowest set bit
            ids.push_back((int)(w * 64 + bit));
            word &= word - 1;                       // clear lowest set bit
        }
    }
    return ids;
}

// Human-readable form: each word printed as 16 hex digits (64 bits),
// word 0 first (covering the lowest VM ids), space-separated.
string bitmaskToHex(const Bitmask& bm) {
    ostringstream oss;
    for (size_t i = 0; i < bm.size(); ++i) {
        oss << hex << setw(16) << setfill('0') << bm[i];
        if (i + 1 < bm.size()) oss << " ";
    }
    return oss.str();
}

struct VM {
    int id;
    double cpu;
    double ram;
};

struct PM {
    int id;
    double cpuCap, ramCap;
    double cpuUsed = 0, ramUsed = 0;
    Bitmask vms; // bitmask of assigned VM ids -- bit i set <=> VM id i is here.
                 // Must be sized via vms.assign(bitmaskWordCount(totalVmCount), 0)
                 // before use (see makePM below); place()/unplace() index into
                 // it directly by VM id, so an unsized (empty) bitmask will
                 // segfault on the first place() call.

    double cpuFree() const { return cpuCap - cpuUsed; }
    double ramFree() const { return ramCap - ramUsed; }
    bool canFit(const VM& v) const {
        return v.cpu <= cpuFree() + 1e-9 && v.ram <= ramFree() + 1e-9;
    }
    void place(const VM& v) {
        cpuUsed += v.cpu; ramUsed += v.ram;
        vms[v.id / 64] |= (1ULL << (v.id % 64));
    }
    void unplace(const VM& v) {
        cpuUsed -= v.cpu; ramUsed -= v.ram;
        vms[v.id / 64] &= ~(1ULL << (v.id % 64));
    }
    bool hasVm(int vmId) const {
        return (vms[vmId / 64] >> (vmId % 64)) & 1ULL;
    }
    bool empty() const {
        for (uint64_t w : vms) if (w) return false;
        return true;
    }
    int vmCount() const {
        int c = 0;
        for (uint64_t w : vms) c += __builtin_popcountll(w);
        return c;
    }
    vector<int> vmIds() const { return fromBitmask(vms); }
};

// Construct a PM with its bitmask correctly sized upfront for `totalVmCount`
// VMs. Always use this (or manually call vms.assign(...)) instead of
// default-constructing a PM directly -- place()/unplace() assume the
// bitmask is already sized.
PM makePM(int id, double cpuCap, double ramCap, int totalVmCount) {
    PM pm;
    pm.id = id;
    pm.cpuCap = cpuCap;
    pm.ramCap = ramCap;
    pm.vms.assign(bitmaskWordCount(totalVmCount), 0ULL);
    return pm;
}

// One PM "type": a capacity spec plus how many physical machines of that
// spec are actually available. Legacy A/B instances have a single type
// (with a deliberately generous count, effectively unlimited); C instances
// have two distinct types, each with a real, binding supply limit.
struct PMSpec {
    int count;
    double cpuCap, ramCap;
};

struct Instance {
    string name;
    int pmCountBound;      // fallback bound if no LowerBounds.txt entry exists
    vector<PMSpec> pmSpecs; // one (A/B) or more (C) PM types
    int vmCount;
    vector<VM> vms;
};

// Strip a leading UTF-8 BOM (EF BB BF) and any trailing \r (from CRLF line
// endings) so filenames/names compare cleanly regardless of file origin.
string cleanToken(string s) {
    if (s.size() >= 3 &&
        (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
        s = s.substr(3);
    }
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

// LowerBounds.txt format: each line is "<instance_name> <lower_bound>"
// e.g. "VMP_A100 13"
// Robust to a UTF-8 BOM at the start of the file and CRLF line endings,
// both of which are common when the file was saved from Windows/Excel.
unordered_map<string, int> parseLowerBounds(const string& path) {
    unordered_map<string, int> bounds;
    ifstream in(path);
    if (!in) throw runtime_error("cannot open lower bounds file: " + path);
    string name;
    int lb;
    while (in >> name >> lb) {
        bounds[cleanToken(name)] = lb;
    }
    return bounds;
}

// Splits a "a,b" style line into its comma-separated pieces (as strings,
// pre-trimmed). Used for the C-format's "count1,count2" and "cpu,ram" lines.
vector<string> splitComma(const string& s) {
    vector<string> parts;
    stringstream ss(s);
    string tok;
    while (getline(ss, tok, ',')) parts.push_back(tok);
    return parts;
}

Instance parseInstance(const string& path) {
    ifstream in(path);
    if (!in) throw runtime_error("cannot open file: " + path);
    Instance inst;

    getline(in, inst.name);
    inst.name = cleanToken(inst.name);

    string line2;
    getline(in, line2);
    line2 = cleanToken(line2);

    if (line2.find(',') != string::npos) {
        // ---- C-format: two PM types ----
        // line2: "<count1>,<count2>"
        // line3: "<cpuCap1>,<ramCap1>"
        // line4: "<cpuCap2>,<ramCap2>"
        // line5: vmCount
        auto counts = splitComma(line2);
        string line3, line4;
        getline(in, line3); line3 = cleanToken(line3);
        getline(in, line4); line4 = cleanToken(line4);
        auto spec1 = splitComma(line3);
        auto spec2 = splitComma(line4);

        int count1 = stoi(counts[0]);
        int count2 = stoi(counts[1]);
        double cpu1 = stod(spec1[0]), ram1 = stod(spec1[1]);
        double cpu2 = stod(spec2[0]), ram2 = stod(spec2[1]);

        inst.pmSpecs.push_back({count1, cpu1, ram1});
        inst.pmSpecs.push_back({count2, cpu2, ram2});
        inst.pmCountBound = count1 + count2; // fallback bound = total available slots

        in >> inst.vmCount;
    } else {
        // ---- Legacy A/B-format: one PM type ----
        // line2: PM count bound (generous, effectively unlimited)
        // line3: cpuCap
        // line4: ramCap
        // line5: vmCount
        inst.pmCountBound = stoi(line2);
        double cpuCap, ramCap;
        in >> cpuCap >> ramCap >> inst.vmCount;
        inst.pmSpecs.push_back({inst.pmCountBound, cpuCap, ramCap});
    }

    inst.vms.reserve(inst.vmCount);
    for (int i = 0; i < inst.vmCount; ++i) {
        double cpu, ram, ignored;
        in >> cpu >> ram >> ignored;
        inst.vms.push_back({i, cpu, ram});
    }
    return inst;
}

// Best-fit target: PM leaving least leftover capacity (Euclidean norm) after placing vm.
double residualScore(const PM& pm, const VM& vm) {
    double cLeft = pm.cpuFree() - vm.cpu;
    double rLeft = pm.ramFree() - vm.ram;
    return std::sqrt(cLeft * cLeft + rLeft * rLeft);
}

using Clock = chrono::steady_clock;

// ============================================================================
// VNS / ejection-chain feasibility search
//
// Given a fixed number of bins k, try to place every VM using increasingly
// complex move types:
//   1. Direct best-fit move            (N1: single relocate)
//   2. Swap: eject one VM from a bin to make room, then recursively find
//      a home for the ejected VM       (N2/N3: chained eject-and-relocate,
//      i.e. an ejection chain, depth-limited)
//
// This is what lets the search escape the local optima that a plain
// "move whole PMs only" compaction pass gets stuck in: sometimes fitting a
// stubborn VM requires first relocating a *different* VM to clear space.
//
// Bounded by a wall-clock deadline; if the deadline passes with some VMs
// still unplaced, the search reports failure (search "gives up" honestly
// rather than looping forever) so the caller can retry with more bins.
// ============================================================================

// Attempts to find a home for `v`, possibly by ejecting an existing VM from
// a bin and recursively relocating it (chain), up to `maxDepth` ejections.
// Mutates `pms` in place on success; fully reverts its own changes on failure
// so the caller can safely retry other moves.
bool ejectionPlace(const VM& v, vector<PM>& pms, const unordered_map<int, VM>& vmMap,
                    int depth, int maxDepth, const Clock::time_point& deadline,
                    vector<char>& bannedBins) {
    if (Clock::now() > deadline) return false;

    // 1. Direct best-fit placement (cheapest move: no side effects needed).
    int best = -1; double bestScore = numeric_limits<double>::max();
    for (size_t i = 0; i < pms.size(); ++i) {
        if (!pms[i].canFit(v)) continue;
        double s = residualScore(pms[i], v);
        if (s < bestScore) { bestScore = s; best = (int)i; }
    }
    if (best != -1) { pms[best].place(v); return true; }

    if (depth >= maxDepth) return false;

    // 2. Ejection chain: for each bin, try evicting one of its VMs to make
    //    room for v, then recursively relocate the evicted VM elsewhere.
    //    `bannedBins` prevents re-entering a bin we're already mid-eject on
    //    within this chain, which would otherwise just undo prior work / cycle.
    for (size_t b = 0; b < pms.size(); ++b) {
        if (bannedBins[b]) continue;
        if (Clock::now() > deadline) return false;

        vector<int> vmIds = pms[b].vmIds(); // snapshot, since we mutate pms[b] below
        for (int wId : vmIds) {
            if (Clock::now() > deadline) return false;
            const VM& w = vmMap.at(wId);

            pms[b].unplace(w);
            if (pms[b].canFit(v)) {
                pms[b].place(v);
                bannedBins[b] = 1;
                if (ejectionPlace(w, pms, vmMap, depth + 1, maxDepth, deadline, bannedBins)) {
                    return true; // whole chain committed
                }
                // Chain failed downstream -- undo this eject and keep looking.
                pms[b].unplace(v);
                pms[b].place(w);
                bannedBins[b] = 0;
            } else {
                pms[b].place(w); // v still doesn't fit even with w gone; revert
            }
        }
    }
    return false;
}

// Try to pack ALL of `vms` into the given PM slots (one entry per bin, each
// with its own capacity -- lets bins be heterogeneous) within `timeLimitSeconds`.
// Returns {success, pms}. On failure, pms is whatever partial state existed
// when time ran out (not meaningful -- caller should discard and grow k).
pair<bool, vector<PM>> attemptFeasiblePacking(const vector<VM>& vms,
                                               const vector<pair<double, double>>& pmSlots,
                                               double timeLimitSeconds) {
    auto deadline = Clock::now() + chrono::duration_cast<Clock::duration>(
                        chrono::duration<double>(timeLimitSeconds));

    unordered_map<int, VM> vmMap;
    for (auto& v : vms) vmMap[v.id] = v;

    // Seed with a plain Best-Fit-Decreasing pack (by cpu+ram, descending).
    // Whatever doesn't fit becomes the initial "unplaced" queue for the
    // ejection-chain search to resolve.
    vector<VM> ordered = vms;
    sort(ordered.begin(), ordered.end(), [](const VM& a, const VM& b) {
        return (a.cpu + a.ram) > (b.cpu + b.ram);
    });

    int totalVmCount = (int)vms.size(); // VM ids are 0..totalVmCount-1 by construction
    int k = (int)pmSlots.size();
    vector<PM> pms;
    pms.reserve(k);
    for (int i = 0; i < k; ++i) pms.push_back(makePM(i, pmSlots[i].first, pmSlots[i].second, totalVmCount));

    vector<VM> unplaced;
    for (const VM& vm : ordered) {
        int best = -1; double bestScore = numeric_limits<double>::max();
        for (int i = 0; i < k; ++i) {
            if (!pms[i].canFit(vm)) continue;
            double s = residualScore(pms[i], vm);
            if (s < bestScore) { bestScore = s; best = i; }
        }
        if (best == -1) unplaced.push_back(vm);
        else pms[best].place(vm);
    }

    // Resolve unplaced VMs via ejection chains, largest first (hardest to
    // place, so give them first crack at the room that's currently free).
    sort(unplaced.begin(), unplaced.end(), [](const VM& a, const VM& b) {
        return (a.cpu + a.ram) > (b.cpu + b.ram);
    });

    const int maxDepth = 4; // bound on chain length (how many VMs get bumped in one go)

    bool progress = true;
    while (!unplaced.empty() && progress) {
        if (Clock::now() > deadline) break;
        progress = false;
        for (size_t idx = 0; idx < unplaced.size(); ) {
            if (Clock::now() > deadline) break;
            vector<char> bannedBins(pms.size(), 0);
            if (ejectionPlace(unplaced[idx], pms, vmMap, 0, maxDepth, deadline, bannedBins)) {
                unplaced.erase(unplaced.begin() + idx);
                progress = true;
                // don't advance idx -- vector shifted left
            } else {
                ++idx;
            }
        }
    }

    return {unplaced.empty(), pms};
}

// Expands each PM type into individual (cpuCap, ramCap) slots, one per
// available machine of that type, then sorts the whole pool descending by
// total capacity (cpu+ram). For a target bin count k, taking the pool's
// first k slots is a greedy way to prefer the "roomiest" available PMs
// first while respecting each type's real supply limit -- it's a heuristic
// choice of *which* PMs to use, not an exhaustive search over all type
// mixes, but works well when one type is strictly larger than another.
vector<pair<double, double>> buildPmPool(const vector<PMSpec>& specs) {
    vector<pair<double, double>> pool;
    for (auto& spec : specs) {
        for (int i = 0; i < spec.count; ++i) pool.push_back({spec.cpuCap, spec.ramCap});
    }
    sort(pool.begin(), pool.end(), [](const pair<double,double>& a, const pair<double,double>& b) {
        return (a.first + a.second) > (b.first + b.second);
    });
    return pool;
}

// Summary of solving one instance -- used by both single-file and batch modes.
struct SolveResult {
    string name;
    int lowerBound = 0;
    int vmCount = 0;              // total VMs in the instance (needed to size bitmasks)
    bool success = false;
    int pmsUsed = 0;
    int finalK = 0;              // target k that finally succeeded
    double totalTimeSeconds = 0; // summed across all k attempts
    vector<PM> solution;         // active (non-empty) PMs only
};

// Runs the full "start at lower bound, 5s VNS attempt, grow k and restart on
// failure" search for one instance. This is the same logic that used to live
// directly in main(), pulled out so it can be reused across many files.
//
// The pool of available PM slots is built once (expanding every PM type by
// its real supply count, sorted roomiest-first -- see buildPmPool). For a
// given k we simply take the pool's first k slots: this both handles the
// legacy single-type case (pool = k identical slots) and the C-format
// two-type case (pool mixes both types, biggest-first) with the same code
// path. k can never exceed the pool size -- there just aren't more physical
// machines than that available, at any type mix.
SolveResult solveInstance(const Instance& inst, int lowerBound, double timeLimitSeconds,
                           int maxAttempts, bool verbose) {
    SolveResult result;
    result.name = inst.name;
    result.lowerBound = lowerBound;
    result.vmCount = inst.vmCount;

    vector<pair<double, double>> pool = buildPmPool(inst.pmSpecs);
    int maxAvailable = (int)pool.size();

    if (lowerBound > maxAvailable) {
        if (verbose) {
            cerr << "  ERROR: lower bound (" << lowerBound << ") exceeds total available PMs ("
                 << maxAvailable << ") across all types -- instance cannot be solved as specified.\n";
        }
        result.success = false;
        return result;
    }

    int k = lowerBound;
    for (int attempt = 0; attempt < maxAttempts && k <= maxAvailable; ++attempt) {
        if (verbose) cout << "  Trying k = " << k << " PMs (budget " << timeLimitSeconds << "s)... " << flush;
        vector<pair<double, double>> slots(pool.begin(), pool.begin() + k);
        auto t0 = Clock::now();
        auto [ok, pms] = attemptFeasiblePacking(inst.vms, slots, timeLimitSeconds);
        double elapsed = chrono::duration<double>(Clock::now() - t0).count();
        result.totalTimeSeconds += elapsed;

        if (ok) {
            if (verbose) cout << "SUCCESS in " << elapsed << "s\n";
            vector<PM> active;
            for (auto& pm : pms) if (!pm.empty()) active.push_back(pm);
            result.success = true;
            result.pmsUsed = (int)active.size();
            result.finalK = k;
            result.solution = std::move(active);
            return result;
        } else {
            if (verbose) cout << "timed out after " << elapsed << "s -- increasing PM count\n";
            k++;
        }
    }

    if (verbose && k > maxAvailable) {
        cerr << "  ERROR: exhausted all " << maxAvailable
             << " available PMs across all types without finding a feasible packing.\n";
    }
    result.success = false;
    return result;
}

// Writes the detailed per-PM assignment for one solved instance to a file.
// Each PM's VM set is written both as a readable id list (VMs=...) and as a
// bitmask (Bitmask=...): one or more 64-bit hex words, bit i of the overall
// bitmask set <=> VM id i is on this PM. Word 0 covers VM ids 0-63, word 1
// covers 64-127, etc. -- read left to right, lowest ids first.
void writeInstanceResult(const string& path, const SolveResult& r) {
    ofstream out(path);
    out << "Instance: " << r.name << "\n";
    out << "Lower bound: " << r.lowerBound << "\n";
    out << "PMs used: " << r.pmsUsed << "\n";
    out << "Optimal: " << (r.pmsUsed == r.lowerBound ? "yes" : "no") << "\n";
    out << "Time: " << r.totalTimeSeconds << "s\n";
    out << "Bitmask words per PM: " << bitmaskWordCount(r.vmCount)
        << " (covers VM ids 0.." << (r.vmCount - 1) << ")\n\n";
    for (size_t i = 0; i < r.solution.size(); ++i) {
        auto& pm = r.solution[i];
        out << "PM " << i << " CPU=" << pm.cpuUsed << "/" << pm.cpuCap
            << " RAM=" << pm.ramUsed << "/" << pm.ramCap << " VMs=";
        for (int id : pm.vmIds()) out << id << ",";
        out << " Bitmask=" << bitmaskToHex(pm.vms);
        out << "\n";
    }
}

// ============================================================================
// Batch mode: recursively walk a directory tree (e.g. dataset/Instances/),
// find every instance file, solve it, and write:
//   - one detailed per-instance result file per instance
//   - one summary.csv covering the whole batch
// ============================================================================
int runBatch(const string& rootDir, const string& lowerBoundsPath, const string& outputDir) {
    unordered_map<string, int> lowerBounds = parseLowerBounds(lowerBoundsPath);

    filesystem::create_directories(outputDir);

    // Collect all candidate instance files first (so we can report total count
    // and process in a stable, sorted order).
    vector<filesystem::path> files;
    for (auto& entry : filesystem::recursive_directory_iterator(rootDir)) {
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        // Accept ".vmp" files, and also files with no extension whose name
        // starts with the same prefix pattern (covers datasets where the
        // instance files were saved without an extension).
        if (p.extension() == ".vmp" || p.extension().empty()) {
            files.push_back(p);
        }
    }
    sort(files.begin(), files.end());

    if (files.empty()) {
        cerr << "No instance files found under " << rootDir << "\n";
        return 1;
    }

    cout << "Found " << files.size() << " instance file(s) under " << rootDir << "\n\n";

    string summaryPath = (filesystem::path(outputDir) / "summary.csv").string();
    ofstream summary(summaryPath);
    summary << "instance,lower_bound,pms_used,gap,optimal,time_seconds\n";

    const double timeLimitSeconds = 5.0;
    const int maxAttempts = 2000;

    int doneCount = 0, optimalCount = 0, failCount = 0;

    for (auto& path : files) {
        
        doneCount++;
        Instance inst;
        try {
            inst = parseInstance(path.string());
        } catch (const exception& e) {
            cerr << "[" << doneCount << "/" << files.size() << "] SKIP " << path.string()
                    << " -- parse error: " << e.what() << "\n";
            continue;
        }

        auto it = lowerBounds.find(inst.name);
        int lb;
        if (it == lowerBounds.end()) {
            cerr << "[" << doneCount << "/" << files.size() << "] WARNING: no lower bound for '"
                    << inst.name << "' -- falling back to file's own bound (" << inst.pmCountBound << ")\n";
            lb = inst.pmCountBound;
        } else {
            lb = it->second;
        }

        cout << "[" << doneCount << "/" << files.size() << "] " << inst.name
                << " (lower bound " << lb << ", " << inst.vmCount << " VMs)\n";
        for(int denemeSayi=0; denemeSayi < 10;++denemeSayi){
            SolveResult res = solveInstance(inst, lb, timeLimitSeconds, maxAttempts, /*verbose=*/true);
    
            if (!res.success) {
                cout << "  FAILED to find a feasible packing within " << maxAttempts << " attempts.\n\n";
                summary << inst.name << "," << lb << ",,,failed," << res.totalTimeSeconds << "\n";
                failCount++;
                continue;
            }
    
            bool optimal = (res.pmsUsed == lb);
            if (optimal) optimalCount++;
            cout << "  -> " << res.pmsUsed << " PMs used"
                 << (optimal ? " (OPTIMAL)" : " (" + to_string(res.pmsUsed - lb) + " above lower bound)")
                 << ", total time " << res.totalTimeSeconds << "s\n\n";
    
            summary << inst.name << "," << lb << "," << res.pmsUsed << ","
                     << (res.pmsUsed - lb) << "," << (optimal ? "yes" : "no") << ","
                     << res.totalTimeSeconds << "\n";
            summary.flush();
        }
/*
        string resultPath = (filesystem::path(outputDir) / (inst.name + "_result.txt")).string();
        writeInstanceResult(resultPath, res);
*/
    }

    cout << "=== BATCH DONE: " << doneCount << " instance(s) processed, "
         << optimalCount << " optimal, " << failCount << " failed ===\n";
    cout << "Summary written to " << summaryPath << "\n";
    cout << "Per-instance results written to " << outputDir << "\n";

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "usage (single file): " << argv[0] << " <file.vmp> <LowerBounds.txt> [output_file.txt]\n";
        cerr << "usage (batch mode):  " << argv[0] << " <instancesRootDir> <LowerBounds.txt> <outputDir>\n";
        return 1;
    }

    string inputPath = argv[1];
    string lowerBoundsPath = argv[2];

    if (filesystem::is_directory(inputPath)) {
        string outputDir = (argc >= 4) ? argv[3] : "vmp_results";
        return runBatch(inputPath, lowerBoundsPath, outputDir);
    }

    // ---- single-file mode (unchanged behavior) ----
    string outputPath = (argc >= 4) ? argv[3] : "vmp_result.txt";

    Instance inst = parseInstance(inputPath);
    unordered_map<string, int> lowerBounds = parseLowerBounds(lowerBoundsPath);

    int lb;
    auto it = lowerBounds.find(inst.name);
    if (it == lowerBounds.end()) {
        cerr << "WARNING: no lower bound found for instance '" << inst.name
             << "' in " << lowerBoundsPath << " -- falling back to file's own upper bound ("
             << inst.pmCountBound << ")\n";
        lb = inst.pmCountBound;
    } else {
        lb = it->second;
    }

    cout << "Instance: " << inst.name << "\n";
    cout << "Lower bound: " << lb << "  VM count: " << inst.vmCount << "\n";
    cout << "PM types:\n";
    for (size_t t = 0; t < inst.pmSpecs.size(); ++t) {
        auto& spec = inst.pmSpecs[t];
        cout << "  type " << (t + 1) << ": count=" << spec.count
             << " CPU cap=" << spec.cpuCap << " RAM cap=" << spec.ramCap << "\n";
    }
    cout << "\n";

    SolveResult res = solveInstance(inst, lb, 5.0, 2000, /*verbose=*/true);

    if (!res.success) {
        cerr << "Could not find a feasible packing within attempt limit.\n";
        return 1;
    }

    cout << "\n=== SOLUTION: " << res.pmsUsed << " PMs used (target was k=" << res.finalK << ") ===\n\n";
    for (size_t i = 0; i < res.solution.size(); ++i) {
        auto& pm = res.solution[i];
        cout << "PM " << i
             << " | CPU " << pm.cpuUsed << "/" << pm.cpuCap
             << " | RAM " << pm.ramUsed << "/" << pm.ramCap
             << " | VMs(" << pm.vmCount() << "): ";
        for (int id : pm.vmIds()) cout << id << " ";
        cout << "\n";
    }

    cout << "\nKnown lower bound: " << lb << " PMs\n";
    if (res.pmsUsed == lb) cout << "Result MATCHES the lower bound -> optimal.\n";
    else cout << "Result is " << (res.pmsUsed - lb) << " PM(s) above the lower bound.\n";

    writeInstanceResult(outputPath, res);
    cout << "\nWrote assignment to " << outputPath << "\n";

    return 0;
}
