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

#include <experimental/filesystem>

const int MAX=10;
// When a move is worse than bestFit, accept it with this probability (exploration).
const double WORSE_ACCEPT_PROBABILITY = 0.003;

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

    for (int i = 0; i < m1.size(); i++) {
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
//Dataset C has different format
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
    int pm1Count = stoi(s2);
    getline(stringStream, s2, ',');
    int pm2Count = stoi(s2);
    TOTAL_PM_COUNT = pm1Count + pm2Count;

    //3rd line is CPU & RAM caps of pm1s
    getline(f, s);
    stringstream  stringStream2(s);
    getline(stringStream2, s2, ',');
    int cpuCap1 = stoi(s2);
    getline(stringStream2, s2, ',');
    int ramCap1 = stoi(s2);
    
    //4th Line is CPU & RAM caps of pm2s
    getline(f, s);
    stringstream  stringStream3(s);
    getline(stringStream3, s2, ',');
    int cpuCap2 = stoi(s2);
    getline(stringStream3, s2, ',');
    int ramCap2 = stoi(s2);    

    //5th Line is total VM count
    getline(f, s);
    TOTAL_VM_COUNT = stoi(s);

    cpu_cap = cpuCap1 + cpuCap2;
    vm_CPU_Req = new int[TOTAL_VM_COUNT];
    vm_RAM_Req = new int[TOTAL_VM_COUNT];
    
    //array for cpu & ram caps of PMs

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

vector<vector<unsigned long>> initialSolution(){
    const int chunkCount = (TOTAL_VM_COUNT + CONTAINER_SIZE - 1) / CONTAINER_SIZE;
    vector<vector<unsigned long>> solution(
        TOTAL_PM_COUNT, vector<unsigned long>(chunkCount, 0UL));

    mt19937 gen(random_device{}());
    uniform_int_distribution<> distrib(0, TOTAL_PM_COUNT - 1);

    for (int vmId = 0; vmId < TOTAL_VM_COUNT; ++vmId) {
        vector<unsigned long> &pm = solution[distrib(gen)];
        const int chunk = vmId / CONTAINER_SIZE;
        pm[chunk] |= (1UL << (vmId % CONTAINER_SIZE));
    }
    return solution;
}

vector<vector<unsigned long>> initialize(ifstream &f){
    readFile(f);
    return initialSolution();
}

static unsigned long pmExcessFitness(const vector<unsigned long> &pm) {
    int pmCpuUsage = 0;
    int pmRamUsage = 0;
    for (size_t chunk = 0; chunk < pm.size(); ++chunk) {
        unsigned long word = pm[chunk];
        while (word) {
#if defined(__GNUC__) || defined(__clang__)
            int bit = __builtin_ctzl(word); //Index of the lowest set bit
#else
            int bit = 0;
            while (((word >> bit) & 1UL) == 0) ++bit;
#endif
            int vmId = static_cast<int>(chunk) * CONTAINER_SIZE + bit;
            if (vmId < TOTAL_VM_COUNT) {
                pmCpuUsage += vm_CPU_Req[vmId];
                pmRamUsage += vm_RAM_Req[vmId];
            }
            word &= word - 1;
        }
    }
    int cpuExceed = pmCpuUsage - cpu_cap;
    int ramExceed = pmRamUsage - ram_cap;
    return static_cast<unsigned long>(
        (cpuExceed > 0 ? cpuExceed : 0) * (ramExceed > 0 ? ramExceed : 0));
}

unsigned long fitnessFunction(vector<vector<unsigned long>> solution) {
    unsigned long total = 0;
    for (const vector<unsigned long> &pm : solution) {
        total += pmExcessFitness(pm);
    }
    return total;
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

// Runs for 5 seconds max
unsigned long run(vector<vector<unsigned long>> &solution){

    mt19937 gen(random_device{}());
    uniform_int_distribution<> pmDistr(0, TOTAL_PM_COUNT - 1);
    uniform_int_distribution<> vmDistr(0, TOTAL_VM_COUNT - 1);
    uniform_real_distribution<> acceptDistr(0.0, 1.0);

    vector<unsigned long> pmFitness(solution.size());
    unsigned long bestFit = 0;
    for (size_t i = 0; i < solution.size(); ++i) {
        pmFitness[i] = pmExcessFitness(solution[i]);
        bestFit += pmFitness[i];
    }

    auto limit = std::chrono::seconds(5);
    auto start = std::chrono::steady_clock::now();

    while ((std::chrono::steady_clock::now() - start) < limit) {
        if (bestFit == 0) break;

        int pm1Idx = pmDistr(gen);
        int pm2Idx = pmDistr(gen);
        int vm1 = vmDistr(gen);
        int vm2 = vmDistr(gen);

        unsigned long oldPm1Fit = pmFitness[pm1Idx];
        unsigned long oldPm2Fit = pmFitness[pm2Idx];

        swapBits(solution[pm1Idx], solution[pm2Idx], vm1, vm2);

        unsigned long newPm1Fit = pmExcessFitness(solution[pm1Idx]);
        unsigned long newPm2Fit = pmExcessFitness(solution[pm2Idx]);
        unsigned long currentFit = bestFit - oldPm1Fit - oldPm2Fit + newPm1Fit + newPm2Fit;

        bool acceptMove = currentFit < bestFit;
        //  probability to accept a move that is worse than bestFit
        if (!acceptMove &&
            acceptDistr(gen) < WORSE_ACCEPT_PROBABILITY) {
            acceptMove = true;
        }

        if (acceptMove) {
            bestFit = currentFit;
            pmFitness[pm1Idx] = newPm1Fit;
            pmFitness[pm2Idx] = newPm2Fit;
        } else {
            swapBits(solution[pm1Idx], solution[pm2Idx], vm1, vm2);
        }
    }
    return bestFit;
}


void openDataset(string datasetPath){
    int counter=0;
    ofstream outputFile("outputQuality.csv");

    outputFile << "File Name" <<"," << "Total PM Used" <<"," << "Lower Bound" <<","
    << "Solution Quality" <<"," << "Fitness(Excess CPUxRAM)" <<","<< "Elapsed Time(microseconds)" << endl;

    for (const auto& folder : fs::directory_iterator(datasetPath)){
        if (fs::is_directory(folder)){
            //cout << "Processing folder: " << folder.path().filename() << "\n";
            for (const auto& file : fs::directory_iterator(folder)) {
                for(int i = 0; i < MAX; i++){ // to run same data file multiple times
                    if (fs::is_regular_file(file)) {// free function instead of member
                        //cout << " Processing  File: " << file.path() << "\n";
                        ifstream f(file.path().string());
                        if (!f.is_open()) {
                            cerr << "Error opening the file!"<<file.path().string()<<endl;
                            exit(1);
                        }

                        //create initial solution
                        vector<vector<unsigned long>> solution = initialize(f);
                        TOTAL_PM_COUNT = PmLowerBounds[counter];
                        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                        unsigned long bestFit = run(solution);

                        while (bestFit != 0) {
                            TOTAL_PM_COUNT++;
                            if (solution.size() < static_cast<size_t>(TOTAL_PM_COUNT)) {
                                //solution.push_back(createEmptyM());
                                ssolution = initialSolution();
                            }
                            bestFit = run(solution);
                        }
                        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                        //Write out the Solution Quality

                        outputFile << file.path().string() <<","
                            << TOTAL_PM_COUNT <<"," << PmLowerBounds[counter] <<","
                            << 100 * (TOTAL_PM_COUNT / (double)PmLowerBounds[counter] - 1 ) <<","
                            << bestFit <<","
                            << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()  << endl;
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

int main(int argc, char *argv[])
{

    ifstream infile("LowerBounds.txt");
    string line;
    int i=0;
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
            PmLowerBounds[i]= stoi(line);
            i++;
        }
    }

    //openDataset(string(argv[1]),100);
    openDataset("./dataset/Instances/");

    return 0;
}
