#include "game.h"
#include "video.h"
#include "interrupts.h"
#include "timer.h"
#include "keyboard.h"


/*
 * Draw the character represented by extended ASCII code |ascii_code| in color
 * |color| at offset |offset| from the upper-left hand of the display.
 */
void draw_ext_ascii(unsigned char ascii_code, int offset, int color) {
    unsigned char to_print[2];
    to_print[1] = '\0';
    to_print[0] = ascii_code;

    write_string(color, to_print, offset);
}

/*
 * Draw a rainbow across the |y|th horizontal line of the screen. Lines are
 * zero-indexed.
 */
void draw_rainbow(int y) {
    char * block = "                ";  // 16 spaces
    write_string(BLACK_ON_RED, block, WIDTH * y);
    write_string(BLACK_ON_YELLOW, block, WIDTH * y + 16 * 1);
    write_string(BLACK_ON_GREEN, block, WIDTH * y + 16 * 2);
    write_string(BLACK_ON_BLUE, block, WIDTH * y + 16 * 3);
    write_string(BLACK_ON_MAGENTA, block, WIDTH * y + 16 * 4);
}

/*
 * Set up the video display by painting the background, drawing the ground, and
 * drawing the goal.
 */
void draw_world(void) {
    // Paint the background
    paint_display(WHITE_ON_CYAN);

    // Draw the ground
    draw_rainbow(23);
    draw_rainbow(24);

    // Draw the goal
    write_string(BLACK_ON_GREEN, "  ", WIDTH * (HEIGHT - 3) - 2);
    draw_ext_ascii(222, WIDTH * (HEIGHT - 2) - 1, GREEN_ON_CYAN);
    draw_ext_ascii(221, WIDTH * (HEIGHT - 2) - 2, GREEN_ON_CYAN);
}

/*
 * Initialize the members of the global |player| struct.
 */
void init_player(void) {
    player.head = 149;
    player.body = 219;
    player.color = BLACK_ON_CYAN;
    player.x_coord = 0;
    player.y_coord = 21;
}

/*
 * Draw the player's avatar on screen
 */
void draw_player(void) {
    draw_ext_ascii(player.head,
                   player.y_coord * WIDTH + player.x_coord,
                   player.color);
    draw_ext_ascii(player.body,
                   (player.y_coord + 1) * WIDTH + player.x_coord,
                   player.color);
}

/*
 * Erase the player's avatar that was previously drawn at position given by
 * |prev_x|, |prev_y|
 */
void erase_player_at_prev_position(int prev_x, int prev_y) {
    write_string(BLACK_ON_CYAN, " ", prev_y * 80 + prev_x);
    write_string(BLACK_ON_CYAN, " ", (prev_y + 1) * 80 + prev_x);
}

/*
 * Update the global |player| struct based on displacement from keyboard
 * interrupts, and re-draw the player's avatar if needed.
 */
void update_player_position(void) {
    int displacement = get_new_displacement();
    if (displacement == 0) {
        return;
    }
    int prev_x_coord = player.x_coord;
    player.x_coord += displacement;
    if (player.x_coord >= WIDTH - 1) {
        player.x_coord = WIDTH - 1;
        win = 1;
    }
    else if (player.x_coord < 0) {
        player.x_coord = 0;
    }
    draw_player();
    erase_player_at_prev_position(prev_x_coord, player.y_coord);
}

/*
 * Initialize the global |shots| array of shot structs
 */
void init_guns(void) {
    int i;
    for (i = 0; i < N_GUNS; i++) {
        shots[i].x_coord = (WIDTH - 2)  / N_GUNS * (i + 1);
        shots[i].y_coord = 0;
        shots[i].color = RED_ON_CYAN;
        shots[i].speed = N_GUNS - i;
    }
}

/*
 * Update the global |shots| struct based on elapsed time ticks (shots fall
 * with time) and re-draw shots as needed.
 */
void update_shots(int num_ticks) {
    int i;
    char * o = "*";
    for (i = 0; i < N_GUNS; i++) {
        if (!(num_ticks % shots[i].speed)) {
            int last_y_coord = shots[i].y_coord;
            shots[i].y_coord++;
            if (shots[i].y_coord >= 23) {
                shots[i].y_coord = 0;
            }
            // Draw new shot
            write_string(shots[i].color, o,
                         shots[i].y_coord * WIDTH + shots[i].x_coord);
            // Erase previous shot
            write_string(RED_ON_CYAN, " ",
                         last_y_coord * WIDTH + shots[i].x_coord);
        }
        if (shots[i].y_coord >= 23) {
            shots[i].y_coord = 0;
        }
    }
}

/*
 * Check for a collision of the player's avatar with a shot/fireball.
 * If there is a collision, then set the global |win| variable to -1
 * to indicate defeat.
 */
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

/* Write a message in the center of the screen and then loop forever */
void write_final_message(char * message) {
    write_string(RED, message, 10 * 80 + 30);
    while(1);
}

/*
 * If the gave has ended, then print an appropriate message and loop forever
 */
void check_win(void) {
    if (win == -1) {
        write_final_message("DEATH FROM ABOVE");
    }
    else if (win == 1) {
        write_final_message("VICTORY!");
    }
}

void mainloop(void) {
    int last_tick = -1;
    int current_tick;
    // Loop forever, so that we don't fall back into the bootloader code
    while (1) {
        check_win();
        current_tick = get_num_ticks();
        if (current_tick != last_tick) {
            update_shots(current_tick);
            last_tick = current_tick;
        }
        update_player_position();
        check_collision();
    }
}

/* This is the entry-point for the game! */
void c_start(void) {
    // Initialize video, interrupt, timer, and keyboard subsystems
    init_video();
    init_interrupts();
    init_timer();
    init_keyboard();

    // Initialize game state
    init_player();
    init_guns();
    win = 0;
    draw_world();
    draw_player();

    enable_interrupts();

    mainloop();
}
