#include "backend/set_op.h"
#include <iostream>
#include "emp-ot/emp-ot.h"
extern "C" {
#include <relic/relic.h>
}

using namespace std;

int main(int argc, char **argv) {
    cout << "***************" << endl;
    cout << "start local encoding" << endl;

    //initialization
    int THREADS_NUM = atoi(argv[4]);
    ThreadPool * pool = new ThreadPool(THREADS_NUM);
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
    pc_get_ord(q);
    SET_OP setOp(q, pool);
    bn_t sk;
    bn_new(sk)

    //read sk from file
    string skfile;
    skfile = argv[1];
    setOp.read_sk_from_file(skfile, sk);
    int size[2];
    string s_setname, s_encode;
    s_setname = argv[2];
    s_encode = argv[3];

    //do local encoding
    auto clock = clock_start();
    size[0] = setOp.serv_encode(sk, s_setname, s_encode, q);
    cout << "local encoding: " << time_from(clock)/1000 << endl;

    cout << "************************" << endl;
}
