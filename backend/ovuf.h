#ifndef __OVUF_H
#define __OVUF_H

#include "emp-ot/emp-ot.h"
#include "backend/bn_utils.h"

extern "C" {
#include <relic/relic.h>
}

#include <iostream>
#include "backend/ole.h"
using namespace std;
template<typename IO>
class OVUF {
public:
    IO *io;
    COT<IO> *ot;
    bn_t q;
    OLE<IO> *ole;
    g1_t g1;
    g2_t g2;
    bn_t inverse_helper;
    block seed;
    bn_t *GR;
    ThreadPool *pool;
    int modulus;
    int kappa;
    int secpara;
    int party;
    int threads;

    OVUF(int party, IO *io, COT<IO> *ot, OLE<IO> *ole, bn_t q2, int modulus, int kappa, int secpara, g1_t g11, g2_t g22,
          ThreadPool *pool) : io(io), ot(ot), ole(ole),
                              party(party) {
        this->secpara = secpara;
        this->modulus = modulus;
        this->kappa = kappa;
        this->party = party;
        bn_new(q);
        bn_copy(this->q, q2);
        g1_new(g1);
        g1_copy(this->g1, g11);
        g2_new(g2);
        g2_copy(this->g2, g22);
        bn_new(inverse_helper);
        bn_set_dig(inverse_helper, 2);
        bn_sub(inverse_helper, q, inverse_helper);//set inverse_helper = q-2
        bn_copy(this->inverse_helper, inverse_helper);
        this->pool = pool;
        this->threads = pool->size();
        PRG prg;
        prg.random_block(&seed, 1);
        prg.reseed(&seed);//reset the PRG with another seed
        GR = new bn_t[kappa + 2 * secpara];
        for (int i = 0; i < kappa + 2 * secpara; ++i) {
            bn_new(GR[i]);
        }
        expand(GR, kappa + 2 * secpara, seed, q);//expand GR to required length by seed
    }

    ~OVUF() {
        for (int i = 0; i < kappa + 2 * secpara; ++i) {
            bn_free(GR[i]);
        }
        delete[] GR;
        bn_free(inverse_helper);
        g2_free(g2);
        g1_free(g1);
        bn_free(q);
    }

