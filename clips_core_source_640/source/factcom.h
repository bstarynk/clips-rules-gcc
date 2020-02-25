   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*               FACT COMMANDS HEADER FILE             */
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
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            Increment/Decrement_ClearReadyLocks API.        */
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

#ifndef _H_factcom

#pragma once

#define _H_factcom

#include "evaluatn.h"

void CL_FactCommandDefinitions (Environment *);
void CL_AssertCommand (Environment *, UDFContext *, UDFValue *);
void CL_RetractCommand (Environment *, UDFContext *, UDFValue *);
void CL_AssertStringFunction (Environment *, UDFContext *, UDFValue *);
void CL_FactsCommand (Environment *, UDFContext *, UDFValue *);
void CL_Facts (Environment *, const char *, Defmodule *, long long, long long,
	       long long);
void CL_SetFactDuplicationCommand (Environment *, UDFContext *, UDFValue *);
void CL_GetFactDuplicationCommand (Environment *, UDFContext *, UDFValue *);
void CL_Save_FactsCommand (Environment *, UDFContext *, UDFValue *);
void CL_Load_FactsCommand (Environment *, UDFContext *, UDFValue *);
bool CL_Save_Facts (Environment *, const char *, CL_SaveScope);
bool CL_Save_FactsDriver (Environment *, const char *, CL_SaveScope,
			  struct expr *);
bool CL_Load_Facts (Environment *, const char *);
bool CL_Load_FactsFromString (Environment *, const char *, size_t);
void CL_FactIndexFunction (Environment *, UDFContext *, UDFValue *);

#endif /* _H_factcom */
