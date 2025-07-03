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
 * @file     smack_sl.h
 *
 * @brief    Smack NVM application code example file.
 *
 * @version  v1.0
 * @date     2020-05-20
 *
 * @note
 */

/*lint -save -e960 */

#ifndef _SMACK_SL_H_
#define _SMACK_SL_H_


/** @addtogroup Infineon
 * @{
 */

/** @addtogroup Smack_sl
 * @{
 */


/** @addtogroup fw_config
 * @{
 */

extern void example_handler(void);
extern void hardfault_handler(void);

extern uint16_t get_threshold_from_voltage(float);

extern const uint8_t smack_sl_tag[];

// Offer a counter for external access
extern uint32_t sl_counter;

typedef enum 
{
    POWER_POWER_OFF = 0, 
    POWER_READY_FOR_PASSCODE = 1, 
    POWER_HARVESTING = 2,
    POWER_HARVESTING_DONE = 3,
    POWER_IDLE = 4
} Power_State_enum_t; 

typedef enum 
{
    LOCK_LOCKED = 0, 
    LOCK_UNLOCKING = 1, 
    LOCK_UNLOCKED = 2,
    LOCK_LOCKING = 3,
} Lock_State_enum_t; 

#define MCU_VALID         0xDDDDDDDD
#define PASSCODE          0x12344321
#define ZERO_32           0x00000000
#define PC_VAL            0x55555555
#define PC_INVAL          0x99999999
#define HARVESTING_DONE   0xBADAB00B
#define REGISTER_RQ       0xEFEFEFEF
#define SERIAL_NUMBER     0xFEDCBA20
#define REG_ERROR         0x88888888

#define MAX_MOTOR_ROTATIONS 8

/** @} */ /* End of group fw_config */


/** @} */ /* End of group Smack_sl */

/** @} */ /* End of group Infineon */

#endif /* _SMACK_SL_H_ */
