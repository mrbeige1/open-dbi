// aes.h - AES-128 (ECB / CTR / XTS) for NCA crypto.
//
// CLEAN-ROOM: implemented from FIPS-197 (AES) and IEEE 1619 (XTS), plus the public
// NCA conventions (switchbrew): header = AES-128-XTS with 0x200-byte sectors and a
// big-endian sector tweak; sections = AES-128-CTR. No third-party code.
#pragma once
#include <cstdint>
#include <cstddef>

namespace dbi::crypto {

class Aes128 {
public:
    void setKey(const uint8_t key[16]);                       // expands enc + dec schedules
    void encryptBlock(const uint8_t in[16], uint8_t out[16]) const;
    void decryptBlock(const uint8_t in[16], uint8_t out[16]) const;
private:
    uint8_t ek_[176];
    uint8_t dk_[176];
};

// AES-128-CTR. Encryption and decryption are identical (XOR with keystream).
// `ctr` is the initial 16-byte counter block; it is treated big-endian for increments.
void aesCtr(const uint8_t key[16], const uint8_t ctr[16],
            const uint8_t* in, uint8_t* out, size_t len);

// Decrypt one NCA-style XTS sector: key1=data key, key2=tweak key, sector index
// encoded big-endian into the 16-byte tweak. `len` is a multiple of 16 (0x200 here).
void aesXtsDecryptSector(const uint8_t key1[16], const uint8_t key2[16],
                         uint64_t sector, const uint8_t* in, uint8_t* out, size_t len);

} // namespace dbi::crypto
