#ifndef __OLE_H
#define __OLE_H
#include "emp-ot/emp-ot.h"
#include "backend/bn_utils.h"

extern "C" {
#include <relic/relic.h>
}

#define OT_TYPE_IKNP 0
#define OT_TYPE_FERRET 1
#define OT_TYPE OT_TYPE_IKNP

#include <iostream>
#include <time.h>
using namespace std;
template<typename IO>
class OLE {
public:
    IO *io;
    COT<IO> *ot;
    bn_t *exp;
    CCRH ccrh;
    int modulus;
    int kappa;
    bn_t q;
    int sec_para;
    int party;
    ThreadPool *pool;
    int threads;

    OLE(int party, IO *io, COT<IO> *ot, bn_t q2, int modulus, int kappa, int sec_para, ThreadPool *pool) : io(io),
                                                                                                            ot(ot),
                                                                                                            party(party) {
        bn_new(q);
        bn_copy(this->q, q2);
        this->pool = pool;
        this->threads = pool->size();
        this->modulus = modulus;
        this->kappa = kappa;
        this->sec_para = sec_para;
        exp = new bn_t[modulus];
        for (int i = 0; i < modulus; ++i) {
            bn_new(exp[i]);
            bn_zero(exp[i]);
            bn_set_bit(exp[i], i, 1);
            bn_mod(exp[i], exp[i], q);
        }
    }

    ~OLE() {
        for (int i = 0; i < modulus; ++i) {
            bn_free(exp[i]);
        }
        delete[] exp;
        bn_free(q);
    }

    //BN_new all memory before calling this function!
    void cot_equal_list(bn_t *out, const bn_t *in, const int in_length, const bn_t *GR, bool *bits3) { // MtA protocol in Figure 6
        bn_t *msg;
        msg = new bn_t[2 * in_length * (modulus + kappa) + 4 * sec_para * in_length];
        multi_task_initbn(msg, 2 * in_length * (modulus + kappa) + 4 * sec_para * in_length);
        bn_t *tmp = msg + in_length * (modulus + kappa) + 2 * sec_para * in_length;
        block *raw = new block[in_length * (modulus + kappa) + 2 * in_length * sec_para];
        if (party == BOB) {
            bn_t *encode_corr;
            encode_corr = new bn_t[in_length * (modulus + kappa) + 2 * sec_para * in_length]; //encode n bn value 2*n*kappa Zq and 2*s Fq^n
            multi_task_initbn(encode_corr, in_length * (modulus + kappa) + 2 * sec_para * in_length);
            multi_task_encode_corr_batch(encode_corr, in, in_length); // generate a1...a1...an...an||a1...an...a1...an length: 2*n*kappa+2*n*s

	        ot->send_cot(raw, in_length * (modulus + kappa) + 2 * sec_para * in_length);
            io->flush();
            multi_task_computeB(msg, out, raw, encode_corr, in_length * (modulus + kappa) + 2 * sec_para * in_length);
	        send_bn_list(io, msg, in_length * (modulus + kappa) + 2 * in_length * sec_para);
            io->flush();
            delete[] encode_corr;
        } else {
            bool *bits = new bool[(modulus + kappa) * in_length + 2 * sec_para];
            encode_batch(bits, in, in_length, GR); // use GR to encode in[]
            bool *bits2 = new bool[2 * sec_para * in_length];
            for (int i = 0; i < 2 * sec_para; ++i) {
                for (int j = 0; j < in_length; ++j) {
                    bits2[i * in_length + j] = bits[i + in_length * (modulus + kappa)];
                }
            }
            for (int i = 0; i < in_length * (modulus + kappa); ++i) {
                bits3[i] = bits[i];
            }
            for (int i = in_length * (modulus + kappa);
                 i < in_length * (modulus + kappa) + 2 * sec_para * in_length; ++i) {
                bits3[i] = bits2[i - in_length * (modulus + kappa)];
            }
            ot->recv_cot(raw, bits3, in_length * (modulus + kappa) + 2 * sec_para * in_length);
            recv_bn_list(io, tmp, in_length * (modulus + kappa) + 2 * in_length * sec_para);
	        multi_task_computeA(out, raw, tmp, bits3, in_length * (modulus + kappa) + 2 * sec_para * in_length);
            delete[] bits2;
            delete[] bits;
        }
        delete[] raw;
        delete[] msg;
    }

