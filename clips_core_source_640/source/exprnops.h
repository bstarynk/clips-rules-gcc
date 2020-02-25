   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/18/16            */
   /*                                                     */
   /*          EXPRESSION OPERATIONS HEADER FILE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides utility routines for manipulating and   */
/*   examining expressions.                                  */
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
/*      6.30: Add CL_NegateExpression function.                 */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#ifndef _H_exprnops

#pragma once

#define _H_exprnops

#include "expressn.h"

bool CL_ConstantExpression (struct expr *);
void CL_PrintExpression (Environment *, const char *, struct expr *);
unsigned long CL_ExpressionSize (struct expr *);
unsigned short CL_CountArguments (struct expr *);
struct expr *CL_CopyExpression (Environment *, struct expr *);
bool CL_ExpressionContainsVariables (struct expr *, bool);
bool CL_IdenticalExpression (struct expr *, struct expr *);
struct expr *CL_GenConstant (Environment *, unsigned short, void *);
bool CL_CheckArgumentAgainstRestriction (Environment *, struct expr *,
					 unsigned);
bool CL_ConstantType (int);
struct expr *CL_CombineExpressions (Environment *, struct expr *,
				    struct expr *);
struct expr *CL_AppendExpressions (struct expr *, struct expr *);
struct expr *CL_NegateExpression (Environment *, struct expr *);

#endif /* _H_exprnops */
