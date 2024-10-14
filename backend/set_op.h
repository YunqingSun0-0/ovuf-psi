#ifndef SET_OP_H
#define SET_OP_H

#include <openssl/bn.h>
#include "emp-tool/emp-tool.h"
#include "bn_utils.h"
#include <iostream>
#include <cerrno>

extern "C" {
#include <relic/relic.h>
}

class SET_OP {
public:
    ThreadPool *pool;
    int threads;
    bn_t q;
    bn_t inverse_helper;

    SET_OP(bn_t q2, ThreadPool *pool) {
        this->pool = pool;
        this->threads = pool->size();
        bn_new(q)
        bn_copy(this->q, q2);
        bn_new(inverse_helper)
        bn_set_dig(inverse_helper, 2);
        bn_sub(inverse_helper, q, inverse_helper);
    }

    ~SET_OP() {
        bn_free(inverse_helper)
        bn_free(q)
    }

    //read size in char form from the first line
    int get_set_size(const string filename) {
        FILE *fp;
        fp = fopen(filename.c_str(), "rb+");
        uint32_t length;
        uint8_t lengtharr[1024];
        fgets((char *) lengtharr, 1024, fp);
        length = atoi((char *) lengtharr);
        fclose(fp);
        return length;
    }

    int serv_encode(const bn_t sk, string filename, string filename2, bn_t q) {

        uint32_t index = read_size_from_file(filename);
        bn_t *in = new bn_t[index];
        g1_t *out = new g1_t[index];
        multi_task_init(in, out, index);//multi-thread initialize in[] out[]

        //read set elements from file
        read_bn_from_file(filename, in, q);

        //multi-thread local encoding
        uint8_t indexarr[4];
        *(uint32_t *) indexarr = index;
        uint8_t *encode = new uint8_t[8 * index];
        multi_task_local_compute(sk, encode, in, index);
        uint8_t *data = new uint8_t[8 * index + 4];
        for (int i = 0; i < 4; ++i) {
            data[i] = indexarr[i];
        }
        for (uint32_t i = 4; i < 4 + 8 * index; ++i) {
            data[i] = encode[i - 4];
        }

        //write to file
        write_to_file(data, 8 * index + 4, filename2);

        //free
        delete[] encode;
        delete[] in;
        delete[] out;
        delete[] data;

        return index;
    }


    void task_local_compute(const bn_t sk, uint8_t *encode, bn_t *in, int start, int end) {
        //Compute g^{1/(in[i]+sk)}
        bn_t tmp;
        bn_new(tmp)
        g1_t out;
        g1_new(out);
        uint8_t *dig = new uint8_t[Hash::DIGEST_SIZE];
        int i = start;
        for (; i < end; ++i) {
            bn_add(tmp, sk, in[i]);
            bn_mxp(tmp, tmp, inverse_helper, q);
            g1_mul_gen(out, tmp);
            Hashg1(dig, out);
            memcpy(encode + 8 * i, dig, 8);
        }
        g1_free(out);
        bn_free(tmp);
        delete[] dig;
    }

    void task_local_compute_set(const bn_t sk, uint8_t *encode, bn_t *in, char *set, uint32_t *indexlength, int start, int end) {
        int i;
        int arr = 0;
        for (i = 0; i < start; ++i) {
            arr = arr + indexlength[i];
        }
        char *ptr = set + arr;
        bn_t tmp;
        bn_new(tmp)
        g1_t out;
        g1_new(out);
        uint8_t *dig = new uint8_t[Hash::DIGEST_SIZE];
        i = start;
        for (; i < end; ++i) {
            Hc2b(in[i], ptr, indexlength[i] - 1, q);
            ptr = ptr + indexlength[i];
            bn_add(tmp, sk, in[i]);
            bn_mxp(tmp, tmp, inverse_helper, q);
            g1_mul_gen(out, tmp);
            Hashg1(dig, out);
            memcpy(encode + 8 * i, dig, 8);
        }
        g1_free(out);
        bn_free(tmp);
        delete[] dig;
    }

