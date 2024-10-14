#ifndef __BN_UTILS_H__
#define __BN_UTILS_H__

#include <openssl/bn.h>
#include "emp-tool/emp-tool.h"

extern "C" {
#include <relic/relic.h>
}
using namespace emp;

inline void expand(bn_t *out, int length, block seed, bn_t q) {
	PRG prg(&seed);
	block * tmp = new block[2*length];
	prg.random_block(tmp, 2*length);
	for(int i = 0; i < length; ++i) {
		bn_read_bin(out[i], (uint8_t *) (tmp+2*i), 32);
		bn_mod(out[i], out[i], q);
	}
	delete[] tmp;
}

inline void H(bn_t out, const block b, const bn_t q, CCRH &ccrh) {
	block arr[2];
	arr[0] = b ^ makeBlock(0, 1);
	arr[1] = b ^ makeBlock(0, 2);
	ccrh.H<2>(arr, arr);
	bn_read_bin(out, (uint8_t *) arr, 32);
	bn_mod(out, out, q);
}
inline void Hc2b(bn_t out, const char * s, int size, const bn_t q) {
    block arr[2];
    Hash::hash_once(arr, s, size);
    bn_read_bin(out, (uint8_t *) arr, 32);
    bn_mod(out, out, q);
}
inline void Hashg1(unsigned char *dig, g1_t in) {
	uint32_t length = g1_size_bin(in, 0);
	uint8_t arr[length];
	g1_write_bin(arr, length, in, 0);
    Hash::hash_once(dig, arr, length);
}
inline void Hashg2(unsigned char *dig, g2_t in) {
	uint32_t length = g2_size_bin(in, 0);
	uint8_t arr[length];
	g2_write_bin(arr, length, in, 0);
    Hash::hash_once(dig, arr, length);
}

inline void send_bn(NetIO *io, const bn_t bn) {
	uint8_t arr[RLC_CEIL(RLC_BN_BITS, 8)];
	uint32_t length = bn_size_bin(bn);
	bn_write_bin(arr, length, bn);
	io->send_data(&length, sizeof(uint32_t));
	io->send_data(arr, length);
}

inline void recv_bn(NetIO *io, bn_t bn) {
	uint8_t arr[RLC_CEIL(RLC_BN_BITS, 8)];
	uint32_t length = -1;
	io->recv_data(&length, sizeof(uint32_t));
	io->recv_data(arr, length);
	bn_read_bin(bn, arr, length);
}

inline void send_char_list(NetIO *io, const unsigned char *data, const int num) {
	uint32_t length = 8 * num;
	io->send_data(data, length);
}

inline void recv_char_list(NetIO *io, unsigned char *data, const int num) {
	io->recv_data(data, 8 * num);
}

inline void send_bn_list(NetIO *io, const bn_t *bn, const int num) {
	uint8_t *arr = new uint8_t[RLC_CEIL(RLC_BN_BITS, 8) * num];
	uint32_t *length = new uint32_t[num];
	uint32_t sum_length = 0;
	uint8_t *ptr = arr;
	for (int i = 0; i < num; ++i) {
		length[i] = bn_size_bin(bn[i]);
		bn_write_bin(ptr, length[i], bn[i]);
		sum_length = sum_length + length[i];
		ptr += length[i];
	}
	io->send_data(length, sizeof(uint32_t) * num);
	io->send_data(arr, sum_length);
	delete[] arr;
	delete[] length;
}

inline void recv_bn_list(NetIO *io, bn_t *bn, const int num) {
	uint8_t *arr = new uint8_t[RLC_CEIL(RLC_BN_BITS, 8) * num];
	uint32_t *length = new uint32_t[num];
	uint32_t sum_length = 0;
	uint8_t *ptr = arr;
	io->recv_data(length, sizeof(uint32_t) * num);
	for (int i = 0; i < num; ++i) {
		sum_length = sum_length + length[i];
	}
	io->recv_data(arr, sum_length);
	for (int i = 0; i < num; ++i) {
		bn_read_bin(bn[i], ptr, length[i]);
		ptr += length[i];
	}
	delete[] arr;
	delete[] length;
}

inline void send_g1(NetIO *io, g1_t g) {
	unsigned char arr[1000];
	uint32_t length = g1_size_bin(g, 0);
	g1_write_bin(arr, length, g, 0);// 0/1 indicates point compression
	io->send_data(&length, sizeof(uint32_t));
	io->send_data(arr, length);
}

inline void recv_g1(NetIO *io, g1_t g) {
	unsigned char arr[1000];
	uint32_t length = -1;
	io->recv_data(&length, sizeof(uint32_t));
	io->recv_data(arr, length);
	g1_read_bin(g, arr, length);
}

inline void send_g1_list(NetIO *io, const g1_t *g, const int num) {
	uint8_t *arr = new uint8_t[1000 * num];
	uint32_t *length = new uint32_t[num];
	uint32_t sum_length = 0;
	uint8_t *ptr = arr;
	for (int i = 0; i < num; ++i) {
		length[i] = g1_size_bin(g[i], 0);
		g1_write_bin(ptr, length[i], g[i], 0);
		sum_length = sum_length + length[i];
		ptr += length[i];
	}
	io->send_data(length, sizeof(uint32_t) * num);
	io->send_data(arr, sum_length);
	delete[] arr;
	delete[] length;
}

inline void recv_g1_list(NetIO *io, g1_t *g, const int num) {
	uint8_t *arr = new uint8_t[1000 * num];
	uint32_t *length = new uint32_t[num];
	uint32_t sum_length = 0;
	uint8_t *ptr = arr;
	io->recv_data(length, sizeof(uint32_t) * num);
	for (int i = 0; i < num; ++i) {
		//        std::cout<<"length "<< i << ":" << length[i] <<endl;
		sum_length = sum_length + length[i];
	}
	io->recv_data(arr, sum_length);
	for (int i = 0; i < num; ++i) {
		g1_read_bin(g[i], ptr, length[i]);
		ptr += length[i];
		//        std::cout<<"bn "<< i << ":";
		//        bn_print(bn[i]);
	}
	delete[] arr;
	delete[] length;
}

//TODO add a bool to indicate if compression is needed
inline void send_g2(NetIO *io, g2_t g) {
	unsigned char arr[1000];
	uint32_t length = g2_size_bin(g, 0);
	g2_write_bin(arr, length, g, 0);// 0/1 indicates point compression
	io->send_data(&length, sizeof(uint32_t));
	io->send_data(arr, length);
}

inline void recv_g2(NetIO *io, g2_t g) {
	unsigned char arr[1000];
	uint32_t length = -1;
	io->recv_data(&length, sizeof(uint32_t));
	io->recv_data(arr, length);
	g2_read_bin(g, arr, length);
}

#endif// __BN_UTILS_H__
