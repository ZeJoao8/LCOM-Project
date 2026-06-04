/**
 * @file game.c
 * @brief Composition root / state-machine glue for the Jetpack-Joyride game.
 *        Owns the single GameModel instance and wires the public API used by
 *        main.c to the model (logic), view (rendering) and controller (input).
 */
#include <stdio.h>
#include <string.h>
#include "game/game.h"
#include "game/entities.h"
#include "game/model.h"
#include "game/view.h"
#include "game/controller.h"
#include "game/net.h"
#include "game/scores.h"
#include "devices/video_gr/video_gr.h"   /* h_res, v_res */
#include "devices/rtc/rtc.h"

/* The whole game state lives here. */
static GameModel M;


int game_authenticate_user(const char *name, const char *password) {
    /* Usamos "users.txt" direto para criar na pasta do jogo, evitando erros de permissões */
    FILE *f = fopen("users.txt", "r");
    
    char f_name[32], f_pass[32];
    int f_xp;

    if (f) {
        while (fscanf(f, "%s %s %d", f_name, f_pass, &f_xp) == 3) {
            if (strcmp(f_name, name) == 0) {
                if (strcmp(f_pass, password) == 0) { 
                    fclose(f); 
                    return f_xp; /* Utilizador existe e a password está certa */
                } else { 
                    fclose(f); 
                    return -1; /* Password errada */
                }
            }
        }
        fclose(f);
    }

    /* Se chegou aqui, a conta não existe. Vamos criá-la (Append mode "a") */
    f = fopen("users.txt", "a");
    if (!f) return 0; /* Falha de segurança caso o disco esteja protegido */
    
    fprintf(f, "%s %s %d\n", name, password, 0);
    fclose(f);
    return 0;
}

void game_save_user_xp_to_disk(const char *name, int new_xp) {
    char names[100][32], passes[100][32];
    int xps[100], count = 0;
    bool found = false;

    /* 1. Lemos todos os utilizadores existentes */
    FILE *f = fopen("users.txt", "r");
    if (f) {
        while (count < 100 && fscanf(f, "%s %s %d", names[count], passes[count], &xps[count]) == 3) {
            if (strcmp(names[count], name) == 0) {
                xps[count] = new_xp; /* Atualiza o XP do gajo atual */
                found = true;
            }
            count++;
        }
        fclose(f);
    }

    /* 2. Se o utilizador atual não foi encontrado (caso bizarro), adicionamo-lo à lista */
    if (!found && count < 100) {
        strncpy(names[count], name, 31);
        strcpy(passes[count], "1234"); /* password padrão de salvaguarda */
        xps[count] = new_xp;
        count++;
    }

    /* 3. Gravamos a lista atualizada de volta para o ficheiro */
    f = fopen("users.txt", "w");
    if (f) {
        for (int i = 0; i < count; i++) {
            fprintf(f, "%s %s %d\n", names[i], passes[i], xps[i]);
        }
        fclose(f);
    }
}

void game_init(void) {
    model_init(&M, h_res, v_res, rtc_seed());   /* logic + RNG seeded by RTC */
    scores_load(M.high_scores, &M.high_count, MAX_SCORES);  /* persistent leaderboard */


       /* XP/powers start at 0; loaded after login in controller */
    M.xp_total        = 0;
    M.powers_unlocked = 0;
    M.extra_life_used = false;
 
    /* Login state */
    M.input_name[0]      = '\0';  M.input_name_len      = 0;
    M.input_password[0]  = '\0';  M.input_password_len  = 0;
    M.typing_password    = false;
    M.login_error        = false;
    M.logged_in          = false;
 
    /* Leaderboard cache */
    M.lb_count  = 0;
    M.lb_scroll = 0;
 
    /* Start at login screen */
    M.state = STATE_NAME_INPUT;


    view_init();                                /* load sprites (mode already set) */
    game_on_rtc();                              /* prime the clock immediately */
}


void open_leaderboard_top(GameModel *m) {
    /* Força um recarregamento do ficheiro para garantir que está atualizado */
    scores_load(m->high_scores, &m->high_count, MAX_SCORES);
    
    /* Extrai apenas os 5 melhores da tabela global para a tabela visual */
    scores_get_top(m->high_scores, m->high_count, 5, m->lb_entries, &m->lb_count);
    
    m->lb_scroll = 0;
    m->state = STATE_LEADERBOARD_TOP;
}

