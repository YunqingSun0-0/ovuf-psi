#include "emp-ot/emp-ot.h"
#include <random>
#include <iostream>
using namespace std;
inline char converse(char ch) {
    int remain = (ch % 62 + 62) % 62;
    if (remain < 26) return 'a' + remain;
    else if (remain < 52) return 'A' + remain - 26;
    else return '0' + remain - 52;
}
int main(int argc, char **argv) {
    int size; size = ::atoi(argv[1]);
    cout << size << endl;
    PRG prg;
    char *test = new char[32];
    for (int i = 0; i < size-3; ++i) {
        prg.random_data(test, 32);
        for (int j = 0; j < 32; j++) {
            cout << converse(test[j]);
        }
        cout << endl;
    }
    cout << "testcase1wvdmicwtej" << endl;
    cout << "testcase2subroyggbw" << endl;
    cout << "testcase3ediwhxwfbd";
    delete[] test;
    return 0;
}