    void multi_task_local_compute(const bn_t sk, uint8_t *encode, bn_t *in, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, sk, encode, in, start, end]() {
                task_local_compute(sk, encode, in, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_local_compute(sk, encode, in, start, end);
        for (auto &f: fut) f.get();
    }

    void multi_task_local_compute_set(const bn_t sk, uint8_t *encode, bn_t *in, char *set, uint32_t *indexlength, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, sk, encode, in, set, indexlength, start, end]() {
                task_local_compute_set(sk, encode, in, set, indexlength, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_local_compute_set(sk, encode, in, set, indexlength, start, end);
        for (auto &f: fut) f.get();
    }

    uint32_t read_bn_from_file(string setname, bn_t *in, bn_t q) {
        FILE *fp;
        fp = fopen(setname.c_str(), "rb+");
        uint32_t length;
        uint8_t lengtharr[1024];
        fgets((char *) lengtharr, 1024, fp);
        int index = strlen((char *) lengtharr);
        length = atoi((char *) lengtharr);
        fseek(fp, index, 0);
        char *set = new char[1024];
        index = 0;
        uint32_t indexlength = 0;
        while (!feof(fp)) {
            fgets(set, 1024, fp);
            indexlength = strlen((char *) set);
            Hc2b(in[index], set, indexlength - 1, q);
            index++;
        }
        fclose(fp);
        return length;
    }

    void read_set_from_file(string setname, char *set, uint32_t *indexlength) {
        FILE *fp;
        fp = fopen(setname.c_str(), "rb+");
        uint8_t lengtharr[1024];
        fgets((char *) lengtharr, 1024, fp);
        int index = strlen((char *) lengtharr);
        fseek(fp, index, 0);
        char *ptr = set;
        index = 0;
        while (!feof(fp)) {
            fgets(ptr, 1024, fp);
            indexlength[index] = strlen((char *) set);
            ptr = ptr + indexlength[index];
            index++;
        }
        fclose(fp);
    }

    uint32_t read_size_from_file(string setname) {
        FILE *fp;
        fp = fopen(setname.c_str(), "rb+");
        uint32_t length;
        uint8_t lengtharr[1024];
        fgets((char *) lengtharr, 1024, fp);
        length = atoi((char *) lengtharr);
        fclose(fp);
        return length;
    }

    void read_sk_from_file(string setname, bn_t sk) {
        FILE *fp;
        fp = fopen(setname.c_str(), "rb+");
        uint32_t length;
        uint8_t lengtharr[4];
        fread((char *) lengtharr, 1, 4, fp);
        length = *(uint32_t *) lengtharr;
        fseek(fp, 4, 0);
        uint8_t *set = new uint8_t[length];
        fread((char *) set, 1, length, fp);
        bn_read_bin(sk, set, length);
//        bn_print(sk);
        delete[] set;
        fclose(fp);
    }

    void read_pk_from_file(string setname, g2_t pk) {
        FILE *fp;
        fp = fopen(setname.c_str(), "rb+");
        if (fp == NULL) {
            perror("Error:");
        }
        uint32_t length;
        uint8_t lengtharr[4];
        fread((char *) lengtharr, 1, 4, fp);
        length = *(uint32_t *) lengtharr;
        fseek(fp, 4, 0);
        uint8_t *set = new uint8_t[length];
        fread((char *) set, 1, length, fp);
        g2_read_bin(pk, set, length);
//        g2_print(pk);
        delete[] set;
        fclose(fp);
    }

    void write_to_file(uint8_t *data, int length, string filename) {
        FILE *fp;
        fp = fopen(filename.c_str(), "wb+");
        fwrite((char *) data, 1, length, fp);
        fclose(fp);
    }

    void multi_task_init(bn_t *in, g1_t *out, int size) {
        vector<std::future<void>> fut;
        int width = size / threads;
        for (int i = 0; i < threads - 1; ++i) {
            int64_t start = i * width;
            int64_t end = min((i + 1) * width, size);
            fut.push_back(pool->enqueue([this, in, out, start, end]() {
                task_init(in, out, start, end);
            }));
        }
        int64_t start = (threads - 1) * width;
        int64_t end = std::max(threads * width, size);
        task_init(in, out, start, end);
        for (auto &f: fut) f.get();
    }

    void task_init(bn_t *in, g1_t *out, int start, int end) {
        int i = start;
        for (; i < end; ++i) {
            bn_new(in[i])
            g1_new(out[i])
        }
    }
};

#endif //OVUF_SET_OP_H