void open_leaderboard_all(GameModel *m) {
    /* Força um recarregamento do ficheiro */
    scores_load(m->high_scores, &m->high_count, MAX_SCORES);
    
    /* Extrai todas as entradas ordenadas para a tabela visual */
    scores_get_all(m->high_scores, m->high_count, m->lb_entries, &m->lb_count);
    
    m->lb_scroll = 0;
    m->state = STATE_LEADERBOARD_ALL;
}

void game_on_rtc(void) {
    rtc_datetime dt;
    if (rtc_read_datetime(&dt) != 0) return;
    snprintf(M.clock_str, sizeof(M.clock_str), "%02d:%02d:%02d",
             dt.hour, dt.min, dt.sec);
    snprintf(M.date_str, sizeof(M.date_str), "%02d/%02d/%02d",
             dt.day, dt.month, dt.year);
}

void game_cleanup(void) {
    /* Everything is statically allocated — nothing to free. */
}


void game_add_xp(int amount) {
    M.xp_total += amount;
 
    /* Desbloqueia poderes por limiares (só se ainda não estiver desbloqueado) */
    if (M.xp_total >= XP_THRESH_1 && !(M.powers_unlocked & POWER_DOUBLE_COINS))
        game_unlock_power(POWER_DOUBLE_COINS);
    if (M.xp_total >= XP_THRESH_2 && !(M.powers_unlocked & POWER_SHIELD_START))
        game_unlock_power(POWER_SHIELD_START);
    if (M.xp_total >= XP_THRESH_3 && !(M.powers_unlocked & POWER_SLOW_MISSILES))
        game_unlock_power(POWER_SLOW_MISSILES);
    if (M.xp_total >= XP_THRESH_4 && !(M.powers_unlocked & POWER_EXTRA_LIFE))
        game_unlock_power(POWER_EXTRA_LIFE);
 
    scores_save_xp(M.xp_total);   /* persiste imediatamente */
}
 
int game_get_xp(void) {
    return M.xp_total;
}
 
/* ── FEATURE: Powers API ──────────────────────────────────────────────────── */
void game_unlock_power(uint32_t power_flag) {
    M.powers_unlocked |= power_flag;
    /* (opcionalmente: mostrar popup "Poder desbloqueado!" via spawn_popup) */
}
 
bool game_has_power(uint32_t power_flag) {
    return (M.powers_unlocked & power_flag) != 0;
}
 

const char *game_get_input_name(void) {
    return M.input_name;
}
 

void game_update(void) {
    static game_state_t prev = STATE_MENU;

    model_update(&M);
    net_poll(&M);    /* apply any opponent packets that arrived */
    net_tick(&M);    /* broadcast our state in 2-player mode    */

    /* On entering game-over from a solo run, persist the score with the RTC
     * timestamp captured for this run. */
    if (M.state == STATE_GAMEOVER && prev != STATE_GAMEOVER && !M.versus) {
        /* Acumula XP desta run antes de pedir o nome */
        int dist_xp = (M.frame_count / 60) * XP_PER_METER;  /* aprox. por segundo */
        game_add_xp(dist_xp);   /* moedas/powerups já foram adicionados durante a run */
 
        const char *current_user = (M.input_name_len > 0) ? M.input_name : "PLAYER";
        game_save_user_xp_to_disk(current_user, M.xp_total);


        /* Atualiza high score de sessão */
        if (M.score > M.high_score) M.high_score = M.score;
 
        char tstr[SCORE_WHEN_LEN];
        rtc_datetime dt;
        if (rtc_read_datetime(&dt) == 0) {
            sprintf(tstr, "%02d/%02d/%02d %02d:%02d:%02d", 
                    dt.day, dt.month, dt.year, dt.hour, dt.min, dt.sec);
        } else {
            snprintf(tstr, sizeof(tstr), "%s %s",
                     M.date_str[0]  ? M.date_str  : "01/01/26",
                     M.clock_str[0] ? M.clock_str : "12:00:00");
        }

        /* Inserimos na leaderboard passando a string gerada */
        M.last_rank = scores_insert(M.high_scores, &M.high_count, MAX_SCORES, current_user, M.score, tstr);
        
        scores_save(M.high_scores, M.high_count);

        scores_get_top(M.high_scores, M.high_count, 5, M.lb_entries, &M.lb_count);
        
       
        
    }

prev = M.state;


}

void game_draw(void) {
    view_render(&M);
}

void game_handle_kbd(uint8_t scancode) {
    controller_handle_kbd(&M, scancode);
}

void game_handle_mouse(int16_t dx, int16_t dy, bool lb_pressed) {
    controller_handle_mouse(&M, dx, dy, lb_pressed);
}

game_state_t game_get_state(void) {
    return M.state;
}
