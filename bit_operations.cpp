#include <stdio.h>
#include <iostream>
using namespace std;
#include <cmath>
#include <vector>

const int TOTAL_VM_COUNT = 192;
const int CONTAINER_SIZE = sizeof(long)*8;

/*
bool hasSameVM(unsigned long pm1,unsigned long pm2) {
    return checkSameVM(pm1,pm2) > 0;
}*/

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
    int totalChunks = std::ceil(TOTAL_VM_COUNT / CONTAINER_SIZE); // sorun olabilir
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
        cout << format("{:b}",n ) << " ";
    }
    cout<<endl;
}

vector<unsigned long> checkSameVM(vector<unsigned long> pm1,vector<unsigned long> pm2){
    return bitwiseAndMs(pm1,pm2);
}

/*  there should be only single '1' bit in vm
    pm can have multiple '1' bits    
*/
vector<unsigned long> removeSpecificVM(vector<unsigned long> pm,vector<unsigned long> vm){
    //return pm & (~vm);
    return bitwiseAndMs(pm,bitwiseNotMs(vm));
}

/*
 Second case VM count > 64 so that its more than 1 number
 sizeof(long)   = 64
 sizeof(int)    = 32 use int if it's easier to do operations
 PM is a vector
 VM is a vector
*/
int main()
{
    vector<unsigned long> pm1 = createEmptyM();
    pm1 = bitwiseOrMs(pm1,idToVM(1));
    pm1 = bitwiseOrMs(pm1,idToVM(65));
    pm1 = bitwiseOrMs(pm1,idToVM(129));
    printPM(pm1);
    cout<<"countVectorVMs(pm1) = "<<countVectorVMs(pm1)<<endl;
    
    cout<<"checkSameVM(pm1,idToVM(1)) = "<<endl;
    printPM(checkSameVM(pm1,idToVM(1)));
    cout<<endl;
    cout<<"removeSpecificVM(pm1,idToVM(1)) = "<<endl;
    printPM(removeSpecificVM(pm1,idToVM(1)));
    cout<<endl;
    //printPM(bitwiseSRMs(pm1,65));
    
    return 0;
}