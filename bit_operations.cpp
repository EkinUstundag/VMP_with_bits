#include "bit_operations.h"

#include <algorithm>
#include <bitset>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <functional>
#include <numeric>
#include <random>
#include <sstream>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

// --- Global instance state ---
int TOTAL_VM_COUNT = 0;
int TOTAL_PM_COUNT = 0;
int cpu_cap = 0;
int ram_cap = 0;
int cpu_cap2 = 0;
int ram_cap2 = 0;
int pmType1Count = 0;
int pmType2Count = 0;
int* vm_CPU_Req = nullptr;
int* vm_RAM_Req = nullptr;
int* pm_CPU = nullptr;
int* pm_RAM = nullptr;
std::string instanceName;
std::map<std::string, int> PmLowerBounds;

namespace {

constexpr int kBenchmarkRuns = 10;
constexpr float kSwapProbability = 0.0f;
constexpr float kMoveProbability = 1.0f;
constexpr int kMoveCandidates = 50;//24
constexpr int kStallLimit = 3000;
const auto kSearchDuration = std::chrono::seconds(5);

using std::mt19937;
using std::uniform_int_distribution;
using std::uniform_real_distribution;

// --- Bit iteration ---

int lowestSetBit(unsigned long word) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzl(word);
#else
    int bit = 0;
    while (((word >> bit) & 1UL) == 0) ++bit;
    return bit;
#endif
}

void forEachVmOnPm(const VmPlacement& pm, const std::function<void(int vmId)>& fn) {
    for (size_t chunk = 0; chunk < pm.size(); ++chunk) {
        unsigned long word = pm[chunk];
        while (word) {
            const int bit = lowestSetBit(word);
            const int vmId = static_cast<int>(chunk) * CONTAINER_SIZE + bit;
            if (vmId < TOTAL_VM_COUNT) {
                fn(vmId);
            }
            word &= word - 1;
        }
    }
}

// --- Fitness ---

unsigned long overloadFitness(int cpuExcess, int ramExcess) {
    const int cpuPenalty = (cpuExcess > 0 ? cpuExcess : 0);
    const int ramPenalty = (ramExcess > 0 ? ramExcess : 0);
    if (cpuPenalty == 0) return static_cast<unsigned long>(ramPenalty);
    if (ramPenalty == 0) return static_cast<unsigned long>(cpuPenalty);
    return static_cast<unsigned long>(cpuPenalty * ramPenalty);
}

unsigned long pmFitnessValue(int cpuUsed, int ramUsed, int pmIndex) {
    return overloadFitness(cpuUsed - pm_CPU[pmIndex], ramUsed - pm_RAM[pmIndex]);
}

void pmResourceUsage(const VmPlacement& pm, int& cpu, int& ram) {
    cpu = 0;
    ram = 0;
    forEachVmOnPm(pm, [&](int vmId) {
        cpu += vm_CPU_Req[vmId];
        ram += vm_RAM_Req[vmId];
    });
}

int randomVmOnPm(const VmPlacement& pm, mt19937& gen) {
    const int vmCount = static_cast<int>(countVectorVMs(pm));
    if (vmCount == 0) return -1;

    uniform_int_distribution<> pick(0, vmCount - 1);
    const int target = pick(gen);
    int seen = 0;
    int chosen = -1;
    forEachVmOnPm(pm, [&](int vmId) {
        if (seen == target) chosen = vmId;
        ++seen;
    });
    return chosen;
}

// --- Local search state ---

struct LocalSearchState {
    Solution& solution;
    const int pmCount;
    std::vector<int> pmCpu;
    std::vector<int> pmRam;
    std::vector<unsigned long> pmFitness;
    std::vector<int> overloaded;
    std::vector<int> underloaded;
    unsigned long totalFitness = 0;

    explicit LocalSearchState(Solution& sol)
        : solution(sol),
          pmCount(TOTAL_PM_COUNT),
          pmCpu(pmCount),
          pmRam(pmCount),
          pmFitness(pmCount) {
        overloaded.reserve(pmCount);
        underloaded.reserve(pmCount);
    }

