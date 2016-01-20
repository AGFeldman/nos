#include "game.h"
#include "video.h"
#include "interrupts.h"
#include "timer.h"
#include "keyboard.h"



void draw_world(void) {
    paint_display(WHITE_ON_CYAN);
    char * line = "                                                                                ";
    write_string(WHITE_ON_LIGHT_GRAY, line, WIDTH * 23);
    write_string(WHITE_ON_LIGHT_GRAY, line, WIDTH * 24);
}

void init_player(void) {
    player.head = 149;
    player.body = 219;
    player.color = BLACK_ON_CYAN;
    player.x_coord = 0;
    player.y_coord = 21;
}

void draw_ext_ascii(unsigned char ascii_code, int offset, int color) {
    unsigned char to_print[2];
    to_print[1] = '\0';
    to_print[0] = ascii_code;

    write_string(color, to_print, offset);
}

void draw_player(void) {
//    disable_interrupts();
    draw_ext_ascii(player.head, player.y_coord * WIDTH + player.x_coord, player.color);
    draw_ext_ascii(player.body, (player.y_coord + 1) * WIDTH + player.x_coord, player.color);
//    enable_interrupts();
}

void init_guns(void) {
    int i;
    for (i = 0; i < N_GUNS; i++) {
        shots[i].x_coord = WIDTH * (i + 1) / N_GUNS;
        shots[i].y_coord = 0;
        shots[i].color = RED_ON_CYAN;
        shots[i].speed = 10 - i;
    }
}

void erase_shots(void) {
    int i;
    for (i = 0; i < N_GUNS; i++) {
        write_string(RED_ON_CYAN, " ", shots[i].y_coord * WIDTH + shots[i].x_coord);
    }
}

void update_shots(int num_ticks) {
    int i;
    for (i = 0; i < N_GUNS; i++) {
        if (!(num_ticks % shots[i].speed)) {
            shots[i].y_coord = shots[i].y_coord + 1;
        }
        if (shots[i].y_coord >= 23) {
            shots[i].y_coord = 0;
        }
    }
}

void draw_shots(void) {
    int i;
    char * o = "*";
    for (i = 0; i < N_GUNS; i++) {
        write_string(shots[i].color, o, shots[i].y_coord * WIDTH + shots[i].x_coord);
    }
}

void check_collision(void) {
    int i;
    for (i = 0; i < N_GUNS; i++) {
        if (shots[i].speed && shots[i].x_coord == player.x_coord &&
            (shots[i].y_coord == player.y_coord || 
             shots[i].y_coord == player.y_coord + 1)) {
            win = -1;
        }
    }
}

void check_win(void) {
    char * message;
    if (win == -1) {
        message = "DEATH FROM ABOVE";
    }
    else if (win == 1) {
        message = "VICTORY!";
    }
        write_string(RED, message, 10 * WIDTH + 30);
}

/* This is the entry-point for the game! */
void c_start(void) {
    // TODO(agf): Remove this block comment
    /*        You will need to initialize various subsystems here.  This
     *        would include the interrupt handling mechanism, and the various
     *        systems that use interrupts.  Once this is done, you can call
     *        enable_interrupts() to start interrupt handling, and go on to
     *        do whatever else you decide to do!
     */

    init_video();
    init_interrupts();
    init_timer();
    init_keyboard();

    init_player();

    win = 0;


    enable_interrupts();

    draw_world();
    draw_player();


    /* Loop forever, so that we don't fall back into the bootloader code. */
    while (1) {
    }
}
