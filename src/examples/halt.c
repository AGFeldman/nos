/* halt.c

   Simple program to test whether running a user program works.

   Just invokes a system call that shuts down the OS. */

#include <syscall.h>

int
main (void)
{
    void *buf;
    unsigned int n = 7;
    int m = 5;
    /*
    printf("I am halt\n");
    */
    read(m, buf, n);
    /*
    printf("still halt\n");
    */
    /*
  halt ();
  */
  /* not reached */
}