    void syncFromSolution() {
        totalFitness = 0;
        overloaded.clear();
        underloaded.clear();
        for (int i = 0; i < pmCount; ++i) {
            pmResourceUsage(solution[i], pmCpu[i], pmRam[i]);
            pmFitness[i] = pmFitnessValue(pmCpu[i], pmRam[i], i);
            totalFitness += pmFitness[i];
            (pmFitness[i] > 0 ? overloaded : underloaded).push_back(i);
        }
    }

    void refreshLoadLists() {
        overloaded.clear();
        underloaded.clear();
        for (int i = 0; i < pmCount; ++i) {
            (pmFitness[i] > 0 ? overloaded : underloaded).push_back(i);
        }
    }

    unsigned long candidateTotalFitness(
        int src, int dst,
        int newCpuSrc, int newRamSrc,
        int newCpuDst, int newRamDst) const {
        const unsigned long fSrc = pmFitnessValue(newCpuSrc, newRamSrc, src);
        const unsigned long fDst = pmFitnessValue(newCpuDst, newRamDst, dst);
        return totalFitness - pmFitness[src] - pmFitness[dst] + fSrc + fDst;
    }

    void commitPairUpdate(int src, int dst) {
        const unsigned long oldSrc = pmFitness[src];
        const unsigned long oldDst = pmFitness[dst];
        pmFitness[src] = pmFitnessValue(pmCpu[src], pmRam[src], src);
        pmFitness[dst] = pmFitnessValue(pmCpu[dst], pmRam[dst], dst);
        totalFitness = totalFitness - oldSrc - oldDst + pmFitness[src] + pmFitness[dst];
        refreshLoadLists();
    }
};

struct MoveCandidate {
    int dst = -1;
    int partnerVm = -1;
    unsigned long fitness = ULONG_MAX;
};

int pickSourcePm(const LocalSearchState& state, mt19937& gen) {
    if (!state.overloaded.empty()) {
        uniform_int_distribution<> dist(0, static_cast<int>(state.overloaded.size()) - 1);
        return state.overloaded[dist(gen)];
    }
    uniform_int_distribution<> dist(0, state.pmCount - 1);
    return dist(gen);
}

MoveCandidate findBestMoveCandidate(
    const LocalSearchState& state,
    mt19937& gen,
    int src,
    int vm,
    bool swap,
    uniform_int_distribution<>& destDist) {
    MoveCandidate best;
    /*
    for (int c = 0; c < kMoveCandidates; ++c) {
        const int dst = destDist(gen);
        if (dst == src) {c--; continue;} 
**/ for (int dst:state.underloaded) {
        int partnerVm = -1;
        int newCpuSrc, newRamSrc, newCpuDst, newRamDst;

        if (swap) {
            partnerVm = randomVmOnPm(state.solution[dst], gen);
            if (partnerVm < 0) continue;
            newCpuSrc = state.pmCpu[src] - vm_CPU_Req[vm] + vm_CPU_Req[partnerVm];
            newRamSrc = state.pmRam[src] - vm_RAM_Req[vm] + vm_RAM_Req[partnerVm];
            newCpuDst = state.pmCpu[dst] + vm_CPU_Req[vm] - vm_CPU_Req[partnerVm];
            newRamDst = state.pmRam[dst] + vm_RAM_Req[vm] - vm_RAM_Req[partnerVm];
        } else {
            newCpuSrc = state.pmCpu[src] - vm_CPU_Req[vm];
            newRamSrc = state.pmRam[src] - vm_RAM_Req[vm];
            newCpuDst = state.pmCpu[dst] + vm_CPU_Req[vm];
            newRamDst = state.pmRam[dst] + vm_RAM_Req[vm];
        }

        const unsigned long candFit = state.candidateTotalFitness(
            src, dst, newCpuSrc, newRamSrc, newCpuDst, newRamDst);
        if (candFit < best.fitness) {
            best = {dst, partnerVm, candFit};
        }
    }
    return best;
}

void applyMove(LocalSearchState& state, int src, int dst, int vm) {
    moveBit(state.solution[src], state.solution[dst], vm);
    state.pmCpu[src] -= vm_CPU_Req[vm];
    state.pmRam[src] -= vm_RAM_Req[vm];
    state.pmCpu[dst] += vm_CPU_Req[vm];
    state.pmRam[dst] += vm_RAM_Req[vm];
}

