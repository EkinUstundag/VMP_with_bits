#include <stdio.h>
#include <iostream>
#include <cmath>
#include <bitset>
#include "bit_operations.h"
#include <fstream>
#include <sstream>
#include <random>
#include <iostream>
#include <chrono>
#include <climits>

#include <experimental/filesystem>

const int MAX=10;
namespace std {
    namespace fs = experimental::filesystem;
}
//namespace fs = std::experimental/filesystem;

//Brian Kernighan Algorithm
unsigned long countVMs(unsigned long vm){

    unsigned long count =0;
    while(vm > 0){
        vm = vm & (vm-1);
        count++;
    }
    return count;
}

//Brian Kernighan Algorithm with vector VMs
unsigned long countVectorVMs(vector<unsigned long> vm){
    unsigned long count =0;
    for(int i=0;i<vm.size();i++){
        count += countVMs(vm.at(i));
    }
    return count;
}

vector<unsigned long> idToVM(int id){

    vector<unsigned long> vm = {};
    int totalChunks = std::ceil((double)TOTAL_VM_COUNT / CONTAINER_SIZE);
    int chunk= id / CONTAINER_SIZE ;

    for(int i=0;i<totalChunks;i++){

        if( i == chunk){
            int coord = id % CONTAINER_SIZE;
            unsigned long num = 1;
            num = num << coord;
            vm.insert(vm.begin(), num);
        }
        else vm.insert(vm.begin(), 0);
    }
    return vm;
}

vector<unsigned long> createEmptyM(){

    vector<unsigned long> vm = {};
    for(int i=0;i<TOTAL_VM_COUNT;i+=CONTAINER_SIZE){
        vm.insert(vm.begin(), 0);
    }
    return vm;
}

vector<unsigned long> bitwiseOrMs(vector<unsigned long> m1,vector<unsigned long> m2){

    vector<unsigned long> resultVector={};

    for(int i=0; i< m1.size();i++){
        unsigned long result = m1.at(i) | m2.at(i);
        resultVector.push_back(result);
    }
    return resultVector;
}

vector<unsigned long> bitwiseAndMs(vector<unsigned long> m1,vector<unsigned long> m2){

    vector<unsigned long> resultVector={};

    for(int i=0; i< m1.size();i++){
        unsigned long result = m1.at(i) & m2.at(i);
        resultVector.push_back(result);
    }
    return resultVector;
}

vector<unsigned long> bitwiseXorMs(vector<unsigned long> m1,vector<unsigned long> m2){

    vector<unsigned long> resultVector={};

    for(int i=0; i< m1.size();i++){
        unsigned long result = m1.at(i) ^ m2.at(i);
        resultVector.push_back(result);
    }
    return resultVector;
}

vector<unsigned long> bitwiseNotMs(vector<unsigned long> m1){

    vector<unsigned long> resultVector={};

    for(int i=0; i< m1.size();i++){
        resultVector.push_back(~m1.at(i));
    }
    return resultVector;
}

vector<unsigned long> bitwiseSLMs(vector<unsigned long> m1, unsigned long shift) {
    vector<unsigned long> result = createEmptyM();
    int blockShift = shift / CONTAINER_SIZE;     // how many whole elements to shift
    int bitShift   = shift % CONTAINER_SIZE;     // remaining bit shift

    for (int i = m1.size() - 1; i >= 0; i--) {
        if (i + blockShift >= m1.size()) continue;
        // Shift current block
        result.at(i) |= (m1.at(i + blockShift) << bitShift);
        //result[i] |= m1[i - blockShift] << bitShift;

        // Handle carry from previous block
        if ( bitShift != 0 && i + blockShift + 2 <=m1.size()) {

            //result[i] |= m1[i - blockShift - 1] >> (BITS - bitShift);
            result.at(i) |= (m1.at(i + blockShift+1) >> (CONTAINER_SIZE - bitShift));
        }
    }
    return result;
}

