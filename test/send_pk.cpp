#include "backend/set_op.h"
#include <iostream>
#include "emp-ot/emp-ot.h"
#include "backend/bn_utils.h"
extern "C" {
#include <relic/relic.h>
}

using namespace std;
#define THREADS_NUM 1

int main(int argc, char **argv) {
    cout << "***************" << endl;
    cout << "server send pk starts" << endl;
    int port;
    int party;
    parse_party_and_port(argv, &party, &port);
    NetIO *ios[1];
    for (int i = 0; i < 1; ++i)
        ios[i] = new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i);

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
    SET_OP setOp(q, pool); //init set operation
    g2_t pk;
    g2_new(pk)

    string pkfile;
    pkfile = argv[3];
    if(party == ALICE){ //ALICE/party 1/server
        //read pk from file and send
        setOp.read_pk_from_file(pkfile, pk);
        send_g2(ios[0], pk);
        cout << "pk sent" << endl;
    }else{
        //recv pk from server (ALICE/party 1)
        recv_g2(ios[0], pk);
        uint32_t length2 = g2_size_bin(pk, 0);
        uint8_t length2arr[4];
        *(uint32_t*)length2arr = length2;
        uint8_t pkdata[length2];
        g2_write_bin(pkdata, length2, pk, 0);
        uint8_t data2[length2+4];
        for(int i=0; i< 4; ++i){
            data2[i] = length2arr[i];
        }
        for(uint32_t i=4; i< 4+length2; ++i){
            data2[i] = pkdata[i-4];
        }
        //write pk to file
        setOp.write_to_file(data2, length2+4, pkfile);
        cout << "pk received" << endl;
    }
    cout << "************************" << endl;
}
