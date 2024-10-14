#include "backend/psi.h"
#include "emp-ot/emp-ot.h"
#include "backend/set_op.h"
#include <iostream>

extern "C" {
#include <relic/relic.h>
}

using namespace std;
using namespace emp;

int main(int argc, char **argv) {
	cout << "**********************" << endl;
	cout<< "start_look_up:" << endl; 
	string s_encode, r_encode;
	s_encode = argv[1];
	r_encode = argv[2];
	auto t1 = clock_start();
	std::unordered_set<uint64_t> hashTable;
	FILE  *fpS;
	fpS = fopen(s_encode.c_str(), "rb+");
	uint32_t lengthS;
	fread(&lengthS, 4, 1, fpS);
	fseek(fpS, 4, 0);
	uint64_t *dataS = new uint64_t[lengthS];
	fread(dataS, 8, lengthS, fpS);
	fclose(fpS);
	for(int i = 0; i < lengthS; ++i)
		hashTable.insert(dataS[i]);

	cout << "insert to hashtable:" << time_from(t1)/1000 << endl;
	auto t2 = clock_start();

	FILE  *fpR = fopen(r_encode.c_str(), "rb+");
	uint32_t lengthR;
	fread(&lengthR, 4, 1, fpR);
	fseek(fpR, 4, 0);
	uint64_t *dataR = new uint64_t[lengthR];
	fread(dataR, 8, lengthR, fpR);
	fclose(fpR);
	int intersection = 0;
	for(int i = 0; i < lengthR; ++i){
		if(hashTable.find(dataR[i]) != hashTable.end()){
			intersection++;
//			cout << "intersection index:" << i;
		}
	}
	cout << "lookup in hashtable:" << time_from(t2)/1000 << endl; 
	cout << "intersection size:" << intersection << endl;
	cout << "*****************************" << endl;
	return 0;
}
