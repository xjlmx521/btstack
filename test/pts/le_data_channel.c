/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

// *****************************************************************************
/* EXAMPLE_START(ble_peripheral): LE Data Channel Test
 *
 */
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack_config.h"

#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "stdin_support.h"
 
 static void show_usage(void);

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static bd_addr_t pts_address = { 0x00, 0x02, 0x72, 0xDC, 0x31, 0xC1};
static int pts_address_type = 0;
static bd_addr_t master_address = { 0x00, 0x02, 0x72, 0xDC, 0x31, 0xC1};
static int master_addr_type = 0;
static hci_con_handle_t handle;
static uint32_t ui_passkey;
static int  ui_digits_for_passkey;

// general discoverable flags
static uint8_t adv_general_discoverable[] = { 2, 01, 02 };

const uint16_t psm_x = 0xf0;

uint8_t receive_buffer_X[100];

static void gap_run(void){
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_address;
    uint16_t psm;
    uint16_t local_cid;

    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        show_usage();
                    }
                    break;
                
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            handle = little_endian_read_16(packet, 4);
                            printf("Connection handle 0x%04x\n", handle);
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    break;
                    
                case L2CAP_EVENT_INCOMING_CONNECTION: {
                    uint16_t l2cap_cid  = little_endian_read_16(packet, 12);
#if 1
                    printf("L2CAP Accepting incoming connection request\n"); 
                    l2cap_le_accept_connection(l2cap_cid, receive_buffer_X, sizeof(receive_buffer_X), 1);
#else
                    printf("L2CAP Decline incoming connection request\n"); 
                    l2cap_le_decline_connection(l2cap_cid);
#endif
                    break;
                }

                case L2CAP_EVENT_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    reverse_bd_addr(&packet[3], event_address);
                    psm = little_endian_read_16(packet, 11); 
                    local_cid = little_endian_read_16(packet, 13); 
                    handle = little_endian_read_16(packet, 9);
                    if (packet[2] == 0) {
                        printf("Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                               bd_addr_to_str(event_address), handle, psm, local_cid,  little_endian_read_16(packet, 15));
                    } else {
                        printf("L2CAP connection to device %s failed. status code %u\n", bd_addr_to_str(event_address), packet[2]);
                    }
                    break;

                case SM_EVENT_JUST_WORKS_REQUEST:
                    printf("SM_EVENT_JUST_WORKS_REQUEST\n");
                    sm_just_works_confirm(little_endian_read_16(packet, 2));
                    break;

                case SM_EVENT_PASSKEY_INPUT_NUMBER:
                    // display number
                    master_addr_type = packet[4];
                    reverse_bd_addr(&packet[5], event_address);
                    printf("\nGAP Bonding %s (%u): Enter 6 digit passkey: '", bd_addr_to_str(master_address), master_addr_type);
                    fflush(stdout);
                    ui_passkey = 0;
                    ui_digits_for_passkey = 6;
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
                    // display number
                    printf("\nGAP Bonding %s (%u): Display Passkey '%06u\n", bd_addr_to_str(master_address), master_addr_type, little_endian_read_32(packet, 11));
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_CANCEL: 
                    printf("\nGAP Bonding %s (%u): Display cancel\n", bd_addr_to_str(master_address), master_addr_type);
                    break;

                case SM_EVENT_AUTHORIZATION_REQUEST:
                    // auto-authorize connection if requested
                    sm_authorization_grant(little_endian_read_16(packet, 2));
                    break;

                default:
                    break;
            }
    }
    gap_run();
}

void show_usage(void){
    bd_addr_t iut_address;
    uint8_t uit_addr_type;

    // gap_local_bd_addr(iut_address);
    gap_advertisements_get_address(&uit_addr_type, iut_address);

    printf("\n--- CLI for LE Data Channel %s ---\n", bd_addr_to_str(iut_address));
    printf("a - connect to type %u address %s PSM 0x%02x\n", pts_address_type, bd_addr_to_str(pts_address), psm_x);
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}
static uint8_t buffer_x[1000];
static uint16_t cid_x;

static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    char buffer;
    read(ds->fd, &buffer, 1);

    // passkey input
    if (ui_digits_for_passkey){
        if (buffer < '0' || buffer > '9') return;
        printf("%c", buffer);
        fflush(stdout);
        ui_passkey = ui_passkey * 10 + buffer - '0';
        ui_digits_for_passkey--;
        if (ui_digits_for_passkey == 0){
            printf("\nSending Passkey '%06x'\n", ui_passkey);
            sm_passkey_input(handle, ui_passkey);
        }
        return;
    }

    switch (buffer){
        case 'a':
            printf("Creating connection to %s\n", bd_addr_to_str(pts_address));
            gap_advertisements_enable(0);
            l2cap_le_create_channel(&app_packet_handler,pts_address, pts_address_type, psm_x, buffer_x, sizeof(buffer_x), 1, LEVEL_0, &cid_x);
            break;

        default:
            show_usage();
            break;

    }
    return;
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    
    printf("BTstack LE Data Channel test starting up...\n");

    // register for HCI events
    hci_event_callback_registration.callback = &app_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // set up l2cap_le
    l2cap_init();
    
    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION); 
    sm_event_callback_registration.callback = &app_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    btstack_stdin_setup(stdin_process);

    // gap_random_address_set_update_period(5000);
    // gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);
    gap_advertisements_set_data(sizeof(adv_general_discoverable), adv_general_discoverable);
    gap_advertisements_enable(1);
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(0);

    // le data channel setup
    l2cap_le_register_service(&app_packet_handler, psm_x, LEVEL_0);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}

/* EXAMPLE_END */