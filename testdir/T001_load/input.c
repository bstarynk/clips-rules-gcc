// 
//
//  https://github.com/bstarynk/clips-rules-gcc
//
//  file testdir/T001_load/input.c
//
//  Copyright © 2020 CEA (Commissariat à l'énergie atomique et aux énergies alternatives)
//  contributed by Basile Starynkevitch.

#include <stdio.h>

int
main (int argc, char **argv)
{
  printf ("hello from %s:", argv[0]);
  for (int ix = 1; ix < argc; ix++)
    printf (" %s", argv[ix]);
  putchar ('\n');
  fflush (NULL);
  return 0;
}

// end of file testdir/T001_load/input.c