    void cot_unequal_list(bn_t *out, const bn_t *in, const int in_length, const bn_t *GR, bool *bits3) { // U-MtA protocol in Figure 10
        bn_t *msg;
        msg = new bn_t[2 * in_length * (modulus + kappa) + 4 * sec_para * in_length];
        multi_task_initbn(msg, 2 * in_length * (modulus + kappa) + 4 * sec_para * in_length);
        bn_t *tmp = msg + in_length * (modulus + kappa) + 2 * sec_para * in_length;
        block *raw = new block[ in_length * (modulus + kappa) + 2 * in_length * sec_para];
        if (party == BOB) {
            bn_t *encode_corr;
            encode_corr = new bn_t[in_length * (modulus + kappa) +
                                   2 * sec_para * in_length]; //encode n bn value 2*n*kappa Zq and 2*s Fq^n
            multi_task_initbn(encode_corr, in_length * (modulus + kappa) + 2 * sec_para * in_length);
            multi_task_encode_corr_one(encode_corr, in, in_length);
            // generate a1...a1...an...an||a1...an...a1...an length: 2*n*kappa+2*n*s
	        ot->send_cot(raw, in_length * (modulus + kappa) + 2 * sec_para * in_length);
            io->flush();
            multi_task_computeB(msg, out, raw, encode_corr, in_length * (modulus + kappa) + 2 * sec_para * in_length);
	        send_bn_list(io, msg, in_length * (modulus + kappa) + 2 * in_length * sec_para);
            io->flush();
            delete[] encode_corr;
        } else {
            bool *bits = new bool[modulus + kappa + 2 * sec_para];
            encode_one(bits, in[0], GR);
            for (int i = 0; i < modulus + kappa  + 2 * sec_para; ++i) {
                for (int j = 0; j < in_length; ++j) {
                    bits3[i * in_length + j] = bits[i];
                }
            }
            ot->recv_cot(raw, bits3, in_length * (modulus + kappa) + 2 * sec_para * in_length);
            recv_bn_list(io, tmp, in_length * (modulus + kappa) + 2 * in_length * sec_para);
	        multi_task_computeA(out, raw, tmp, bits3, in_length * (modulus + kappa) + 2 * sec_para * in_length);
            delete[] bits;
        }
        delete[] raw;
        delete[] msg;
    }

    void recover1(bn_t *recover, const bn_t *in, const bn_t *GR, const int in_length, int start, int end) {
        bn_t tmp2;
        bn_new(tmp2);
        bn_t tmp1;
        bn_new(tmp1);
        int i = start;
        for (; i < end; ++i) {
            bn_zero(tmp2);
            for (int j = i * (kappa + modulus); j < i  * (kappa + modulus) + modulus; ++j) {
                bn_mul(tmp1, exp[j - i * (kappa + modulus)], in[j]);
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
            }
            for (int j = i  * (kappa + modulus) + modulus; j <( i +1)  * (kappa + modulus); ++j) {
                bn_mul(tmp1, GR[j - (i  * (kappa + modulus) + modulus)], in[j]);
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
            }
            int index = 0;
            for (int j = in_length * (kappa + modulus)  + i;
                 j < in_length * (kappa + modulus)  + 2 * sec_para * in_length; j = j + in_length) {
                bn_mul(tmp1, GR[index + kappa], in[j]);
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
                index++;
            }
            bn_copy(recover[i], tmp2);
        }
        bn_free(tmp1);
        bn_free(tmp2);
    }

