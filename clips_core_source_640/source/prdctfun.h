   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/08/18            */
   /*                                                     */
   /*            PREDICATE FUNCTIONS HEADER FILE          */
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
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
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
/*            Deprecated the pointerp function and added     */
/*            the external-addressp function.                */
/*                                                           */
/*************************************************************/

#ifndef _H_prdctfun

#pragma once

#define _H_prdctfun

   void                           CL_PredicateFunctionDefinitions(Environment *);
   void                           CL_EqFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_NeqFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_StringpFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SymbolpFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_LexemepFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_NumberpFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_FloatpFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_IntegerpFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_MultifieldpFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_ExternalAddresspFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_NotFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AndFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_OrFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_LessThanOrEqualFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_GreaterThanOrEqualFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_LessThanFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_GreaterThanFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_NumericEqualFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_NumericNotEqualFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_OddpFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_EvenpFunction(Environment *,UDFContext *,UDFValue *);

#endif /* _H_prdctfun */



