/**********************************************************************
 * Copyright (c) 2015 Andrew Poelstra                                 *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/

#ifndef SECP256K1_MODULE_ECDH_TESTS_H
#define SECP256K1_MODULE_ECDH_TESTS_H

int ecdh_hash_function_test_fail(unsigned char *output, const unsigned char *x, const unsigned char *y, void *data) {
    (void)output;
    (void)x;
    (void)y;
    (void)data;
    return 0;
}

int ecdh_hash_function_custom(unsigned char *output, const unsigned char *x, const unsigned char *y, void *data) {
    (void)data;
    /* Save x and y as uncompressed public key */
    output[0] = 0x04;
    memcpy(output + 1, x, 32);
    memcpy(output + 33, y, 32);
    return 1;
}

void test_ecdh_api(void) {
    /* Setup context that just counts errors */
    rustsecp256k1_v0_3_1_context *tctx = rustsecp256k1_v0_3_1_context_create(SECP256K1_CONTEXT_SIGN);
    rustsecp256k1_v0_3_1_pubkey point;
    unsigned char res[32];
    unsigned char s_one[32] = { 0 };
    int32_t ecount = 0;
    s_one[31] = 1;

    rustsecp256k1_v0_3_1_context_set_error_callback(tctx, counting_illegal_callback_fn, &ecount);
    rustsecp256k1_v0_3_1_context_set_illegal_callback(tctx, counting_illegal_callback_fn, &ecount);
    CHECK(rustsecp256k1_v0_3_1_ec_pubkey_create(tctx, &point, s_one) == 1);

    /* Check all NULLs are detected */
    CHECK(rustsecp256k1_v0_3_1_ecdh(tctx, res, &point, s_one, NULL, NULL) == 1);
    CHECK(ecount == 0);
    CHECK(rustsecp256k1_v0_3_1_ecdh(tctx, NULL, &point, s_one, NULL, NULL) == 0);
    CHECK(ecount == 1);
    CHECK(rustsecp256k1_v0_3_1_ecdh(tctx, res, NULL, s_one, NULL, NULL) == 0);
    CHECK(ecount == 2);
    CHECK(rustsecp256k1_v0_3_1_ecdh(tctx, res, &point, NULL, NULL, NULL) == 0);
    CHECK(ecount == 3);
    CHECK(rustsecp256k1_v0_3_1_ecdh(tctx, res, &point, s_one, NULL, NULL) == 1);
    CHECK(ecount == 3);

    /* Cleanup */
    rustsecp256k1_v0_3_1_context_destroy(tctx);
}

void test_ecdh_generator_basepoint(void) {
    unsigned char s_one[32] = { 0 };
    rustsecp256k1_v0_3_1_pubkey point[2];
    int i;

    s_one[31] = 1;
    /* Check against pubkey creation when the basepoint is the generator */
    for (i = 0; i < 100; ++i) {
        rustsecp256k1_v0_3_1_sha256 sha;
        unsigned char s_b32[32];
        unsigned char output_ecdh[65];
        unsigned char output_ser[32];
        unsigned char point_ser[65];
        size_t point_ser_len = sizeof(point_ser);
        rustsecp256k1_v0_3_1_scalar s;

        random_scalar_order(&s);
        rustsecp256k1_v0_3_1_scalar_get_b32(s_b32, &s);

        CHECK(rustsecp256k1_v0_3_1_ec_pubkey_create(ctx, &point[0], s_one) == 1);
        CHECK(rustsecp256k1_v0_3_1_ec_pubkey_create(ctx, &point[1], s_b32) == 1);

        /* compute using ECDH function with custom hash function */
        CHECK(rustsecp256k1_v0_3_1_ecdh(ctx, output_ecdh, &point[0], s_b32, ecdh_hash_function_custom, NULL) == 1);
        /* compute "explicitly" */
        CHECK(rustsecp256k1_v0_3_1_ec_pubkey_serialize(ctx, point_ser, &point_ser_len, &point[1], SECP256K1_EC_UNCOMPRESSED) == 1);
        /* compare */
        CHECK(memcmp(output_ecdh, point_ser, 65) == 0);

        /* compute using ECDH function with default hash function */
        CHECK(rustsecp256k1_v0_3_1_ecdh(ctx, output_ecdh, &point[0], s_b32, NULL, NULL) == 1);
        /* compute "explicitly" */
        CHECK(rustsecp256k1_v0_3_1_ec_pubkey_serialize(ctx, point_ser, &point_ser_len, &point[1], SECP256K1_EC_COMPRESSED) == 1);
        rustsecp256k1_v0_3_1_sha256_initialize(&sha);
        rustsecp256k1_v0_3_1_sha256_write(&sha, point_ser, point_ser_len);
        rustsecp256k1_v0_3_1_sha256_finalize(&sha, output_ser);
        /* compare */
        CHECK(memcmp(output_ecdh, output_ser, 32) == 0);
    }
}

void test_bad_scalar(void) {
    unsigned char s_zero[32] = { 0 };
    unsigned char s_overflow[32] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
        0xba, 0xae, 0xdc, 0xe6, 0xaf, 0x48, 0xa0, 0x3b,
        0xbf, 0xd2, 0x5e, 0x8c, 0xd0, 0x36, 0x41, 0x41
    };
    unsigned char s_rand[32] = { 0 };
    unsigned char output[32];
    rustsecp256k1_v0_3_1_scalar rand;
    rustsecp256k1_v0_3_1_pubkey point;

    /* Create random point */
    random_scalar_order(&rand);
    rustsecp256k1_v0_3_1_scalar_get_b32(s_rand, &rand);
    CHECK(rustsecp256k1_v0_3_1_ec_pubkey_create(ctx, &point, s_rand) == 1);

    /* Try to multiply it by bad values */
    CHECK(rustsecp256k1_v0_3_1_ecdh(ctx, output, &point, s_zero, NULL, NULL) == 0);
    CHECK(rustsecp256k1_v0_3_1_ecdh(ctx, output, &point, s_overflow, NULL, NULL) == 0);
    /* ...and a good one */
    s_overflow[31] -= 1;
    CHECK(rustsecp256k1_v0_3_1_ecdh(ctx, output, &point, s_overflow, NULL, NULL) == 1);

    /* Hash function failure results in ecdh failure */
    CHECK(rustsecp256k1_v0_3_1_ecdh(ctx, output, &point, s_overflow, ecdh_hash_function_test_fail, NULL) == 0);
}

void run_ecdh_tests(void) {
    test_ecdh_api();
    test_ecdh_generator_basepoint();
    test_bad_scalar();
}

#endif /* SECP256K1_MODULE_ECDH_TESTS_H */
