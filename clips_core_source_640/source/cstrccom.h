   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/02/18            */
   /*                                                     */
   /*           CONSTRUCT COMMAND HEADER MODULE           */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added CL_ConstructsDeletable function.            */
/*                                                           */
/*      6.30: Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

#ifndef _H_cstrccom

#pragma once

#define _H_cstrccom

typedef bool ConstructGetCL_WatchFunction(void *);
typedef void ConstructSetCL_WatchFunction(void *,bool);

#include "moduldef.h"
#include "constrct.h"

typedef void ConstructActionFunction(Environment *,ConstructHeader *,void *);

#if (! RUN_TIME)
   void                           CL_AddConstructToModule(ConstructHeader *);
#endif
   bool                           CL_DeleteNamedConstruct(Environment *,const char *,Construct *);
   ConstructHeader               *CL_FindNamedConstructInModule(Environment *,const char *,Construct *);
   ConstructHeader               *CL_FindNamedConstructInModuleOrImports(Environment *,const char *,Construct *);
   void                           CL_UndefconstructCommand(UDFContext *,const char *,Construct *);
   bool                           CL_PPConstruct(Environment *,const char *,const char *,Construct *);
   const char                    *CL_PPConstructNil(Environment *,const char *,Construct *);
   CLIPSLexeme                   *CL_GetConstructModuleCommand(UDFContext *,const char *,Construct *);
   Defmodule                     *CL_GetConstructModule(Environment *,const char *,Construct *);
   bool                           CL_Undefconstruct(Environment *,ConstructHeader *,Construct *);
   bool                           CL_UndefconstructAll(Environment *,Construct *);
   void                           CL_SaveConstruct(Environment *,Defmodule *,const char *,Construct *);
   const char                    *CL_GetConstructNameString(ConstructHeader *);
   const char                    *CL_GetConstructModuleName(ConstructHeader *);
   CLIPSLexeme                   *CL_GetConstructNamePointer(ConstructHeader *);
   void                           CL_GetConstructListFunction(UDFContext *,UDFValue *,Construct *);
   void                           CL_GetConstructList(Environment *,UDFValue *,Construct *,
                                                   Defmodule *);
   void                           CL_ListConstructCommand(UDFContext *,Construct *);
   void                           CL_ListConstruct(Environment *,Construct *,const char *,Defmodule *);
   void                           CL_SetNextConstruct(ConstructHeader *,ConstructHeader *);
   struct defmoduleItemHeader    *CL_GetConstructModuleItem(ConstructHeader *);
   const char                    *CL_GetConstructPPFoCL_rm(ConstructHeader *);
   void                           CL_PPConstructCommand(UDFContext *,const char *,Construct *,UDFValue *);
   ConstructHeader               *CL_GetNextConstructItem(Environment *,ConstructHeader *,unsigned);
   struct defmoduleItemHeader    *CL_GetConstructModuleItemByIndex(Environment *,Defmodule *,unsigned);
   void                           CL_FreeConstructHeaderModule(Environment *,struct defmoduleItemHeader *,
                                                                   Construct *);
   void                           CL_DoForAllConstructs(Environment *,
                                                     ConstructActionFunction *,
                                                     unsigned,bool,void *);
   void                           CL_DoForAllConstructsInModule(Environment *,Defmodule *,
                                                             ConstructActionFunction *,
                                                             unsigned,bool,void *);
   void                           CL_InitializeConstructHeader(Environment *,const char *,ConstructType,
                                                            ConstructHeader *,CLIPSLexeme *);
   void                           SetConstructPPFoCL_rm(Environment *,ConstructHeader *,const char *);
   ConstructHeader        *CL_LookupConstruct(Environment *,Construct *,const char *,bool);
#if DEBUGGING_FUNCTIONS
   bool                           CL_ConstructPrintCL_WatchAccess(Environment *,Construct *,const char *,
                                            Expression *,
                                            ConstructGetCL_WatchFunction *,
                                            ConstructSetCL_WatchFunction *);
   bool                           CL_ConstructSetCL_WatchAccess(Environment *,Construct *,bool,
                                            Expression *,
                                            ConstructGetCL_WatchFunction *,
                                            ConstructSetCL_WatchFunction *);
#endif
   bool                           CL_ConstructsDeletable(Environment *);

#endif



