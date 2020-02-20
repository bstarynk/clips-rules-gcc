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
/*            (EnvCL_SetParserErrorCallback).                   */
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
/*            Increment/DecrementCL_ClearReadyLocks API.        */
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
/*            Modified EnvCL_Clear to return completion status. */
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

typedef void CL_SaveCL_CallFunction(Environment *,Defmodule *,const char *,void *);
typedef struct saveCL_CallFunctionItem CL_SaveCL_CallFunctionItem;

typedef void ParserErrorFunction(Environment *,const char *,const char *,const char *,long,void *);
typedef bool BeforeCL_ResetFunction(Environment *);

#define CHS (ConstructHeader *)

struct saveCL_CallFunctionItem
  {
   const char *name;
   CL_SaveCL_CallFunction *func;
   int priority;
   CL_SaveCL_CallFunctionItem *next;
   void *context;
  };

struct construct
  {
   const char *constructName;
   const char *pluralName;
   bool (*parseFunction)(Environment *,const char *);
   CL_FindConstructFunction *findFunction;
   CLIPSLexeme *(*getConstructNameFunction)(ConstructHeader *);
   const char *(*getPPFoCL_rmFunction)(ConstructHeader *);
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
   CL_SaveCL_CallFunctionItem *ListOfCL_SaveFunctions;
   bool PrintWhileCL_Loading;
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
   struct voidCL_CallFunctionItem *ListOfCL_ResetFunctions;
   struct voidCL_CallFunctionItem *ListOfCL_ClearFunctions;
   struct boolCL_CallFunctionItem *ListOfCL_ClearReadyFunctions;
   bool Executing;
   BeforeCL_ResetFunction *BeforeCL_ResetCallback;
  };

#define ConstructData(theEnv) ((struct constructData *) GetEnvironmentData(theEnv,CONSTRUCT_DATA))

   bool                           CL_Clear(Environment *);
   void                           CL_Reset(Environment *);
   bool                           CL_Save(Environment *,const char *);

   void                           CL_InitializeConstructData(Environment *);
   bool                           CL_AddCL_ResetFunction(Environment *,const char *,VoidCL_CallFunction *,int,void *);
   bool                           CL_RemoveCL_ResetFunction(Environment *,const char *);
   bool                           CL_AddCL_ClearReadyFunction(Environment *,const char *,BoolCL_CallFunction *,int,void *);
   bool                           RemoveCL_ClearReadyFunction(Environment *,const char *);
   bool                           CL_AddCL_ClearFunction(Environment *,const char *,VoidCL_CallFunction *,int,void *);
   bool                           RemoveCL_ClearFunction(Environment *,const char *);
   void                           IncrementCL_ClearReadyLocks(Environment *);
   void                           DecrementCL_ClearReadyLocks(Environment *);
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
   void                           CL_SetCompilationsCL_Watch(Environment *,bool);
   bool                           CL_GetCompilationsCL_Watch(Environment *);
   void                           SetPrintWhileCL_Loading(Environment *,bool);
   bool                           CL_GetPrintWhileCL_Loading(Environment *);
   void                           SetCL_LoadInProgress(Environment *,bool);
   bool                           CL_GetCL_LoadInProgress(Environment *);
   bool                           CL_ExecutingConstruct(Environment *);
   void                           SetCL_ExecutingConstruct(Environment *,bool);
   void                           CL_InitializeConstructs(Environment *);
   BeforeCL_ResetFunction           *SetBeforeCL_ResetFunction(Environment *,BeforeCL_ResetFunction *);
   void                           CL_ResetCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_ClearCommand(Environment *,UDFContext *,UDFValue *);
   bool                           CL_ClearReady(Environment *);
   Construct                     *CL_FindConstruct(Environment *,const char *);
   void                           CL_DeinstallConstructHeader(Environment *,ConstructHeader *);
   void                           CL_DestroyConstructHeader(Environment *,ConstructHeader *);
   ParserErrorFunction           *CL_SetParserErrorCallback(Environment *,ParserErrorFunction *,void *);
   
   bool                           CL_AddCL_SaveFunction(Environment *,const char *,CL_SaveCL_CallFunction *,int,void *);
   bool                           CL_RemoveCL_SaveFunction(Environment *,const char *);
   CL_SaveCL_CallFunctionItem          *CL_AddCL_SaveFunctionToCallList(Environment *,const char *,int,
                                                            CL_SaveCL_CallFunction *,CL_SaveCL_CallFunctionItem *,void *);
   CL_SaveCL_CallFunctionItem          *CL_RemoveCL_SaveFunctionFromCallList(Environment *,const char *,
                                                                 CL_SaveCL_CallFunctionItem *,bool *);
   void                           CL_DeallocateCL_SaveCallList(Environment *,CL_SaveCL_CallFunctionItem *);

#endif /* _H_constrct */




