#ifndef PQCLEAN_DILITHIUM2_MASK_OPTMEM_NTT_H
#define PQCLEAN_DILITHIUM2_MASK_OPTMEM_NTT_H
#include "params.h"
#include <stdint.h>

void PQCLEAN_DILITHIUM2_MASK_OPTMEM_ntt(int32_t a[N]);

void PQCLEAN_DILITHIUM2_MASK_OPTMEM_invntt_tomont(int32_t a[N]);

#endif
