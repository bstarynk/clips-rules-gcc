   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*              COMMAND LINE HEADER FILE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides a set of routines for processing        */
/*   commands entered at the top level prompt.               */
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
/*            Refactored several functions and added         */
/*            additional functions for use by an interface   */
/*            layered on top of CLIPS.                       */
/*                                                           */
/*      6.30: Local variables set with the bind function     */
/*            persist until a reset/clear command is issued. */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Metrowerks CodeWarrior (MAC_MCW, IBM_MCW) is   */
/*            no longer supported.                           */
/*                                                           */
/*            UTF-8 support.                                 */
/*                                                           */
/*            Command history and editing support            */
/*                                                           */
/*            Used CL_genstrcpy instead of strcpy.              */
/*                                                           */
/*            Added before command execution callback        */
/*            function.                                      */
/*                                                           */
/*            Fixed CL_RouteCommand return value.               */
/*                                                           */
/*            Added AwaitingInput flag.                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Refactored code to reduce header dependencies  */
/*            in sysdep.c.                                   */
/*                                                           */
/*            Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_commline

#pragma once

#define _H_commline

#define COMMANDLINE_DATA 40

#include <stdlib.h>

typedef void AfterPromptFunction(Environment *);
typedef bool BeforeCommandExecutionFunction(Environment *);
typedef void EventFunction(Environment *);

struct commandLineData
  {
   bool CL_EvaluatingCL_TopLevelCommand;
   bool CL_HaltCL_CommandLoopCL_Batch;
#if ! RUN_TIME
   struct expr *CurrentCommand;
   char *CommandString;
   size_t MaximumCharacters;
   bool ParsingCL_TopLevelCommand;
   const char *BannerString;
   EventFunction *EventCallback;
   AfterPromptFunction *AfterPromptCallback;
   BeforeCommandExecutionFunction *BeforeCommandExecutionCallback;
#endif
  };

#define CommandLineData(theEnv) ((struct commandLineData *) GetEnvironmentData(theEnv,COMMANDLINE_DATA))

   void                           CL_InitializeCommandLineData(Environment *);
   bool                           CL_ExpandCommandString(Environment *,int);
   void                           CL_FlushCommandString(Environment *);
   void                           CL_SetCommandString(Environment *,const char *);
   void                           CL_AppendCommandString(Environment *,const char *);
   void                           CL_InsertCommandString(Environment *,const char *,unsigned);
   char                          *CL_GetCommandString(Environment *);
   int                            CL_CompleteCommand(const char *);
   void                           CL_CommandLoop(Environment *);
   void                           CL_CommandLoopCL_Batch(Environment *);
   void                           CL_CommandLoopCL_BatchDriver(Environment *);
   void                           CL_PrintPrompt(Environment *);
   void                           CL_PrintBanner(Environment *);
   void                           CL_SetAfterPromptFunction(Environment *,AfterPromptFunction *);
   void                           CL_SetBeforeCommandExecutionFunction(Environment *,BeforeCommandExecutionFunction *);
   bool                           CL_RouteCommand(Environment *,const char *,bool);
   EventFunction                 *CL_SetEventFunction(Environment *,EventFunction *);
   bool                           CL_TopLevelCommand(Environment *);
   void                           CL_AppendNCommandString(Environment *,const char *,unsigned);
   void                           CL_SetNCommandString(Environment *,const char *,unsigned);
   const char                    *CL_GetCommandCompletionString(Environment *,const char *,size_t);
   bool                           CL_ExecuteIfCommandComplete(Environment *);
   void                           CL_CommandLoopOnceThenCL_Batch(Environment *);
   bool                           CL_CommandCompleteAndNotEmpty(Environment *);
   void                           SetCL_HaltCL_CommandLoopCL_Batch(Environment *,bool);
   bool                           GetCL_HaltCL_CommandLoopCL_Batch(Environment *);
   void                           CL_RerouteStdin(Environment *,int,char *[]);

#endif





