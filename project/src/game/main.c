// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>
#include <stdint.h>
#include <stdio.h>

// Include your device headers
#include <lcom/timer.h>
#include "devices/kbc.h"
#include "devices/mouse.h"
#include "devices/video_gr.h"
#include "devices/i8042.h"
#include "devices/i8254.h"

// External variables from your drivers
extern uint8_t scancode;
extern uint8_t packet_byte_idx;
extern bool mouse_error;
// Assuming you have this defined in your mouse.h/.c
extern struct packet mouse_packet; 

// --- GAME STATES ---
typedef enum {
    MAIN_MENU,
    PLAYING,
    GAME_OVER,
    EXIT_GAME
} game_state_t;

// Global state variable
game_state_t current_state = MAIN_MENU;

int main(int argc, char *argv[]) {
    // Sets the language of LCF messages
    lcf_set_language("EN-US");
    // Hands control over to LCF
    if (lcf_start(argc, argv)) return 1;
    // LCF clean up tasks
    lcf_cleanup();
    return 0;
}

int (proj_main_loop)(int argc, char *argv[]) {
    uint8_t timer_bit_no, kbd_bit_no, mouse_bit_no;

    // =======================================================
    // 1. HARDWARE INITIALIZATION
    // =======================================================
    
    // Start Video Mode (e.g., 800x600 Direct Color)
    if (vg_init(0x115) == NULL) {
        printf("Error: Failed to initialize video!\n");
        return 1;
    }
    
    // Enable mouse data reporting BEFORE subscribing
    if (mouse_write_cmd(MOUSE_ENABLE_DATA_REP) != 0) return 1;

    // Subscribe to all interrupts
    if (timer_subscribe_int(&timer_bit_no) != 0) return 1;
    if (kbd_subscribe_int(&kbd_bit_no) != 0) return 1;
    if (mouse_subscribe_int(&mouse_bit_no) != 0) return 1;

    // Create IRQ masks
    uint32_t timer_irq = BIT(timer_bit_no);
    uint32_t kbd_irq = BIT(kbd_bit_no);
    uint32_t mouse_irq = BIT(mouse_bit_no);

    int ipc_status;
    message msg;

    // =======================================================
    // 2. MAIN GAME LOOP
    // =======================================================
    while (current_state != EXIT_GAME) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0) continue;

        if (is_ipc_notify(ipc_status) && _ENDPOINT_P(msg.m_source) == HARDWARE) {
            
            // --- TIMER EVENTS (Frame Generation / Logic Update) ---
            if (msg.m_notify.interrupts & timer_irq) {
                timer_int_handler();
                
                // Route the logic based on the current state
                switch (current_state) {
                    case MAIN_MENU:
                        // update_menu_logic();
                        // draw_menu();
                        break;
                    case PLAYING:
                        // update_game_physics();
                        // draw_gameplay();
                        break;
                    default: 
                        break;
                }
                
                // TODO: Implement Double Buffering copy here
                // copy_buffer_to_vram(); 
            }

            // --- KEYBOARD EVENTS (User Input) ---
            if (msg.m_notify.interrupts & kbd_irq) {
                kbc_ih();
                
                // Universal exit condition (ESC Key)
                if (scancode == ESC_BREAKCODE) {
                    current_state = EXIT_GAME; 
                } 
                else {
                    // TODO: Pass 'scancode' to your game logic
                    // handle_keyboard_input(scancode);
                }
            }

            // --- MOUSE EVENTS (User Input) ---
            if (msg.m_notify.interrupts & mouse_irq) {
                mouse_ih();
                
                if (!mouse_error) {
                    mouse_sync_packet();
                    
                    if (packet_byte_idx == 3) {
                        // Packet is complete
                        mouse_parse_packet();
                        packet_byte_idx = 0;
                        
                        // TODO: Update cursor coordinates based on mouse_packet
                        // handle_mouse_input(&mouse_packet);
                    }
                }
            }
        }
    }

    // =======================================================
    // 3. HARDWARE TEARDOWN & CLEANUP
    // =======================================================
    
    // Unsubscribe from all interrupts
    if (mouse_unsubscribe_int() != 0) return 1;
    if (kbd_unsubscribe_int() != 0) return 1;
    if (timer_unsubscribe_int() != 0) return 1;
    
    // Disable mouse data reporting
    if (mouse_write_cmd(MOUSE_DISABLE_DATA_REP) != 0) return 1;
    
    // CRITICAL: Always exit video mode to avoid a locked Minix terminal
    if (vg_exit() != 0) return 1;

    return 0;
}