vector<unsigned long> bitwiseSRMs(vector<unsigned long> m1, unsigned long shift) {
    vector<unsigned long> result = createEmptyM();
    int blockShift = shift / CONTAINER_SIZE;     // how many whole elements to shift
    int bitShift   = shift % CONTAINER_SIZE;     // remaining bit shift

    for (unsigned long i = 0; i < m1.size(); i++) {
        if (i - blockShift < 0) continue;
        // Shift current block
        result.at(i) |= (m1.at(i - blockShift) >> bitShift);

        // Handle carry from previous block
        if ( bitShift != 0 && i - blockShift >0) {
            result.at(i) |= (m1.at(i - blockShift-1) << (CONTAINER_SIZE - bitShift));
        }
    }
    return result;
}

/*
    Prints a VM or a PM
*/
void printPM(vector<unsigned long> pm) {

    for (unsigned long n : pm){
        cout<<bitset<CONTAINER_SIZE>(n)<<" ";
        //cout << format("{:b}",n ) << " ";
    }
    cout<<endl;
}

unsigned long checkSameVM(vector<unsigned long> pm1,vector<unsigned long> pm2){
    return countVectorVMs(bitwiseAndMs(pm1,pm2)) == 1;
}

/*  there should be only single '1' bit in vm
    pm can have multiple '1' bits
*/
vector<unsigned long> removeSpecificVM(vector<unsigned long> pm,vector<unsigned long> vm){
    //return pm & (~vm);
    return bitwiseAndMs(pm,bitwiseNotMs(vm));
}

//for reading A & B datasets
void readFile(ifstream &f){
    string s;
    //1st Line is File Name
    getline(f, s);
    instanceName = s;
    //2nd Line is total PM count this will NOT be used in our code
    getline(f, s);
    TOTAL_PM_COUNT = stoi(s);

    //3rd Line is CPU cap
    getline(f, s);
    cpu_cap = stoi(s);
    //4th Line is RAM cap
    getline(f, s);
    ram_cap = stoi(s);
    //5th Line is total VM count
    getline(f, s);
    TOTAL_VM_COUNT = stoi(s);

    vm_CPU_Req = new int[TOTAL_VM_COUNT];
    vm_RAM_Req = new int[TOTAL_VM_COUNT];
    pm_CPU = new int[TOTAL_PM_COUNT];
    pm_RAM = new int[TOTAL_PM_COUNT];
    for (int j = 0; j < TOTAL_PM_COUNT; ++j) {
        pm_CPU[j] = cpu_cap;
        pm_RAM[j] = ram_cap;
    }
    int i=0;
    while (getline(f, s)){
        stringstream  stringStream(s);
        string s2;
        getline(stringStream, s2, ' ');
        vm_CPU_Req[i]= stoi(s2);
        getline(stringStream, s2, ' ');
        vm_RAM_Req[i]= stoi(s2);
        i++;
    }
}
//For reading C dataset
void readFileC(ifstream &f){
    string s;
    string s2;

    //1st Line is File Name
    getline(f, s);
    instanceName = s;
    //2nd Line is 2 integers representing counts of 2 types of PMs
    getline(f, s);
    stringstream  stringStream(s);
    getline(stringStream, s2, ',');
    pmType1Count = stoi(s2);
    getline(stringStream, s2, ',');
    pmType2Count = stoi(s2);
    TOTAL_PM_COUNT = pmType1Count + pmType2Count;

    //3rd line is CPU & RAM caps of pm1s
    getline(f, s);
    stringstream  stringStream2(s);
    getline(stringStream2, s2, ',');
    cpu_cap = stoi(s2);
    getline(stringStream2, s2, ',');
    ram_cap = stoi(s2);

    //4th Line is CPU & RAM caps of pm2s
    getline(f, s);
    stringstream  stringStream3(s);
    getline(stringStream3, s2, ',');
    cpu_cap2 = stoi(s2);
    getline(stringStream3, s2, ',');
    ram_cap2 = stoi(s2);

    //5th Line is total VM count
    getline(f, s);
    TOTAL_VM_COUNT = stoi(s);

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

    int i=0;
    while (getline(f, s)){
        stringstream  stringStream4(s);
        getline(stringStream4, s2, ' ');
        vm_CPU_Req[i]= stoi(s2);
        getline(stringStream4, s2, ' ');
        vm_RAM_Req[i]= stoi(s2);
        i++;
    }
}

void printSolution(vector<vector<unsigned long>> solution) {

    cout<< "The solution is:" <<endl;
    for (vector<unsigned long> pm : solution){
        printPM(pm);
    }
}

