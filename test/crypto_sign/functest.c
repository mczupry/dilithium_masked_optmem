#include "api.h"
#include "randombytes.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NTESTS
#define NTESTS 5
#endif

#define MLEN 1024

const uint8_t canary[8] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
};

/* allocate a bit more for all keys and messages and
 * make sure it is not touched by the implementations.
 */
static void write_canary(uint8_t *d) {
    for (size_t i = 0; i < 8; i++) {
        d[i] = canary[i];
    }
}

static int check_canary(const uint8_t *d) {
    for (size_t i = 0; i < 8; i++) {
        if (d[i] != canary[i]) {
            return -1;
        }
    }
    return 0;
}

/** Safe malloc */
inline static void* malloc_s(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        perror("Malloc failed!");
        exit(1);
    }
    return ptr;
}

#if REQUIRES_BUF
inline static void* realloc_s(void *ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (ptr == NULL) {
        perror("Realloc failed!");
        exit(1);
    }
    return ptr;
}
#endif

// https://stackoverflow.com/a/1489985/1711232
#define PASTER(x, y) x##_##y
#define EVALUATOR(x, y) PASTER(x, y)
#define NAMESPACE(fun) EVALUATOR(PQCLEAN_NAMESPACE, fun)

#define CRYPTO_PUBLICKEYBYTES NAMESPACE(CRYPTO_PUBLICKEYBYTES)
#define CRYPTO_SECRETKEYBYTES NAMESPACE(CRYPTO_SECRETKEYBYTES)
#define CRYPTO_BYTES          NAMESPACE(CRYPTO_BYTES)
#define CRYPTO_ALGNAME        NAMESPACE(CRYPTO_ALGNAME)

#define crypto_sign_keypair NAMESPACE(crypto_sign_keypair)
#define crypto_sign NAMESPACE(crypto_sign)
#define crypto_sign_open NAMESPACE(crypto_sign_open)
#define crypto_sign_signature NAMESPACE(crypto_sign_signature)
#define crypto_sign_verify NAMESPACE(crypto_sign_verify)

#define RETURNS_ZERO(f)                           \
    if ((f) != 0) {                               \
        puts("(f) returned non-zero returncode"); \
        res = 1;                                  \
        goto end;                                 \
    }

// https://stackoverflow.com/a/55243651/248065
#define MY_TRUTHY_VALUE_X 1
#define CAT(x,y) CAT_(x,y)
#define CAT_(x,y) x##y
#define HAS_NAMESPACE(x) CAT(CAT(MY_TRUTHY_VALUE_,CAT(PQCLEAN_NAMESPACE,CAT(_,x))),X)

#if !HAS_NAMESPACE(API_H)
#error "namespace not properly defined for header guard"
#endif

#if REQUIRES_BUF
#define GEN_BUF_BYTES NAMESPACE(GEN_BUF_BYTES)
#define SIGN_BUF_BYTES NAMESPACE(SIGN_BUF_BYTES)
#define VER_BUF_BYTES NAMESPACE(VER_BUF_BYTES)
#endif

