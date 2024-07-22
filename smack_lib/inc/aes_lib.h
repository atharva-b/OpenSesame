/* ============================================================================
** Copyright (c) 2021 Infineon Technologies AG
**               All rights reserved.
**               www.infineon.com
** ============================================================================
**
** ============================================================================
** Redistribution and use of this software only permitted to the extent
** expressly agreed with Infineon Technologies AG.
** ============================================================================
*
*/

/**
 * @file     aes_lib.h
 *
 * @brief    This module extends the aes_drv driver of the ROM library.
 *
 * @version  v1.0
 * @date     2021-08-01
 *
 */

#ifndef AES_LIB_H_
#define AES_LIB_H_

/** @addtogroup Infineon
 * @{
 */

/** @addtogroup Smack
 * @{
 */


/** @addtogroup aes_lib
 * @{
 */

#include "aes_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  A union to store one block of AES128 data, accessible either as byte or word array
 */
typedef union
{
    uint8_t  b[16];     // 16 bytes (128 bits) -> byte stream
    uint32_t w[4];      // 4 words of 4 bytes (128 bits) -> words in network byte order
} aes_block_t;


/**
 * @brief  This function loads a 128-bit wide key into the AES hardware accelerator and performs the calculation of round
 *         keys for decryption type of operation
 *
 *         This is a clone of the aes_load_key() function of the ROM library which takes byte streams as an argument instead
 *         of uint32 arrays in host byte order.
 * @param  key, pointer to a byte array which holds the AES key
 * @return None
 */
extern void aes_load_key_ba(const aes_block_t* key);

/**
 * @brief  This function calculates a 128-bit AES decryption or encryption operation.
 *
 *         The calculated 128-bit wide result of operation will be stored in an array of 16 bytes by the function.
 *         The input data is also taken from an array of 16 bytes which may be a part of a data stream.
 *         The pointers to input data and result can be the same if input data is not needed anymore after the operation.
 *         The calculation of an 128-bit operation is hardware accelerated and takes 16 processor cycles.
 *         It assumes, that a valid key was loaded before into the hardware accelerator using function aes_load_key(const uint32_t* key).
 *
 *         This is a clone of the calc_aes() function of the ROM library which takes byte streams as arguments instead
 *         of uint32 arrays in host byte order.
 * @param  result  pointer to a byte array receiving the result
 * @param  data    pointer to a byte arry holding the input data
 * @param  op_type aes_operation_type defines the type of operation. Can be of value "encrypt" or "decrypt
 * @return None
 */
extern void calc_aes_ba(aes_block_t* result, const aes_block_t* data, aes_operation_type op_type);


extern uint32_t rand_lib(void);

// faster version: skip getting random noise from sense unit
extern void generate_random_number_fast(aes_block_t* random_number);

// library version: get at least some random noise from sense unit
extern void generate_random_number_lib(aes_block_t* random_number);


//=================================================================

// not a reset but initialization with defaults
//extern void sense_reset(void);

#ifdef __cplusplus
}
#endif

/** @} */ /* End of group aes_lib */

/** @} */ /* End of group Smack */

/** @} */ /* End of group Infineon */

#endif /* AES_LIB_H_ */
