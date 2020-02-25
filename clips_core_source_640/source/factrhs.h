   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*        FACT RHS PATTERN PARSER HEADER MODULE        */
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
/*      6.30: Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*************************************************************/

#ifndef _H_factrhs

#pragma once

#define _H_factrhs

#include "expressn.h"
#include "factmngr.h"
#include "scanner.h"
#include "symbol.h"

   struct expr                   *CL_BuildRHS_Assert(Environment *,const char *,struct token *,bool *,bool,bool,const char *);
   struct expr                   *Get_AssertArgument(Environment *,const char *,struct token *,bool *,TokenType,bool,bool *);
   struct expr                   *CL_GetRHSPattern(Environment*,const char *,struct token *,bool *,bool,
                                                       bool,bool,TokenType);
   Fact                          *CL_StringToFact(Environment *,const char *);

#endif /* _H_factrhs */

