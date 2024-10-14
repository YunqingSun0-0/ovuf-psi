#include "backend/psi.h"
#include <iostream>
#include "emp-ot/emp-ot.h"
extern "C" {
#include <relic/relic.h>
}

using namespace std;

int main(int argc, char **argv) {
	cout << "******************" << endl;
	cout << "start_gen_key_pair:" << endl;
    string skfile, pkfile;
    skfile = argv[1];
    pkfile = argv[2];

    ThreadPool *pool = new ThreadPool(8);

    if (core_init() != RLC_OK) {
        core_clean();
        return 1;
    }
    if (pc_param_set_any() != RLC_OK) {
        RLC_THROW(ERR_NO_CURVE);
        core_clean();
        return 0;
    }
    bn_t q;
    bn_new(q);
    if (pc_param_set_any() == RLC_OK) {
        pc_get_ord(q);
    }

    SET_OP setOp(q,pool);

    bn_t sk;
    g2_t pk;
    bn_new(sk)
    g2_new(pk)
    bn_rand_mod(sk, q);
    g2_mul_gen(pk, sk);
    bn_print(sk);
    g2_print(pk);
    auto t1 = clock_start();
    //transform sk to uint8_t
    uint32_t length1 = bn_size_bin(sk);
    uint8_t length1arr[4];
    *(uint32_t*)length1arr = length1;
    uint8_t skdata[length1];
    bn_write_bin(skdata, length1, sk);
    uint8_t data1[length1+4];
    for(int i=0; i< 4; ++i){
        data1[i] = length1arr[i];
    }
    for(int i=4; i< 4+length1; ++i){
        data1[i] = skdata[i-4];
    }
    //write to skfile
    setOp.write_to_file(data1, length1+4, skfile);

    //transform pk to uint8_t
    uint32_t length2 = g2_size_bin(pk, 0);
    uint8_t length2arr[4];
    *(uint32_t*)length2arr = length2;
    uint8_t pkdata[length2];
    g2_write_bin(pkdata, length2, pk, 0);
    uint8_t data2[length2+4];
    for(int i=0; i< 4; ++i){
        data2[i] = length2arr[i];
    }
    for(int i=4; i< 4+length2; ++i){
        data2[i] = pkdata[i-4];
    }
    //write to pkfile
    setOp.write_to_file(data2, length2+4, pkfile);
    cout << "Time for generate_key" << time_from(t1)/1000 << endl;
    cout << "**************************" << endl;
}
