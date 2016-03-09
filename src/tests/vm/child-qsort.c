/* Reads a 128 kB file onto the stack and "sorts" the bytes in
   it, using quick sort, a multi-pass divide and conquer
   algorithm.  The sorted data is written back to the same file
   in-place. */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/qsort.h"

const char *test_name = "child-qsort";

int
main (int argc UNUSED, char *argv[])
{
  int handle;
  unsigned char buf[128 * 1024];
  size_t size;

  quiet = false;

  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);

  size = read (handle, buf, sizeof buf);
  msg("AGF child-qsort %s 1", argv[1]);
  qsort_bytes (buf, sizeof buf);
  msg("AGF child-qsort %s 2", argv[1]);
  seek (handle, 0);
  msg("AGF child-qsort %s 3", argv[1]);
  write (handle, buf, size);
  msg("AGF child-qsort %s 4", argv[1]);
  close (handle);
  msg("AGF child-qsort %s 5", argv[1]);

  return 72;
}
