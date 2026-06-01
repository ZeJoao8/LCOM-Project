#include <lcom/lcf.h>
#include <stdint.h>
#include <stdio.h>

#include <lcom/timer.h>
#include "devices/kbc.h"
#include "devices/mouse.h"
#include "devices/video_gr.h"
#include "devices/i8042.h"
#include "devices/i8254.h"

#include "game/game.h"

/* Variáveis externas dos drivers */
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

int (proj_main_loop)(int argc, char *argv[]) {
    uint8_t timer_bit_no, kbd_bit_no, mouse_bit_no;

    /* 1. INICIALIZAÇÃO DO HARDWARE */
    
    /* Iniciar modo vídeo (800x600x24 Direct Color) */
    if (vg_init(0x115) == NULL) {
        printf("Error: Failed to initialize video!\n");
        return 1;
    }
    
    /* Inicializar estado do jogo */
    game_init();

    /* Ativar envio de dados do rato ANTES de subscrever */
    if (mouse_write_cmd(MOUSE_ENABLE_DATA_REP) != 0) return 1;

    /* Subscrever todas as interrupções */
    if (timer_subscribe_int(&timer_bit_no) != 0) return 1;
    if (kbd_subscribe_int(&kbd_bit_no) != 0) return 1;
    if (mouse_subscribe_int(&mouse_bit_no) != 0) return 1;

    /* Criar máscaras IRQ */
    uint32_t timer_irq = BIT(timer_bit_no);
    uint32_t kbd_irq = BIT(kbd_bit_no);
    uint32_t mouse_irq = BIT(mouse_bit_no);

    int ipc_status;
    message msg;

    /* 2. LOOP PRINCIPAL DO JOGO */
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
                    }
                }
            }
        }
    }

    // =======================================================
    // 3. HARDWARE TEARDOWN & CLEANUP
    // =======================================================
    
    if (mouse_unsubscribe_int() != 0) return 1;
    if (kbd_unsubscribe_int() != 0) return 1;
    if (timer_unsubscribe_int() != 0) return 1;
    
    game_cleanup();

    if (mouse_write_cmd(MOUSE_DISABLE_DATA_REP) != 0) return 1;
    
    if (vg_exit() != 0) return 1;

    return 0;
}
