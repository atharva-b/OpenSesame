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
 * @file     sys_tim_lib.h
 *
 * @brief    This module extends the system timer driver of the ROM library.
 *
 * @version  v1.0
 * @date     2021-08-01
 *
 */

#ifndef _SYS_TIM_LIB_H_
#define _SYS_TIM_LIB_H_

/** @addtogroup Infineon
 * @{
 */

/** @addtogroup Smack
 * @{
 */


/** @addtogroup sys_tim_lib
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wait for "ticks" clock cycles on one System Timer channel using WFI to conserve power.
 *        The system timer channels have a width of 16 bits and are clocked by the same clock a the CPU core.
 *        This function uses one 16 bit timers, so delays are limited to a bit more than 2ms @28MHz.
 *        Resources used: one system timer channel, one interrupt.
 * @param channel    system timer channel to use (0...5)
 * @param period     number of ticks to wait (16 bits)
 * @param irq_number interrupt number to use (9...14)
 */
extern void sys_tim_singleshot(const uint8_t channel, const uint16_t period, const uint8_t irq_number);

/**
 * @brief Wait for "ticks" clock cycles on one System Timer channel using WFI to conserve power.
 *        The system timer channels have a width of 16 bits and are clocked by the same clock a the CPU core.
 *        This function uses a loop around the 16 bit timer to increase the delay counter to 32 bits.
 *        Resources used: one system timer channel, one interrupt.
 * @param channel    system timer channel to use (0...5)
 * @param period     number of ticks to wait (32 bits)
 * @param irq_number interrupt number to use (9...14)
 */
extern void sys_tim_singleshot_32(const uint8_t channel, const uint32_t period, const uint8_t irq_number);

// The cyclic cascaded timer function below is intended to be used in one of two ways:
//
// Setup the cascade as one 32 bit timer by configuring both prescaler and period_counter to 0xffff.
// Use sys_tim_cyclic_cascaded_get_combined() to read the current 32 bit count.
// - or -
// Setup the prescaler to generate a well know clock (e.g. milliseconds), set period_counter to desired wraparound.
// Use sys_tim_cyclic_cascaded_get_upper() to get current count in units of well known clock (e.g. milliseconds).

/**
 * @brief Setup a cascaded timer pair running in background (no irq) to measure time distances
 * @param channel    system timer channel to use (1...5)
 * @param period_prescaler period of prescaler (e.g. clock of upper counter)
 * @param period_counter   period of upper counter
 */
extern void sys_tim_cyclic_cascaded(uint8_t channel, uint16_t period_prescaler, uint16_t period_counter);

/**
 * @brief stop/disable the cascaded timer pair
 * @param channel    system timer channel to use (1...5)
 */
extern void sys_tim_cyclic_cascaded_stop(uint8_t channel);

/**
 * @brief Read upper counter of cascaded timer pair
 * @param channel    system timer channel to use (1...5)
 * @return           current counter value (upper 16 bit)
 */
extern uint16_t sys_tim_cyclic_cascaded_get_upper(uint8_t channel);

/**
 * @brief Read combined counter of cascaded timer pair
 * @param channel    system timer channel to use (1...5)
 * @return           current counter value (32 bit)
 */
extern uint32_t sys_tim_cyclic_cascaded_get_combined(uint8_t channel);

/**
 * @brief Shutdown system timer, e.g. stop clock
 */
extern void sys_tim_close(void);


/**
 * @brief Configure a PWM channel.
 *        PWM operation is supported on channel 0.
 *        This function does *not* configure a GPIO pin for output of the signal. This must be
 *        done separately.
 *        The PWM is configured in "inverted" mode which gives (IMHO) the expected behavior:
 *        The output is high until the duty value is reached, then it is switched to low.
 * @param period     period of the PWM signal (ticks)
 * @param duty       active phase of PWM signal (ticks)
 */
extern void sys_tim_pwm_config(uint16_t period, uint16_t duty);


/**
 * @brief Start PWM operation.
 *        PWM operation is supported on channel 0.
 *        The channel must be configured first with sys_tim_pwm_config().
 */
extern void sys_tim_pwm_start(void);


/**
 * @brief Stop PWM operation.
 *        PWM operation is supported on channel 0.
 */
extern void sys_tim_pwm_stop(void);


#ifdef __cplusplus
}
#endif

/** @} */ /* End of group sys_tim_lib */

/** @} */ /* End of group Smack */

/** @} */ /* End of group Infineon */

#endif /* _SYS_TIM_LIB_H_ */
