#include "mouse.h"
#include "i8042.h"
#include "kbc.h"
#include "utils.h"
#include <lcom/lcf.h>
#include <stdint.h>

// hook_id começa com o valor da IRQ do rato (12)
int mouse_hook_id = 12;
uint8_t mouse_bytes[3]; // guarda os 3 bytes do pacote atual
struct packet mouse_packet;
uint8_t packet_byte_idx = 0; // índice do próximo byte a preencher no pacote
uint8_t mouse_current_byte;
bool mouse_error = false;

int(mouse_subscribe_int)(uint8_t *bit_no) {
  if (bit_no == NULL)
    return 1;
  // devolve o hook_id para usar como máscara nas interrupções
  *bit_no = mouse_hook_id;
  if (sys_irqsetpolicy(MOUSE_IRQ, IRQ_REENABLE | IRQ_EXCLUSIVE,
                       &mouse_hook_id) != 0)
    return 1;
  return 0;
}

int(mouse_unsubscribe_int)() {
  if (sys_irqrmpolicy(&mouse_hook_id) != 0)
    return 1;
  return 0;
}

// interrupt handler do rato - lê um byte da porta de dados do KBC
void(mouse_ih)() {
  uint8_t status;
  mouse_error = false;

  // lê o registo de status para saber se há dados disponíveis
  if (util_sys_inb(KBC_ST_REG, &status) != 0) {
    mouse_error = true;
    return;
  }

  if (status & KBC_OBF) {
    // KBC_AUX = 1 significa que o dado vem do rato, não do teclado
    if ((status & KBC_AUX) == 0) {
      mouse_error = true;
      return;
    }
    if (util_sys_inb(KBC_OUT_BUF, &mouse_current_byte) != 0) {
      mouse_error = true;
      return;
    }
    // verifica se houve erros de paridade ou timeout
    if ((status & (KBC_PARITY_ERR | KBC_TIMEOUT_ERR)) != 0) {
      mouse_error = true;
      return;
    }
  } else {
    mouse_error = true;
  }
}

// garante que os bytes são lidos na ordem certa (byte 0 primeiro)
void(mouse_sync_packet)() {
  // o byte 0 do pacote tem sempre o bit 3 a 1 (MOUSE_SYNC)
  // se não tiver, estamos fora de sincronismo e descartamos o byte
  if (packet_byte_idx == 0 && (mouse_current_byte & MOUSE_SYNC) == 0) {
    return;
  }

  mouse_bytes[packet_byte_idx] = mouse_current_byte;
  packet_byte_idx++;
}

// interpreta os 3 bytes do pacote e preenche a struct
void(mouse_parse_packet)() {
  mouse_packet.bytes[0] = mouse_bytes[0];
  mouse_packet.bytes[1] = mouse_bytes[1];
  mouse_packet.bytes[2] = mouse_bytes[2];

  // botões: cada bit do byte 0 corresponde a um botão
  mouse_packet.lb = mouse_bytes[0] & MOUSE_LB;
  mouse_packet.rb = mouse_bytes[0] & MOUSE_RB;
  mouse_packet.mb = mouse_bytes[0] & MOUSE_MB;

  // se o bit de sinal estiver a 1, o valor é negativo (complemento para 2 com 9 bits)
  mouse_packet.delta_x = (mouse_bytes[0] & MOUSE_X_SIGN)
                           ? (0xFF00 | mouse_bytes[1])
                           : mouse_bytes[1];

  mouse_packet.delta_y = (mouse_bytes[0] & MOUSE_Y_SIGN)
                           ? (0xFF00 | mouse_bytes[2])
                           : mouse_bytes[2];

  mouse_packet.x_ov = mouse_bytes[0] & MOUSE_X_OVF;
  mouse_packet.y_ov = mouse_bytes[0] & MOUSE_Y_OVF;
}

// junta todo o pipeline numa função só: lê, sincroniza, interpreta e imprime
// devolve true quando um pacote completo foi processado
bool(mouse_process_interrupt)() {
  mouse_ih();
  if (mouse_error)
    return false;

  mouse_sync_packet();
  if (packet_byte_idx != 3) // pacote ainda não está completo
    return false;

  mouse_parse_packet();
  mouse_print_packet(&mouse_packet);
  packet_byte_idx = 0; // reset para o próximo pacote
  return true;
}

// envia um comando ao rato via KBC (prefixo 0xD4 obrigatório)
int(mouse_write_cmd)(uint8_t cmd) {
  uint8_t ack_byte;
  int retries = 10;

  while (retries-- > 0) {
    // 0xD4 diz ao KBC para redirecionar o próximo byte para o rato
    if (kbc_issue_cmd(KBC_WRITE_MOUSE) != 0)
      return 1;
    if (kbc_write_data(cmd) != 0)
      return 1;
    // verifica o ACK: 0xFA = ok, 0xFE = tentar outra vez, 0xFC = erro
    if (kbc_read_data(&ack_byte) == 0) {
      if (ack_byte == MOUSE_ACK)
        return 0;
      if (ack_byte == MOUSE_ERR)
        return 1;
      // NACK -> retry
    }
  }

  return 1;
}
