#include <stdio.h>
#include <iostream>
#include <cmath>
#include <bitset>
#include "bit_operations.h"
#include <fstream>
#include <sstream>
#include <random>
#include <iostream>

#include <experimental/filesystem>
namespace std {
    namespace filesystem = experimental::filesystem;
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
    
    TOTAL_PM_COUNT = 13; // theoretical minimum for VMP_A100
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

void printSolution() {
    
    cout<< "The solution is:" <<endl;
    for (vector<unsigned long> pm : solution){
        printPM(pm); 
    }
}

void initialSolution(){
    
    solution = {};
    for(int i=0;i<TOTAL_PM_COUNT;i++){
        solution.push_back(createEmptyM());
    }//Empty solution
    
    int min = 0;
    int max = TOTAL_PM_COUNT - 1;
    // Initialize a random number generator
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> distrib(min, max);
    
    //randomly assign bits to PMs
    for(int i=0;i<TOTAL_VM_COUNT;i++){
        // for every VM, i, choose a random PM, solution.at(distrib(gen)) 
        solution.at(distrib(gen)) = bitwiseOrMs(solution.at(distrib(gen)), idToVM(i));
    }
}

void initialize(ifstream &f){
    
    readFile(f);
    initialSolution();
}

/* Her PM için HER VM gezmek çok gereksiz, Her VM için Her PM gezilebilir
*/ 
unsigned long fitnessFunction(vector<vector<unsigned long>> solution){

    int total=0;
    for(vector<unsigned long> pm : solution){
        int pmCpuUsage=0;
        int pmRamUsage=0;        
        for(int i=0;i<TOTAL_VM_COUNT;i++){
            if(countVectorVMs(bitwiseAndMs(pm,idToVM(i)))) {
                pmCpuUsage += vm_CPU_Req[i];
                pmRamUsage += vm_RAM_Req[i];
            }
        }
        int cpuExceed = pmCpuUsage - cpu_cap;
        int ramExceed = pmRamUsage - ram_cap;
        total += (cpuExceed > 0 ? cpuExceed : 0) * (ramExceed > 0 ? ramExceed : 0);
    }
    return total;
}
/*
Move a bit from source PM to dest PM
*/
void moveBit(vector<unsigned long> &source, vector<unsigned long> &dest, int coord){
    vector<unsigned long> bit = idToVM(coord);
    source = removeSpecificVM(source,bit);
    dest = bitwiseOrMs(dest,bit);
}

/*
    Swap the bits of PMs
*/
void swapBits(vector<unsigned long> &pm1, vector<unsigned long> &pm2, int coord1,int coord2){
    moveBit(pm1,pm2,coord1);
    moveBit(pm2,pm1,coord2);
}

void run(int epochCount){
    

        int pmMin = 0;
        int pmMax = TOTAL_PM_COUNT - 1;
        // Initialize a random number generator
        random_device rd;
        mt19937 gen(rd());
        // Random PM
        uniform_int_distribution<> pmDistr(pmMin, pmMax);
    
        int vmMin = 0;
        int vmMax = TOTAL_VM_COUNT - 1;
        // Random VM
        uniform_int_distribution<> vmDistr(vmMin, vmMax);

    for(int i=0;i<epochCount;i++){

        vector<vector<unsigned long>> currentSolution(solution);
        unsigned long bestSolution = fitnessFunction(solution);
        if(bestSolution == 0)
            break;
        swapBits(currentSolution.at(pmDistr(gen)),currentSolution.at(pmDistr(gen))
        ,vmDistr(gen),vmDistr(gen));
        
        if ( fitnessFunction(currentSolution) < bestSolution ){
            vector<vector<unsigned long>> copy(currentSolution);
            solution = copy;
        }    
    }
}

vector<string> openDataset(string path){
    std::vector<std::string> file_list;
    try {
        if (filesystem::exists(path) && filesystem::is_directory(path)) {
            for (const auto& entry : filesystem::directory_iterator(path)) {
                // Add only regular files to the list
                if (filesystem::is_regular_file(entry.path())) {
                    file_list.push_back(entry.path().string());
                }
            }
        }
    } catch (const filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return file_list;
}

int main(int argc, char *argv[])
{
    /* bit operations test
    TOTAL_VM_COUNT = 125;
    cout<<"CONTAINER_SIZE = " << CONTAINER_SIZE <<endl;
    vector<unsigned long> pm1 = createEmptyM();
    pm1 = bitwiseOrMs(pm1,idToVM(1));
    pm1 = bitwiseOrMs(pm1,idToVM(65));
    //pm1 = bitwiseOrMs(pm1,idToVM(129));
    cout<<"initial pm 1,65 th bits are on"<<endl;
    printPM(pm1);
    cout<<"countVectorVMs(pm1) = "<<countVectorVMs(pm1)<<endl;
    cout<<"checkSameVM(pm1,idToVM(1)) = "<< checkSameVM(pm1,idToVM(1)) <<endl;
    cout<<endl;
    cout<<"removeSpecificVM(pm1,idToVM(1)) = "<<endl;
    printPM(removeSpecificVM(pm1,idToVM(1)));
    cout<<endl;
    //printPM(bitwiseSRMs(pm1,65));*/

    /*
    //ifstream f("VMP_A100.vmp");
    ifstream f(argv[1]);
    // Check if the file is 
    // successfully opened
    if (!f.is_open()) {
        cerr << "Error opening the file!";
        return 1;
    }
    initialize(f);
    //printSolution();
    cout<<"Initial Solution Fitness = "<< fitnessFunction(solution) <<endl;
    run(700);
    cout<<"Final Solution Fitness = "<< fitnessFunction(solution) <<endl;
    // Close the file
    f.close();*/

    std::string path = "./dataset/Instances/VMP_A100"; // Your target folder


    // Display the list
    for (const auto& file : file_list) {
        std::cout << file << std::endl;
    }

    return 0;
}