static int test_sign(void) {
    /*
     * This is most likely going to be aligned by the compiler.
     * 16 extra bytes for canary
     * 1 extra byte for unalignment
     */
    uint8_t *pk_aligned = malloc_s(CRYPTO_PUBLICKEYBYTES + 16 + 1);
    uint8_t *sk_aligned = malloc_s(CRYPTO_SECRETKEYBYTES + 16 + 1);
    uint8_t *sm_aligned = malloc_s(MLEN + CRYPTO_BYTES + 16 + 1);
    uint8_t *m_aligned = malloc_s(MLEN + 16 + 1);

    /*
     * Make sure all pointers are odd.
     * This ensures that the implementation does not assume anything about the
     * data alignment. For example this would catch if an implementation
     * directly uses these pointers to load into vector registers using movdqa.
     */
    uint8_t *pk = (uint8_t *) ((uintptr_t) pk_aligned|(uintptr_t) 1);
    uint8_t *sk = (uint8_t *) ((uintptr_t) sk_aligned|(uintptr_t) 1);
    uint8_t *sm = (uint8_t *) ((uintptr_t) sm_aligned|(uintptr_t) 1);
    uint8_t *m  = (uint8_t *) ((uintptr_t) m_aligned|(uintptr_t) 1);
#if REQUIRES_BUF
	uint8_t *buf = malloc_s(GEN_BUF_BYTES);
#endif

    size_t mlen;
    size_t smlen;
    int returncode;
    int res = 0;

    int i;
    /*
     * Write 8 byte canary before and after the actual memory regions.
     * This is used to validate that the implementation does not assume
     * anything about the placement of data in memory
     * (e.g., assuming that the pk is always behind the sk)
     */
    write_canary(pk);
    write_canary(pk + CRYPTO_PUBLICKEYBYTES + 8);
    write_canary(sk);
    write_canary(sk + CRYPTO_SECRETKEYBYTES + 8);
    write_canary(sm);
    write_canary(sm + MLEN + CRYPTO_BYTES + 8);
    write_canary(m);
    write_canary(m + MLEN + 8);

    for (i = 0; i < NTESTS; i++) {
#if REQUIRES_BUF
		buf = (uint8_t *) realloc_s((void *)buf, GEN_BUF_BYTES);
        RETURNS_ZERO(crypto_sign_keypair(pk + 8, sk + 8, buf));

        randombytes(m + 8, MLEN);
        buf = (uint8_t *) realloc_s((void *)buf, SIGN_BUF_BYTES);
        RETURNS_ZERO(crypto_sign(sm + 8, &smlen, m + 8, MLEN, sk + 8, buf));
		
		buf = (uint8_t *) realloc_s((void *)buf, VER_BUF_BYTES);
        // By relying on m == sm we prevent having to allocate CRYPTO_BYTES
        // twice
        if ((returncode =
                 crypto_sign_open(sm + 8, &mlen, sm + 8, smlen, pk + 8, buf)) != 0) {
#else
        RETURNS_ZERO(crypto_sign_keypair(pk + 8, sk + 8));

        randombytes(m + 8, MLEN);
        RETURNS_ZERO(crypto_sign(sm + 8, &smlen, m + 8, MLEN, sk + 8));

        // By relying on m == sm we prevent having to allocate CRYPTO_BYTES
        // twice
        if ((returncode =
                 crypto_sign_open(sm + 8, &mlen, sm + 8, smlen, pk + 8)) != 0) {
#endif
            fprintf(stderr, "ERROR Signature did not verify correctly!\n");
            if (returncode > 0) {
                fprintf(stderr, "ERROR return code should be < 0 on failure");
            }
            res = 1;
            goto end;
        }
        // Validate that the implementation did not touch the canary
        if (check_canary(pk) || check_canary(pk + CRYPTO_PUBLICKEYBYTES + 8) ||
            check_canary(sk) || check_canary(sk + CRYPTO_SECRETKEYBYTES + 8) ||
            check_canary(sm) || check_canary(sm + MLEN + CRYPTO_BYTES + 8) ||
            check_canary(m) || check_canary(m + MLEN + 8)) {
            fprintf(stderr, "ERROR canary overwritten\n");
            res = 1;
            goto end;
        }
    }
end:
    free(pk_aligned);
    free(sk_aligned);
    free(sm_aligned);
    free(m_aligned);
#if REQUIRES_BUF
    free(buf);
#endif
    return res;
}

static int test_sign_detached(void) {
    /*
     * This is most likely going to be aligned by the compiler.
     * 16 extra bytes for canary
     * 1 extra byte for unalignment
     */
    uint8_t *pk_aligned = malloc_s(CRYPTO_PUBLICKEYBYTES + 16 + 1);
    uint8_t *sk_aligned = malloc_s(CRYPTO_SECRETKEYBYTES + 16 + 1);
    uint8_t *sig_aligned = malloc_s(CRYPTO_BYTES + 16 + 1);
    uint8_t *m_aligned = malloc_s(MLEN + 16 + 1);

    /*
     * Make sure all pointers are odd.
     * This ensures that the implementation does not assume anything about the
     * data alignment. For example this would catch if an implementation
     * directly uses these pointers to load into vector registers using movdqa.
     */
    uint8_t *pk = (uint8_t *) ((uintptr_t) pk_aligned|(uintptr_t) 1);
    uint8_t *sk = (uint8_t *) ((uintptr_t) sk_aligned|(uintptr_t) 1);
    uint8_t *sig = (uint8_t *) ((uintptr_t) sig_aligned|(uintptr_t) 1);
    uint8_t *m  = (uint8_t *) ((uintptr_t) m_aligned|(uintptr_t) 1);
#if REQUIRES_BUF
	uint8_t *buf = malloc_s(GEN_BUF_BYTES);
#endif

    size_t siglen;
    int returncode;
    int res = 0;

    int i;
    /*
     * Write 8 byte canary before and after the actual memory regions.
     * This is used to validate that the implementation does not assume
     * anything about the placement of data in memory
     * (e.g., assuming that the pk is always behind the sk)
     */
    write_canary(pk);
    write_canary(pk + CRYPTO_PUBLICKEYBYTES + 8);
    write_canary(sk);
    write_canary(sk + CRYPTO_SECRETKEYBYTES + 8);
    write_canary(sig);
    write_canary(sig + CRYPTO_BYTES + 8);
    write_canary(m);
    write_canary(m + MLEN + 8);

    for (i = 0; i < NTESTS; i++) {
#if REQUIRES_BUF
		buf = (uint8_t *) realloc_s((void *)buf, GEN_BUF_BYTES);
        RETURNS_ZERO(crypto_sign_keypair(pk + 8, sk + 8, buf));

        randombytes(m + 8, MLEN);
        buf = (uint8_t *) realloc_s((void *)buf, SIGN_BUF_BYTES);
        RETURNS_ZERO(crypto_sign_signature(sig + 8, &siglen, m + 8, MLEN, sk + 8, buf));
		
		buf = (uint8_t *) realloc_s((void *)buf, VER_BUF_BYTES);
        if ((returncode =
                crypto_sign_verify(sig + 8, siglen, m + 8, MLEN, pk + 8, buf)) != 0) {
#else
        RETURNS_ZERO(crypto_sign_keypair(pk + 8, sk + 8));

        randombytes(m + 8, MLEN);
        RETURNS_ZERO(crypto_sign_signature(sig + 8, &siglen, m + 8, MLEN, sk + 8));

        if ((returncode =
                crypto_sign_verify(sig + 8, siglen, m + 8, MLEN, pk + 8)) != 0) {
#endif
            fprintf(stderr, "ERROR Signature did not verify correctly!\n");
            if (returncode > 0) {
                fprintf(stderr, "ERROR return code should be < 0 on failure");
            }
            res = 1;
            goto end;
        }
        // Validate that the implementation did not touch the canary
        if (check_canary(pk) || check_canary(pk + CRYPTO_PUBLICKEYBYTES + 8) ||
            check_canary(sk) || check_canary(sk + CRYPTO_SECRETKEYBYTES + 8) ||
            check_canary(sig) || check_canary(sig + CRYPTO_BYTES + 8) ||
            check_canary(m) || check_canary(m + MLEN + 8)) {
            fprintf(stderr, "ERROR canary overwritten\n");
            res = 1;
            goto end;
        }
    }

end:
    free(pk_aligned);
    free(sk_aligned);
    free(sig_aligned);
    free(m_aligned);
#if REQUIRES_BUF
    free(buf);
#endif

    return res;
}

static int test_wrong_pk(void) {
    uint8_t *pk = malloc_s(CRYPTO_PUBLICKEYBYTES);
    uint8_t *pk2 = malloc_s(CRYPTO_PUBLICKEYBYTES);
    uint8_t *sk = malloc_s(CRYPTO_SECRETKEYBYTES);
    uint8_t *sm = malloc_s(MLEN + CRYPTO_BYTES);
    uint8_t *m = malloc_s(MLEN);
#if REQUIRES_BUF
	uint8_t *buf = malloc_s(GEN_BUF_BYTES);
#endif

    size_t mlen;
    size_t smlen;

    int returncode, res = 0;

    int i;

    for (i = 0; i < NTESTS; i++) {
#if REQUIRES_BUF
		buf = (uint8_t *) realloc_s((void *)buf, GEN_BUF_BYTES);
        RETURNS_ZERO(crypto_sign_keypair(pk2, sk, buf));

        RETURNS_ZERO(crypto_sign_keypair(pk, sk, buf));

        randombytes(m, MLEN);
        buf = (uint8_t *) realloc_s((void *)buf, SIGN_BUF_BYTES);
        RETURNS_ZERO(crypto_sign(sm, &smlen, m, MLEN, sk, buf));
		
		buf = (uint8_t *) realloc_s((void *)buf, VER_BUF_BYTES);
        // By relying on m == sm we prevent having to allocate CRYPTO_BYTES
        // twice
        returncode = crypto_sign_open(sm, &mlen, sm, smlen, pk2, buf);
#else
        RETURNS_ZERO(crypto_sign_keypair(pk2, sk));

        RETURNS_ZERO(crypto_sign_keypair(pk, sk));

        randombytes(m, MLEN);
        RETURNS_ZERO(crypto_sign(sm, &smlen, m, MLEN, sk));

        // By relying on m == sm we prevent having to allocate CRYPTO_BYTES
        // twice
        returncode = crypto_sign_open(sm, &mlen, sm, smlen, pk2);
#endif
        if (!returncode) {
            fprintf(stderr, "ERROR Signature did verify correctly under wrong public key!\n");
            if (returncode > 0) {
                fprintf(stderr, "ERROR return code should be < 0");
            }
            res = 1;
            goto end;
        }
    }
end:
    free(pk);
    free(pk2);
    free(sk);
    free(sm);
    free(m);
#if REQUIRES_BUF
    free(buf);
#endif
    return res;
}

int main(void) {
    // check if CRYPTO_ALGNAME is printable
    puts(CRYPTO_ALGNAME);
    int result = 0;
    result += test_sign();
    result += test_sign_detached();
    result += test_wrong_pk();

    return result;
}
