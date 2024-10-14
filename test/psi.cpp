#include "backend/psi.h"
#include "emp-ot/emp-ot.h"
#include "backend/set_op.h"
#include <iostream>
extern "C" {
#include <relic/relic.h>
}

#define FERRET_IO_NUM 4
#define IKNP_IO_NUM 1

#if OT_TYPE == OT_TYPE_FERRET
 #define IO_NUM FERRET_IO_NUM
#else
 #define IO_NUM IKNP_IO_NUM
#endif

using namespace std;
using namespace emp;
int main(int argc, char **argv) {
    cout << "*****************" << endl;
    cout << "interactive psi begins:" << endl;
    int port;
    int party;
    int secpara = 40;
    int modulus = 256;
    int kappa = 256;
    int THREADS_NUM = atoi(argv[3]);
    ThreadPool *pool = new ThreadPool(THREADS_NUM);
    parse_party_and_port(argv, &party, &port);
    NetIO *ios[IO_NUM];
    for (int i = 0; i < IO_NUM; ++i)
        ios[i] = new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i);

    auto clock = clock_start();

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
    g1_t g1;
    g2_t g2;
    g1_new(g1);
    g2_new(g2);
    g1_get_gen(g1);
    g2_get_gen(g2);
    
#if OT_TYPE == OT_TYPE_FERRET
    //Use FERRET COT to generate COTs
    int inverseparty;
    if(party == 1){
        inverseparty = 2;
    }else{
        inverseparty = 1;
    }
    FerretCOT<NetIO> *cot = new FerretCOT<NetIO>(inverseparty, IO_NUM, ios, true, true, ferret_b13);
#else
    //Use IKNP COT to generate COTs
    IKNP <NetIO> *cot = new IKNP <NetIO> (ios[0], true);
    if (party == BOB) {
        cot->setup_send();
    } else {
        cot->setup_recv();
    }
#endif
    OLE<NetIO> *ole = new OLE<NetIO>(party, ios[0], cot, q, modulus, kappa, secpara, pool);
    OVUF<NetIO> *ovuf = new OVUF<NetIO>(party, ios[0], cot, ole, q, modulus, kappa, secpara, g1, g2, pool);
    SET_OP *setOp = new SET_OP(q, pool);
    PSI<NetIO> psi(party, ios[0], cot, ole, ovuf, setOp, q, g1, g2, pool);
    bn_t sk;
    g2_t pk;
    bn_new(sk)
    g2_new(pk)
    string keyfile;
    keyfile = argv[4];
    string r_setname, r_encode;

    //read pk/sk from file
    if (party == BOB) {
        setOp->read_pk_from_file(keyfile, pk);
        r_setname = argv[5];
        r_encode = argv[6];
    } else {
        setOp->read_sk_from_file(keyfile, sk);
    }

    //both parties interactively compute client's set encoding
    
    psi.receiver_compute(r_setname, r_encode, sk, pk);
    cout << "receiver encoding: " << time_from(clock)/1000 << endl;

    uint64_t counter = 0;
    for(int i = 0; i< IO_NUM; ++i){
        counter = counter + ios[i]->counter;
    }
    std::cout << "bandwidth:" << counter << endl;

    //free
    g2_free(pk)
    bn_free(sk)
    delete setOp;
    delete ovuf;
    delete ole;
    delete cot;
    g1_free(g2)
    g1_free(g1)
    bn_free(q)
    for (int i = 0; i < IO_NUM; ++i)
        delete ios[i];
    delete pool;
    core_clean();
    cout << "*********************" << endl;
}

