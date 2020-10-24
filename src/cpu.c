/** @file cpu.c
 *  @brief This code simulates a system with caching between the CPU and the
 *  main memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "memory.h"
#include "byutr.h"

/*
 * Command line argument: Trace file.
 */
int main(int argc, char *argv[])
{
  FILE *tracef;
  p2AddrTr tr;

  if (argc < 2)
  {
    printf("Usage: %s filename\n", argv[0]);
    exit(1);
  }

  /* fopen(argv[1], "r") -> fopen(argv[1], "rb")
   * Windows doesn't follow POSIX here and fopen needs the 'b' to function
   * properly.
   */
  if ((tracef = fopen(argv[1], "rb")) == NULL)
  {
    printf("Could not open file: %s\n", argv[1]);
    exit(1);
  }

  memory_init(); /* Initialize the memory subsystem */

  /* Loop through the trace file and simulate memory accesses */
  while (!feof(tracef))
  {
    if (fread(&tr, sizeof(p2AddrTr), 1, tracef) == 1)
    {
      switch(tr.reqtype)
      {
      case FETCH:    memory_fetch(tr.addr, NULL); break;
      case MEMREAD:  memory_read(tr.addr, NULL); break;
      case MEMWRITE: memory_write(tr.addr, NULL); break;
      default: printf("Ignoring trace record with type %d\n", tr.reqtype);
      }
    }
  }

  fclose(tracef);

  memory_finish(); /* Deinitialize the memory subsystem */

  return 0;
}