void applySwap(LocalSearchState& state, int src, int dst, int vm, int partnerVm) {
    swapBits(state.solution[src], state.solution[dst], vm, partnerVm);
    state.pmCpu[src] -= vm_CPU_Req[vm];
    state.pmRam[src] -= vm_RAM_Req[vm];
    state.pmCpu[dst] += vm_CPU_Req[vm];
    state.pmRam[dst] += vm_RAM_Req[vm];
    state.pmCpu[src] += vm_CPU_Req[partnerVm];
    state.pmRam[src] += vm_RAM_Req[partnerVm];
    state.pmCpu[dst] -= vm_CPU_Req[partnerVm];
    state.pmRam[dst] -= vm_RAM_Req[partnerVm];
}

bool runElimination(LocalSearchState& state, mt19937& gen) {
    if (state.overloaded.empty() || state.underloaded.empty()) {
        return false;
    }

    //bool anyMoved = false;
    const std::vector<int> sources = state.overloaded;
    const int totalChunks = (TOTAL_VM_COUNT + CONTAINER_SIZE - 1) / CONTAINER_SIZE;
    VmPlacement eliminatedVMs(totalChunks, 0UL);
    for (int src : sources) {
        //uniform_int_distribution<> ulDist(0, static_cast<int>(state.underloaded.size()) - 1);
        //const int dst = state.underloaded[ulDist(gen)];
        //if (dst == src) continue;
        //moveBit(state.solution[src], state.solution[dst], vm);
        //anyMoved = true;        
        const int vm = randomVmOnPm(state.solution[src], gen);
        if (vm < 0) continue;
        const int chunk = vm / CONTAINER_SIZE;
        unsigned long mask = 1UL << (vm % CONTAINER_SIZE);

        state.solution[src][chunk] &= ~mask;
        eliminatedVMs[chunk] |= mask;
    }
    state.syncFromSolution();
    uniform_int_distribution<> dist(0, state.pmCount - 1);
    forEachVmOnPm(eliminatedVMs, [&](int vmId) {
        const int src = dist(gen);
        if (vmId < 0) return;
        const int chunk = vmId / CONTAINER_SIZE;
        unsigned long mask2 = 1UL << (vmId % CONTAINER_SIZE);
        eliminatedVMs[chunk] &= mask2;
        state.solution[src][chunk] |= mask2;
    });
    

    return true;
}

bool runGuidedStep(LocalSearchState& state, mt19937& gen, bool swap, uniform_int_distribution<>& destDist) {
    state.syncFromSolution();
    const int src = pickSourcePm(state, gen);
    const int vm = randomVmOnPm(state.solution[src], gen);
    if (vm < 0) return false;

    const MoveCandidate best = findBestMoveCandidate(state, gen, src, vm, swap, destDist);
    if (best.dst < 0 || best.fitness >= state.totalFitness) {
        return false;
    }

    if (swap) {
        applySwap(state, src, best.dst, vm, best.partnerVm);
    } else {
        applyMove(state, src, best.dst, vm);
    }
    state.commitPairUpdate(src, best.dst);
    return true;
}

}  // namespace

// --- Bit-vector VM helpers ---

unsigned long countVMs(unsigned long vm) {
    unsigned long count = 0;
    while (vm > 0) {
        vm &= vm - 1;
        ++count;
    }
    return count;
}

unsigned long countVectorVMs(const VmPlacement& vm) {
    unsigned long count = 0;
    for (unsigned long word : vm) {
        count += countVMs(word);
    }
    return count;
}

VmPlacement idToVM(int id) {
    const int totalChunks = (TOTAL_VM_COUNT + CONTAINER_SIZE - 1) / CONTAINER_SIZE;
    const int chunk = id / CONTAINER_SIZE;
    VmPlacement vm(totalChunks, 0UL);
    vm[chunk] = 1UL << (id % CONTAINER_SIZE);
    return vm;
}

VmPlacement createEmptyM() {
    const int totalChunks = (TOTAL_VM_COUNT + CONTAINER_SIZE - 1) / CONTAINER_SIZE;
    return VmPlacement(totalChunks, 0UL);
}

VmPlacement bitwiseOrMs(const VmPlacement& m1, const VmPlacement& m2) {
    VmPlacement result(m1.size());
    for (size_t i = 0; i < m1.size(); ++i) {
        result[i] = m1[i] | m2[i];
    }
    return result;
}

