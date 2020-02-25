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
printf "running %s in %s\n" $(realpath $(which $0)) $(pwd)
parentdir=$(dirname $(realpath $(which $0)))
printf "parentdir is %s\n" $parentdir
/bin/ls -l $parentdir/../../Makefile $(realpath $parentdir/../../Makefile)
tempsource=$(tempfile -p CLIPSGCCsrc -s .bash)
(cd  $parentdir/../.. ; make   print-test-settings) > $tempsource
function remove_tempsource() {
    rm -vf $tempsource
}
trap remove_tempsource EXIT INT TERM ERR
printf "::::: %s :::::\n" $tempsource
head $tempsource
printf "===== end %s =====\n\n" $tempsource
source $tempsource
printf "using TARGET_GCC=%s\n" $TARGET_GCC
printf "with CLIPS_GCC_PLUGIN=%s\n" $CLIPS_GCC_PLUGIN
