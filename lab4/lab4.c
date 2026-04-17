// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>

#include <stdint.h>
#include <stdio.h>

// Any header files included below this line should have been created by you
#include "mouse.h"
#include "i8042.h"

// External variables from your other modules
extern uint8_t mouse_byte;
extern bool mouse_error;
extern int timer_counter; // Needs to be exposed from your timer.c (Lab 2)

int main(int argc, char *argv[]) {
  // sets the language of LCF messages (can be either EN-US or PT-PT)
  lcf_set_language("EN-US");

  // enables to log function invocations that are being "wrapped" by LCF
  // [comment this out if you don't want/need/ it]
  lcf_trace_calls("/home/lcom/labs/lab4/trace.txt");

  // enables to save the output of printf function calls on a file
  // [comment this out if you don't want/need it]
  lcf_log_output("/home/lcom/labs/lab4/output.txt");

  // handles control over to LCF
  // [LCF handles command line arguments and invokes the right function]
  if (lcf_start(argc, argv))
    return 1;

  // LCF clean up tasks
  // [must be the last statement before return]
  lcf_cleanup();

  return 0;
}

int (mouse_test_packet)(uint32_t cnt) {
    int ipc_status;
    message msg;
    uint8_t bit_no;
    
    uint8_t packet_bytes[3];
    int byte_index = 0;
    uint32_t packets_read = 0;
    struct packet pp;

    // 1. Enable data reporting (using the framework's function, as instructed)
    if (mouse_enable_data_reporting() != 0) return 1;

    // 2. Subscribe to mouse interrupts
    if (mouse_subscribe_int(&bit_no) != 0) return 1;
    uint32_t irq_set = BIT(bit_no);

    // 3. Main loop (runs until we collect 'cnt' complete packets)
    while (packets_read < cnt) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0) continue;
        
        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
                    if (msg.m_notify.interrupts & irq_set) {
                        
                        mouse_ih(); // Read the byte from the hardware
                        if (mouse_error) continue; // Ignore corrupted bytes

                        // Synchronization: Bit 3 of the 1st byte must ALWAYS be 1
                        if (byte_index == 0 && (mouse_byte & BIT(3)) == 0) {
                            continue; // Out of sync. Ignore this byte and wait for a valid start.
                        }

                        // Store the byte and advance the index
                        packet_bytes[byte_index] = mouse_byte;
                        byte_index++;

                        // If we have collected all 3 bytes, the packet is ready
                        if (byte_index == 3) { 
                            mouse_parse_packet(packet_bytes, &pp);
                            mouse_print_packet(&pp);
                            
                            byte_index = 0; // Reset index for the next packet
                            packets_read++; // Increment the completed packet counter
                        }
                    }
                    break;
            }
        }
    }

    // 4. Cleanup
    if (mouse_unsubscribe_int() != 0) return 1;
    
    // The lab guide strictly requires YOU to manually disable data reporting at the end
    if (mouse_write_cmd(DIS_DATA_REP) != 0) return 1;

    return 0;
}

int (mouse_test_async)(uint8_t idle_time) {
    int ipc_status;
    message msg;
    uint8_t mouse_bit_no, timer_bit_no;
    
    uint8_t packet_bytes[3];
    int byte_index = 0;
    struct packet pp;

    // Get the current system frequency to calculate seconds accurately (usually 60Hz)
    uint32_t freq = sys_hz(); 

    // 1. Manually enable data reporting (we cannot use mouse_enable_data_reporting here)
    if (mouse_write_cmd(EN_DATA_REP) != 0) return 1;

    // 2. Subscribe to BOTH Mouse and Timer 0 interrupts
    if (mouse_subscribe_int(&mouse_bit_no) != 0) return 1;
    // Note: Ensure your timer library from Lab 2 is linked in the Makefile
    if (timer_subscribe_int(&timer_bit_no) != 0) return 1; 

    uint32_t mouse_irq_set = BIT(mouse_bit_no);
    uint32_t timer_irq_set = BIT(timer_bit_no);
    
    timer_counter = 0; // Reset the timer counter

    // 3. Loop until inactivity time reaches the requested idle_time
    // (timer_counter / freq) gives us the elapsed time in seconds
    while ((timer_counter / freq) < idle_time) {
        if (driver_receive(ANY, &msg, &ipc_status) != 0) continue;
        
        if (is_ipc_notify(ipc_status)) {
            switch (_ENDPOINT_P(msg.m_source)) {
                case HARDWARE:
                    
                    // --- MOUSE INTERRUPT HANDLING ---
                    if (msg.m_notify.interrupts & mouse_irq_set) {
                        mouse_ih();
                        if (mouse_error) continue;

                        // IMPORTANT: We received user input, so reset the idle timer!
                        timer_counter = 0;

                        // Same synchronization logic as before
                        if (byte_index == 0 && (mouse_byte & BIT(3)) == 0) {
                            continue;
                        }

                        packet_bytes[byte_index] = mouse_byte;
                        byte_index++;

                        if (byte_index == 3) {
                            mouse_parse_packet(packet_bytes, &pp);
                            mouse_print_packet(&pp);
                            byte_index = 0;
                        }
                    }

                    // --- TIMER INTERRUPT HANDLING ---
                    if (msg.m_notify.interrupts & timer_irq_set) {
                        timer_int_handler(); // Increments timer_counter
                    }
                    
                    break;
            }
        }
    }

    // 4. Cleanup everything
    if (mouse_write_cmd(DIS_DATA_REP) != 0) return 1;
    if (mouse_unsubscribe_int() != 0) return 1;
    // Manually disable data reporting
    if (mouse_write_cmd(DIS_DATA_REP) != 0) return 1;

    return 0;
}
