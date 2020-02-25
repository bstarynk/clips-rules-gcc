   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*           MULTIFIELD_TYPE FUNCTIONS HEADER FILE          */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary Riley and Brian Dantes                          */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*            Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Moved CL_ImplodeMultifield to multifld.c.         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Fixed memory leaks when error occurred.        */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed linkage issue when DEFMODULE_CONSTRUCT   */
/*            compiler flag is set to 0.                     */
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
/*************************************************************/

#ifndef _H_multifun

#pragma once

#define _H_multifun

#include "evaluatn.h"

#define VALUE_NOT_FOUND SIZE_MAX

   void                    CL_MultifieldFunctionDefinitions(Environment *);
#if MULTIFIELD_FUNCTIONS
   void                    CL_DeleteFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_ReplaceFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_Delete_MemberFunction(Environment *,UDFContext *,UDFValue *);
   void                    Replace_MemberFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_InsertFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_ExplodeFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_ImplodeFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_SubseqFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_FirstFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_RestFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_NthFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_SubsetpFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_MemberFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_Multifield_PrognFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_ForeachFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_GetMvPrognField(Environment *,UDFContext *,UDFValue *);
   void                    CL_GetMvPrognIndex(Environment *,UDFContext *,UDFValue *);
   bool                    CL_FindDOsInSegment(UDFValue *,unsigned int,UDFValue *,
                                            size_t *,size_t *,size_t *,unsigned int);
#endif
   bool                    CL_ReplaceMultiValueFieldSizet(Environment *,UDFValue *,UDFValue *,
                                                  size_t,size_t,UDFValue *,const char *);
   bool                    CL_InsertMultiValueField(Environment *,UDFValue *,UDFValue *,
                                                 size_t,UDFValue *,const char *);
   void                    CL_MVRangeError(Environment *,long long,long long,size_t,const char *);
   size_t                  CL_FindValueInMultifield(UDFValue *,UDFValue *);

#endif /* _H_multifun */