VmPlacement bitwiseAndMs(const VmPlacement& m1, const VmPlacement& m2) {
    VmPlacement result(m1.size());
    for (size_t i = 0; i < m1.size(); ++i) {
        result[i] = m1[i] & m2[i];
    }
    return result;
}

VmPlacement bitwiseXorMs(const VmPlacement& m1, const VmPlacement& m2) {
    VmPlacement result(m1.size());
    for (size_t i = 0; i < m1.size(); ++i) {
        result[i] = m1[i] ^ m2[i];
    }
    return result;
}

VmPlacement bitwiseNotMs(const VmPlacement& m1) {
    VmPlacement result(m1.size());
    for (size_t i = 0; i < m1.size(); ++i) {
        result[i] = ~m1[i];
    }
    return result;
}

VmPlacement bitwiseSLMs(const VmPlacement& m1, unsigned long shift) {
    VmPlacement result = createEmptyM();
    const int blockShift = static_cast<int>(shift / CONTAINER_SIZE);
    const int bitShift = static_cast<int>(shift % CONTAINER_SIZE);

    for (int i = static_cast<int>(m1.size()) - 1; i >= 0; --i) {
        if (i + blockShift >= static_cast<int>(m1.size())) continue;
        result[i] |= (m1[i + blockShift] << bitShift);
        if (bitShift != 0 && i + blockShift + 2 <= static_cast<int>(m1.size())) {
            result[i] |= (m1[i + blockShift + 1] >> (CONTAINER_SIZE - bitShift));
        }
    }
    return result;
}

VmPlacement bitwiseSRMs(const VmPlacement& m1, unsigned long shift) {
    VmPlacement result = createEmptyM();
    const int blockShift = static_cast<int>(shift / CONTAINER_SIZE);
    const int bitShift = static_cast<int>(shift % CONTAINER_SIZE);

    for (size_t i = 0; i < m1.size(); ++i) {
        if (static_cast<int>(i) - blockShift < 0) continue;
        result[i] |= (m1[i - blockShift] >> bitShift);
        if (bitShift != 0 && static_cast<int>(i) - blockShift > 0) {
            result[i] |= (m1[i - blockShift - 1] << (CONTAINER_SIZE - bitShift));
        }
    }
    return result;
}

void printPM(const VmPlacement& pm) {
    for (unsigned long n : pm) {
        std::cout << std::bitset<CONTAINER_SIZE>(n) << ' ';
    }
    std::cout << '\n';
}

void printSolution(const Solution& solution) {
    for (VmPlacement pm : solution) {
        printPM(pm);
    }
}

unsigned long checkSameVM(const VmPlacement& pm1, const VmPlacement& pm2) {
    return countVectorVMs(bitwiseAndMs(pm1, pm2)) == 1;
}

VmPlacement removeSpecificVM(const VmPlacement& pm, const VmPlacement& vm) {
    return bitwiseAndMs(pm, bitwiseNotMs(vm));
}

// --- I/O ---

void readFile(std::ifstream& f) {
    std::string line;
    std::getline(f, line);
    instanceName = line;

    std::getline(f, line);
    TOTAL_PM_COUNT = stoi(line);
    std::getline(f, line);
    cpu_cap = stoi(line);
    std::getline(f, line);
    ram_cap = stoi(line);
    std::getline(f, line);
    TOTAL_VM_COUNT = stoi(line);

    vm_CPU_Req = new int[TOTAL_VM_COUNT];
    vm_RAM_Req = new int[TOTAL_VM_COUNT];
    pm_CPU = new int[TOTAL_PM_COUNT];
    pm_RAM = new int[TOTAL_PM_COUNT];
    for (int j = 0; j < TOTAL_PM_COUNT; ++j) {
        pm_CPU[j] = cpu_cap;
        pm_RAM[j] = ram_cap;
    }

    int vmId = 0;
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string token;
        std::getline(ss, token, ' ');
        vm_CPU_Req[vmId] = stoi(token);
        std::getline(ss, token, ' ');
        vm_RAM_Req[vmId] = stoi(token);
        ++vmId;
    }
}

