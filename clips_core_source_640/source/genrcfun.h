   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/01/16            */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Removed IMPERATIVE_METHODS compilation flag.   */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when DEBUGGING_FUNCTIONS   */
/*            is set to 0 and PROFILING_FUNCTIONS is set to  */
/*            1.                                             */
/*                                                           */
/*            Fixed typing issue when OBJECT_SYSTEM          */
/*            compiler flag is set to 0.                     */
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
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_genrcfun

#pragma once

#define _H_genrcfun

typedef struct defgenericModule DEFGENERIC_MODULE;
typedef struct restriction RESTRICTION;
typedef struct defmethod Defmethod;
typedef struct defgeneric Defgeneric;

#include <stdio.h>

#include "entities.h"
#include "conscomp.h"
#include "constrct.h"
#include "expressn.h"
#include "evaluatn.h"
#include "moduldef.h"
#include "symbol.h"

#define METHOD_NOT_FOUND USHRT_MAX
#define RESTRICTIONS_UNBOUNDED USHRT_MAX

struct defgenericModule
  {
   struct defmoduleItemHeader header;
  };

struct restriction
  {
   void **types;
   Expression *query;
   unsigned short tcnt;
  };

struct defmethod
  {
   ConstructHeader header;
   unsigned short index;
   unsigned busy;
   unsigned short restrictionCount;
   unsigned short minRestrictions;
   unsigned short maxRestrictions;
   unsigned short localVarCount;
   unsigned system : 1;
   unsigned trace : 1;
   RESTRICTION *restrictions;
   Expression *actions;
  };

struct defgeneric
  {
   ConstructHeader header;
   unsigned busy; // TBD bool?
   bool trace;
   Defmethod *methods;
   unsigned short mcnt;
   unsigned short new_index;
  };

#define DEFGENERIC_DATA 27

struct defgenericData
  {
   Construct *DefgenericConstruct;
   unsigned int CL_DefgenericModuleIndex;
   EntityRecord GenericEntityRecord;
#if DEBUGGING_FUNCTIONS
   bool CL_WatchGenerics;
   bool CL_WatchMethods;
#endif
   Defgeneric *CurrentGeneric;
   Defmethod *CurrentMethod;
   UDFValue *GenericCurrentArgument;
#if (! RUN_TIME) && (! BLOAD_ONLY)
   unsigned OldGenericBusy_Save;
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   struct CodeGeneratorItem *DefgenericCodeItem;
#endif
  };

#define DefgenericData(theEnv) ((struct defgenericData *) GetEnvironmentData(theEnv,DEFGENERIC_DATA))
#define CL_SaveBusyCount(gfunc)    (DefgenericData(theEnv)->OldGenericBusy_Save = gfunc->busy)
#define RestoreBusyCount(gfunc) (gfunc->busy = DefgenericData(theEnv)->OldGenericBusy_Save)

#if ! RUN_TIME
   bool                           CL_ClearDefgenericsReady(Environment *,void *);
   void                          *CL_Allocate_DefgenericModule(Environment *);
   void                           Free_DefgenericModule(Environment *,void *);
#else
   void                           Defgeneric_RunTimeInitialize(Environment *);
#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)

   bool                           CL_ClearDefmethods(Environment *);
   bool                           CL_RemoveAllExplicitMethods(Environment *,Defgeneric *);
   void                           CL_RemoveDefgeneric(Environment *,Defgeneric *);
   bool                           CL_ClearDefgenerics(Environment *);
   void                           CL_MethodAlterError(Environment *,Defgeneric *);
   void                           CL_DeleteMethodInfo(Environment *,Defgeneric *,Defmethod *);
   void                           CL_DestroyMethodInfo(Environment *,Defgeneric *,Defmethod *);
   bool                           CL_MethodsExecuting(Defgeneric *);
#endif
#if ! OBJECT_SYSTEM
   bool                           SubsumeType(int,int);
#endif

   unsigned short                 CL_FindMethodByIndex(Defgeneric *,unsigned short);
#if DEBUGGING_FUNCTIONS || PROFILING_FUNCTIONS
   void                           CL_PrintMethod(Environment *,Defmethod *,String_Builder *);
#endif
#if DEBUGGING_FUNCTIONS
   void                           CL_PreviewGeneric(Environment *,UDFContext *,UDFValue *);
#endif
   Defgeneric                    *CL_CheckGenericExists(Environment *,const char *,const char *);
   unsigned short                 CL_CheckMethodExists(Environment *,const char *,Defgeneric *,unsigned short);

#if ! OBJECT_SYSTEM
   const char                    *TypeName(Environment *,int);
#endif

   void                           CL_PrintGenericName(Environment *,const char *,Defgeneric *);

#endif /* _H_genrcfun */