static void pmResourceUsage(const vector<unsigned long> &pm, int &cpu, int &ram) {
    cpu = 0;
    ram = 0;
    for (size_t chunk = 0; chunk < pm.size(); ++chunk) {
        unsigned long word = pm[chunk];
        while (word) {
#if defined(__GNUC__) || defined(__clang__)
            int bit = __builtin_ctzl(word);
#else
            int bit = 0;
            while (((word >> bit) & 1UL) == 0) ++bit;
#endif
            int vmId = static_cast<int>(chunk) * CONTAINER_SIZE + bit;
            if (vmId < TOTAL_VM_COUNT) {
                cpu += vm_CPU_Req[vmId];
                ram += vm_RAM_Req[vmId];
            }
            word &= word - 1;
        }
    }
}

//returns the fitness for cpu & ram
static unsigned long fitness(int cpu, int ram) {
    int cpuPenalty = (cpu > 0 ? cpu : 0);
    int ramPenalty = (ram > 0 ? ram : 0);

    if (cpuPenalty == 0) return static_cast<unsigned long>(ramPenalty);
    if (ramPenalty == 0) return static_cast<unsigned long>(cpuPenalty);
    return static_cast<unsigned long>(cpuPenalty * ramPenalty);
}

// Fitness function based on how much CPU/RAM exceeded in the PM
static unsigned long fitnessFunction(int cpu, int ram, int pmIndex) {
    int cpuExceed = cpu - pm_CPU[pmIndex];
    int ramExceed = ram - pm_RAM[pmIndex];
    return fitness(cpuExceed, ramExceed);
}

vector<vector<unsigned long>> initialSolution(){
    const int chunkCount = (TOTAL_VM_COUNT + CONTAINER_SIZE - 1) / CONTAINER_SIZE;
    vector<vector<unsigned long>> solution(
        TOTAL_PM_COUNT, vector<unsigned long>(chunkCount, 0UL));
    
    //available CPU/RAM specs of PMs
    vector<int> pmCpu(TOTAL_PM_COUNT, cpu_cap);
    vector<int> pmRam(TOTAL_PM_COUNT, ram_cap);
    
    vector<int> pmOrder(TOTAL_PM_COUNT);
    iota(pmOrder.begin(), pmOrder.end(), 0);    
    
    mt19937 gen(random_device{}());
    uniform_int_distribution<> distrib(0, TOTAL_PM_COUNT - 1);

    for (int vmId = 0; vmId < TOTAL_VM_COUNT; ++vmId) {
        //area this VM covers
        const unsigned long vmFitness = fitness(
            vm_CPU_Req[vmId], vm_RAM_Req[vmId]);

        //Sort PMs according to their fitness, ascending
        sort(pmOrder.begin(), pmOrder.end(), [&](int a, int b) {
            unsigned long fitA = fitness(pmCpu[a],
                 pmRam[a]);
            unsigned long fitB = fitness(pmCpu[b],
                 pmRam[b]);
            return fitA > fitB;
        });

        int assignedPm = -1;
        for (int pmIdx : pmOrder) {
            unsigned long pmFitness = fitness(pmCpu[pmIdx],
                 pmRam[pmIdx]);
            if (pmFitness >= vmFitness) {
                assignedPm = pmIdx;
                break;
            }
        }

        if (assignedPm < 0) {
            assignedPm = distrib(gen);
        }

        vector<unsigned long> &pm = solution[assignedPm];
        const int chunk = vmId / CONTAINER_SIZE;
        pm[chunk] |= (1UL << (vmId % CONTAINER_SIZE));
        pmCpu[assignedPm] -= vm_CPU_Req[vmId];
        pmRam[assignedPm] -= vm_RAM_Req[vmId];
    }

    return solution;
}

vector<vector<unsigned long>> initialize(ifstream &f, bool isCDataset){
    if (isCDataset) {
        readFileC(f);
    } else {
        readFile(f);
    }
    return initialSolution();
}

