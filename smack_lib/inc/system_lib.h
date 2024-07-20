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
 * @file     system_lib.h
 *
 * @brief    This module provides helper functions for system access.
 *
 * @version  v1.0
 * @date     2021-08-01
 *
 */

#ifndef INC_SYSTEM_LIB_H_
#define INC_SYSTEM_LIB_H_

/** @addtogroup Infineon
 * @{
 */

/** @addtogroup Smack
 * @{
 */


/** @addtogroup system_lib
 * @{
 */

#include "pmu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Check if RF field is present.
 * @return true: RF field present
 */
extern bool check_rf_field(void);


/**
 * @brief  Read current vclamp setting.
 * @return vclamp setting
 */
extern uint8_t vclamp_get(void);


/**
 * @brief  Set vclamp setting.
 * @param  vclamp value (0...2)
 */
extern void vclamp_set(uint8_t value);


/**
 * @brief This function returns the wakeup source which caused to leave power save mode.
 * @return wakeup_source_t wake up source of recent wake/up return from power save mode (can be of value stb_tim, nfc, wakeup_pin, unclear (unclear means simultaneous set of  wake up events))
 */
extern wakeup_source_t get_wakeup_source_lib(void);


/**
 *
 */
extern void rtc_init_lib(void);


#ifdef __cplusplus
}
#endif

/** @} */ /* End of group system_lib */

/** @} */ /* End of group Smack */

/** @} */ /* End of group Infineon */

#endif /* INC_SYSTEM_LIB_H_ */
