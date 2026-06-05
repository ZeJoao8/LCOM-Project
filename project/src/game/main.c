/**
 * @file main.c
 * @brief Program entry point and the main interrupt-driven event loop. Sets the
 *        video mode, initialises the game, subscribes all hardware interrupts
 *        (timer, keyboard, mouse, RTC, serial), dispatches each interrupt to the
 *        game, and tears everything down on exit.
 */
#include <lcom/lcf.h>
#include <stdint.h>
#include <stdio.h>

#include <lcom/timer.h>
#include "devices/kbc/kbc.h"
#include "devices/mouse/mouse.h"
#include "devices/video_gr/video_gr.h"
#include "devices/kbc/i8042.h"
#include "devices/timer/i8254.h"
#include "devices/rtc/rtc.h"
#include "devices/serial/serial.h"

#include "game/game.h"

/* Driver globals filled by the interrupt handlers. */
extern uint8_t scancode;
extern uint8_t packet_byte_idx;
extern bool mouse_error;
extern struct packet mouse_packet;

int main(int argc, char *argv[]) {
    lcf_set_language("EN-US");
    if (lcf_start(argc, argv)) return 1;
    lcf_cleanup();
    return 0;
}

/**
 * @brief Main game loop. Initialises hardware and game state, then services
 *        hardware interrupts until the game reaches STATE_EXIT, dispatching each
 *        IRQ to the appropriate driver handler and game callback. Cleans up all
 *        subscriptions and the video mode before returning.
 * @return 0 on a clean exit, 1 on any initialisation/teardown failure.
 */
int (proj_main_loop)(int argc, char *argv[]) {
    uint8_t timer_bit_no, kbd_bit_no, mouse_bit_no, rtc_bit_no, serial_bit_no;

    /* 1. HARDWARE INITIALISATION */

    /* Set the video mode (800x600x24 direct colour). */
    if (vg_init(0x115) == NULL) {
        printf("Error: Failed to initialize video!\n");
        return 1;
    }

    /* Initialise game state. */
    game_init();

    /* Enable mouse data reporting BEFORE subscribing its interrupt. */
    if (mouse_write_cmd(MOUSE_ENABLE_DATA_REP) != 0) return 1;

    /* Subscribe every interrupt. */
    if (timer_subscribe_int(&timer_bit_no) != 0) return 1;
    if (kbd_subscribe_int(&kbd_bit_no) != 0) return 1;
    if (mouse_subscribe_int(&mouse_bit_no) != 0) return 1;
    if (rtc_subscribe_int(&rtc_bit_no) != 0) return 1;
    if (serial_subscribe_int(&serial_bit_no) != 0) return 1;

    /* RTC: enable the update-ended interrupt (1 Hz clock refresh). */
    if (rtc_enable_update_int() != 0) return 1;

    /* Serial port (COM1) for 2-player mode. */
    if (serial_config() != 0) return 1;

    /* Build the IRQ bit masks. */
    uint32_t timer_irq = BIT(timer_bit_no);
    uint32_t kbd_irq = BIT(kbd_bit_no);
    uint32_t mouse_irq = BIT(mouse_bit_no);
    uint32_t rtc_irq = BIT(rtc_bit_no);
    uint32_t serial_irq = BIT(serial_bit_no);

    int ipc_status;
    message msg;

    /* 2. MAIN GAME LOOP */
    while (game_get_state() != STATE_EXIT) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0) continue;

        if (is_ipc_notify(ipc_status) && _ENDPOINT_P(msg.m_source) == HARDWARE) {
            
            // --- TIMER EVENTS (60fps game tick) ---
            if (msg.m_notify.interrupts & timer_irq) {
                timer_int_handler();
                game_update();
                game_draw();
            }

            // --- KEYBOARD EVENTS ---
            if (msg.m_notify.interrupts & kbd_irq) {
                kbc_ih();
                game_handle_kbd(scancode);
            }

            // --- MOUSE EVENTS ---
            if (msg.m_notify.interrupts & mouse_irq) {
                mouse_ih();

                if (!mouse_error) {
                    mouse_sync_packet();

                    if (packet_byte_idx == 3) {
                        mouse_parse_packet();
                        packet_byte_idx = 0;
                        game_handle_mouse(
                            mouse_packet.delta_x,
                            mouse_packet.delta_y,
                            mouse_packet.lb
                        );
                    }
                }
            }

            // --- RTC EVENTS (1 Hz update-ended -> refresh clock) ---
            if (msg.m_notify.interrupts & rtc_irq) {
                rtc_ih();
                game_on_rtc();
            }

            // --- SERIAL EVENTS (2-player link) ---
            if (msg.m_notify.interrupts & serial_irq) {
                serial_ih();
            }
        }
    }

    /* 3. HARDWARE TEARDOWN & CLEANUP */


    if (serial_unsubscribe_int() != 0) return 1;
    rtc_disable_update_int();
    if (rtc_unsubscribe_int() != 0) return 1;
    if (mouse_unsubscribe_int() != 0) return 1;
    if (kbd_unsubscribe_int() != 0) return 1;
    if (timer_unsubscribe_int() != 0) return 1;

    game_cleanup();

    if (mouse_write_cmd(MOUSE_DISABLE_DATA_REP) != 0) return 1;
    
    if (vg_exit() != 0) return 1;

    return 0;
}
