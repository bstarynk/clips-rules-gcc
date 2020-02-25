   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*             BASIC MATH FUNCTIONS MODULE             */
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
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
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
/*            Auto-float-dividend always enabled.            */
/*                                                           */
/*************************************************************/

#ifndef _H_bmathfun

#pragma once

#define _H_bmathfun

#include "evaluatn.h"

void CL_BasicMathFunctionDefinitions (Environment *);
void CL_AdditionFunction (Environment *, UDFContext *, UDFValue *);
void CL_MultiplicationFunction (Environment *, UDFContext *, UDFValue *);
void CL_SubtractionFunction (Environment *, UDFContext *, UDFValue *);
void CL_DivisionFunction (Environment *, UDFContext *, UDFValue *);
void CL_DivFunction (Environment *, UDFContext *, UDFValue *);
void CL_IntegerFunction (Environment *, UDFContext *, UDFValue *);
void CL_FloatFunction (Environment *, UDFContext *, UDFValue *);
void CL_AbsFunction (Environment *, UDFContext *, UDFValue *);
void CL_MinFunction (Environment *, UDFContext *, UDFValue *);
void CL_MaxFunction (Environment *, UDFContext *, UDFValue *);

#endif
