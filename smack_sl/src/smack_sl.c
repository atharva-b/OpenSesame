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


#ifndef wait_about_1ms
#define WAIT_ABOUT_1MS   0x8000   //!< clock tick constant ~1ms @ 28MHz
#endif


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

void open_sesame_state_machine(void)
{
    bool authenticated = false; 
    SM_State_enum_t current_state = SM_LOCKED;     // how do we know if it is locked or unlocked here....

    while (true)
    {
        /* States 1 and 5 are safe states, i.e. we should be able to loop infinitely in them */
        switch (current_state)
        {
            /* STATE 1:  Locked, Idle */
            // TODO: clear variable that indicates that it is verified
            // TODO: wait for NFC; how to check if we have an NFC signal? Should be an interrupt, check cl_uart_handler or hw_field_off_handler
            // TODO: save NFC data, propagate to next state 
            case SM_LOCKED:
                authenticated = false;
                break;
            
            /* STATE 2 : Locked, Verifying */
            // maybe investigate dandeliion protocol, but this is extra
            // TODO: decrypt data, compare passcodes (for the time being, just do a straight comparison)
            // TODO: if incorrect passcode, return to state 1, else continue
            // TODO: set variable that indicates that it is verified 
            // TODO: if charging interrupt not received, move to state 3, if received, move to state 4
            case SM_AUTHENTICATE_UL:
                break;


            /* STATE 3: Locked, Verified, Charging */
            // TODO: wait for charging of the capacitor to occur
            // TODO: will need to set up an interrupt (or something similar) to determine when the capacitor is charged 
            // TODO: should set a timer as a timeout in case charging does not happen, if timeout go to state 1? -> should determine action here
            // TODO: move to state 4 when charging interrupt received
            case SM_CHARGING_UL:
                break;

            /* STATE 4: Unlocking */
            // TODO: send signal to H-bridge to move motor
            // TODO: move to state 5
            case SM_UNLOCKING:
                break;

            /* STATE 5: Unlocked, Idle */
            // TODO: clear variable that indicates that it is verified
            // TODO: similar to state 1
            // TODO: move to state 6
            case SM_UNLOCKED:
                break;

            /* STATE 6: Unlocked, Verifying */
            // TODO: similar to state 2
            // TODO: if incorrect passcode, return to state 5, else continue
            // TODO: move to state 7 if no charging interrupt, if received move to state 8
            case SM_AUTHENTICATE_L:
                break;

            /* STATE 7: Unlocked, Verifying, Charging */
            // TODO: similar to state 3
            // TODO: move to state 8 when charging interrupt received
            case SM_CHARGING_L:
                break;
            
            /* STATE 8: Locking */
            // TODO: similar to state 4, but with opposite polarity so that the motor will move to a locked position
            // TODO: move to state 1
            case SM_LOCKING:
                break;
            
            default:
                break;
        }

    }
}

void led_blink(void) {
    single_gpio_iocfg(true, false, true, false, false, 0);
    for(int i = 0; i < 5; i++) {
        set_singlegpio_out(0x1, 0);
        sys_tim_singleshot_32(0, WAIT_ABOUT_1MS * 3000, 14);
        set_singlegpio_out(0x0, 0);
    }
}

// Start of the application program
void _nvm_start(void)
{
    nfc_init(); 
    init_dand();
    /* ****************** THIS CODE SHOULD NOT BE ALTERED FOR THE TIME BEING ******************** */
    while (true)
    {
        nfc_state_machine();
        /*
        // Uncomment these lines if you want to output data on pin 1
        SCUS_GPIO_OUT_EN__SET(1);
        SCUC_GPIO_OUT_DAT__SET(0x1 & BIT_TO_PUT_ON_PIN);
        */
        asm("WFI"); 
    }

}

