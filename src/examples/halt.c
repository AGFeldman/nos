/* halt.c

   Simple program to test whether running a user program works.

   Just invokes a system call that shuts down the OS. */

#include <syscall.h>

int
main (void)
{
    void *buf;
    unsigned int n;
    read(1, buf, n);
    /*
  halt ();
  */
  /* not reached */
}
