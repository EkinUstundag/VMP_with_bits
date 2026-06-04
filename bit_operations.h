#ifndef BIT_OPERATIONS_H_INCLUDED
#define BIT_OPERATIONS_H_INCLUDED

#include <fstream>
#include <map>
#include <string>
#include <vector>

constexpr int CONTAINER_SIZE = static_cast<int>(sizeof(unsigned long) * 8);

// --- Problem instance (filled by readFile / readFileC) ---
extern int TOTAL_VM_COUNT;
extern int TOTAL_PM_COUNT;

extern int cpu_cap;
extern int ram_cap;
extern int cpu_cap2;
extern int ram_cap2;

extern int pmType1Count;
extern int pmType2Count;

extern int* vm_CPU_Req;
extern int* vm_RAM_Req;
extern int* pm_CPU;
extern int* pm_RAM;

extern std::string instanceName;
extern std::map<std::string, int> PmLowerBounds;

using VmPlacement = std::vector<unsigned long>;
using Solution = std::vector<VmPlacement>;

// --- Bit-vector VM helpers ---
unsigned long countVMs(unsigned long word);
unsigned long countVectorVMs(const VmPlacement& pm);
VmPlacement idToVM(int id);
VmPlacement createEmptyM();
VmPlacement bitwiseOrMs(const VmPlacement& a, const VmPlacement& b);
VmPlacement bitwiseAndMs(const VmPlacement& a, const VmPlacement& b);
VmPlacement bitwiseXorMs(const VmPlacement& a, const VmPlacement& b);
VmPlacement bitwiseNotMs(const VmPlacement& a);
VmPlacement bitwiseSLMs(const VmPlacement& a, unsigned long shift);
VmPlacement bitwiseSRMs(const VmPlacement& a, unsigned long shift);
void printPM(const VmPlacement& pm);
unsigned long checkSameVM(const VmPlacement& a, const VmPlacement& b);
VmPlacement removeSpecificVM(const VmPlacement& pm, const VmPlacement& vm);

// --- I/O and initialization ---
void readFile(std::ifstream& f);
void readFileC(std::ifstream& f);
Solution initialize(std::ifstream& f, bool isCDataset);
void initializeLowerBounds(std::ifstream& infile);

// --- Local search operators ---
void moveBit(VmPlacement& source, VmPlacement& dest, int vmId);
void swapBits(VmPlacement& pm1, VmPlacement& pm2, int vmId1, int vmId2);
unsigned long run(Solution& solution);

// --- Benchmark driver ---
void openDataset(const std::string& datasetPath);

#endif
