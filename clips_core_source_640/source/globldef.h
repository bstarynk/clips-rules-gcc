   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/13/17            */
   /*                                                     */
   /*                DEFGLOBAL HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warning.                              */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_globldef

#pragma once

#define _H_globldef

typedef struct defglobal Defglobal;

#include "conscomp.h"
#include "constrct.h"
#include "cstrccom.h"
#include "evaluatn.h"
#include "expressn.h"
#include "moduldef.h"
#include "symbol.h"

#define DEFGLOBAL_DATA 1

struct defglobalData
  {
   Construct *DefglobalConstruct;
   unsigned CL_DefglobalModuleIndex;
   bool ChangeToGlobals;
#if DEBUGGING_FUNCTIONS
   bool CL_WatchGlobals;
#endif
   bool CL_ResetGlobals;
   struct entityRecord GlobalInfo;
   struct entityRecord DefglobalPtrRecord;
   long LastModuleIndex;
   Defmodule *TheDefmodule;
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   struct CodeGeneratorItem *DefglobalCodeItem;
#endif
  };

struct defglobal
  {
   ConstructHeader header;
   unsigned int watch   : 1;
   unsigned int inScope : 1;
   long busyCount;
   CLIPSValue current;
   struct expr *initial;
  };

struct defglobalModule
  {
   struct defmoduleItemHeader header;
  };

#define DefglobalData(theEnv) ((struct defglobalData *) GetEnvironmentData(theEnv,DEFGLOBAL_DATA))

   void                           CL_InitializeDefglobals(Environment *);
   Defglobal                     *CL_FindDefglobal(Environment *,const char *);
   Defglobal                     *CL_FindDefglobalInModule(Environment *,const char *);
   Defglobal                     *CL_GetNextDefglobal(Environment *,Defglobal *);
   void                           CreateInitialFactDefglobal(void);
   bool                           CL_DefglobalIsDeletable(Defglobal *);
   struct defglobalModule        *GetCL_DefglobalModuleItem(Environment *,Defmodule *);
   void                           CL_QSetDefglobalValue(Environment *,Defglobal *,UDFValue *,bool);
   Defglobal                     *QCL_FindDefglobal(Environment *,CLIPSLexeme *);
   void                           CL_DefglobalValueFoCL_rm(Defglobal *,StringCL_Builder *);
   bool                           CL_GetGlobalsChanged(Environment *);
   void                           CL_SetGlobalsChanged(Environment *,bool);
   void                           CL_DefglobalGetValue(Defglobal *,CLIPSValue *);
   void                           CL_DefglobalSetValue(Defglobal *,CLIPSValue *);
   void                           CL_DefglobalSetInteger(Defglobal *,long long);
   void                           CL_DefglobalSetFloat(Defglobal *,double);
   void                           CL_DefglobalSetSymbol(Defglobal *,const char *);
   void                           CL_DefglobalSetString(Defglobal *,const char *);
   void                           CL_DefglobalSetCL_InstanceName(Defglobal *,const char *);
   void                           CL_DefglobalSetCLIPSInteger(Defglobal *,CLIPSInteger *);
   void                           CL_DefglobalSetCLIPSFloat(Defglobal *,CLIPSFloat *);
   void                           CL_DefglobalSetCLIPSLexeme(Defglobal *,CLIPSLexeme *);
   void                           CL_DefglobalSetFact(Defglobal *,Fact *);
   void                           CL_DefglobalSetInstance(Defglobal *,Instance *);
   void                           CL_DefglobalSetMultifield(Defglobal *,Multifield *);
   void                           CL_DefglobalSetCLIPSExternalAddress(Defglobal *,CLIPSExternalAddress *);
   void                           CL_UpdateDefglobalScope(Environment *);
   Defglobal                     *CL_GetNextDefglobalInScope(Environment *,Defglobal *);
   bool                           CL_QGetDefglobalUDFValue(Environment *,Defglobal *,UDFValue *);
   const char                    *CL_DefglobalModule(Defglobal *);
   const char                    *CL_DefglobalName(Defglobal *);
   const char                    *CL_DefglobalPPFoCL_rm(Defglobal *);
#if RUN_TIME
   void                           DefglobalCL_RunTimeInitialize(Environment *);
#endif

#endif /* _H_globldef */