    //BN_new all memory before calling this function!
    //input (Bob) : elements; input (Alice) : sk
    void compute_batch(g1_t *output, bn_t *input, const g2_t pk, const int length) {
	    bn_t *in1 = new bn_t[2 * length];
        bn_t *out1 = new bn_t[2 * length * (kappa + modulus) + 4 * length * secpara];
        multi_task_rand_in(in1, 2 * length); //init in1[] and set random value
        ole->multi_task_initbn(out1, 2 * length * (kappa + modulus) + 4 * length * secpara);//init out1[]

        bn_t *in2 = in1 + length;
        bn_t *out2 = out1 +  length * (kappa + modulus) + 2 * length * secpara;
	    if (party == ALICE) {
            ole->multi_task_bn_copy(in2, input, length); //set in2[] = sk (Alice)
        } else {
            ole->multi_task_bn_copy(in1, input, length); //set in1[] = elements (Bob)
        }
        bool *bits = new bool[length * (kappa + modulus) + 2 * secpara * length];//encode n bn value 2*n*kappa+2s

        ole->cot_equal_list(out1, in1, length, GR, bits);     //compute non-leakage ole for in1[] = random (Alice) and in1[] = elements (Bob). The out need to be recovered later
        ole->cot_unequal_list(out2, in2, length, GR, bits);   //compute non-leakage ole for in2[] = sk (Alice) and in2[] = random (Bob). The out need to be recovered later

        //BOB receive seed from ALICE and expand to needed length
        if (party == BOB) {
            io->recv_block(&seed, 1);
            expand(GR, kappa + 2 * secpara, seed, q);
        } else {
            io->send_block(&seed, 1);
        }

        //recover secret share from out[]
        bn_t *recover1 = new bn_t[length * 2];
        ole->multi_task_initbn(recover1, length * 2);
        bn_t *recover2 = recover1 + length;
        ole->multi_task_recover1(recover1, out1, GR, length);//recover Alice ti BOb ri such that ti+ri=ai*yi
        ole->multi_task_recover2(recover2, out2, GR, length);//recover Alice oi BOb pi such that oi+pi=bi*sk

        //send d and e, compute c, compute output
	    bn_t *msg = new bn_t[2 * length];
        g1_t *tmp2 = new g1_t[2 * length];
        ole->multi_task_initbn(msg, 2 * length);
        ole->multi_task_initg1(tmp2, 2 * length);
        bn_t *tmp = msg + length;
        g1_t *h = tmp2 + length;
        gt_t e2;
        gt_new(e2);
        pc_map(e2, g1, g2); //pairing operation

        if (party == ALICE) {
            //checkpoint 1
            uint8_t * dig = new uint8_t[Hash::DIGEST_SIZE];
            g2_t checkpoint; g2_new(checkpoint);
            g2_mul_gen(checkpoint, recover2[0]);
            g2_norm(checkpoint, checkpoint);
            Hashg2(dig, checkpoint);
            io->send_data(dig, Hash::DIGEST_SIZE); //compute H(g^z) and send it to Bob

            //recover v
            multi_task_ALICE_compute1(msg, recover1, recover2, in1, in2, length); //compute step (3) of OVUF protocol
            send_bn_list(io, msg, length); // send it (m) to client
            io->flush();
            recv_bn_list(io, tmp, length); //receive (u) from client
            multi_task_ALICE_compute2(h, tmp, msg, in1, length); //compute step (4) of OVUF protocol
            send_g1_list(io, h, length); // send h to client
            io->flush();
        } else {
            //checkpoint 1
            uint8_t * dig = new uint8_t[Hash::DIGEST_SIZE];
            io->recv_data(dig, Hash::DIGEST_SIZE); // receive H(g^z)
            uint8_t * check = new uint8_t[Hash::DIGEST_SIZE];
            g2_t checkpoint, tmpcheck;
            g2_new(checkpoint); g2_new(tmpcheck); 
            g2_mul_gen(checkpoint, recover2[0]);
            g2_norm(checkpoint, checkpoint);
            g2_mul(tmpcheck, pk, in2[0]);
            g2_norm(tmpcheck, tmpcheck);
            g2_sub(checkpoint, tmpcheck, checkpoint);
            g2_norm(checkpoint, checkpoint);
            Hashg2(check, checkpoint); //compute H(pk^\zeta/g^o)
            for(int i = 0; i < Hash::DIGEST_SIZE; i++){
                if(check[i] != dig[i]){
                    cout << "Fail at checkpoint 1" << endl;
                    break;
                }   
            }

            gt_t e1;
            gt_new(e1);
            g2_t b;
            g2_new(b);
            g1_t h1;
            g1_new(h1);
            multi_task_BOB_compute1(msg, recover1, recover2, in1, in2, length); //compute (u)
            recv_bn_list(io, tmp, length); //receive (m) from server
            send_bn_list(io, msg, length); //send (u) to server
            multi_task_BOB_compute2(msg, tmp, length); // recover v=m+u
            recv_g1_list(io, tmp2, length); //receive h from server
            multi_task_BOB_compute3(output, msg, tmp2, in1, in2, pk, e2, b, e1, h1, length); //compute the final result and check: step (4) and (5)
	        g1_free(h1);
            g2_free(b);
            gt_free(e1);
        }
        gt_free(e2);
	    delete[] tmp2;
        delete[] msg;
        delete[] recover1;
        delete[] bits;
        delete[] out1;
        delete[] in1;

    }


    void task_ALCIE_compute1(bn_t *msg, bn_t *recover1, bn_t *recover2, bn_t *in1, bn_t *in2, int start, int end) {
        //compute step (4) of OVUF protocol
        int i = start;
        for (; i < end; ++i) {
            bn_mul(msg[i], in1[i], in2[i]);
            bn_mod(msg[i], msg[i], q);
            bn_add(msg[i], msg[i], recover1[i]);
            bn_add(msg[i], msg[i], recover2[i]);
            bn_mod(msg[i], msg[i], q);
        }
    }

