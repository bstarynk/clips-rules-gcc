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


Each of the following source files should be processed by our
`renaming-ed-script.zsh` script (for [zsh](https://zsh.org/)...).


  * `CLIPS-source/analysis.h`
  * `CLIPS-source/argacces.h`
  * `CLIPS-source/bload.h`
  * `CLIPS-source/bmathfun.h`
  * `CLIPS-source/bsave.h`
  * `CLIPS-source/classcom.h`
  * `CLIPS-source/classexm.h`
  * `CLIPS-source/classfun.h`
  * `CLIPS-source/classinf.h`
  * `CLIPS-source/classini.h`
  * `CLIPS-source/classpsr.h`
  * `CLIPS-source/clips.h`
  * `CLIPS-source/clsltpsr.h`
  * `CLIPS-source/commline.h`
  * `CLIPS-source/conscomp.h`
  * `CLIPS-source/constant.h`
  * `CLIPS-source/constrct.h`
  * `CLIPS-source/constrnt.h`
  * `CLIPS-source/crstrtgy.h`
  * `CLIPS-source/cstrcbin.h`
  * `CLIPS-source/cstrccmp.h`
  * `CLIPS-source/cstrccom.h`
  * `CLIPS-source/cstrcpsr.h`
  * `CLIPS-source/cstrnbin.h`
  * `CLIPS-source/cstrnchk.h`
  * `CLIPS-source/cstrncmp.h`
  * `CLIPS-source/cstrnops.h`
  * `CLIPS-source/cstrnpsr.h`
  * `CLIPS-source/cstrnutl.h`
  * `CLIPS-source/default.h`
  * `CLIPS-source/defins.h`
  * `CLIPS-source/developr.h`
  * `CLIPS-source/dffctbin.h`
  * `CLIPS-source/dffctbsc.h`
  * `CLIPS-source/dffctcmp.h`
  * `CLIPS-source/dffctdef.h`
  * `CLIPS-source/dffctpsr.h`
  * `CLIPS-source/dffnxbin.h`
  * `CLIPS-source/dffnxcmp.h`
  * `CLIPS-source/dffnxexe.h`
  * `CLIPS-source/dffnxfun.h`
  * `CLIPS-source/dffnxpsr.h`
  * `CLIPS-source/dfinsbin.h`
  * `CLIPS-source/dfinscmp.h`
  * `CLIPS-source/drive.h`
  * `CLIPS-source/emathfun.h`
  * `CLIPS-source/engine.h`
  * `CLIPS-source/entities.h`
  * `CLIPS-source/envrnbld.h`
  * `CLIPS-source/envrnmnt.h`
  * `CLIPS-source/evaluatn.h`
  * `CLIPS-source/expressn.h`
  * `CLIPS-source/exprnbin.h`
  * `CLIPS-source/exprnops.h`
  * `CLIPS-source/exprnpsr.h`
  * `CLIPS-source/extnfunc.h`
  * `CLIPS-source/factbin.h`
  * `CLIPS-source/factbld.h`
  * `CLIPS-source/factcmp.h`
  * `CLIPS-source/factcom.h`
  * `CLIPS-source/factfun.h`
  * `CLIPS-source/factgen.h`
  * `CLIPS-source/facthsh.h`
  * `CLIPS-source/factlhs.h`
  * `CLIPS-source/factmch.h`
  * `CLIPS-source/factmngr.h`
  * `CLIPS-source/factprt.h`
  * `CLIPS-source/factqpsr.h`
  * `CLIPS-source/factqury.h`
  * `CLIPS-source/factrete.h`
  * `CLIPS-source/factrhs.h`
  * `CLIPS-source/filecom.h`
  * `CLIPS-source/filertr.h`
  * `CLIPS-source/fileutil.h`
  * `CLIPS-source/generate.h`
  * `CLIPS-source/genrcbin.h`
  * `CLIPS-source/genrccmp.h`
  * `CLIPS-source/genrccom.h`
  * `CLIPS-source/genrcexe.h`
  * `CLIPS-source/genrcfun.h`
  * `CLIPS-source/genrcpsr.h`
  * `CLIPS-source/globlbin.h`
  * `CLIPS-source/globlbsc.h`
  * `CLIPS-source/globlcmp.h`
  * `CLIPS-source/globlcom.h`
  * `CLIPS-source/globldef.h`
  * `CLIPS-source/globlpsr.h`
  * `CLIPS-source/immthpsr.h`
  * `CLIPS-source/incrrset.h`
  * `CLIPS-source/inherpsr.h`
  * `CLIPS-source/inscom.h`
  * `CLIPS-source/insfile.h`
  * `CLIPS-source/insfun.h`
  * `CLIPS-source/insmngr.h`
  * `CLIPS-source/insmoddp.h`
  * `CLIPS-source/insmult.h`
  * `CLIPS-source/inspsr.h`
  * `CLIPS-source/insquery.h`
  * `CLIPS-source/insqypsr.h`
  * `CLIPS-source/iofun.h`
  * `CLIPS-source/lgcldpnd.h`
  * `CLIPS-source/match.h`
  * `CLIPS-source/memalloc.h`
  * `CLIPS-source/miscfun.h`
  * `CLIPS-source/modulbin.h`
  * `CLIPS-source/modulbsc.h`
  * `CLIPS-source/modulcmp.h`
  * `CLIPS-source/moduldef.h`
  * `CLIPS-source/modulpsr.h`
  * `CLIPS-source/modulutl.h`
  * `CLIPS-source/msgcom.h`
  * `CLIPS-source/msgfun.h`
  * `CLIPS-source/msgpass.h`
  * `CLIPS-source/msgpsr.h`
  * `CLIPS-source/multifld.h`
  * `CLIPS-source/multifun.h`
  * `CLIPS-source/network.h`
  * `CLIPS-source/objbin.h`
  * `CLIPS-source/objcmp.h`
  * `CLIPS-source/object.h`
  * `CLIPS-source/objrtbin.h`
  * `CLIPS-source/objrtbld.h`
  * `CLIPS-source/objrtcmp.h`
  * `CLIPS-source/objrtfnx.h`
  * `CLIPS-source/objrtgen.h`
  * `CLIPS-source/objrtmch.h`
  * `CLIPS-source/parsefun.h`
  * `CLIPS-source/pattern.h`
  * `CLIPS-source/pprint.h`
  * `CLIPS-source/prccode.h`
  * `CLIPS-source/prcdrfun.h`
  * `CLIPS-source/prcdrpsr.h`
  * `CLIPS-source/prdctfun.h`
  * `CLIPS-source/prntutil.h`
  * `CLIPS-source/proflfun.h`
  * `CLIPS-source/reorder.h`
  * `CLIPS-source/reteutil.h`
  * `CLIPS-source/retract.h`
  * `CLIPS-source/router.h`
  * `CLIPS-source/rulebin.h`
  * `CLIPS-source/rulebld.h`
  * `CLIPS-source/rulebsc.h`
  * `CLIPS-source/rulecmp.h`
  * `CLIPS-source/rulecom.h`
  * `CLIPS-source/rulecstr.h`
  * `CLIPS-source/ruledef.h`
  * `CLIPS-source/ruledlt.h`
  * `CLIPS-source/rulelhs.h`
  * `CLIPS-source/rulepsr.h`
  * `CLIPS-source/scanner.h`
  * `CLIPS-source/setup.h`
  * `CLIPS-source/sortfun.h`
  * `CLIPS-source/strngfun.h`
  * `CLIPS-source/strngrtr.h`
  * `CLIPS-source/symblbin.h`
  * `CLIPS-source/symblcmp.h`
  * `CLIPS-source/symbol.h`
  * `CLIPS-source/sysdep.h`
  * `CLIPS-source/textpro.h`
  * `CLIPS-source/tmpltbin.h`
  * `CLIPS-source/tmpltbsc.h`
  * `CLIPS-source/tmpltcmp.h`
  * `CLIPS-source/tmpltdef.h`
  * `CLIPS-source/tmpltfun.h`
  * `CLIPS-source/tmpltlhs.h`
  * `CLIPS-source/tmpltpsr.h`
  * `CLIPS-source/tmpltrhs.h`
  * `CLIPS-source/tmpltutl.h`
  * `CLIPS-source/userdata.h`
  * `CLIPS-source/usrsetup.h`
  * `CLIPS-source/utility.h`
  * `CLIPS-source/watch.h`


-----

#### Copyright © 2020 CEA (Commissariat à l'énergie atomique et aux énergies alternatives)

contributed by Basile Starynkevitch (France). For technical questions
contact `basile@starynkevitch.net` or `basile.starynkevitch@cea.fr`

