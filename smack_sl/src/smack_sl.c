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

/** @file     smack_sl.c
 *  @brief    holds smack_sl() which serves as the entry point for the application code after all the low-level ROM boot code is done
 */

// standard libs
// included by core_cm0.h: #include <stdint.h>
#include "core_cm0.h"
#include <stdbool.h>
#include <string.h>

// Smack ROM lib
#include "rom_lib.h"

// Smack NVM lib
#include "sys_tim_lib.h"

// smack_sl project
#include "smack_sl.h"
#include "smack_dataexchange.h"

#include "shc_lib.h"


#ifndef wait_about_1ms
#define WAIT_ABOUT_1MS   0x8000   //!< clock tick constant ~1ms @ 28MHz
#endif

#define MCU_VALID 0xA55B00B5
#define PASSCODE 0x12344321
#define ZERO_32 0x00000000
#define PC_VAL 0x55555555
#define PC_INVAL 0x99999999
#define HARVESTING_DONE 0xBADAB00B

#define REGISTRATION_STRUCT_ADDR ((registration_data_t*) 0x0001EF00)

typedef struct __attribute__((aligned(128))) {
    uint32_t registered; 
    uint32_t passcode;
    uint32_t lock_state;
    uint32_t rfu2;
} registration_data_t;

// registration_data_t registration_data_s __attribute__((section(".registration_section"))) = {
//     .registered = 0xFFFFFFFF,
//     .passcode = 0xFFFFFFFF,
//     .lock_state = 0xFFFFFFFF,
//     .rfu2 = 0xFFFFFFFF
// };

registration_data_t* registration_data __attribute__((section(".registration_section"))) = REGISTRATION_STRUCT_ADDR;

// uint32_t reg_data[4];
/* NDEF tag defined by user.
 * To activate this tag, set the field "tag_type_2_ptr" in aparams.
 */
const uint8_t smack_sl_tag[] =                    /**< [0x3a0:0x3ff] 96 Bytes Tag2 area          */
{
    0x05, 0xc0, 0xbe, 0xef,      /**< BLOCK0 UID0,1,2,3,4 */
    0xde, 0xad, 0x00, 0x00,      /**< BLOCK1 UID5,6,7,0x00 */
    0xff, 0xff, 0xff, 0xff,      /**< BLOCK2 Internal Lock  Byte0 and Byte1 are relevant 0xff means read only*/
    0xE1,                        /**< Capability Container CC_0 Magic Number fixed to 0xE1 for type 2 Tag */
    0x10,                        /**< Capability Container CC_1 Mapping version default 0x10 */
    0x0b,                        /**< Capability Container CC_2 size 22 Blocks --> 88 Bytes / 8 = 11d */
    0x0f,                        /**< Capability Container CC_3 Access Conditions , 0x0f  for read only tag w/o security*/
    0x03, 0x4f, 0xd1, 0x01,      /**< BLOCK4  NDEF, Length, MB + ME + Well known, ID length */
    0x4b, 0x54, 0x02,  'e',      /**< BLOCK5  Payload lenth, ID, language length, lang0 */
    'n',   'I',  'n',  'f',      /**< BLOCK6  lang1, payload .... */
    'i',   'n',  'e',  'o',      /**< BLOCK7  */
    'n',   ' ',  'T',  'e',      /**< BLOCK8  */
    'c',   'h',  'n',  'o',      /**< BLOCK9  */
    'l',   'o',  'g',  'i',      /**< BLOCK10 */
    'e',   's',  ' ',  'A',      /**< BLOCK11 */
    'G',   ' ',  ' ',  'N',      /**< BLOCK12 */
    'G',   'C',  '1',  '0',      /**< BLOCK13 */
    '8',   '0',  ' ',  ' ',      /**< BLOCK14 */
    'S',   'm',  'A',  'c',      /**< BLOCK15 */
    'K',   ' ',  ' ',  '0',      /**< BLOCK16 */
    '5',   'C',  '0',  'B',      /**< BLOCK17 */
    'E',   'E',  'F',  'D',      /**< BLOCK18 */
    'E',   'A',  'D',  '0',      /**< BLOCK19 */
    '0',   '0',  '0',  ' ',      /**< BLOCK20 */
    ' ',   'V',  '4',  '.',      /**< BLOCK21 */
    '0',   '.',  '1',  ' ',      /**< BLOCK22 */
    '(',   'N',  'V',  'M',      /**< BLOCK23 */
    ')',  0xfe, 0xff, 0xff,      /**< BLOCK24 */
    0xff, 0xff, 0xff, 0xff       /**< BLOCK25 not used */
};




