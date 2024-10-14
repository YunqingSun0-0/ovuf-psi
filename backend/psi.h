#ifndef OLE_PSI_H
#define OLE_PSI_H

#include "emp-ot/emp-ot.h"
#include "backend/bn_utils.h"
#include <iostream>
#include "backend/ole.h"
#include "backend/ovuf.h"
#include <fstream>
#include <unordered_set>
#include "backend/set_op.h"
#include <cstdint>

extern "C" {
#include <relic/relic.h>
}

template<typename IO>
class PSI {
public:
    IO *io;
    COT<IO> *ot;
    bn_t q;
    OLE<IO> *ole;
    OVUF<IO> *ovuf;
    SET_OP *setOp;
    int party;
    g1_t g1;
    g2_t g2;
    bn_t inverse_helper;
    ThreadPool *pool;
    int threads;

    PSI(int party, IO *io, COT<IO> *ot, OLE<IO> *ole, OVUF<IO> *ovuf, SET_OP *setOp, bn_t q2, g1_t g11, g2_t g22,
        ThreadPool *pool) : io(io), ot(ot), ole(ole), ovuf(ovuf), setOp(setOp), party(party) {
        bn_new(q);
        bn_copy(this->q, q2);
        g1_new(g1);
        g1_copy(this->g1, g11);
        g2_new(g2);
        g2_copy(this->g2, g22);
        bn_new(inverse_helper)
        bn_set_dig(inverse_helper, 2);
        bn_sub(inverse_helper, q, inverse_helper); //set inverse_helper = q-2
        this->pool = pool;
        this->threads = pool->size();
    }

    ~PSI() {
        bn_free(inverse_helper);
        g2_free(g2);
        g1_free(g1);
        bn_free(q);
    }

    void receiver_compute(string r_setname, string r_encode, bn_t sk, g2_t pk) {
        //let client (party 1) send its private set size to server. Both party init.
	    int size;
        if (party == ALICE) {
            io->recv_data(&size, 4);
        } else {
            size = setOp->get_set_size(r_setname);
            io->send_data(&size, 4);
        }
        bn_t *in = new bn_t[size];
        g1_t *out = new g1_t[size];
        ole->multi_task_initbn(in, size);
        ole->multi_task_initg1(out, size);


        int index = 0;
        uint8_t indexarr[4];
	    if (party == BOB) {
            //read elements from file to in[i]
     		index = setOp->read_bn_from_file(r_setname, in, q);
            *(uint32_t *) indexarr = index;
        } else {
            //set in[i] as secret key sk
            for (int i = 0; i < size; ++i)
                bn_copy(in[i], sk);
        }

        ovuf->compute_batch(out, in, pk, size);// OVUF protocol encodes element in[i] (BoB) by using sk (Alice) and output out[i] (Bob)

        if (party == BOB) {
            uint8_t *data = new uint8_t[8 * size + 4];
            uint8_t *encode = new uint8_t[8 * size];
            multi_task_g1_to_hash(size, out, encode); // use multi-thread to hash the encoded element out[i] (Bob) to encode[i] (Bob)
            for (int i = 0; i < 4; ++i) {
                data[i] = indexarr[i];
            }
            for (int i = 4; i < 4 + 8 * index; ++i) {
                data[i] = encode[i - 4];
            }
            setOp->write_to_file(data, 8 * size + 4, r_encode); //write encode[i] to a file. with size inserted beforehand.
            delete[] encode;
            delete[] data;
	    }
        delete[] out;
        delete[] in;
    }

    void task(uint8_t *encode, g1_t *out, int start, int end) {
        uint8_t *dig = new uint8_t[Hash::DIGEST_SIZE];
        int i = start;
        for (; i < end; ++i) {
            Hashg1(dig, out[i]);
            memcpy(encode + 8 * i, dig, 8);
        }
        delete[] dig;
    }

    void multi_task_g1_to_hash(int size, g1_t *out, uint8_t *encode) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, encode, out, start, end]() {
                task(encode, out, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task(encode, out, start, end);
        for (auto &f: fut) f.get();
    }
};


#endif //OLE_PSI_H
