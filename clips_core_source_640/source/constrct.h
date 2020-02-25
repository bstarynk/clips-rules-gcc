   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/01/16            */
   /*                                                     */
   /*                  CONSTRUCT MODULE                   */
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
/*      6.24: Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Changed garbage collection algorithm.          */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added code for capturing errors/warnings       */
/*            (Env_SetParserErrorCallback).                   */
/*                                                           */
/*            Fixed issue with save function when multiple   */
/*            defmodules exist.                              */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            Increment/Decrement_ClearReadyLocks API.        */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
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
/*            Modified Env_Clear to return completion status. */
/*                                                           */
/*            File name/line count displayed for errors      */
/*            and warnings during load command.              */
/*                                                           */
/*************************************************************/

#ifndef _H_constrct

#pragma once

#define _H_constrct

typedef struct construct Construct;

#include "entities.h"
#include "userdata.h"
#include "moduldef.h"
#include "utility.h"

typedef void CL_Save_CallFunction(Environment *,Defmodule *,const char *,void *);
typedef struct save_CallFunctionItem CL_Save_CallFunctionItem;

typedef void ParserErrorFunction(Environment *,const char *,const char *,const char *,long,void *);
typedef bool Before_ResetFunction(Environment *);

#define CHS (ConstructHeader *)

struct save_CallFunctionItem
  {
   const char *name;
   CL_Save_CallFunction *func;
   int priority;
   CL_Save_CallFunctionItem *next;
   void *context;
  };

struct construct
  {
   const char *constructName;
   const char *pluralName;
   bool (*parseFunction)(Environment *,const char *);
   CL_FindConstructFunction *findFunction;
   CLIPSLexeme *(*getConstructNameFunction)(ConstructHeader *);
   const char *(*getPPFo_rmFunction)(ConstructHeader *);
   struct defmoduleItemHeader *(*getModuleItemFunction)(ConstructHeader *);
   GetNextConstructFunction *getNextItemFunction;
   void (*setNextItemFunction)(ConstructHeader *,ConstructHeader *);
   IsConstructDeletableFunction *isConstructDeletableFunction;
   DeleteConstructFunction *deleteFunction;
   FreeConstructFunction *freeFunction;
   Construct *next;
  };

#define CONSTRUCT_DATA 42

struct constructData
  {
   bool CL_ClearReadyInProgress;
   bool CL_ClearInProgress;
   bool CL_ResetReadyInProgress;
   bool CL_ResetInProgress;
   short CL_ClearReadyLocks;
   int DanglingConstructs;
#if (! RUN_TIME) && (! BLOAD_ONLY)
   CL_Save_CallFunctionItem *ListOf_SaveFunctions;
   bool PrintWhile_Loading;
   bool CL_LoadInProgress;
   bool CL_WatchCompilations;
   bool CL_CheckSyntaxMode;
   bool ParsingConstruct;
   char *ErrorString;
   char *WarningString;
   char *ParsingFileName;
   char *ErrorFileName;
   char *WarningFileName;
   long ErrLineNumber;
   long WrnLineNumber;
   int errorCaptureRouterCount;
   size_t MaxErrChars;
   size_t CurErrPos;
   size_t MaxWrnChars;
   size_t CurWrnPos;
   ParserErrorFunction *ParserErrorCallback;
   void *ParserErrorContext;
#endif
   Construct *ListOfConstructs;
   struct void_CallFunctionItem *ListOf_ResetFunctions;
   struct void_CallFunctionItem *ListOf_ClearFunctions;
   struct bool_CallFunctionItem *ListOf_ClearReadyFunctions;
   bool Executing;
   Before_ResetFunction *Before_ResetCallback;
  };

#define ConstructData(theEnv) ((struct constructData *) GetEnvironmentData(theEnv,CONSTRUCT_DATA))

   bool                           CL_Clear(Environment *);
   void                           CL_Reset(Environment *);
   bool                           CL_Save(Environment *,const char *);

   void                           CL_InitializeConstructData(Environment *);
   bool                           CL_Add_ResetFunction(Environment *,const char *,Void_CallFunction *,int,void *);
   bool                           CL_Remove_ResetFunction(Environment *,const char *);
   bool                           CL_Add_ClearReadyFunction(Environment *,const char *,Bool_CallFunction *,int,void *);
   bool                           Remove_ClearReadyFunction(Environment *,const char *);
   bool                           CL_Add_ClearFunction(Environment *,const char *,Void_CallFunction *,int,void *);
   bool                           Remove_ClearFunction(Environment *,const char *);
   void                           Increment_ClearReadyLocks(Environment *);
   void                           Decrement_ClearReadyLocks(Environment *);
   Construct                     *CL_AddConstruct(Environment *,const char *,const char *,
                                               bool (*)(Environment *,const char *),
                                               CL_FindConstructFunction *,
                                               CLIPSLexeme *(*)(ConstructHeader *),
                                               const char *(*)(ConstructHeader *),
                                               struct defmoduleItemHeader *(*)(ConstructHeader *),
                                               GetNextConstructFunction *,
                                               void (*)(ConstructHeader *,ConstructHeader *),
                                               IsConstructDeletableFunction *,
                                               DeleteConstructFunction *,
                                               FreeConstructFunction *);
   bool                           CL_RemoveConstruct(Environment *,const char *);
   void                           CL_SetCompilations_Watch(Environment *,bool);
   bool                           CL_GetCompilations_Watch(Environment *);
   void                           SetPrintWhile_Loading(Environment *,bool);
   bool                           CL_GetPrintWhile_Loading(Environment *);
   void                           Set_LoadInProgress(Environment *,bool);
   bool                           CL_Get_LoadInProgress(Environment *);
   bool                           CL_ExecutingConstruct(Environment *);
   void                           Set_ExecutingConstruct(Environment *,bool);
   void                           CL_InitializeConstructs(Environment *);
   Before_ResetFunction           *SetBefore_ResetFunction(Environment *,Before_ResetFunction *);
   void                           CL_ResetCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_ClearCommand(Environment *,UDFContext *,UDFValue *);
   bool                           CL_ClearReady(Environment *);
   Construct                     *CL_FindConstruct(Environment *,const char *);
   void                           CL_DeinstallConstructHeader(Environment *,ConstructHeader *);
   void                           CL_DestroyConstructHeader(Environment *,ConstructHeader *);
   ParserErrorFunction           *CL_SetParserErrorCallback(Environment *,ParserErrorFunction *,void *);
   
   bool                           CL_Add_SaveFunction(Environment *,const char *,CL_Save_CallFunction *,int,void *);
   bool                           CL_Remove_SaveFunction(Environment *,const char *);
   CL_Save_CallFunctionItem          *CL_Add_SaveFunctionToCallList(Environment *,const char *,int,
                                                            CL_Save_CallFunction *,CL_Save_CallFunctionItem *,void *);
   CL_Save_CallFunctionItem          *CL_Remove_SaveFunctionFromCallList(Environment *,const char *,
                                                                 CL_Save_CallFunctionItem *,bool *);
   void                           CL_Deallocate_SaveCallList(Environment *,CL_Save_CallFunctionItem *);

#endif /* _H_constrct */