void readFileC(std::ifstream& f) {
    std::string line;
    std::string token;

    std::getline(f, line);
    instanceName = line;

    std::getline(f, line);
    {
        std::stringstream ss(line);
        std::getline(ss, token, ',');
        pmType1Count = stoi(token);
        std::getline(ss, token, ',');
        pmType2Count = stoi(token);
    }
    TOTAL_PM_COUNT = pmType1Count + pmType2Count;

    std::getline(f, line);
    {
        std::stringstream ss(line);
        std::getline(ss, token, ',');
        cpu_cap = stoi(token);
        std::getline(ss, token, ',');
        ram_cap = stoi(token);
    }

    std::getline(f, line);
    {
        std::stringstream ss(line);
        std::getline(ss, token, ',');
        cpu_cap2 = stoi(token);
        std::getline(ss, token, ',');
        ram_cap2 = stoi(token);
    }

    std::getline(f, line);
    TOTAL_VM_COUNT = stoi(line);

    vm_CPU_Req = new int[TOTAL_VM_COUNT];
    vm_RAM_Req = new int[TOTAL_VM_COUNT];
    pm_CPU = new int[TOTAL_PM_COUNT];
    pm_RAM = new int[TOTAL_PM_COUNT];
    for (int j = 0; j < pmType1Count; ++j) {
        pm_CPU[j] = cpu_cap;
        pm_RAM[j] = ram_cap;
    }
    for (int j = pmType1Count; j < TOTAL_PM_COUNT; ++j) {
        pm_CPU[j] = cpu_cap2;
        pm_RAM[j] = ram_cap2;
    }

    int vmId = 0;
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::getline(ss, token, ' ');
        vm_CPU_Req[vmId] = stoi(token);
        std::getline(ss, token, ' ');
        vm_RAM_Req[vmId] = stoi(token);
        ++vmId;
    }
}

Solution initialSolution() {
    const int chunkCount = (TOTAL_VM_COUNT + CONTAINER_SIZE - 1) / CONTAINER_SIZE;
    Solution solution(TOTAL_PM_COUNT, VmPlacement(chunkCount, 0UL));

    std::vector<int> pmCpu(TOTAL_PM_COUNT);
    std::vector<int> pmRam(TOTAL_PM_COUNT);
    for (int j = 0; j < TOTAL_PM_COUNT; ++j) {
        pmCpu[j] = pm_CPU[j];
        pmRam[j] = pm_RAM[j];
    }

    std::vector<int> pmOrder(TOTAL_PM_COUNT);
    std::iota(pmOrder.begin(), pmOrder.end(), 0);

    mt19937 gen(std::random_device{}());
    uniform_int_distribution<> pmPick(0, TOTAL_PM_COUNT - 1);

    for (int vmId = 0; vmId < TOTAL_VM_COUNT; ++vmId) {
        const unsigned long vmFit = overloadFitness(vm_CPU_Req[vmId], vm_RAM_Req[vmId]);

        std::sort(pmOrder.begin(), pmOrder.end(), [&](int a, int b) {
            const unsigned long fitA = overloadFitness(pmCpu[a], pmRam[a]);
            const unsigned long fitB = overloadFitness(pmCpu[b], pmRam[b]);
            return fitA > fitB;
        });

        int assignedPm = -1;
        for (int pmIdx : pmOrder) {
            if (overloadFitness(pmCpu[pmIdx], pmRam[pmIdx]) >= vmFit) {
                assignedPm = pmIdx;
                break;
            }
        }
        if (assignedPm < 0) {
            assignedPm = pmPick(gen);
        }

        VmPlacement& pm = solution[assignedPm];
        const int chunk = vmId / CONTAINER_SIZE;
        pm[chunk] |= (1UL << (vmId % CONTAINER_SIZE));
        pmCpu[assignedPm] -= vm_CPU_Req[vmId];
        pmRam[assignedPm] -= vm_RAM_Req[vmId];
    }

    return solution;
}

Solution initialize(std::ifstream& f, bool isCDataset) {
    if (isCDataset) {
        readFileC(f);
    } else {
        readFile(f);
    }
    return initialSolution();
}

