# 
#  https://github.com/bstarynk/clips-rules-gcc
#
#  file Makefile for GNU make v4 or better
#
#  Copyright © 2020 CEA (Commissariat à l'énergie atomique et aux énergies alternatives)
#  contributed by Basile Starynkevitch.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

.PHONY: all clean plugin indent  test

CLGCC_GIT_ID := $(shell ./generate-gitid.sh)
CC= gcc
TARGET_GCC?= $(CC)
CXX= g++
RM= rm -f
MV= mv -v 
ASTYLE= astyle
ASTYLEFLAGS= --style=gnu -s2
INDENT= indent
INDENTFLAGS= --gnu-style--no-tabs --honour-newlines

GCCPLUGIN_DIR:= $(shell $(TARGET_GCC) -print-file-name=plugin)

CLGCC_PLUGIN_CXXSOURCES := $(wildcard clips*.cc)
CLGCC_PLUGIN_CXXHEADERS := $(wildcard clips*.hh)
CLGCC_PLUGIN_OBJECTS = $(patsubst %.cc,%.o, $(CLGCC_PLUGIN_CXXSOURCES))
CLIPS_CSOURCES := $(wildcard CLIPS-source/CL*.c)
CLIPS_CHEADERS :=  $(wildcard CLIPS-source/*.h)
CLIPS_OBJECTS =  $(patsubst CLIPS-source/%.c, CLIPS-source/%.o, $(CLIPS_CSOURCES))
CLGCC_GENFLAGS= -fPIC
CLGCC_OPTIMFLAGS= -O1 -g
CLGCC_WARNFLAGS= -Wall -Wextra
CLGCC_CWARNFLAGS= -Wmissing-prototypes
CLGCC_CXXWARNFLAGS=
CLGCC_PREPROFLAGS= -I /usr/local/include -I CLIPS-source/ 

CFLAGS= $(CLGCC_PREPROFLAGS) $(CLGCC_OPTIMFLAGS) $(CLGCC_WARNFLAGS) $(CLGCC_CWARNFLAGS) $(CLGCC_GENFLAGS)
CXXFLAGS= $(CLGCC_PREPROFLAGS) $(CLGCC_OPTIMFLAGS) $(CLGCC_WARNFLAGS) $(CLGCC_CXXWARNFLAGS) $(CLGCC_GENFLAGS)

all: plugin

## conventionally, files starting with _ are generated
## so
clean:
	$(RM) *.o *.so CLIPS-source/*.o
	$(RM) *.orig CLIPS-source/*.orig
	$(RM) _* *tmp CLIPS-source/_* CLIPS-source/*tmp
	$(RM) *~ *% CLIPS-source/*~ CLIPS-source/*%

plugin: clips-gcc-plugin.so

indent:
	for f in $(CLGCC_PLUGIN_CXXSOURCES) ; do \
	    $(ASTYLE) $(ASTYLEFLAGS) $$f ; \
	done
	for f in $(CLGCC_PLUGIN_CXXHEADERS) ; do \
	    $(ASTYLE) $(ASTYLEFLAGS) $$f ; \
	done
	for g in $(CLIPS_CSOURCES) ; do \
	    $(INDENT) $(INDENTFLAGS) $$g ; \
	done
	for g in $(CLIPS_CHEADERS) ; do \
	    $(INDENT) $(INDENTFLAGS) $$g ; \
	done

clips-gcc-plugin.so: _timestamp.c $(CLGCC_PLUGIN_OBJECTS) $(CLIPS_OBJECTS)
	$(CXX) $(CXXFLAGS) -shared _timestamp.c $(CLGCC_PLUGIN_OBJECTS) $(CLIPS_OBJECTS) -o $@
	$(MV) _timestamp.c _timestamp.c~

_timestamp.c: generate-timestamp.sh Makefile $(CLGCC_PLUGIN_CXXSOURCES) $(CLGCC_PLUGIN_CXXHEADERS) $(CLIPS_CSOURCES) $(CLIPS_CHEADERS)
	./generate-timestamp.sh $@ > $@-tmp
	$(MV) $@-tmp $@

CLIPS-source/%.o: CLIPS-source/%.c
	$(CC) $(CFLAGS) -DCLIPS_SOURCE $^ -c  -MMD -MF $(patsubst CLIPS-source/%.o, CLIPS-source/_%.mk, $@) -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -I $(GCCPLUGIN_DIR)/include -DCLIPSGCC_SOURCE $^ -c  -MMD -MF  $(patsubst %.o, _%.mk, $@) -o $@

-include $(wildcard _*.mk)

-include $(wildcard CLIPS-source/_*.mk)

### end of Makefile for https://github.com/bstarynk/clips-rules-gcc
