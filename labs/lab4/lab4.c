// IMPORTANT: you must include the following line in all your C files
#include <lcom/lcf.h>

#include <stdint.h>
#include <stdio.h>

// Any header files included below this line should have been created by you
#include "i8042.h"
#include "mouse.h"

extern int timer_counter;

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

int(mouse_test_packet)(uint32_t cnt) {
  uint8_t bit_no;

  // ativa o envio de dados do rato e subscreve as interrupções
  if (mouse_enable_data_reporting() != 0)
    return 1;

  if (mouse_subscribe_int(&bit_no) != 0)
    return 1;

  int ipc_status, r;
  message msg;
  uint32_t irq_set = BIT(bit_no); // mascara de bits para identificar a IRQ do rato
  uint32_t packet_count = 0;

  // loop principal - só sai quando receber cnt pacotes completos
  while (packet_count < cnt) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0)
      continue;

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
      case HARDWARE:
        // verifica se a interrupção é mesmo do rato
        if (msg.m_notify.interrupts & irq_set) {
          // processa a interrupção - retorna true quando um pacote fica completo
          if (mouse_process_interrupt())
            packet_count++;
        }
        break;
      default:
        break;
      }
    }
  }

  // cancela a subscrição e desativa o envio de dados
  if (mouse_unsubscribe_int() != 0)
    return 1;
  if (mouse_write_cmd(MOUSE_DISABLE_DATA_REP) != 0)
    return 1;

  return 0;
}

int(mouse_test_async)(uint8_t idle_time) {
  uint8_t mouse_bit_no, timer_bit_no;

  // ativa o rato e subscreve tanto o rato como o timer
  if (mouse_write_cmd(MOUSE_ENABLE_DATA_REP) != 0)
    return 1;

  if (mouse_subscribe_int(&mouse_bit_no) != 0)
    return 1;
  if (timer_subscribe_int(&timer_bit_no) != 0)
    return 1;

  int ipc_status, r;
  message msg;
  uint32_t mouse_set = BIT(mouse_bit_no);
  uint32_t timer_set = BIT(timer_bit_no);
  uint32_t freq = sys_hz(); // frequência do timer (normalmente 60 Hz)

  timer_counter = 0;

  // sai do loop quando o rato estiver idle durante idle_time segundos
  while ((uint32_t)timer_counter < idle_time * freq) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0)
      continue;

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
      case HARDWARE:
        // interrupção do timer - incrementa o contador de tempo
        if (msg.m_notify.interrupts & timer_set) {
          timer_int_handler();
        }
        // interrupção do rato - se receber pacote, reseta o tempo de idle
        if (msg.m_notify.interrupts & mouse_set) {
          if (mouse_process_interrupt())
            timer_counter = 0;
        }
        break;
      default:
        break;
      }
    }
  }

  // limpeza: cancela subscrições e desativa o rato
  if (timer_unsubscribe_int() != 0)
    return 1;
  if (mouse_unsubscribe_int() != 0)
    return 1;
  if (mouse_write_cmd(MOUSE_DISABLE_DATA_REP) != 0)
    return 1;

  return 0;
}
