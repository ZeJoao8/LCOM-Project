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

  // Activa o envio de pacotes do rato com a função da LCF
  if (mouse_enable_data_reporting() != 0)
    return 1;

  // Subscreve as interrupções do rato, guardando o número do bit ativado em bit_no
  if (mouse_subscribe_int(&bit_no) != 0)
    return 1;

  int ipc_status, r;
  message msg;
  // O irq_set transforma o número do bit ativado numa máscara de 32 bits
  uint32_t irq_set = BIT(bit_no);
  uint32_t packet_count = 0;

  while (packet_count < cnt) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0) {
      continue;
    }

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
      case HARDWARE:
        if (msg.m_notify.interrupts & irq_set) {
          // Lê a interrupção do rato (preenche mouse_current_byte)
          mouse_ih();
          if (!mouse_error) {
            // Sincroniza o array de entrada, descartando lixo se necessário
            mouse_sync_packet();
            // Quando a variável atinge 3 (1 pacote completo = 3 bytes)
            if (packet_byte_idx == 3) {
              mouse_parse_packet(); // Guarda os dados lidos na struct mouse_packet
              mouse_print_packet(&mouse_packet); // Imprime o pacote lido
              packet_byte_idx = 0; // Reinicia a variável para a próxima leitura
              packet_count++; // Incrementa os pacotes válidos processados
            }
          }
        }
        break;
      default:
        break;
      }
    }
  }

  if (mouse_unsubscribe_int() != 0)
    return 1;
  if (mouse_write_cmd(MOUSE_DISABLE_DATA_REP) != 0)
    return 1;

  return 0;
}

int(mouse_test_async)(uint8_t idle_time) {
  uint8_t mouse_bit_no, timer_bit_no;

  if (mouse_write_cmd(MOUSE_ENABLE_DATA_REP) != 0)
    return 1;

  if (mouse_subscribe_int(&mouse_bit_no) != 0)
    return 1;
  if (timer_subscribe_int(&timer_bit_no) != 0)
    return 1;

  int ipc_status, r;
  message msg;
  // Cria as máscaras de bits necessárias à interceção destas interrupções em específico
  uint32_t mouse_set = BIT(mouse_bit_no);
  uint32_t timer_set = BIT(timer_bit_no);
  uint32_t freq = sys_hz();

  timer_counter = 0;

  // Só encerra o loop de leitura se atingir o "timeout" pretendido (x segundos em idle)
  while ((uint32_t)timer_counter < idle_time * freq) {
    if ((r = driver_receive(ANY, &msg, &ipc_status)) != 0)
      continue;

    if (is_ipc_notify(ipc_status)) {
      switch (_ENDPOINT_P(msg.m_source)) {
      case HARDWARE:
        // Caso venha uma interrupção do timer
        if (msg.m_notify.interrupts & timer_set) {
          timer_int_handler(); // Aumenta o timer_counter
        }
        // Caso venha uma interrupção do rato
        if (msg.m_notify.interrupts & mouse_set) {
          mouse_ih();
          if (!mouse_error) {
            mouse_sync_packet();
            if (packet_byte_idx == 3) {
              mouse_parse_packet();
              mouse_print_packet(&mouse_packet);
              packet_byte_idx = 0;
              timer_counter = 0; // Detetou pack do rato -> reseta o tempo à espera
            }
          }
        }
        break;
      default:
        break;
      }
    }
  }

  if (timer_unsubscribe_int() != 0)
    return 1;
  if (mouse_unsubscribe_int() != 0)
    return 1;
  if (mouse_write_cmd(MOUSE_DISABLE_DATA_REP) != 0)
    return 1;

  return 0;
}
