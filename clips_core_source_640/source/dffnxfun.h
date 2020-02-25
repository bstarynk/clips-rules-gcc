   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/01/16            */
   /*                                                     */
   /*              DEFFUNCTION HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*            Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
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
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added missing initializer for ENTITY_RECORD.   */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
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

#ifndef _H_dffnxfun

#pragma once

#define _H_dffnxfun

typedef struct deffunction Deffunction;
typedef struct deffunctionModuleData CL_DeffunctionModuleData;

#include "entities.h"
#include "moduldef.h"
#include "constrct.h"
#include "evaluatn.h"

struct deffunctionModuleData
  {
   struct defmoduleItemHeader header;
  };

struct deffunction
  {
   ConstructHeader header;
   unsigned busy;
   unsigned executing;
   bool trace;
   Expression *code;
   unsigned short minNumberOfParameters;
   unsigned short maxNumberOfParameters;
   unsigned short numberOfLocalVars;
  };

#define DEFFUNCTION_DATA 23

struct deffunctionData
  {
   Construct *DeffunctionConstruct;
   unsigned CL_DeffunctionModuleIndex;
   EntityRecord DeffunctionEntityRecord;
#if DEBUGGING_FUNCTIONS
   bool CL_WatchDeffunctions;
#endif
   struct CodeGeneratorItem *DeffunctionCodeItem;
   Deffunction *ExecutingDeffunction;
  };

#define DeffunctionData(theEnv) ((struct deffunctionData *) GetEnvironmentData(theEnv,DEFFUNCTION_DATA))

   bool                           CL_CheckDeffunctionCall(Environment *,Deffunction *,int);
   void                           DeffunctionGetBind(UDFValue *);
   void                           DFRtnUnknown(UDFValue *);
   void                           DFWildargs(UDFValue *);
   const char                    *CL_DeffunctionModule(Deffunction *);
   Deffunction                   *CL_FindDeffunction(Environment *,const char *);
   Deffunction                   *CL_FindDeffunctionInModule(Environment *,const char *);
   void                           CL_GetDeffunctionList(Environment *,CLIPSValue *,Defmodule *);
   const char                    *CL_DeffunctionName(Deffunction *);
   CLIPSLexeme                   *Get_DeffunctionNamePointer(Environment *,Deffunction *);
   const char                    *CL_DeffunctionPPFo_rm(Deffunction *);
   Deffunction                   *CL_GetNextDeffunction(Environment *,Deffunction *);
   bool                           CL_DeffunctionIsDeletable(Deffunction *);
   void                           SetCL_DeffunctionPPFo_rm(Environment *,Deffunction *,const char *);
   bool                           CL_Undeffunction(Deffunction *,Environment *);
   void                           CL_GetDeffunctionListFunction(Environment *,UDFContext *,UDFValue *);
   void                           Get_DeffunctionModuleCommand(Environment *,UDFContext *,UDFValue *);
   Deffunction                   *CL_LookupDeffunctionByMdlOrScope(Environment *,const char *);
   Deffunction                   *CL_LookupDeffunctionInScope(Environment *,const char *);
#if (! BLOAD_ONLY) && (! RUN_TIME)
   void                           CL_RemoveDeffunction(Environment *,Deffunction *);
#endif
   void                           CL_SetupDeffunctions(Environment *);
   void                           CL_UndeffunctionCommand(Environment *,UDFContext *,UDFValue *);
#if DEBUGGING_FUNCTIONS
   bool                           CL_DeffunctionGet_Watch(Deffunction *);
   void                           CL_ListDeffunctions(Environment *,const char *,Defmodule *);
   void                           CL_DeffunctionSet_Watch(Deffunction *,bool);
   void                           CL_ListDeffunctionsCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_PPDeffunctionCommand(Environment *,UDFContext *,UDFValue *);
#endif

#if RUN_TIME
   void                           Deffunction_RunTimeInitialize(Environment *);
#endif

#endif /* _H_dffnxfun */






