#include <openssl/rand.h>
#include <openssl/err.h>

#include <string>
#include <iostream>
#include <fstream>
#include <exception>
#include <cstring>
#include <iomanip>
#include <algorithm>

#include <sodium.h>

#include "util.h"

namespace libzerocash {

void printChar(const unsigned char c) {
    for(int j = 8; j >= 0; j--) {
        std::cout << ((c >> j) & 1);
    }
    std::cout << std::endl;
}

void printVector(const std::vector<bool>& v) {
    std::cout << v.size() << " MSB ";
    for(size_t i = 0; i < v.size(); i++) {
        std::cout << v.at(i);
    }
    std::cout << " LSB" << std::endl;
}

void printVector(const std::string str, const std::vector<bool>& v) {
    std::cout << str << " " << v.size() << " MSB ";
    for(size_t i = 0; i < v.size(); i++) {
        std::cout << v.at(i);
    }
    std::cout << " LSB" << std::endl;
}

void printVectorAsHex(const std::vector<bool>& v) {
    unsigned char bytes[int(v.size() / 8)];
    convertVectorToBytes(v, bytes);

    for(int i = 0; i < int(v.size() / 8); i++) {
        std::cout << std::setw(2) << std::setfill('0') << std::hex << (int) bytes[i];
    }
    std::cout << std::dec << std::endl;
}

void printVectorAsHex(const std::string str, const std::vector<bool>& v) {
    unsigned char bytes[int(v.size() / 8)];
    convertVectorToBytes(v, bytes);

    std::cout << str << " ";
    for(int i = 0; i < int(v.size() / 8); i++) {
        std::cout << std::setw(2) << std::setfill('0') << std::hex << (int) bytes[i];
    }
    std::cout << std::dec << std::endl;
}

void printBytesVector(const std::vector<unsigned char>& v) {
    std::vector<bool> boolVec(v.size() * 8);
    convertBytesVectorToVector(v, boolVec);
    printVector(boolVec);
}

void printBytesVector(const std::string str, const std::vector<unsigned char>& v) {
    std::vector<bool> boolVec(v.size() * 8);
    convertBytesVectorToVector(v, boolVec);
    printVector(str, boolVec);
}

void printBytesVectorAsHex(const std::vector<unsigned char>& v) {
    std::vector<bool> boolVec(v.size() * 8);
    convertBytesVectorToVector(v, boolVec);
    printVectorAsHex(boolVec);
}

void printBytesVectorAsHex(const std::string str, const std::vector<unsigned char>& v) {
    std::vector<bool> boolVec(v.size() * 8);
    convertBytesVectorToVector(v, boolVec);
    printVectorAsHex(str, boolVec);
}

void getRandBytes(unsigned char* bytes, int num) {
    randombytes_buf(bytes, num);
}

void convertBytesToVector(const unsigned char* bytes, std::vector<bool>& v) {
    int numBytes = v.size() / 8;
    unsigned char c;
    for(int i = 0; i < numBytes; i++) {
        c = bytes[i];

        for(int j = 0; j < 8; j++) {
            v.at((i*8)+j) = ((c >> (7-j)) & 1);
        }
    }
}

void convertVectorToBytes(const std::vector<bool>& v, unsigned char* bytes) {
    int numBytes = v.size() / 8;
    unsigned char c = '\0';

    for(int i = 0; i < numBytes; i++) {
        c = '\0';
        for(int j = 0; j < 8; j++) {
            if(j == 7)
                c = ((c | v.at((i*8)+j)));
            else
                c = ((c | v.at((i*8)+j)) << 1);
        }
        bytes[i] = c;
    }
}

void convertBytesToBytesVector(const unsigned char* bytes, std::vector<unsigned char>& v) {
    for(size_t i = 0; i < v.size(); i++) {
        v.at(i) = bytes[i];
    }
}

void convertBytesVectorToBytes(const std::vector<unsigned char>& v, unsigned char* bytes) {
    for(size_t i = 0; i < v.size(); i++) {
        bytes[i] = v.at(i);
    }
}

void convertBytesVectorToVector(const std::vector<unsigned char>& bytes, std::vector<bool>& v) {
	v.resize(bytes.size() * 8);
    unsigned char bytesArr[bytes.size()];
    convertBytesVectorToBytes(bytes, bytesArr);
    convertBytesToVector(bytesArr, v);
}

void convertVectorToBytesVector(const std::vector<bool>& v, std::vector<unsigned char>& bytes) {
    unsigned char bytesArr[int(ceil(v.size() / 8.))];
    convertVectorToBytes(v, bytesArr);
    convertBytesToBytesVector(bytesArr, bytes);
}

void convertIntToBytesVector(const uint64_t val_int, std::vector<unsigned char>& bytes) {
     for(size_t i = 0; i < bytes.size(); i++) {
         bytes[bytes.size()-1-i] = (val_int >> (i * 8));
    }
}

void convertIntToVector(uint64_t val, std::vector<bool>& v)
{
    v.resize(64);
    for(unsigned int i = 0; i < 64; ++i, val >>= 1) {
        v.at(63 - i) = val & 0x01;
    }
}

uint64_t convertVectorToInt(const std::vector<bool>& v) {
    if (v.size() > 64) {
        throw std::length_error ("boolean vector can't be larger than 64 bits");
    }

    uint64_t result = 0;
    for (size_t i=0; i<v.size();i++) {
        if (v.at(i)) {
            result |= (uint64_t)1 << ((v.size() - 1) - i);
        }
    }

    return result;
}

uint64_t convertBytesVectorToInt(const std::vector<unsigned char>& bytes) {
    uint64_t val_int = 0;

    for(size_t i = 0; i < bytes.size(); i++) {
        val_int = val_int + (((uint64_t)bytes[i]) << ((bytes.size()-1-i) * 8));
    }

    return val_int;
}

void concatenateVectors(const std::vector<bool>& A, const std::vector<bool>& B, std::vector<bool>& result) {
    result.reserve(A.size() + B.size());
    result.insert(result.end(), A.begin(), A.end());
    result.insert(result.end(), B.begin(), B.end());
}

void concatenateVectors(const std::vector<unsigned char>& A, const std::vector<unsigned char>& B, std::vector<unsigned char>& result) {
    result.reserve(A.size() + B.size());
    result.insert(result.end(), A.begin(), A.end());
    result.insert(result.end(), B.begin(), B.end());
}

void concatenateVectors(const std::vector<bool>& A, const std::vector<bool>& B, const std::vector<bool>& C, std::vector<bool>& result) {
    result.reserve(A.size() + B.size() + C.size());
    result.insert(result.end(), A.begin(), A.end());
    result.insert(result.end(), B.begin(), B.end());
    result.insert(result.end(), C.begin(), C.end());
}

void concatenateVectors(const std::vector<unsigned char>& A, const std::vector<unsigned char>& B, const std::vector<unsigned char>& C, std::vector<unsigned char>& result) {
    result.reserve(A.size() + B.size() + C.size());
    result.insert(result.end(), A.begin(), A.end());
    result.insert(result.end(), B.begin(), B.end());
    result.insert(result.end(), C.begin(), C.end());
}

void sha256(const unsigned char* input, unsigned char* hash, int len) {
	SHA256_CTX_mod ctx256;

	sha256_init(&ctx256);
	sha256_update(&ctx256, input, len);
	sha256_final_no_padding(&ctx256, hash);
}

void sha256(SHA256_CTX_mod* ctx256, const unsigned char* input, unsigned char* hash, int len) {
	sha256_init(ctx256);
	sha256_update(ctx256, input, len);
	sha256_final_no_padding(ctx256, hash);
}

void hashVector(SHA256_CTX_mod* ctx256, const std::vector<bool> input, std::vector<bool>& output) {
    int size = int(input.size() / 8);
    unsigned char bytes[size];
    convertVectorToBytes(input, bytes);

    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256(ctx256, bytes, hash, (int)size);

    convertBytesToVector(hash, output);
}

void hashVector(SHA256_CTX_mod* ctx256, const std::vector<unsigned char> input, std::vector<unsigned char>& output) {
    int size = int(input.size());
    unsigned char bytes[size];
    convertBytesVectorToBytes(input, bytes);

    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256(ctx256, bytes, hash, (int)size);

    convertBytesToBytesVector(hash, output);
}

void hashVector(const std::vector<bool> input, std::vector<bool>& output) {
	SHA256_CTX_mod ctx256;

    int size = int(input.size() / 8);
    unsigned char bytes[size];
    convertVectorToBytes(input, bytes);

    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256(&ctx256, bytes, hash, (int)size);

    convertBytesToVector(hash, output);
}

void hashVector(const std::vector<unsigned char> input, std::vector<unsigned char>& output) {
	SHA256_CTX_mod ctx256;

    int size = int(input.size());
    unsigned char bytes[size];
    convertBytesVectorToBytes(input, bytes);

    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256(&ctx256, bytes, hash, (int)size);

    convertBytesToBytesVector(hash, output);
}

void hashVectors(SHA256_CTX_mod* ctx256, const std::vector<bool> left, const std::vector<bool> right, std::vector<bool>& output) {
    std::vector<bool> concat;
    concatenateVectors(left, right, concat);

    int size = int(concat.size() / 8);
    unsigned char bytes[size];
    convertVectorToBytes(concat, bytes);

    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256(ctx256, bytes, hash, (int)size);

    convertBytesToVector(hash, output);
}

void hashVectors(SHA256_CTX_mod* ctx256, const std::vector<unsigned char> left, const std::vector<unsigned char> right, std::vector<unsigned char>& output) {
    std::vector<unsigned char> concat;
    concatenateVectors(left, right, concat);

    int size = int(concat.size());
    unsigned char bytes[size];
    convertBytesVectorToBytes(concat, bytes);

    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256(ctx256, bytes, hash, (int)size);

    convertBytesToBytesVector(hash, output);
}

void hashVectors(const std::vector<bool> left, const std::vector<bool> right, std::vector<bool>& output) {
	std::cout << std::endl;

    std::vector<bool> concat;
    concatenateVectors(left, right, concat);

    int size = int(concat.size() / 8);
    unsigned char bytes[size];
    convertVectorToBytes(concat, bytes);

    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256(bytes, hash, (int)size);

    convertBytesToVector(hash, output);
}

void hashVectors(const std::vector<unsigned char> left, const std::vector<unsigned char> right, std::vector<unsigned char>& output) {
    std::vector<unsigned char> concat;
    concatenateVectors(left, right, concat);

    int size = int(concat.size());
    unsigned char bytes[size];
    convertBytesVectorToBytes(concat, bytes);

    unsigned char hash[SHA256_BLOCK_SIZE];
    sha256(bytes, hash, (int)size);

    convertBytesToBytesVector(hash, output);
}

bool VectorIsZero(const std::vector<bool> test) {
    // XXX: not time safe
	return (test.end() == std::find(test.begin(), test.end(), true));
}

size_t countOnes(const std::vector<bool>& vec) {
    return count(vec.begin(), vec.end(), true);
}

std::vector<unsigned char> vectorSlice(const std::vector<unsigned char>& vec, size_t start, size_t length) {
    std::vector<unsigned char> slice(length);
    for (size_t i = 0; i < length; i++) {
        slice.at(i) = vec.at(start + i);
    }
    return slice;
}

} /* namespace libzerocash */

