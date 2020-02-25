   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*       DEFTEMPLATE BASIC COMMANDS HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the deftemplate     */
/*   construct such as clear, reset, save, undeftemplate,    */
/*   ppdeftemplate, list-deftemplates, and                   */
/*   get-deftemplate-list.                                   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove compiler warnings     */
/*            when ENVIRONMENT_API_ONLY flag is set.         */
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

#ifndef _H_tmpltbsc

#pragma once

#define _H_tmpltbsc

#include "evaluatn.h"
#include "tmpltdef.h"

void CL_DeftemplateBasicCommands (Environment *);
void CL_UndeftemplateCommand (Environment *, UDFContext *, UDFValue *);
bool CL_Undeftemplate (Deftemplate *, Environment *);
void CL_GetDeftemplateListFunction (Environment *, UDFContext *, UDFValue *);
void CL_GetDeftemplateList (Environment *, CLIPSValue *, Defmodule *);
void CL_DeftemplateModuleFunction (Environment *, UDFContext *, UDFValue *);
#if DEBUGGING_FUNCTIONS
void CL_PPDeftemplateCommand (Environment *, UDFContext *, UDFValue *);
bool CL_PPDeftemplate (Environment *, const char *, const char *);
void CL_ListDeftemplatesCommand (Environment *, UDFContext *, UDFValue *);
void CL_ListDeftemplates (Environment *, const char *, Defmodule *);
bool CL_DeftemplateGet_Watch (Deftemplate *);
void CL_DeftemplateSet_Watch (Deftemplate *, bool);
bool CL_Deftemplate_WatchAccess (Environment *, int, bool, struct expr *);
bool CL_Deftemplate_WatchPrint (Environment *, const char *, int,
				struct expr *);
#endif

#endif /* _H_tmpltbsc */
