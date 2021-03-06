#ifndef VIDEO_H
#define VIDEO_H


/* Available colors from the 16-color palette used for EGA and VGA, and
 * also for text-mode VGA output.
 */
#define BLACK          0
#define BLUE           1
#define GREEN          2
#define CYAN           3
#define RED            4
#define MAGENTA        5
#define BROWN          6
#define LIGHT_GRAY     7
#define DARK_GRAY      8
#define LIGHT_BLUE     9
#define LIGHT_GREEN   10
#define LIGHT_CYAN    11
#define LIGHT_RED     12
#define LIGHT_MAGENTA 13
#define YELLOW        14
#define WHITE         15

/* Colors with non-black backgrounds */
#define WHITE_ON_BLUE 0x1f
#define WHITE_ON_CYAN 0x3f
#define BLACK_ON_CYAN 0x30
#define BLACK_ON_RED 0x40
#define BLACK_ON_YELLOW 0xe0
#define BLACK_ON_GREEN 0x20
#define BLACK_ON_BLUE 0x10
#define BLACK_ON_MAGENTA 0x50
#define RED_ON_CYAN 0x34
#define GREEN_ON_CYAN 0x32
#define WHITE_ON_LIGHT_GRAY 0x7f

void write_string(int color, char *string, int offset);

void paint_display(int color);

void init_video(void);


#endif /* VIDEO_H */
