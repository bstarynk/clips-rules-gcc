   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/02/18            */
   /*                                                     */
   /*         DEFMODULE BASIC COMMANDS HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the deffacts        */
/*   construct such as clear, reset, save, undeffacts,       */
/*   ppdeffacts, list-deffacts, and get-deffacts-list.       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
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
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

#ifndef _H_modulbsc

#pragma once

#define _H_modulbsc

#include "evaluatn.h"

void CL_DefmoduleBasicCommands (Environment *);
void CL_GetDefmoduleListFunction (Environment *, UDFContext *, UDFValue *);
void CL_GetDefmoduleList (Environment *, CLIPSValue *);
void CL_PPDefmoduleCommand (Environment *, UDFContext *, UDFValue *);
bool CL_PPDefmodule (Environment *, const char *, const char *);
const char *CL_PPDefmoduleNil (Environment *, const char *);
void CL_ListDefmodulesCommand (Environment *, UDFContext *, UDFValue *);
void CL_ListDefmodules (Environment *, const char *);

#endif /* _H_modulbsc */
