#ifndef GAME_H
#define GAME_H

// There will be N_GUNS number of falling columns of fireballs
#define N_GUNS 15
#define WIDTH 80
#define HEIGHT 25

// Hold data for a falling shot / "fireball"
typedef struct shot {
    int x_coord;
    int y_coord;
    int color;
    // Unlike what you might expect, a smaller |speed| value means that the
    // shot falls faster
    int speed;
} shot;

shot shots[N_GUNS];

// Hold data for the player's avatar
typedef struct person {
    int x_coord;
    int y_coord;
    int color;
    char head;  // Extended ASCII character to represent head
    char body;  // Extended ASCII character to represent body
} person;

person player;

// 0 if the game is still going, 1 if victory, -1 if defeat
char win;

void draw_player(void);

void check_win(void);

void check_collision(void);

void erase_shots(void);

void update_shots(int num_ticks);

void draw_shots(void);


#endif /* GAME_H */