void moveBit(VmPlacement& source, VmPlacement& dest, int vmId) {
    if (&source == &dest) return;
    const int chunk = vmId / CONTAINER_SIZE;
    const int bit = vmId % CONTAINER_SIZE;
    const unsigned long mask = 1UL << bit;
    if ((source[chunk] & mask) == 0) return;
    source[chunk] &= ~mask;
    dest[chunk] |= mask;
}

void swapBits(VmPlacement& pm1, VmPlacement& pm2, int vmId1, int vmId2) {
    if (&pm1 == &pm2) return;
    moveBit(pm1, pm2, vmId1);
    moveBit(pm2, pm1, vmId2);
}

unsigned long run(Solution& solution) {
    LocalSearchState state(solution);
    state.syncFromSolution();
    if (state.totalFitness == 0) return 0;

    mt19937 gen(std::random_device{}());
    uniform_real_distribution<double> scenarioDist(0.0, 1.0);
    uniform_int_distribution<> destDist(0, state.pmCount - 1);

    int stalls = 0;
    const auto start = std::chrono::steady_clock::now();

    while ((std::chrono::steady_clock::now() - start) < kSearchDuration &&
           state.totalFitness > 0 && stalls < kStallLimit) {
        const double roll = scenarioDist(gen);

        if (roll >= kSwapProbability + kMoveProbability) {
            if (!runElimination(state, gen)) ++stalls;
            else stalls = 0;
            continue;
        }

        const bool swap = roll < kSwapProbability;
        if (!runGuidedStep(state, gen, swap, destDist)) {
            if (!runElimination(state, gen)) ++stalls;
            else stalls = 0;
        } else {
            stalls = 0;
        }
    }


    return state.totalFitness;
}

void initializeLowerBounds(std::ifstream& infile) {
    PmLowerBounds.clear();
    std::string line;
    while (std::getline(infile, line)) {
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line = line.substr(3);
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string fileName;
        int lowerBound;
        if (ss >> fileName >> lowerBound) {
            PmLowerBounds[fileName] = lowerBound;
        }
    }
}

void openDataset(const std::string& datasetPath) {
    std::ofstream outputFile("outputQuality.csv");
    outputFile << "File Name,Total PM Used,Lower Bound,Solution Quality,Fitness(Excess CPUxRAM),Elapsed Time(nanoseconds)\n";

    for (const auto& folder : fs::directory_iterator(datasetPath)) {
        if (!fs::is_directory(folder)) continue;

        for (const auto& file : fs::directory_iterator(folder)) {
            if (!fs::is_regular_file(file)) continue;
            bool tookToLong=false;
            for (int runIdx = 0; runIdx < kBenchmarkRuns; ++runIdx) { 
                std::ifstream f(file.path().string());
                if (!f.is_open()) {
                    std::cerr << "Error opening the file!" << file.path().string() << '\n';
                    std::exit(1);
                }

                const std::string filePath = file.path().string();
                const bool isCDataset = filePath.find('C') != std::string::npos;
                Solution solution = initialize(f, isCDataset);
                const int filePmCount = TOTAL_PM_COUNT;
                const std::string lbKey = file.path().stem().string();
                const int lowerBound = PmLowerBounds.count(lbKey) ? PmLowerBounds[lbKey] : filePmCount;

                TOTAL_PM_COUNT = lowerBound;
                const auto begin = std::chrono::steady_clock::now();
                unsigned long bestFit = run(solution);

                while (bestFit != 0 && TOTAL_PM_COUNT < filePmCount) {
                    ++TOTAL_PM_COUNT;
                    if (solution.size() < static_cast<size_t>(TOTAL_PM_COUNT)) {
                        solution = initialSolution();
                    }
                    bestFit = run(solution);
                }

                const auto end = std::chrono::steady_clock::now();
                outputFile << lbKey << ','
                           << TOTAL_PM_COUNT << ',' << lowerBound << ','
                           << 100 * (TOTAL_PM_COUNT / static_cast<double>(lowerBound) - 1) << ','
                           << bestFit << ','
                           << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()
                           << '\n';
                outputFile.flush();
            }
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::ifstream infile("LowerBounds.txt");
    if (!infile.is_open()) {
        std::cerr << "Error opening LowerBounds.txt\n";
        return 1;
    }

    initializeLowerBounds(infile);
    openDataset("./dataset/Instances/");
    return 0;
}