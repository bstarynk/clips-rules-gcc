   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*             ARGUMENT ACCESS HEADER FILE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides access routines for accessing arguments */
/*   passed to user or system functions defined using the    */
/*   DefineFunction protocol.                                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added CL_IllegalLogicalNameMessage function.      */
/*                                                           */
/*      6.30: Support for long long integers.                */
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

#ifndef _H_argacces

#pragma once

#define _H_argacces

#include "expressn.h"
#include "evaluatn.h"
#include "moduldef.h"

const char *CL_GetLogicalName (UDFContext *, const char *);
const char *CL_GetFileName (UDFContext *);
const char *CL_GetConstructName (UDFContext *, const char *, const char *);
void CL_ExpectedCountError (Environment *, const char *, int, unsigned int);
void CL_OpenErrorMessage (Environment *, const char *, const char *);
bool CL_CheckFunctionArgCount (Environment *, struct functionDefinition *,
			       int);
void CL_ExpectedTypeError0 (Environment *, const char *, unsigned int);
void CL_ExpectedTypeError1 (Environment *, const char *, unsigned int,
			    const char *);
void CL_ExpectedTypeError2 (Environment *, const char *, unsigned int);
Defmodule *CL_GetModuleName (UDFContext *, unsigned int, bool *);
void *CL_GetFactOrInstanceArgument (UDFContext *, unsigned int, UDFValue *);
void CL_IllegalLogicalNameMessage (Environment *, const char *);

#endif
