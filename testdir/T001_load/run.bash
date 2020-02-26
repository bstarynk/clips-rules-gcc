#!/bin/bash
# 
#  https://github.com/bstarynk/clips-rules-gcc
#
#  file testdir/T001_load/run.bash
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
this_script=$(realpath $(which $0))
printf "running %s in %s\n" $this_script $(pwd)
parentdir=$(dirname $this_script)
printf "parentdir is %s\n" $parentdir
/bin/ls -l $parentdir/../../Makefile $(realpath $parentdir/../../Makefile)
tempsource=$(tempfile -p CLIPSGCCsrc -s .bash)
tempasm=$(tempfile -p CLIPSGCCasm -s .s)
(cd  $parentdir/../.. ; make   print-test-settings) > $tempsource
function perhaps_remove_temporary_files() {
    if [ -z "$CLIPSGCC_KEEP_TEMPORARY" ]; then
	rm -vf $tempsource $tempasm
    else
	printf '# %s keeping temporary files %s %s with $CLIPSGCC_KEEP_TEMPORARY \n' $0 $tempsource $tempasm
    fi
}
trap perhaps_remove_temporary_files EXIT INT TERM ERR
printf "::::: %s :::::\n" $tempsource
head $tempsource
printf "===== end %s =====\n\n" $tempsource
source $tempsource
printf "# %s parentdir %s, cwd %s\n" $0 $parentdir $(pwd)
printf "# %s using TARGET_GCC=%s\n" $0 $TARGET_GCC
printf "# %s with CLIPS_GCC_PLUGIN=%s\n" $0 $CLIPS_GCC_PLUGIN
printf "\n###### $0 running: ######\n" $0
printf '# $TARGET_GCC -O1 -S -v -fplugin=$CLISP_GCC_PLUGIN \\\n'
printf '#    -fplugin-arg-clipsgccplug-project=%s \\\n' $(basename $(dirname $parentdir))
printf '#    -fplugin-arg-clipsgccplug-load=%s \\\n' $parentdir/
