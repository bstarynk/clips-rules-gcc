# clips-rules-gcc

## a GCC plugin embedding the CLIPS rules engine for Linux

My ([Basile Starynkevitch](http://starynkevitch.net/Basile/), employed
at [CEA, LIST](http://www-list.cea.fr/) in France) work on
`clips-rules-gcc` is partly funded (in 2020) by the European Union,
Horizon H2020 programme, [CHARIOT](http://chariotproject.eu/) project,
under Grant Agreement No 780075. Within CHARIOT I will focus on
analysis of some kind of
[IoT](https://en.wikipedia.org/wiki/Internet_of_things) software coded
in C or C++ and (cross-) compiled by [GCC](http://gcc.gnu.org/) on
some Linux desktop.

The `clips-rules-gcc` plugin might be interacting with
[Bismon](http://github.com/bstarynk/bismon). It is embedding the
[CLIPS rules](http://www.clipsrules.net/) engine.

The copyright owner is my employer [CEA, LIST](https://www-list.cea.fr/) in France.

### renaming C functions with `CL_`  prefix


The first thing to do (inspired by [GTK](https://gtk.org/) coding
conventions) is to rename every public C function with a `CL_` prefix
to ease embedding in a GCC plugin coded in C++. See rationale in [C++
dlopen mini HOWTO](https://www.tldp.org/HOWTO/html_single/C++-dlopen/)

In commit 792575c2720e91a2721a6f9a7429a8 all `CLIPS-source/*.c` files
except `main.c` have been renamed with a `CL_` prefix; all public C
functions have been renamed with a `CL_` prefix.


-----

#### Copyright © 2020 CEA (Commissariat à l'énergie atomique et aux énergies alternatives)

contributed by Basile Starynkevitch (France). For technical questions
contact `basile@starynkevitch.net` or `basile.starynkevitch@cea.fr`

