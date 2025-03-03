/* ============================================================================
** Copyright (c) 2021 Infineon Technologies AG
**               All rights reserved.
**               www.infineon.com
** ============================================================================
**
** Redistribution and use of this software only permitted to the extent
** expressly agreed with Infineon Technologies AG.
** ============================================================================
*
*/

/** @file smack_sl.c
 *  @brief Entry point for the application code after low-level ROM boot.
 */

// standard libs
#include "core_cm0.h"
#include <stdbool.h>
#include <stdint.h>

// ROM and peripheral libraries
#include "rom_lib.h"
#include "sys_tim_lib.h"
#include "shc_lib.h"
#include "gpio.h"

// smack_sl project files
#include "smack_sl.h"
#include "smack_dataexchange.h"

//---------------------------------------------------------------------
// NDEF Tag Definition
//---------------------------------------------------------------------
/* NDEF tag defined by user.
 * To activate this tag, set the field "tag_type_2_ptr" in APARAM.
 */
const uint8_t smack_sl_tag[] =
{
    0x05, 0xc0, 0xbe, 0xef,
    0xde, 0xad, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff,
    0xE1,
    0x10,
    0x0b,
    0x0f,
    0x03, 0x4f, 0xd1, 0x01,
    0x4b, 0x54, 0x02, 'e',
    'n', 'I', 'n', 'f',
    'i', 'n', 'e', 'o',
    'n', ' ', 'T', 'e',
    'c', 'h', 'n', 'o',
    'l', 'o', 'g', 'i',
    'e', 's', ' ', 'A',
    'G', ' ', ' ', 'N',
    'G', 'C', '1', '0',
    '8', '0', ' ', ' ',
    'S', 'm', 'A', 'c',
    'K', ' ', ' ', '0',
    '5', 'C', '0', 'B',
    'E', 'E', 'F', 'D',
    'E', 'A', 'D', '0',
    '0', '0', ' ', ' ',
    'V', '4', '.', '0',
    '.', '1', ' ', '(',
    'N', 'V', 'M', ')',
    0xfe, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff
};

//---------------------------------------------------------------------
// Definitions
//---------------------------------------------------------------------
#ifndef wait_about_1ms
#define WAIT_ABOUT_1MS   0x8000   //!< clock tick constant ~1ms @ 28MHz
#endif

#define MCU_VALID         0xA55B00B5
#define PASSCODE          0x12344321
#define ZERO_32           0x00000000
#define PC_VAL            0x55555555
#define PC_INVAL          0x99999999
#define HARVESTING_DONE   0xBADAB00B

/*
   Previous firmware stored version data in the last flash page (0x1EFF4–0x1EFFF).
   To avoid conflicts, we now store the LED state in the previous page.
   Assuming a page size of 128 bytes, the previous page spans 0x0001EF00–0x0001EF7F.
   Here we reserve address 0x0001EF10 for the LED state.
*/
#define LOCK_STATE_ADDR    0x0001EF10   // New LED state address
#define LED_GPIO          1            // LED is connected to GPIO1

/* TODO:
1. Add registration section in Linker_config; randomly accessing memory seems very unsafe
2. Add registration info LOW (PRIORITY)
*/

//---------------------------------------------------------------------
// Global Variables
//---------------------------------------------------------------------
uint32_t sl_counter;
Power_State_enum_t current_state = POWER_POWER_OFF;
uint32_t turn_cycles = 0;

/**
 * H-BRIDGE LAYOUT
 * 
 *      ---------*VDD_HB*---------
 *      |                        |   
 *      |                        |
 *      *HS1*                 *HS2*
 *      |                        |
 *      |---*M_A*        *M_B*---|
 *      |                        |
 *      |                        |
 *      *LS1*                 *LS2*
 *      |                        |
 *      |                        |
 *      -----------*GND*----------
 * 
 */

/** _nvm_start() is the main() routine of the application code:
 * - when building the image (rom or ram), it is called by Reset_Handler() (see startup_smack.c) after SystemInit().
 * - when building and running the unit tests, it is not called, as far as I know, it is not subject to unit testing.
 * - when building and running integration and/or system tests, it is called by sc_main() upon simulation start.
 * This is also the reason of why it is called _start() and not main(): The VP has a higher layer main() which
 * calls sc_main() and _start(). Having two main's will fail when linking the VP executable.
 *
 * @note _start() cannot be made static, it is referenced by startup_smack.c and also by the interface
 * to the Virtual Prototype. But linting does not know about such external references. The default
 * approach to solve is to add '//lint -e765'. Our linter is plain old, we don't get a new one thanks to
 * the application owner from IT and it has a bug which renders -e765 useless. The only option we had
 * is to suppress the warning in au_misra2_fixes.lnt
 *
 * @return nothing
 */

