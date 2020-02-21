#!/bin/bash
# file generate-timestamp.sh of 
printf "// generated file %s -- DONT EDIT\n" $1
date +"const char clgcc_timestamp[]=\"%c\";%nconst unsigned long clgcc_timelong=%sL;%n"
printf "const char clgcc_topdirectory[]=\"%s\";\n" $(realpath $(pwd))

if git status|grep -q 'nothing to commit' ; then
    endgitid='";'
else
    endgitid='+";'
fi
(echo -n 'const char clgcc_gitid[]="'; 
 git log --format=oneline -q -1 | cut '-d '  -f1 | tr -d '\n';
     echo $endgitid)  

(echo -n 'const char clgcc_lastgittag[]="'; (git describe --abbrev=0 --all || echo '*notag*') | tr -d '\n\r\f\"\\\\'; echo '";')

(echo -n 'const char clgcc_lastgitcommit[]="' ; \
 git log --format=oneline --abbrev=12 --abbrev-commit -q  \
     | head -1 | tr -d '\n\r\f\"\\\\' ; \
 echo '";') 

git archive -o /tmp/clgcc-$$.tar.gz HEAD 
trap "/bin/rm /tmp/clgcc-$$.tar.gz" EXIT INT 

cp -va /tmp/clgcc-$$.tar.gz $HOME/tmp/clgcc.tar.gz >& /dev/stderr

(echo -n 'const char clgcc_md5sum[]="' ; cat $(tar tf /tmp/clgcc-$$.tar.gz | grep -v '/$') | md5sum | tr -d '\n -'  ;  echo '";')

(echo  'const char*const clgcc_files[]= {' ; tar tf /tmp/clgcc-$$.tar.gz | grep -v '/$' | tr -s " \n"  | sed 's/^\(.*\)$/ "\1\",/';  echo ' (const char*)0} ;')

(echo  'const char*const clgcc_subdirectories[]= {' ; tar tf /tmp/clgcc-$$.tar.gz | grep  '/$' | tr -s " \n"  | sed 's/^\(.*\)$/ "\1\",/';  echo ' (const char*)0} ;')

printf "const char clgcc_makefile[]=\"%s\";\n"   $(realpath Makefile)
