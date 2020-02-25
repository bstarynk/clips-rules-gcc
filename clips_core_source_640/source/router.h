   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  01/15/18            */
   /*                                                     */
   /*                 ROUTER HEADER FILE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides a centralized mechanism for handling    */
/*   input and output requests.                              */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed conversion of '\r' to '\n' from the    */
/*            EnvGetcRouter function.                        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added support for passing context info_rmation  */
/*            to the router functions.                       */
/*                                                           */
/*      6.30: Fixed issues with passing context to routers.  */
/*                                                           */
/*            Added AwaitingInput flag.                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Added STDOUT and STDIN logical name            */
/*            definitions.                                   */
/*                                                           */
/*      6.40: Added CL_InputBufferCount function.               */
/*                                                           */
/*            Added check for reuse of existing router name. */
/*                                                           */
/*            Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Changed return values for router functions.    */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            Callbacks must be environment aware.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Removed WPROMPT, WDISPLAY, WTRACE, and WDIALOG */
/*            logical names.                                 */
/*                                                           */
/*************************************************************/

#ifndef _H_router

#pragma once

#define _H_router

#include <stdio.h>

typedef struct router Router;
typedef bool RouterQueryFunction (Environment *, const char *, void *);
typedef void Router_WriteFunction (Environment *, const char *, const char *,
				   void *);
typedef void RouterExitFunction (Environment *, int, void *);
typedef int Router_ReadFunction (Environment *, const char *, void *);
typedef int RouterUnreadFunction (Environment *, const char *, int, void *);

extern const char *STDOUT;
extern const char *STDIN;
extern const char *STDERR;
extern const char *STDWRN;

#define ROUTER_DATA 46

struct router
{
  const char *name;
  bool active;
  int priority;
  void *context;
  RouterQueryFunction *queryCallback;
  Router_WriteFunction *writeCallback;
  RouterExitFunction *exitCallback;
  Router_ReadFunction *readCallback;
  RouterUnreadFunction *unreadCallback;
  Router *next;
};

struct routerData
{
  size_t CommandBufferInputCount;
  size_t InputUngets;
  bool AwaitingInput;
  const char *LineCountRouter;
  const char *FastCharGetRouter;
  const char *FastCharGetString;
  long FastCharGetIndex;
  struct router *ListOfRouters;
  FILE *Fast_LoadFilePtr;
  FILE *Fast_SaveFilePtr;
  bool Abort;
};

#define RouterData(theEnv) ((struct routerData *) GetEnvironmentData(theEnv,ROUTER_DATA))

void CL_InitializeDefaultRouters (Environment *);
void CL_WriteString (Environment *, const char *, const char *);
void CL_Write (Environment *, const char *);
void CL_Writeln (Environment *, const char *);
int CL_ReadRouter (Environment *, const char *);
int CL_UnreadRouter (Environment *, const char *, int);
void CL_ExitRouter (Environment *, int);
void CL_AbortExit (Environment *);
bool CL_AddRouter (Environment *, const char *, int,
		   RouterQueryFunction *, Router_WriteFunction *,
		   Router_ReadFunction *, RouterUnreadFunction *,
		   RouterExitFunction *, void *);
bool CL_DeleteRouter (Environment *, const char *);
bool CL_QueryRouters (Environment *, const char *);
bool CL_DeactivateRouter (Environment *, const char *);
bool CL_ActivateRouter (Environment *, const char *);
void SetFast_Load (Environment *, FILE *);
void SetFast_Save (Environment *, FILE *);
FILE *CL_GetFast_Load (Environment *);
FILE *CL_GetFast_Save (Environment *);
void CL_UnrecognizedRouterMessage (Environment *, const char *);
void CL_PrintNRouter (Environment *, const char *, const char *,
		      unsigned long);
size_t CL_InputBufferCount (Environment *);
Router *CL_FindRouter (Environment *, const char *);
bool CL_PrintRouterExists (Environment *, const char *);

#endif /* _H_router */