void _nvm_start(void);

// TODO: Figure out if we actually need this
uint16_t voltage_sweep = 0;
bool done_sweep = false;

//---------------------------------------------------------------------
// Local Function Prototypes
//---------------------------------------------------------------------
void toggle_led_state(void);
uint16_t get_threshold_from_voltage(float input_voltage);

//---------------------------------------------------------------------
// Example Interrupt and Helper Functions
//---------------------------------------------------------------------
static uint32_t example_counter = 0;
void example_handler(void)
{
    example_counter++;
}

void hardfault_handler(void)
{
    uint32_t cnt = 30000000;
    while (cnt--)
    {
        __NOP();
    }
}

void sweep_voltages(void)
{
    Mailbox_t* mbx = get_mailbox_address();
    if (shc_compare(shc_channel_ma, get_threshold_from_voltage(3.0)) == false)
    {
        mbx->content[6] = voltage_sweep;
        done_sweep = true;
    }
    else
    {
        voltage_sweep++;
    }
}

/* Toggle lock state for H-Bridge control */
void toggle_lock(bool *hs1, bool *ls1, bool *hs2, bool *ls2, bool lock)
{
    if (lock)
    {
        if (*hs1)
        {
            *hs1 = false;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if (!*ls1)
        {
            *ls1 = true;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if (!*hs2)
        {
            if (*ls2)
            {
                *ls2 = false;
                set_hb_switch(*hs1, *ls1, *hs2, *ls2);
            }
            *hs2 = true;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if (*ls2)
        {
            *ls2 = false;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
    }
    else
    {
        if (!*hs1)
        {
            if (*ls1)
            {
                *ls1 = false;
                set_hb_switch(*hs1, *ls1, *hs2, *ls2);
            }
            *hs1 = true;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if (*ls1)
        {
            *ls1 = false;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if (*hs2)
        {
            *hs2 = false;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if (!*ls2)
        {
            *ls2 = true;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
    }
}

/* Function to control the motor; as in your original implementation */
void turn_motor(Mailbox_t* mbx, bool* hs1, bool* ls1, bool* hs2, bool* ls2, bool lock)
{
    const uint32_t wait_time_discharge = WAIT_ABOUT_1MS * 32;
    const uint32_t wait_time_charge = WAIT_ABOUT_1MS;

    while (!shc_compare(shc_channel_ma, get_threshold_from_voltage(3.0)))
    {
        mbx->content[5] = 0x22222222;
    }
    toggle_lock(hs1, ls1, hs2, ls2, lock);
    sys_tim_singleshot_32(0, wait_time_discharge, 14);
    if (!lock)
    {
        *ls2 = false;
    }
    else
    {
        *ls1 = false;
    }
    set_hb_switch(*hs1, *ls1, *hs2, *ls2);
    sys_tim_singleshot_32(0, wait_time_charge, 14);
}

/* Helper function: convert voltage (in volts) to threshold ticks. */
uint16_t get_threshold_from_voltage(float input_voltage)
{
    return (uint16_t)(input_voltage * 1000.0F);
}

//---------------------------------------------------------------------
// Persistent LED State Toggle Implementation
//---------------------------------------------------------------------
/**
 * @brief Toggle the persistent LED state stored in NVM and update the physical LED.
 *
 * This function:
 *   - Reads the current LED state from flash memory.
 *   - Toggles it (0 becomes 1; nonzero becomes 0).
 *   - Powers up and configures the NVM.
 *   - Opens the assembly buffer for the flash page containing LOCK_STATE_ADDR.
 *   - Updates the state word in the assembly buffer.
 *   - Erases the flash page.
 *   - Programs the flash page with the new value.
 *   - Verifies the programming.
 *   - Powers down the NVM.
 *   - Updates GPIO1 (using set_singlegpio_out) to reflect the new state.
 *
 * @return The new LED state (0 or 1), or a nonzero error code if an NVM operation fails.
 */
bool toggle_lock_state(void)
{
    volatile uint8_t err;

    // Power up and configure the NVM using ROM routines
    // switch_on_nvm();
    nvm_config();

    // Read the current LED state from flash (assumes memory-mapped NVM)
    uint32_t current_state = *((volatile uint32_t*) LOCK_STATE_ADDR);
    // Toggle state: if 0 then 1; otherwise, set to 0.
    uint32_t new_state = (current_state == 0) ? 1 : 0;

    // Open the assembly buffer for the flash page that includes LOCK_STATE_ADDR
    err = nvm_open_assembly_buffer(LOCK_STATE_ADDR);

    // Write the new LED state into the assembly buffer
    *((volatile uint32_t*) LOCK_STATE_ADDR) = new_state;

    // Erase the flash page (ensure LOCK_STATE_ADDR is in a dedicated page)
    nvm_erase_page();
    nvm_program_page();

    nvm_config();

    return new_state;;
}

void toggle_motor(void)
{
    volatile uint8_t err;

    // for (int i = 0; i < 20; i++)
    // {
    //     turn_motor(mbx, &hs1, &ls1, &hs2, &ls2, !((bool)new_state));
    // }
}

//---------------------------------------------------------------------
// State Machine Functions
//---------------------------------------------------------------------
void run_power_state_machine(void)
{
    bool authenticated = false;
    Mailbox_t* mbx = get_mailbox_address();
    bool hs1 = true, hs2 = false, ls1 = false, ls2 = false;
    bool locked = true;

    while (true)
    {
        switch (current_state)
        {
            case POWER_POWER_OFF:
                mbx->content[1] = MCU_VALID;
                current_state = POWER_READY_FOR_PASSCODE;
                break;

            case POWER_READY_FOR_PASSCODE:
                if (mbx->content[2] == PASSCODE)
                {
                    authenticated = true;
                    current_state = POWER_HARVESTING;
                    mbx->content[3] = PC_VAL;
                    set_hb_switch(hs1, ls1, hs2, ls2);
                }
                else if (mbx->content[2] == ZERO_32)
                {
                    current_state = POWER_READY_FOR_PASSCODE;
                }
                else
                {
                    mbx->content[3] = PC_INVAL;
                    current_state = POWER_IDLE;
                }
                break;

            case POWER_HARVESTING:
                if (shc_compare(shc_channel_ma, get_threshold_from_voltage(3.0)) == true)
                {
                    mbx->content[5] = 0x11111111;
                    current_state = POWER_HARVESTING_DONE;
                }
                break;

            case POWER_HARVESTING_DONE:
                // Power up and configure the NVM using ROM routines
                {
                    bool new_state = toggle_lock_state();
                    
                    // TODO: Not make this an infinite loop
                    for(uint8_t i = 0; i < 5; i++) {
                        turn_motor(mbx, &hs1, &ls1, &hs2, &ls2, new_state);
                    }
                    mbx->content[3] = HARVESTING_DONE;
                    current_state = POWER_IDLE;
                }
                break;

            case POWER_IDLE:
                // Remain idle; add periodic tasks or sleep logic as needed.
                break;

            default:
                break;
        }
    }
}

void run_lock_state_machine(void)
{
    bool authenticated = false;
    Mailbox_t* mbx;

    while (true)
    {
        switch (current_state)
        {
            case LOCK_LOCKED:
                mbx = get_mailbox_address();
                mbx->content[1] = MCU_VALID;
                break;

            case LOCK_UNLOCKING:
                // Implement unlocking procedure here if needed.
                break;

            case LOCK_UNLOCKED:
                // Code for unlocked state.
                break;

            case LOCK_LOCKING:
                // Code for locking state.
                break;

            default:
                break;
        }
    }
}

//---------------------------------------------------------------------
// Application Entry Point
//---------------------------------------------------------------------
void _nvm_start(void)
{
    nfc_init();
    init_dand();
    vars_init();
    shc_init();

    volatile NFC_State_enum_t state = handle_DAND_protocol();
    volatile NFC_Frame_enum_t frame_type = classify_frame();
    nfc_state_machine();

    set_hb_eventctrl(false);

    single_gpio_iocfg(true, false, true, false, false, LED_GPIO);

    // toggle_led_state();

    while (true)
    {
        read_frame();
        frame_type = classify_frame();
        run_power_state_machine();
        // toggle_led_state();
        // sys_tim_singleshot_32(0, WAIT_ABOUT_1MS*32, 14);
        asm("WFI"); // Wait For Interrupt to conserve power.
    }
}
