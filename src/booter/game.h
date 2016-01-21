#ifndef GAME_H
#define GAME_H

#define N_GUNS 15
#define WIDTH 80
#define HEIGHT 25

typedef struct shot {
    int x_coord;
    int y_coord;
    int color;
    int speed;
} shot;

volatile shot shots[N_GUNS];

typedef struct person {
    int x_coord;
    int y_coord;
    int color;
    char head;
    char body;
} person;

volatile person player;

volatile char win;

void draw_player(void);

void check_win(void);

void check_collision(void);

void erase_shots(void);

void update_shots(int num_ticks);

void draw_shots(void);


#endif /* GAME_H */
