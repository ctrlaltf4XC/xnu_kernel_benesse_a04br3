/* SPDX-License-Identifier: MIT
 * NIST SP800-38A F.2 AES-CBC known-answer vectors (public test data).
 * This tests the actual selected BearSSL source, not the kernel adapter ABI.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bearssl_block.h"
static void hex(const char *s, unsigned char *out)
{
    while (*s) { unsigned n; if (sscanf(s, "%2x", &n) != 1) { return; } *out++ = n; s += 2; }
}
int main(void)
{
    const char *keys[] = {
        "2b7e151628aed2a6abf7158809cf4f3c",
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4"
    };
    const char *ciphertexts[] = {
        "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2"
        "73bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7",
        "4f021db243bc633d7178183a9fa071e8b4d9ada9ad7dedf4e5e738763f69145a"
        "571b242012fb7ae07fa9baac3df102e008b0e27988598881d920a9e64f5615cd",
        "f58c4c04d6e5f1ba779eabfb5f7bfbd69cfc4e967edb808d679f777bc6702c7d"
        "39f23369a9d9bacfa530e26304231461b2eb05e2c39be9fcda6c19078c6a9d1b"
    };
    unsigned char plain[64], iv0[16];
    hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710", plain);
    hex("000102030405060708090a0b0c0d0e0f", iv0);
    for (unsigned i = 0; i < 3; i++) {
        unsigned char key[32], expected[64], buffer[64], iv[16];
        br_aes_ct_cbcenc_keys enc; br_aes_ct_cbcdec_keys dec;
        hex(keys[i], key); hex(ciphertexts[i], expected);
        br_aes_ct_cbcenc_init(&enc, key, 16 + 8 * i);
        memcpy(buffer, plain, 64); memcpy(iv, iv0, 16);
        /* Split streaming calls must match the four-block known answer. */
        br_aes_ct_cbcenc_run(&enc, iv, buffer, 16);
        br_aes_ct_cbcenc_run(&enc, iv, buffer + 16, 48);
        if (memcmp(buffer, expected, 64) || memcmp(iv, expected + 48, 16)) { return 1; }
        br_aes_ct_cbcdec_init(&dec, key, 16 + 8 * i);
        memcpy(iv, iv0, 16);
        br_aes_ct_cbcdec_run(&dec, iv, buffer, 64);
        if (memcmp(buffer, plain, 64) || memcmp(iv, expected + 48, 16)) { return 2; }
    }
    puts("AES-CBC 128/192/256 encrypt/decrypt/streaming/IV: PASS");
    return 0;
}