    void task_ALCIE_compute2(g1_t *h, bn_t *tmp, bn_t *msg, bn_t *in1, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            bn_add(msg[i], msg[i], tmp[i]);
            bn_mod(msg[i], msg[i], q);//compute c
            if (bn_is_zero(msg[i]))
                std::cout << "wrong!c=0\n";
            //compute 1/c
            bn_mxp(msg[i], msg[i], inverse_helper, q);
            //compute a/c
            bn_mul(msg[i], msg[i], in1[i]);
            bn_mod(msg[i], msg[i], q);
            //compute h={a/c}g1
            g1_mul_gen(h[i], msg[i]);
        }
    }

    void multi_task_ALICE_compute1(bn_t *msg, bn_t *recover1, bn_t *recover2, bn_t *in1, bn_t *in2, const int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, msg, recover1, recover2, in1, in2, start, end]() {
                task_ALCIE_compute1(msg, recover1, recover2, in1, in2, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_ALCIE_compute1(msg, recover1, recover2, in1, in2, start, end);
        for (auto &f: fut) f.get();
    }

    void multi_task_ALICE_compute2(g1_t *h, bn_t *tmp, bn_t *msg, bn_t *in1, const int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, h, tmp, msg, in1, start, end]() {
                task_ALCIE_compute2(h, tmp, msg, in1, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_ALCIE_compute2(h, tmp, msg, in1, start, end);
        for (auto &f: fut) f.get();
    }

    void task_BOB_compute1(bn_t *msg, bn_t *recover1, bn_t *recover2, bn_t *in1, bn_t *in2, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            bn_mul(msg[i], in1[i], in2[i]);
            bn_mod(msg[i], msg[i], q);
            bn_add(msg[i], msg[i], recover1[i]);
            bn_add(msg[i], msg[i], recover2[i]);
            bn_mod(msg[i], msg[i], q);
        }
    }

    void multi_task_BOB_compute1(bn_t *msg, bn_t *recover1, bn_t *recover2, bn_t *in1, bn_t *in2, const int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, msg, recover1, recover2, in1, in2, start, end]() {
                task_BOB_compute1(msg, recover1, recover2, in1, in2, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_BOB_compute1(msg, recover1, recover2, in1, in2, start, end);
        for (auto &f: fut) f.get();
    }

    void task_BOB_compute2(bn_t *msg, bn_t *tmp, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            bn_add(msg[i], msg[i], tmp[i]);
            bn_mod(msg[i], msg[i], q);//compute c
            if (bn_is_zero(msg[i]))
                std::cout << "wrong!c=0\n";
        }
    }

    void multi_task_BOB_compute2(bn_t *msg, bn_t *tmp, const int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, msg, tmp, start, end]() {
                task_BOB_compute2(msg, tmp, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_BOB_compute2(msg, tmp, start, end);
        for (auto &f: fut) f.get();
    }

    void task_BOB_compute3(g1_t *output, bn_t *msg, g1_t *tmp2, bn_t *in1, bn_t *in2, const g2_t pk, gt_t e2, g2_t xb,
                           gt_t xe1, g1_t xh1, int start, int end) {
        int i = start;
        g2_t b;
        g2_new(b);
        gt_t e1;
        gt_new(e1);
        g1_t h1;
        g1_new(h1);
        for (; i < end; ++i) {
            bn_mxp(msg[i], msg[i], inverse_helper, q);
            //compute b/c
            bn_mul(msg[i], in2[i], msg[i]);
            bn_mod(msg[i], msg[i], q);
            // compute {b/c}g1
            g1_mul_gen(h1, msg[i]);
            // compute Fsk(y)
            g1_add(output[i], h1, tmp2[i]);
            g1_norm(output[i], output[i]);
            g2_mul_gen(b, in1[i]);
            g2_add(b, b, pk);
            g2_norm(b, b);pc_map(e1, output[i], b);
            if (gt_cmp(e1, e2) != RLC_EQ)
                std::cout << "wrong!ReceiverCheck\n";
        }
        g1_free(h1);
        gt_free(e1);
        g2_free(b);
    }

    void
    multi_task_BOB_compute3(g1_t *output, bn_t *msg, g1_t *tmp2, bn_t *in1, bn_t *in2, const g2_t pk, gt_t e2, g2_t b,
                            gt_t e1, g1_t h1, const int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, output, msg, tmp2, in1, in2, pk, e2, b, e1, h1, start, end]() {
                task_BOB_compute3(output, msg, tmp2, in1, in2, pk, e2, b, e1, h1, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_BOB_compute3(output, msg, tmp2, in1, in2, pk, e2, b, e1, h1, start, end);
        for (auto &f: fut) f.get();
    }

    void task_rand_in(bn_t *in1, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            bn_new(in1[i]);
            bn_rand_mod(in1[i], q);
        }
    }

    void multi_task_rand_in(bn_t *in1, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, in1, start, end]() {
                task_rand_in(in1, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_rand_in(in1, start, end);
        for (auto &f: fut) f.get();
    }

};

#endif //__OVUF_H