//Finds a 1 bit on a PM
static int randomVmOnPm(const vector<unsigned long> &pm, mt19937 &gen) {
    int vmCount = static_cast<int>(countVectorVMs(pm));
    if (vmCount == 0) return -1;

    uniform_int_distribution<> pick(0, vmCount - 1);
    int target = pick(gen);
    int seen = 0;
    for (size_t chunk = 0; chunk < pm.size(); ++chunk) {
        unsigned long word = pm[chunk];
        while (word) {
#if defined(__GNUC__) || defined(__clang__)
            int bit = __builtin_ctzl(word);
#else
            int bit = 0;
            while (((word >> bit) & 1UL) == 0) ++bit;
#endif
            int vmId = static_cast<int>(chunk) * CONTAINER_SIZE + bit;
            if (vmId < TOTAL_VM_COUNT) {
                if (seen == target) return vmId;
                ++seen;
            }
            word &= word - 1;
        }
    }
    return -1;
}
/*
Move a bit from source PM to dest PM
*/
void moveBit(vector<unsigned long> &source, vector<unsigned long> &dest, int coord){
    if (&source == &dest) return;
    int chunk = coord / CONTAINER_SIZE;
    int bit = coord % CONTAINER_SIZE;
    unsigned long mask = 1UL << bit;
    if ((source[chunk] & mask) == 0) return;
    source[chunk] &= ~mask;
    dest[chunk] |= mask;
}

/*
    Swap the bits of PMs
*/
void swapBits(vector<unsigned long> &pm1, vector<unsigned long> &pm2, int coord1,int coord2){
    if (&pm1 == &pm2) return;
    moveBit(pm1,pm2,coord1);
    moveBit(pm2,pm1,coord2);
}

// Guided local search: move VMs off overloaded PMs toward better destinations.
unsigned long run(vector<vector<unsigned long>> &solution){
    const int pmCount = TOTAL_PM_COUNT;
    const int moveCandidates = 24;
    const int stallLimit = 3000;

    vector<int> pmCpu(pmCount);//active CPU usages of PMs
    vector<int> pmRam(pmCount);//active RAM usages of PMs
    vector<unsigned long> pmFitness(pmCount);
    vector<int> overloaded;
    overloaded.reserve(pmCount);

    unsigned long bestFit = 0;
    for (int i = 0; i < pmCount; ++i) {
        pmResourceUsage(solution[i], pmCpu[i], pmRam[i]);
        pmFitness[i] = fitnessFunction(pmCpu[i], pmRam[i], i);
        bestFit += pmFitness[i];
        if (pmFitness[i] > 0) overloaded.push_back(i);
    }
    if (bestFit == 0) return 0;

    mt19937 gen(random_device{}());
    uniform_int_distribution<> pmDistr(0, pmCount - 1);
    uniform_int_distribution<> destDistr(0, pmCount - 1);

    int stalls = 0;
    auto limit = std::chrono::seconds(5);
    auto start = std::chrono::steady_clock::now();

    while ((std::chrono::steady_clock::now() - start) < limit &&
           bestFit > 0 && stalls < stallLimit) {
        int src;
        if (!overloaded.empty()) {
            uniform_int_distribution<> ovDistr(0, static_cast<int>(overloaded.size()) - 1);
            src = overloaded[ovDistr(gen)];
        } else {
            src = pmDistr(gen);
        }

        int vm = randomVmOnPm(solution[src], gen);
        if (vm < 0) {
            ++stalls;
            continue;
        }

        unsigned long bestCandFit = ULONG_MAX;
        int bestDst = -1;
        for (int c = 0; c < moveCandidates; ++c) {
            int dst = destDistr(gen);
            if (dst == src) continue;

            int newCpuSrc = pmCpu[src] - vm_CPU_Req[vm];
            int newRamSrc = pmRam[src] - vm_RAM_Req[vm];
            int newCpuDst = pmCpu[dst] + vm_CPU_Req[vm];
            int newRamDst = pmRam[dst] + vm_RAM_Req[vm];

            unsigned long newF1 = fitnessFunction(newCpuSrc, newRamSrc, src);
            unsigned long newF2 = fitnessFunction(newCpuDst, newRamDst, dst);
            unsigned long candFit = bestFit - pmFitness[src] - pmFitness[dst] + newF1 + newF2;

            if (candFit < bestCandFit) {
                bestCandFit = candFit;
                bestDst = dst;
            }
        }

        if (bestDst < 0 || bestCandFit >= bestFit) {
            ++stalls;
            continue;
        }

        moveBit(solution[src], solution[bestDst], vm);
        pmCpu[src] -= vm_CPU_Req[vm];
        pmRam[src] -= vm_RAM_Req[vm];
        pmCpu[bestDst] += vm_CPU_Req[vm];
        pmRam[bestDst] += vm_RAM_Req[vm];

        unsigned long oldSrcFit = pmFitness[src];
        unsigned long oldDstFit = pmFitness[bestDst];
        pmFitness[src] = fitnessFunction(pmCpu[src], pmRam[src], src);
        pmFitness[bestDst] = fitnessFunction(pmCpu[bestDst], pmRam[bestDst], bestDst);
        bestFit = bestFit - oldSrcFit - oldDstFit + pmFitness[src] + pmFitness[bestDst];

        stalls = 0;
        overloaded.clear();
        for (int i = 0; i < pmCount; ++i) {
            if (pmFitness[i] > 0) overloaded.push_back(i);
        }
    }
    return bestFit;
}

