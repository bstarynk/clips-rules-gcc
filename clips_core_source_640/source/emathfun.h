   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*          EXTENDED MATH FUNCTIONS HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for numerous extended math     */
/*   functions including cos, sin, tan, sec, csc, cot, acos, */
/*   asin, atan, asec, acsc, acot, cosh, sinh, tanh, sech,   */
/*   csch, coth, acosh, asinh, atanh, asech, acsch, acoth,   */
/*   mod, exp, log, log10, sqrt, pi, deg-rad, rad-deg,       */
/*   deg-grad, grad-deg, **, and round.                      */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Renamed EX_MATH compiler flag to               */
/*            EXTENDED_MATH_FUNCTIONS.                       */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_emathfun

#pragma once

#define _H_emathfun

   void                           CL_ExtendedMathFunctionDefinitions(Environment *);
#if EXTENDED_MATH_FUNCTIONS
   void                           CL_CosFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SinFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_TanFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SecFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_CscFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_CotFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AcosFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AsinFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AtanFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AsecFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AcscFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AcotFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_CoshFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SinhFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_TanhFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_SechFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_CschFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_CothFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AcoshFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AsinhFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AtanhFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AsechFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AcschFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_AcothFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_RoundFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_ModFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_ExpFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_LogFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_Log10Function(Environment *,UDFContext *,UDFValue *);
   void                           CL_SqrtFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_PiFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_DegRadFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_RadDegFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_DegGradFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_GradDegFunction(Environment *,UDFContext *,UDFValue *);
   void                           CL_PowFunction(Environment *,UDFContext *,UDFValue *);
#endif

#endif /* _H_emathfun */



