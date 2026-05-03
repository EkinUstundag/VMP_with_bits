/******************************************************************************

Welcome to GDB Online.
  GDB online is an online compiler and debugger tool for C, C++, Python, PHP, Ruby, 
  C#, OCaml, VB, Perl, Swift, Prolog, Javascript, Pascal, COBOL, HTML, CSS, JS
  Code, Compile, Run and Debug online from anywhere in world.

*******************************************************************************/
#include <stdio.h>
#include <iostream>
using namespace std;
#include <cmath>
#include <vector>

const int TOTAL_VM_COUNT = 192;
const int CONTAINER_SIZE = sizeof(long)*8;

unsigned long checkSameVM(unsigned long pm1,unsigned long pm2){
    return pm1 & pm2;
}

bool hasSameVM(unsigned long pm1,unsigned long pm2) {
    return checkSameVM(pm1,pm2) > 0;
}

//Brian Kernighan Algorithm
unsigned long countVMs(unsigned long pm){
    
    unsigned long count =0;
    while(pm > 0){
        pm = pm & (pm-1);   
        count++;
    }
    return count++;
}

/*  there should be only single '1' bit in vm
    pm can have multiple '1' bits    
*/
unsigned long removeSpecificVM(unsigned long pm,unsigned long vm){
    
    return pm & (~vm);
    
}

vector<unsigned long> idToVM(int id){
    
    vector<unsigned long> vm = {};
    //my_list.push_back(40);    // Adds to end
    //my_list.push_front(5);    // Adds to beginning
    int totalChunks = std::ceil(TOTAL_VM_COUNT / CONTAINER_SIZE);
    int chunk= id / CONTAINER_SIZE ;
    //Initialize an empty pm
    for(int i=0;i<totalChunks;i++){
        
        if( i == chunk){
            int coord = id % (sizeof(long)*8);
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

        if (i - blockShift < 0) continue;

        // Shift current block
        result.at(i) |= (m1.at(i - blockShift) << bitShift);
        //result[i] |= m1[i - blockShift] << bitShift;

        // Handle carry from previous block
        if ( bitShift != 0 && i + blockShift + 2 <=m1.size()) {
            
            //result[i] |= m1[i - blockShift - 1] >> (BITS - bitShift);
            result.at(i) |= (m1.at(i + blockShift+1) >> (CONTAINER_SIZE - bitShift));
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
    //vector<unsigned long> pm2 = createEmptyM();
    pm1 = bitwiseOrMs(pm1,idToVM(1));
    //printPM(pm1);
    pm1 = bitwiseOrMs(pm1,idToVM(63));
    //printPM(pm1);
    pm1 = bitwiseOrMs(pm1,idToVM(95));
    //printPM(pm1);
    pm1 = bitwiseOrMs(pm1,idToVM(126));
    //printPM(pm1);
    //pm1 = bitwiseOrMs(pm1,idToVM(150));
    //printPM(pm1);
    pm1 = bitwiseOrMs(pm1,idToVM(190));
    printPM(pm1);
    //printPM(idToVM(63));
    
    printPM(bitwiseSLMs(pm1,2));
    
    return 0;
}