void openDataset(string datasetPath){
    int counter=0;
    ofstream outputFile("outputQuality.csv");

    outputFile << "File Name" <<"," << "Total PM Used" <<"," << "Lower Bound" <<","
    << "Solution Quality" <<"," << "Fitness(Excess CPUxRAM)" <<","<< "Elapsed Time(nanoseconds)" << endl;

    for (const auto& folder : fs::directory_iterator(datasetPath)){
        if (fs::is_directory(folder)){
            //cout << "Processing folder: " << folder.path().filename() << "\n";
            for (const auto& file : fs::directory_iterator(folder)) {
                //cout << " Processing  File: " << file.path() << "\n";
                for(int i = 0; i < MAX; i++){ // to run same data file multiple times
                    if (fs::is_regular_file(file)) {// free function instead of member
                        ifstream f(file.path().string());
                        if (!f.is_open()) {
                            cerr << "Error opening the file!"<<file.path().string()<<endl;
                            exit(1);
                        }

                        const string filePath = file.path().string();
                        const bool isCDataset = filePath.find('C') != string::npos;

                        //create initial solution
                        vector<vector<unsigned long>> solution = initialize(f, isCDataset);
                        // TOTAL_PM_COUNT set in the file
                        const int filePmCount = TOTAL_PM_COUNT;
                        // File name
                        string lbKey = file.path().stem().string();
                        // Lowest Possible PM amount
                        int lowerBound = PmLowerBounds.count(lbKey) ? PmLowerBounds[lbKey] : filePmCount;

                        TOTAL_PM_COUNT = lowerBound;
                        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                        unsigned long bestFit = run(solution);

                        while (bestFit != 0 && TOTAL_PM_COUNT < filePmCount) {
                            TOTAL_PM_COUNT++;
                            if (solution.size() < static_cast<size_t>(TOTAL_PM_COUNT)) {
                                solution = initialSolution();
                            }
                            bestFit = run(solution);
                        }
                        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                        //Write out the Solution Quality

                        outputFile << lbKey <<","
                            << TOTAL_PM_COUNT <<"," << lowerBound <<","
                            << 100 * (TOTAL_PM_COUNT / (double)lowerBound - 1 ) <<","
                            << bestFit <<","
                            << std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count()  << endl;
                        outputFile.flush();

                        f.close();
                    }
                }
                counter++;
            }
        }
    }
    outputFile.close();
}

void initializeLowerBounds(ifstream &infile){
    PmLowerBounds.clear();
    string line;
    while (getline(infile, line)) {
        
        // Strip BOM from the first line
        if (line.size() >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            line = line.substr(3);
        }

        // Strip \r in case of Windows line endings
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            stringstream ss(line);
            string fileName;
            int lowerBound;
            
            if (ss >> fileName >> lowerBound) {
                //fileName += ".vmp";
                PmLowerBounds[fileName] = lowerBound;
            }

        }
    }
}

int main(int argc, char *argv[])
{

    ifstream infile("LowerBounds.txt");
    if (!infile.is_open()) {
        cerr << "Error opening LowerBounds.txt" << endl;
        return 1;
    }
    
    initializeLowerBounds(infile);
    //openDataset(string(argv[1]),100);
    openDataset("./dataset/Instances/");

    return 0;
}