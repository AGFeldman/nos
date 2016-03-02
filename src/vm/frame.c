#include "vm/frame.h"
#include "threads/malloc.h"


// The address where the frame table begins
static struct ft_entry * frame_table;


void frame_table_init(void) {
    frame_table = (struct ft_entry *) malloc(NUM_USER_FRAMES *
                                             sizeof(struct ft_entry));
}
