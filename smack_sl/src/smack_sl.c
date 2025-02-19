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

#define NVM_ADDR_REGISTER 0x0001EE00  // second last page in the NVM, avoiding overlap with any firmware

typedef struct registration_data
{
    bool registered;
} registration_data_t ;

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
        // Final state for lock == false: hs1=1, ls1=0, hs2=0, ls2=1
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

void run_power_state_machine(void)
{
    bool authenticated = false; 
    Mailbox_t* mbx = get_mailbox_address();
    bool hs1 = true;
    bool hs2 = false;
    bool ls1 = false;
    bool ls2 = false;

    if (!nvm_open_assembly_buffer(NVM_ADDR_REGISTER) == 0) {}
    uint32_t* nvm_data = (uint32_t*) NVM_ADDR_REGISTER;
    uint32_t num = *nvm_data;
    bool registered = (num == 1);   // read from NVM
    volatile access_state_t access_state = get_nvm_access_state(NVM_ADDR_REGISTER);

    while (true)
    {
        switch (current_state)
        {
            case POWER_POWER_OFF:
                mbx->content[1] = MCU_VALID;
                if(registered)
                    current_state = POWER_READY_FOR_PASSCODE;
                else {
                    current_state = POWER_NOT_REGISTERED;
                }
                break;
            case POWER_NOT_REGISTERED:
                /* registration logic here */
                if(mbx->content[2] == PASSCODE){
                    
                    *nvm_data = 0x00000001;
                    nvm_program_page();
                    registered = true;
                    current_state = POWER_READY_FOR_PASSCODE;
                }
                break;
            case POWER_READY_FOR_PASSCODE:
                if(mbx->content[2] == PASSCODE){
                    authenticated = true;
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
                // sweep_voltages();
                // for (uint8_t i = 0; i < 10; i++)
                for(;;)
                {
                    while (!shc_compare(shc_channel_ma, get_threshold_from_voltage(3.0)))
                    {
                        mbx->content[5] = 0x22222222;
                    } 
                    // this should power the motor
                    toggle_lock(&hs1, &ls1, &hs2, &ls2, false); // hs1, ls1, hs2, ls2   --- THIS IS UNLOCK
                    sys_tim_singleshot_32(0, WAIT_ABOUT_1MS * 1024, 14);  // wait seems to be necessary
                    while (shc_compare(shc_channel_ma, get_threshold_from_voltage(2.5)))
                    {
                        mbx->content[5] = 0x33333333;
                    }

                    ls2 = false;
                    set_hb_switch(hs1, ls1, hs2, ls2);
                    // while (!shc_compare(shc_channel_ma, get_threshold_from_voltage(3.0))) {} 
                    // this should power the motor
                    // toggle_lock(&hs1, &ls1, &hs2, &ls2, false);
                    sys_tim_singleshot_32(0, WAIT_ABOUT_1MS * 511, 14);  // wait seems to be necessary
                    // while (shc_compare(shc_channel_ma, get_threshold_from_voltage(2.5))) {}
                }
                mbx->content[3] = HARVESTING_DONE;
                current_state = POWER_IDLE;
                break;
            case POWER_IDLE:
                switch_off_nvm();
                request_power_saving_mode(true, false, false, false);
                break;
            default:
                switch_off_nvm();
                request_power_saving_mode(true, false, false, false);
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
    nvm_config();

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