// Globals:

// Offer a counter for external access
uint32_t sl_counter;
Power_State_enum_t current_state = POWER_POWER_OFF;
uint32_t turn_cycles = 0;


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

void _nvm_start(void);

// Example for application specific interrupt service routine
//    Function pointer to this routine must be registered in aparam.c file
//    The routine will serve the sys_tick interrupts and will increment a variable
//    which can be easily monitored by the attached debugger
static uint32_t example_counter = 0;
void example_handler(void)
{

    example_counter++;

}

// In case of a Hardfault, spin in a loop for a while before resetting so that a debugger may connect
void hardfault_handler(void)
{
    uint32_t cnt = 30000000;

    while (cnt--)
    {
        __NOP();
    }

}

uint16_t voltage_sweep = 0;
bool done_sweep = false;

void sweep_voltages(void)
{
    Mailbox_t* mbx = get_mailbox_address();
    if (shc_compare(shc_channel_ma, voltage_sweep*100.0F) == false)
    {
        mbx->content[6] = voltage_sweep;
        done_sweep = true;
    }
    else
    {
        voltage_sweep++;
    }
}

// Function to toggle lock
void toggle_lock(bool *hs1, bool *ls1, bool *hs2, bool *ls2, bool lock) {
    if (lock) {
        // Final state for lock == true: hs1=0, ls1=1, hs2=1, ls2=0
        if (*hs1) {
            *hs1 = false;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if(!*ls1){
            *ls1 = true;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if(!*hs2){
            if(*ls2){
                *ls2 = false;
                set_hb_switch(*hs1, *ls1, *hs2, *ls2);
            }
            *hs2 = true;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
        if(*ls2){
            *ls2 = false;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
    } else {
        // Final state for unlock == false: hs1=1, ls1=0, hs2=0, ls2=1
        if (!*hs1) {
            if(*ls1){
                *ls1 = false;
                set_hb_switch(*hs1, *ls1, *hs2, *ls2);
            }
            *hs1 = true;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        } if (*ls1) {
            *ls1 = false;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        } if (*hs2) {
            *hs2 = false;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        } if (!*ls2) {
            *ls2 = true;
            set_hb_switch(*hs1, *ls1, *hs2, *ls2);
        }
    }
}

void turn_motor(Mailbox_t* mbx, bool* hs1, bool* ls1, bool* hs2, bool* ls2, bool lock) {
    const uint32_t wait_time_discharge = WAIT_ABOUT_1MS * 64;
    const uint32_t wait_time_charge = WAIT_ABOUT_1MS;

    while (!shc_compare(shc_channel_ma, get_threshold_from_voltage(3.0)))
    {
        mbx->content[5] = 0x22222222;
    } 
    // this should power the motor
    toggle_lock(hs1, ls1, hs2, ls2, lock); // LOCK - 0110; UNLOCK - 1001
    sys_tim_singleshot_32(0, wait_time_discharge, 14);  // wait seems to be necessary
    // while (shc_compare(shc_channel_ma, get_threshold_from_voltage(2.5)))
    // {
    //     mbx->content[5] = 0x33333333;
    // }

    // open the circuit to charge the capacitor
    if(!lock) {
        *ls2 = false;  
    }
    else {
        *ls1 = false;
    }
    set_hb_switch(*hs1, *ls1, *hs2, *ls2);
    sys_tim_singleshot_32(0, wait_time_charge, 14);  // wait seems to be necessary
}

void *custom_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

bool write_registration_data(registration_data_t* new_data) {
    uint8_t status;

    nvm_config();

    status = nvm_open_assembly_buffer((uint32_t) registration_data);
    if (status != 0x00) {
        return false;
    }

    nvm_erase_page();

    memcpy(registration_data, new_data, sizeof(registration_data_t));

    status = nvm_program_page();
    if (status != 0x00) {
        nvm_abort_program();
        return false;
    }

    nvm_config();

    return true;
}

void run_power_state_machine(void)
{
    nvm_config();
    Mailbox_t* mbx = get_mailbox_address();
    bool hs1 = true;
    bool hs2 = false;
    bool ls1 = false;
    bool ls2 = false;

    // registration_data_t new_data = {1, PASSCODE, 0, 0};
    // write_registration_data(&new_data);

    while (true)
    {
        switch (current_state)
        {
            case POWER_POWER_OFF:
                mbx->content[1] = MCU_VALID;
                current_state = POWER_READY_FOR_PASSCODE;
                break;
            case POWER_READY_FOR_PASSCODE:
                if(mbx->content[2] == PASSCODE){
                    current_state = POWER_HARVESTING;
                    mbx->content[3] = PC_VAL;
                    set_hb_switch(hs1, ls1, hs2, ls2);
                }
                else if(mbx->content[2] == ZERO_32){
                    current_state = POWER_READY_FOR_PASSCODE;
                }
                else {
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
            {
                registration_data_t new_data;
                if(registration_data->lock_state == 0xFFFFFFFF){
                    // registration_data_t new_data = {1, PASSCODE, 0, 0};
                    new_data.registered = 1;
                    new_data.passcode = PASSCODE;
                    new_data.lock_state = 1;
                    new_data.rfu2 = 0;
                } else {
                    // registration_data_t new_data = {1, PASSCODE, ((uint32_t) !((bool)registration_data->lock_state)), 0};
                    new_data.registered = 1;
                    new_data.passcode = PASSCODE;
                    new_data.lock_state = !(registration_data->lock_state == 1);
                    new_data.rfu2 = 0;
                } 
                volatile bool s = write_registration_data(&new_data);
                volatile bool locked = (registration_data->lock_state == 1);

                for(;;)
                {
                    turn_motor(mbx, &hs1, &ls1, &hs2, &ls2, !locked);
                }
                mbx->content[3] = HARVESTING_DONE;
                current_state = POWER_IDLE;
            }
                break;
            case POWER_IDLE:
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

    // ignore comments below
    while (true)
    {
        /* States 1 and 5 are safe states, i.e. we should be able to loop infinitely in them */
        switch (current_state)
        {
            /* STATE 1:  Locked, Idle */
            // TODO: clear variable that indicates that it is verified
            // TODO: wait for NFC; how to check if we have an NFC signal? Should be an interrupt, check cl_uart_handler or hw_field_off_handler
            // TODO: save NFC data, propagate to next state 
            case LOCK_LOCKED:
                mbx = get_mailbox_address();
                mbx->content[1] = MCU_VALID;
                break;
            
            /* STATE 2 : Locked, Verifying */
            // maybe investigate dandeliion protocol, but this is extra
            // TODO: decrypt data, compare passcodes (for the time being, just do a straight comparison)
            // TODO: if incorrect passcode, return to state 1, else continue
            // TODO: set variable that indicates that it is verified 
            // TODO: if charging interrupt not received, move to state 3, if received, move to state 4
            case LOCK_UNLOCKING:
                break;


            /* STATE 3: Locked, Verified, Charging */
            // TODO: wait for charging of the capacitor to occur
            // TODO: will need to set up an interrupt (or something similar) to determine when the capacitor is charged 
            // TODO: should set a timer as a timeout in case charging does not happen, if timeout go to state 1? -> should determine action here
            // TODO: move to state 4 when charging interrupt received
            case LOCK_UNLOCKED:
                break;

            /* STATE 4: Unlocking */
            // TODO: send signal to H-bridge to move motor
            // TODO: move to state 5
            case LOCK_LOCKING:
                break;
            
            default:
                break;
        }

    }
}

uint16_t get_threshold_from_voltage(float input_voltage)
{
    return (uint16_t) (input_voltage*1000.00);
}

// Start of the application program
void _nvm_start(void)
{
    nfc_init(); 
    init_dand();
    vars_init();
    shc_init();
    switch_on_nvm();

    // uint16_t* nvm_mem_address = (uint16_t*)(0x00010800 + sizeof(uint16_t));
    // *nvm_mem_address = 2000;

    single_gpio_iocfg(true, false, true, false, false, 0);
    volatile NFC_State_enum_t state = handle_DAND_protocol();
    volatile NFC_Frame_enum_t frame_type = classify_frame();
    nfc_state_machine();

    set_hb_eventctrl(false);

    while (true)
    {
        read_frame();
        frame_type = classify_frame();

        run_power_state_machine();

        /* ****************** THIS CODE SHOULD NOT BE ALTERED FOR THE TIME BEING ******************** */
        asm("WFI");
    }

}

