   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/26/17            */
   /*                                                     */
   /*          STRING_TYPE FUNCTIONS HEADER FILE          */
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
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added support for UTF-8 strings to str-length, */
/*            str-index, and sub-string functions.           */
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
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_strngfun

#pragma once

#define _H_strngfun

#include "entities.h"

typedef enum
  {
   EE_NO_ERROR = 0,
   EE_PARSING_ERROR,
   EE_PROCESSING_ERROR
  } CL_EvalError;

typedef enum
  {
   BE_NO_ERROR = 0,
   BE_COULD_NOT_BUILD_ERROR,
   BE_CONSTRUCT_NOT_FOUND_ERROR,
   BE_PARSING_ERROR,
  } CL_BuildError;

   CL_BuildError                     CL_Build(Environment *,const char *);
   CL_EvalError                      CL_Eval(Environment *,const char *,CLIPSValue *);
   void                           CL_StringFunctionDefinitions(Environment *);
   void                           CL_StrCatFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SymCatFunction(Environment *,UDFContext *,UDFValue *);
   void                           Str_LengthFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_UpcaseFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_LowcaseFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_StrCompareFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SubStringFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_StrIndexFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_EvalFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_BuildFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_StringToFieldFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_StringToField(Environment *,const char *,UDFValue *);

#endif /* _H_strngfun */






