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

.PHONY: all etags clean plugin indent tests print-test-settings

export MAKE
export MAKELEVEL
export MAKEFLAGS



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

CLGCC_TESTDIRS:=$(shell /bin/ls -d testdir/T[0-9]*[A-Za-z_]*[A-Za-z0-9])
CLGCC_TESTFILES= $(patsubst testdir/T%, test%, $(CLGCC_TESTDIRS))
CLGCC_TESTS:=$(shell echo $(CLGCC_TESTFILES) | sed 's:\(test[0-9]*\)[a-zA-Z_]*:\1:g')

CFLAGS= $(CLGCC_PREPROFLAGS) $(CLGCC_OPTIMFLAGS) $(CLGCC_WARNFLAGS) $(CLGCC_CWARNFLAGS) $(CLGCC_GENFLAGS)
CXXFLAGS= $(CLGCC_PREPROFLAGS) $(CLGCC_OPTIMFLAGS) $(CLGCC_WARNFLAGS) $(CLGCC_CXXWARNFLAGS) $(CLGCC_GENFLAGS)

.PHONY: $(CLGCC_TESTS)

all: plugin
	@echo CLGCC_TESTDIRS= $(CLGCC_TESTDIRS)
	@echo CLGCC_TESTFILES= $(CLGCC_TESTFILES)
	@echo CLGCC_TESTS= $(CLGCC_TESTS)

## conventionally, files starting with _ are generated
## so
clean:
	$(RM) *.o *.so CLIPS-source/*.o
	$(RM) *.orig CLIPS-source/*.orig
	$(RM) _* *tmp CLIPS-source/_* CLIPS-source/*tmp
	$(RM) *~ *% CLIPS-source/*~ CLIPS-source/*%

plugin:
	@echo $(MAKE) $@ with MAKE= $(MAKE) and MAKELEVEL= $(MAKELEVEL) and MAKEFLAGS= $(MAKEFLAGS)
	if [ '$(MAKELEVEL)' -lt 2 ]; then $(MAKE) clipsgccplug.so ; fi
	@echo made $@ with  MAKELEVEL= $(MAKELEVEL)

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

clipsgccplug.so: _timestamp.o $(CLGCC_PLUGIN_OBJECTS) $(CLIPS_OBJECTS)
	$(CXX) $(CXXFLAGS) -shared _timestamp.o $(CLGCC_PLUGIN_OBJECTS) $(CLIPS_OBJECTS) -o $@
	@$(MV) _timestamp.c _timestamp.c~ > /dev/stderr

_timestamp.c: generate-timestamp.sh Makefile $(CLGCC_PLUGIN_CXXSOURCES) $(CLGCC_PLUGIN_CXXHEADERS) $(CLIPS_CSOURCES) $(CLIPS_CHEADERS)
	./generate-timestamp.sh $@ > $@-tmp
	@$(MV) $@-tmp $@ > /dev/stderr

_timestamp.o: _timestamp.c
	$(CC) $(CFLAGS)  $< -c  -o $@

CLIPS-source/%.o: CLIPS-source/%.c
	$(CC) $(CFLAGS) -DCLIPS_SOURCE $< -c  -MMD -MF $(patsubst CLIPS-source/%.o, CLIPS-source/_%.mk, $@) -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -I $(GCCPLUGIN_DIR)/include -DCLIPSGCC_SOURCE $< -c  -MMD -MF  $(patsubst %.o, _%.mk, $@) -o $@


tests: _tests_.mk plugin
	$(MAKE) $(CLGCC_TESTS)

_tests_.mk: | $(CLGCC_TESTFILES)
	@date +'#DO NOT EDIT generated file $@ at %c%n' > $@-tmp
	@for d in $(CLGCC_TESTDIRS) ; do \
	   printf "%s: %s/run.bash\n" $$(echo $$d | sed 's:\(test[0-9]*\)[a-zA-Z_]*:\1:g') $$d ; \
	done >> $@-tmp
	@mv $@-tmp $@

#### the print-test-settings target is called by test scripts testdir/T*/run.bash
print-test-settings: | plugin
	@printf "TARGET_GCC=%s\n" $(TARGET_GCC)
	@printf "CLIPS_GCC_PLUGIN=%s\n" $(realpath clipsgccplug.so)

-include  $(wildcard _*[a-zA-Z].mk)

-include $(wildcard CLIPS-source/_*.mk)

test%: | $(patsubst test%, $(wildcard testdir/T%*/run.bash), $@)
	@echo TEST... $@ running $(wildcard $(patsubst test%, testdir/T%*/run.bash, $@))
	/bin/bash -x  $(wildcard $(patsubst test%, testdir/T%*/run.bash, $@)) < /dev/null
	@echo TEST... done $@

etags: TAGS

TAGS:  $(CLGCC_PLUGIN_CXXSOURCES) $(CLGCC_PLUGIN_CXXHEADERS) $(CLIPS_CSOURCES) $(CLIPS_CHEADERS)
	etags -o $@ $^

ifeq ($(MAKELEVEL),0)
-include _tests_.mk
endif
### end of Makefile for https://github.com/bstarynk/clips-rules-gcc
