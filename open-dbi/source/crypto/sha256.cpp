// sha256.cpp - see sha256.h. Clean-room SHA-256 (FIPS 180-4).
#include "sha256.h"

namespace dbi::crypto {
namespace {

inline uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

// FIPS 180-4 round constants (first 32 bits of the cube roots of the first 64 primes).
const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

} // namespace

void Sha256::reset() {
    h_[0]=0x6a09e667; h_[1]=0xbb67ae85; h_[2]=0x3c6ef372; h_[3]=0xa54ff53a;
    h_[4]=0x510e527f; h_[5]=0x9b05688c; h_[6]=0x1f83d9ab; h_[7]=0x5be0cd19;
    bits_ = 0; buflen_ = 0;
}

void Sha256::processBlock(const uint8_t b[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)b[i*4]<<24) | ((uint32_t)b[i*4+1]<<16) |
               ((uint32_t)b[i*4+2]<<8) | (uint32_t)b[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ror(w[i-15],7) ^ ror(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ror(w[i-2],17) ^ ror(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=h_[0],b2=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],hh=h_[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = ror(e,6) ^ ror(e,11) ^ ror(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + K[i] + w[i];
        uint32_t S0 = ror(a,2) ^ ror(a,13) ^ ror(a,22);
        uint32_t maj = (a & b2) ^ (a & c) ^ (b2 & c);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=c; c=b2; b2=a; a=t1+t2;
    }
    h_[0]+=a; h_[1]+=b2; h_[2]+=c; h_[3]+=d; h_[4]+=e; h_[5]+=f; h_[6]+=g; h_[7]+=hh;
}

void Sha256::update(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    bits_ += (uint64_t)len * 8;
    if (buflen_) {                                    // drain a partial block first
        while (len && buflen_ < 64) { buf_[buflen_++] = *p++; --len; }
        if (buflen_ == 64) { processBlock(buf_); buflen_ = 0; }
    }
    while (len >= 64) { processBlock(p); p += 64; len -= 64; }
    while (len--) buf_[buflen_++] = *p++;             // keep the tail (< 64 bytes)
}

void Sha256::finish(uint8_t out[32]) {
    const uint64_t bits = bits_;                      // capture length before padding
    buf_[buflen_++] = 0x80;                           // append the '1' bit
    if (buflen_ > 56) {                               // no room for the 8-byte length
        while (buflen_ < 64) buf_[buflen_++] = 0;
        processBlock(buf_); buflen_ = 0;
    }
    while (buflen_ < 56) buf_[buflen_++] = 0;
    for (int i = 7; i >= 0; --i) buf_[buflen_++] = (uint8_t)(bits >> (8*i));  // big-endian length
    processBlock(buf_);
    for (int i = 0; i < 8; ++i) {
        out[i*4]   = (uint8_t)(h_[i] >> 24);
        out[i*4+1] = (uint8_t)(h_[i] >> 16);
        out[i*4+2] = (uint8_t)(h_[i] >> 8);
        out[i*4+3] = (uint8_t)(h_[i]);
    }
    reset();
}

void sha256(const void* data, size_t len, uint8_t out[32]) {
    Sha256 s; s.update(data, len); s.finish(out);
}

} // namespace dbi::crypto
