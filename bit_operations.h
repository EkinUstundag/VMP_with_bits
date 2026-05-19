#ifndef BIT_OPERATIONS_H_INCLUDED
#define BIT_OPERATIONS_H_INCLUDED
#include <string>
#include <vector>
using namespace std;

const int CONTAINER_SIZE = sizeof(long)*8;
int TOTAL_VM_COUNT;


int cpu_cap;
int ram_cap;
int* vm_CPU_Req;
int* vm_RAM_Req;
string instanceName;

int PmLowerBounds[600]={};
int TOTAL_PM_COUNT;
// These below will not be used, but can be useful later
int* pm_CPU; //If PMs have different specs, this will be useful
int* pm_RAM; //If PMs have different specs, this will be useful


unsigned long countVMs(unsigned long vm);
unsigned long countVectorVMs(vector<unsigned long> vm);
vector<unsigned long> idToVM(int id);
vector<unsigned long> createEmptyM();
vector<unsigned long> bitwiseOrMs(vector<unsigned long> m1,vector<unsigned long> m2);
vector<unsigned long> bitwiseAndMs(vector<unsigned long> m1,vector<unsigned long> m2);
vector<unsigned long> bitwiseXorMs(vector<unsigned long> m1,vector<unsigned long> m2);
vector<unsigned long> bitwiseNotMs(vector<unsigned long> m1);
vector<unsigned long> bitwiseSLMs(vector<unsigned long> m1, unsigned long shift);
vector<unsigned long> bitwiseSRMs(vector<unsigned long> m1, unsigned long shift);
void printPM(vector<unsigned long> pm);
unsigned long checkSameVM(vector<unsigned long> pm1,vector<unsigned long> pm2);
vector<unsigned long> removeSpecificVM(vector<unsigned long> pm,vector<unsigned long> vm);
#endif // BIT_OPERATIONS_H_INCLUDED
