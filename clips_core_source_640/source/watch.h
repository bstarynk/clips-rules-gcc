   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*                  WATCH HEADER FILE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Support functions for the watch and unwatch      */
/*   commands.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added EnvCL_Set_WatchItem function.                */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
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

#ifndef _H_watch

#pragma once

#define _H_watch

#include "expressn.h"

#define WATCH_DATA 54

typedef struct watchItemRecord CL_WatchItemRecord;

typedef enum
{
  ALL,
  FACTS,
  INSTANCES,
  SLOTS,
  RULES,
  ACTIVATIONS,
  MESSAGES,
  MESSAGE_HANDLERS,
  GENERIC_FUNCTIONS,
  METHODS,
  DEFFUNCTIONS,
  COMPILATIONS,
  STATISTICS,
  GLOBALS,
  FOCUS
} CL_WatchItem;

struct watchItemRecord
{
  const char *name;
  bool *flag;
  int code, priority;
  bool (*accessFunc) (Environment *, int, bool, struct expr *);
  bool (*printFunc) (Environment *, const char *, int, struct expr *);
  CL_WatchItemRecord *next;
};

struct watchData
{
  CL_WatchItemRecord *ListOf_WatchItems;
};

#define CL_WatchData(theEnv) ((struct watchData *) GetEnvironmentData(theEnv,WATCH_DATA))

void CL_Watch (Environment *, CL_WatchItem);
void CL_Unwatch (Environment *, CL_WatchItem);

bool CL_WatchString (Environment *, const char *);
bool CL_UnwatchString (Environment *, const char *);
void CL_Initialize_WatchData (Environment *);
bool CL_Set_WatchItem (Environment *, const char *, bool, struct expr *);
int CL_Get_WatchItem (Environment *, const char *);
bool CL_Add_WatchItem (Environment *, const char *, int, bool *, int,
		       bool (*)(Environment *, int, bool, struct expr *),
		       bool (*)(Environment *, const char *, int,
				struct expr *));
const char *CL_GetNth_WatchName (Environment *, int);
int CL_GetNth_WatchValue (Environment *, int);
void CL_WatchCommand (Environment *, UDFContext *, UDFValue *);
void CL_UnwatchCommand (Environment *, UDFContext *, UDFValue *);
void CL_List_WatchItemsCommand (Environment *, UDFContext *, UDFValue *);
void CL_WatchFunctionDefinitions (Environment *);
void CL_Get_WatchItemCommand (Environment *, UDFContext *, UDFValue *);
bool CL_Get_WatchState (Environment *, CL_WatchItem);
void CL_Set_WatchState (Environment *, CL_WatchItem, bool);

#endif /* _H_watch */