    void multi_task_recover1(bn_t *recover, const bn_t *in, const bn_t *GR, const int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, recover, in, GR, size, start, end]() {
                recover1(recover, in, GR, size, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        recover1(recover, in, GR, size, start, end);
        for (auto &f: fut) f.get();
    }

    void recover2(bn_t *recover, const bn_t *in, const bn_t *GR, const int in_length, int start, int end) {
        bn_t tmp2;
        bn_new(tmp2);
        bn_t tmp1;
        bn_new(tmp1);
        int i = start;
        for (; i < end; ++i) {
            bn_zero(tmp2);
            int index = 0;
            for (int j = i; j < in_length * modulus; j = j + in_length) {
                bn_mul(tmp1, exp[index], in[j]);
                bn_mod(tmp1, tmp1, q);
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
                index++;
            }
            index = 0;
            for (int j = in_length * modulus + i; j < in_length * (modulus + kappa); j = j + in_length) {
                bn_mul(tmp1, GR[index], in[j]);
                bn_mod(tmp1, tmp1, q);
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
                index++;
            }
            index = kappa;
            for (int j = in_length * (modulus + kappa) + i;
                 j < in_length * (modulus + kappa) + 2 * sec_para * in_length; j = j + in_length) {
                bn_mul(tmp1, GR[index], in[j]);
                bn_mod(tmp1, tmp1, q);
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
                index++;
            }
            bn_copy(recover[i], tmp2);
        }
        bn_free(tmp1);
        bn_free(tmp2);
    }

    void multi_task_recover2(bn_t *recover, const bn_t *in, const bn_t *GR, const int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, recover, in, GR, size, start, end]() {
                recover2(recover, in, GR, size, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        recover2(recover, in, GR, size, start, end);
        for (auto &f: fut) f.get();
    }

    void encode_one(bool *bits, const bn_t in, const bn_t *GR) {//bits 长度 2kappa+2s
        bn_t tmp1;
        bn_new(tmp1);
        bn_t tmp2;
        bn_new(tmp2);
        bn_zero(tmp2);
        bool *r = new bool[kappa + 2 * sec_para];
        PRG prg;
        prg.random_bool(r, kappa + 2 * sec_para);

        for (int i = 0; i < kappa + 2 * sec_para; ++i) {
            bn_mul_dig(tmp1, GR[i], r[i]);
            bn_add(tmp2, tmp2, tmp1);
            bn_mod(tmp2, tmp2, q);
        }
        bn_sub(tmp2, in, tmp2);
        bn_mod(tmp2, tmp2, q);
        for (int j = 0; j < modulus; ++j) {
            bits[j] = bn_get_bit(tmp2, j);
        }
        for (int j = modulus; j < modulus + kappa + 2 * sec_para; ++j) {
            bits[j] = r[j - modulus];
        }
        delete[] r;
        bn_free(tmp1);
        bn_free(tmp2);
    }

    void encode_batch(bool *bits, const bn_t *in, const int in_length, const bn_t *GR) {//bits length 2*n*kappa+2s
        bn_t tmp1;
        bn_new(tmp1);
        bn_t tmp3;
        bn_new(tmp3);
        bn_zero(tmp3);
        bool *r = new bool[in_length * kappa + 2 * sec_para]; //r length n*kappa+2s
        PRG prg;
        prg.random_bool(r, in_length * kappa + 2 * sec_para);
        for (int j = in_length * kappa; j < in_length * kappa + 2 * sec_para; ++j) {
            bits[in_length * modulus + j] = r[j];
            bn_mul_dig(tmp1, GR[j - in_length * kappa + kappa], r[j]);//compute <g^R, r0>
            bn_add(tmp3, tmp3, tmp1);
            bn_mod(tmp3, tmp3, q);
        }
        multi_encode_batch_task(bits, r, in, tmp3, in_length, GR);
        delete[] r;
        bn_free(tmp1);
        bn_free(tmp3);
    }

    void multi_encode_batch_task(bool *bits, bool *r, const bn_t *in, bn_t tmp3, const int size,
                                 const bn_t *GR) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, bits, r, in, tmp3, GR, start, end]() {
                encode_batch_task(bits, r, in, tmp3, GR, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        encode_batch_task(bits, r, in, tmp3, GR, start, end);
        for (auto &f: fut) f.get();
    }

    void
    encode_batch_task(bool *bits, bool *r, const bn_t *in, bn_t tmp3, const bn_t *GR, int start,
                      int end) {
        int i = start;
        bn_t tmp2;
        bn_new(tmp2);
        bn_t tmp1;
        bn_new(tmp1);
        for (; i < end; ++i) {
            bn_zero(tmp2);
            for (int j = i * kappa; j < (i + 1) * kappa; ++j) {
                bn_mul_dig(tmp1, GR[j - (i * kappa)], r[j]); //<GR, r1/rn>
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
            }
            bn_add(tmp2, tmp3, tmp2);
            bn_mod(tmp2, tmp2, q);
            bn_sub(tmp2, in[i], tmp2);
            bn_mod(tmp2, tmp2, q);
            for (int j = i * (modulus + kappa); j < i * (modulus + kappa) + modulus; ++j) {
                bits[j] = bn_get_bit(tmp2, j - i * (modulus + kappa));
            }
            for (int j = i * (modulus + kappa) + modulus; j < (i +1)* (modulus + kappa); ++j) {
                bits[j] = r[j - (i+1) * modulus];
            }
        }
        bn_free(tmp1);
        bn_free(tmp2);
    }

    void task_initbn(bn_t *bn1, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            bn_new(bn1[i]);
        }
    }

    void multi_task_initbn(bn_t *bn1, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, bn1, start, end]() {
                task_initbn(bn1, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_initbn(bn1, start, end);
        for (auto &f: fut) f.get();
    }

    void task_initg1(g1_t *g1, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            g1_new(g1[i]);
        }
    }

    void multi_task_initg1(g1_t *g1, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, g1, start, end]() {
                task_initg1(g1, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_initg1(g1, start, end);
        for (auto &f: fut) f.get();
    }

    void task_encode_corr_one(bn_t *out, const bn_t *in, int in_length, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            for (int j = 0; j < in_length * (modulus + kappa) + 2 * sec_para * in_length; j = j + in_length) {
                bn_copy(out[j + i], in[i]);
            }
        }
    }

    void multi_task_encode_corr_one(bn_t *out, const bn_t *in, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, out, in, size, start, end]() {
                task_encode_corr_one(out, in, size, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_encode_corr_one(out, in, size, start, end);
        for (auto &f: fut) f.get();
    }//out length 2*in_length*bit_length+2*sec_para*in_length

    void task_encode_corr_batch(bn_t *out, const bn_t *in, int in_length, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            for (int j = i * (modulus + kappa); j < (i + 1) * (modulus + kappa); ++j) {
                bn_copy(out[j], in[i]);
            }
            for (int j = in_length * (modulus + kappa);
                 j < in_length * (modulus + kappa) + 2 * sec_para * in_length; j = j + in_length) {
                bn_copy(out[j + i], in[i]);
            }
        }
    }

    void multi_task_encode_corr_batch(bn_t *out, const bn_t *in,
                                      int size) { //out length 2*in_length*bit_length+2*sec_para*in_length
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, out, in, size, start, end]() {
                task_encode_corr_batch(out, in, size, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_encode_corr_batch(out, in, size, start, end);
        for (auto &f: fut) f.get();
    }

    void task_computeB(bn_t *msg, bn_t *out, block *raw, const bn_t *encode_corr, int start, int end) {
        bn_t pad1;
        bn_new(pad1);
        bn_t pad2;
        bn_new(pad2);
        bn_t tmp1;
        bn_new(tmp1);
        int i = start;
        for (; i < end; ++i) {
            H(pad1, raw[i], q, ccrh); // output a big number in Zq
            H(pad2, raw[i] ^ ot->Delta, q, ccrh);
            bn_add(msg[i], pad1, pad2);
            bn_add(msg[i], msg[i], encode_corr[i]);
            bn_mod(msg[i], msg[i], q);
            bn_sub(tmp1, q, pad1);
            bn_mod(tmp1, tmp1, q);
            bn_copy(out[i], tmp1); //out[i]=q-pad1
        }
        bn_free(tmp1);
        bn_free(pad2);
        bn_free(pad1);
    }

    void multi_task_computeB(bn_t *msg, bn_t *out, block *raw, const bn_t *encode_corr, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, msg, out, raw, encode_corr, start, end]() {
                task_computeB(msg, out, raw, encode_corr, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_computeB(msg, out, raw, encode_corr, start, end);
        for (auto &f: fut) f.get();
    }

    void task_computeA(bn_t *out, block *raw, bn_t *tmp, bool *bits, int start, int end) {
        bn_t msg;
        bn_new(msg);
        int i = start;
        for (; i < end; ++i) {
            H(msg, raw[i], q, ccrh);//msg=pad1/pad2
            if (bits[i]) {
                bn_sub(msg, tmp[i], msg);//if msg = pad2, msg = pad1+A.in
            }
            bn_mod(msg, msg, q);
            bn_copy(out[i], msg);//out[i]=pad1/(pad2+A.in)
        }
        bn_free(msg);
    }

    void multi_task_computeA(bn_t *out, block *raw, bn_t *tmp, bool *bits, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, out, raw, tmp, bits, start, end]() {
                task_computeA(out, raw, tmp, bits, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_computeA(out, raw, tmp, bits, start, end);
        for (auto &f: fut) f.get();
    }

    void task_bn_copy(bn_t *out, bn_t *in, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            bn_copy(out[i], in[i]);
        }
    }

    void multi_task_bn_copy(bn_t *out, bn_t *in, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, out, in, start, end]() {
                task_bn_copy(out, in, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_bn_copy(out, in, start, end);
        for (auto &f: fut) f.get();
    }

    void test_cot_equal(bn_t *out, const bn_t *in, const int in_length, const bool *bits) {
        bn_t *encode_corr;
        encode_corr = new bn_t[in_length * (modulus + kappa) + 2 * sec_para * in_length]; //encode n bn value 2*n*kappa Zq and 2*s Fq^n
        for (int i = 0; i < in_length * (modulus + kappa) + 2 * sec_para * in_length; ++i) {
            bn_new(encode_corr[i]);
        }
        bn_t tmp;
        bn_new(tmp);
        bn_t tmp2;
        bn_new(tmp2);
        if (party == BOB) {
            multi_task_encode_corr_batch(encode_corr, in, in_length);
            for (int i = 0; i < in_length * (modulus + kappa) + 2 * in_length * sec_para; ++i) {
                send_bn(io, encode_corr[i]);
                send_bn(io, out[i]);
            }
        } else {
            for (int i = 0; i < in_length * (modulus + kappa) + 2 * in_length * sec_para; ++i) {
                recv_bn(io, tmp);
                recv_bn(io, tmp2);
//                bn_mod(out[i], out[i], q);
                bn_mul_dig(tmp, tmp, bits[i]);
                bn_sub(tmp, tmp, out[i]);
//                bn_mod(tmp, tmp, q);
                bn_sub(tmp, tmp, tmp2);
                bn_mod(tmp, tmp, q);
                if (!bn_is_zero(tmp)) {
                    std::cout << "wrong!";
                    std::cout << i << "th bits:" << bits[i];
                }
            }
        }
        //free
        bn_free(tmp);
        bn_free(tmp2);
        delete[] encode_corr;
    }

    void test_cot_unequal(bn_t *out, const bn_t *in, const int in_length, const bool *bits) {
        bn_t *encode_corr;
        encode_corr = new bn_t[in_length * (modulus + kappa) + 2 * sec_para * in_length]; //encode n bn value 2*n*kappa Zq and 2*s Fq^n
        for (int i = 0; i < in_length * (modulus + kappa) + 2 * sec_para * in_length; ++i) {
            bn_new(encode_corr[i]);
        }
        bn_t tmp;
        bn_new(tmp);
        bn_t tmp2;
        bn_new(tmp2);
        if (party == BOB) {
            multi_task_encode_corr_one(encode_corr, in, in_length);
            for (int i = 0; i < in_length * (modulus + kappa) + 2 * sec_para * in_length; ++i) {
                send_bn(io, encode_corr[i]);
                send_bn(io, out[i]);
            }
        } else {
            for (int i = 0; i < in_length * (modulus + kappa) + 2 * sec_para * in_length; ++i) {
                recv_bn(io, tmp);
                recv_bn(io, tmp2);
                bn_mul_dig(tmp, tmp, bits[i]);
                bn_sub(tmp, tmp, out[i]);
                bn_sub(tmp, tmp, tmp2);
                bn_mod(tmp, tmp, q);
                if (!bn_is_zero(tmp)) {
                    std::cout << "wrong!2";
                    std::cout << "bits:" << i << ":" << bits[i];
                }
            }
        }
        //free
        bn_free(tmp2);
        bn_free(tmp);
        delete[] encode_corr;
    }

    void test_encode_one(const bool *bits, const bn_t in, const bn_t *GR) {
        bn_t tmp1;
        bn_new(tmp1);
        bn_t tmp2;
        bn_new(tmp2);
        bn_zero(tmp2);
        for (int i = 0; i < modulus; ++i) {
            bn_mul_dig(tmp1, exp[i], bits[i]);
            bn_add(tmp2, tmp2, tmp1);
            bn_mod(tmp2, tmp2, q);
        }
        for (int i = modulus; i < (modulus + kappa) + 2 * sec_para; ++i) {
            bn_mul_dig(tmp1, GR[i - modulus], bits[i]);
            bn_add(tmp2, tmp2, tmp1);
            bn_mod(tmp2, tmp2, q);
        }
        if (bn_cmp(tmp2, in) != RLC_EQ) {
            std::cout << "recover failed" << endl;
        }
        bn_free(tmp2);
        bn_free(tmp1);
    }

    void test_encode_batch(const bool *bits, const bn_t *in, int in_length, const bn_t *GR) {
        bn_t tmp1;
        bn_new(tmp1);
        bn_t tmp2;
        bn_new(tmp2);
        bn_t tmp3;
        bn_new(tmp3);
        bn_zero(tmp3);
        for (int j = in_length * (modulus + kappa); j < in_length * (modulus + kappa) + 2 * sec_para; ++j) {
            bn_mul_dig(tmp1, GR[j % (in_length * (modulus + kappa)) + kappa], bits[j]);
            bn_add(tmp3, tmp3, tmp1);
            bn_mod(tmp3, tmp3, q);
        }
        for (int i = 0; i < in_length; ++i) {
            bn_zero(tmp2);
            for (int j = i * (modulus + kappa); j < i * (modulus + kappa) + modulus; ++j) {
                bn_mul_dig(tmp1, exp[j - i * (modulus + kappa)], bits[j]);
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
            }
            for (int j = i * (modulus + kappa) + modulus; j < (i + 1) * (modulus + kappa); ++j) {
                bn_mul_dig(tmp1, GR[j - (i * (modulus + kappa) + modulus)], bits[j]);
                bn_add(tmp2, tmp2, tmp1);
                bn_mod(tmp2, tmp2, q);
            }
            bn_add(tmp2, tmp2, tmp3);
            bn_mod(tmp2, tmp2, q);
            if (bn_cmp(tmp2, in[i]) != RLC_EQ) {
                std::cout << "recover failed" << endl;
            }
        }
        bn_free(tmp1);
        bn_free(tmp2);
        bn_free(tmp3);
    }

    void test_recover1(const bn_t *recover, const bn_t *in, const int in_length) {
        //test compute
        bn_t tmp;
        bn_new(tmp);
        bn_t tmp2;
        bn_new(tmp2);
        if (party == BOB) {
            for (int i = 0; i < in_length; ++i) {
                send_bn(io, in[i]);
                send_bn(io, recover[i]);
            }
        } else {
            for (int i = 0; i < in_length; ++i) {
                recv_bn(io, tmp);
                recv_bn(io, tmp2);
                bn_mul(tmp, tmp, in[i]);
                bn_mod(tmp, tmp, q);
                bn_sub(tmp, tmp, recover[i]);
                bn_mod(tmp, tmp, q);
                bn_sub(tmp, tmp, tmp2);
                bn_mod(tmp, tmp, q);
                if (!bn_is_zero(tmp))
                    std::cout << "recover test wrong!1\n";
            }
        }
        bn_free(tmp);
        bn_free(tmp2);
    }

    void test_recover2(const bn_t *recover, const bn_t *in, const int in_length) {
        //test compute
        bn_t tmp;
        bn_new(tmp);
        bn_t tmp2;
        bn_new(tmp2);
        if (party == BOB) {
            for (int i = 0; i < in_length; ++i) {
                send_bn(io, in[i]);
                send_bn(io, recover[i]);
            }
        } else {
            for (int i = 0; i < in_length; ++i) {
                recv_bn(io, tmp);
                recv_bn(io, tmp2);
                bn_mul(tmp, tmp, in[0]);
                bn_mod(tmp, tmp, q);
                bn_sub(tmp, tmp, recover[i]);
                bn_mod(tmp, tmp, q);
                bn_sub(tmp, tmp, tmp2);
                bn_mod(tmp, tmp, q);
                if (!bn_is_zero(tmp))
                    std::cout << "recover test wrong!2\n";
            }
        }
        bn_free(tmp);
        bn_free(tmp2);
    }
};

#endif //

