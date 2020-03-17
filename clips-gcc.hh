
/**
   https://github.com/bstarynk/clips-rules-gcc

   file clips-gcc.hh is the common C++ header.

   Copyright © 2020 CEA (Commissariat à l'énergie atomique et aux énergies alternatives)
   contributed by Basile Starynkevitch.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

**/

#ifndef CLIPS_GCC_HEADER
#define CLIPS_GCC_HEADER 1

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <functional>
#include <deque>

#include <cstdio>
#include <cassert>

#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>

#include "gcc-plugin.h"
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "stringpool.h"
#include "toplev.h"
#include "basic-block.h"
#include "hash-table.h"
#include "vec.h"
#include "ggc.h"
#include "basic-block.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "gimple-fold.h"
#include "tree-eh.h"
#include "gimple-expr.h"
#include "is-a.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "tree.h"
#include "tree-pass.h"
#include "toplev.h"
#include "intl.h"
#include "plugin-version.h"
#include "diagnostic.h"
#include "context.h"

extern "C" {
#include "clips.h"
#include "tmpltpsr.h"
};				// end include clips.h as "C"

// For our CLGCC_DBGPRINTF macro in setup.h we need:
extern "C" int clgcc_debug;
extern "C" FILE* clgcc_dbgfile;
extern "C" const char*CLGCC_basename(const char*);
extern "C" void CLGCC_dodbgprintf(const char*srcfil, int lin, const char*fmt, ...);

// in generated _timestamp.c

extern "C" char clgcc_timestamp[];
extern "C" unsigned long clgcc_timelong;
extern "C" char clgcc_topdirectory[];
extern "C" char clgcc_gitid[];
extern "C" char clgcc_lastgittag[];
extern "C" char clgcc_lastgitcommit[];
extern "C" char clgcc_md5sum[];
extern "C" char*const clgcc_files[];
extern "C" char*const clgcc_subdirectories[];
extern "C" char clgcc_makefile[];


extern "C" double CLGCC_cputime(void);

extern "C" Environment* CLGCC_env;

extern std::string CLGCC_projectstr;
extern std::string CLGCC_translationunitstr;

#endif /*clips-gcc.hh */
