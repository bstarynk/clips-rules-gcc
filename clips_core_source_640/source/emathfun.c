   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/29/18             */
   /*                                                     */
   /*            EXTENDED MATH FUNCTIONS MODULE           */
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
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Gary D. Riley                                        */
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
/*      6.31: Fix for overflow error in div function.        */
/*                                                           */
/*      6.40: Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_GetCL_HaltExecution and       */
/*            SetCL_HaltExecution functions.                    */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Added error codes for get-error and            */
/*            clear-error functions.                         */
/*                                                           */
/*************************************************************/

#include "setup.h"
#include "argacces.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "miscfun.h"
#include "prntutil.h"
#include "router.h"

#include "emathfun.h"

#if EXTENDED_MATH_FUNCTIONS

#include <math.h>

/***************/
/* DEFINITIONS */
/***************/

#ifndef PI
#define PI   3.14159265358979323846
#endif

#ifndef PID2
#define PID2 1.57079632679489661923 /* PI divided by 2 */
#endif

#define SMALLEST_ALLOWED_NUMBER 1e-15
#define dtrunc(x) (((x) < 0.0) ? ceil(x) : floor(x))

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    SingleNumberCheck(UDFContext *,UDFValue *);
   static bool                    TestProximity(double,double);
   static void                    DoCL_mainErrorMessage(UDFContext *,UDFValue *);
   static void                    ArgumentOverflowErrorMessage(UDFContext *,UDFValue *);
   static void                    SingularityErrorMessage(UDFContext *,UDFValue *);
   static double                  genacosh(double);
   static double                  genasinh(double);
   static double                  genatanh(double);
   static double                  genasech(double);
   static double                  genacsch(double);
   static double                  genacoth(double);

/************************************************/
/* CL_ExtendedMathFunctionDefinitions: Initializes */
/*   the extended math functions.               */
/************************************************/
void CL_ExtendedMathFunctionDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   CL_AddUDF(theEnv,"cos","d",1,1,"ld",CL_CosFunction,"CL_CosFunction",NULL);
   CL_AddUDF(theEnv,"sin","d",1,1,"ld",CL_SinFunction,"CL_SinFunction",NULL);
   CL_AddUDF(theEnv,"tan","d",1,1,"ld",CL_TanFunction,"CL_TanFunction",NULL);
   CL_AddUDF(theEnv,"sec","d",1,1,"ld",CL_SecFunction,"CL_SecFunction",NULL);
   CL_AddUDF(theEnv,"csc","d",1,1,"ld",CL_CscFunction,"CL_CscFunction",NULL);
   CL_AddUDF(theEnv,"cot","d",1,1,"ld",CL_CotFunction,"CL_CotFunction",NULL);
   CL_AddUDF(theEnv,"acos","d",1,1,"ld",CL_AcosFunction,"CL_AcosFunction",NULL);
   CL_AddUDF(theEnv,"asin","d",1,1,"ld",CL_AsinFunction,"CL_AsinFunction",NULL);
   CL_AddUDF(theEnv,"atan","d",1,1,"ld",CL_AtanFunction,"CL_AtanFunction",NULL);
   CL_AddUDF(theEnv,"asec","d",1,1,"ld",CL_AsecFunction,"CL_AsecFunction",NULL);
   CL_AddUDF(theEnv,"acsc","d",1,1,"ld",CL_AcscFunction,"CL_AcscFunction",NULL);
   CL_AddUDF(theEnv,"acot","d",1,1,"ld",CL_AcotFunction,"CL_AcotFunction",NULL);
   CL_AddUDF(theEnv,"cosh","d",1,1,"ld",CL_CoshFunction,"CL_CoshFunction",NULL);
   CL_AddUDF(theEnv,"sinh","d",1,1,"ld",CL_SinhFunction,"CL_SinhFunction",NULL);
   CL_AddUDF(theEnv,"tanh","d",1,1,"ld",CL_TanhFunction,"CL_TanhFunction",NULL);
   CL_AddUDF(theEnv,"sech","d",1,1,"ld",CL_SechFunction,"CL_SechFunction",NULL);
   CL_AddUDF(theEnv,"csch","d",1,1,"ld",CL_CschFunction,"CL_CschFunction",NULL);
   CL_AddUDF(theEnv,"coth","d",1,1,"ld",CL_CothFunction,"CL_CothFunction",NULL);
   CL_AddUDF(theEnv,"acosh","d",1,1,"ld",CL_AcoshFunction,"CL_AcoshFunction",NULL);
   CL_AddUDF(theEnv,"asinh","d",1,1,"ld",CL_AsinhFunction,"CL_AsinhFunction",NULL);
   CL_AddUDF(theEnv,"atanh","d",1,1,"ld",CL_AtanhFunction,"CL_AtanhFunction",NULL);
   CL_AddUDF(theEnv,"asech","d",1,1,"ld",CL_AsechFunction,"CL_AsechFunction",NULL);
   CL_AddUDF(theEnv,"acsch","d",1,1,"ld",CL_AcschFunction,"CL_AcschFunction",NULL);
   CL_AddUDF(theEnv,"acoth","d",1,1,"ld",CL_AcothFunction,"CL_AcothFunction",NULL);

   CL_AddUDF(theEnv,"mod","ld",2,2,"ld",CL_ModFunction,"CL_ModFunction",NULL);
   CL_AddUDF(theEnv,"exp","d", 1,1,"ld",CL_ExpFunction,"CL_ExpFunction",NULL);
   CL_AddUDF(theEnv,"log","d",1,1,"ld",CL_LogFunction,"CL_LogFunction",NULL);
   CL_AddUDF(theEnv,"log10","d",1,1,"ld",CL_Log10Function,"CL_Log10Function",NULL);
   CL_AddUDF(theEnv,"sqrt","d",1,1,"ld",CL_SqrtFunction,"CL_SqrtFunction",NULL);
   CL_AddUDF(theEnv,"pi","d",0,0,NULL,CL_PiFunction, "CL_PiFunction",NULL);
   CL_AddUDF(theEnv,"deg-rad","d",1,1,"ld",CL_DegRadFunction, "CL_DegRadFunction",NULL);
   CL_AddUDF(theEnv,"rad-deg","d",1,1,"ld",CL_RadDegFunction, "CL_RadDegFunction",NULL);
   CL_AddUDF(theEnv,"deg-grad","d",1,1,"ld",CL_DegGradFunction,"CL_DegGradFunction",NULL);
   CL_AddUDF(theEnv,"grad-deg","d",1,1,"ld",CL_GradDegFunction,"CL_GradDegFunction",NULL);
   CL_AddUDF(theEnv,"**","d",2,2,"ld",CL_PowFunction,"CL_PowFunction",NULL);
   CL_AddUDF(theEnv,"round","l", 1,1,"ld",CL_RoundFunction,"CL_RoundFunction",NULL);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

/************************************************************/
/* SingleNumberCheck: Retrieves the numeric argument for    */
/*   extended math functions which expect a single floating */
/*   point argument.                                        */
/************************************************************/
static bool SingleNumberCheck(
  UDFContext *context,
  UDFValue *returnValue)
  {
   /*======================================*/
   /* Check that the argument is a number. */
   /*======================================*/

   if (! CL_UDFNthArgument(context,1,NUMBER_BITS,returnValue))
     {
      returnValue->floatValue = CL_CreateFloat(context->environment,0.0);
      return false;
     }

   return true;
  }

/**************************************************************/
/* TestProximity: Returns true if the specified number falls  */
/*   within the specified range, otherwise false is returned. */
/**************************************************************/
static bool TestProximity(
  double theNumber,
  double range)
  {
   if ((theNumber >= (- range)) && (theNumber <= range)) return true;
   else return false;
  }

/********************************************************/
/* DoCL_mainErrorMessage: Generic error message used when  */
/*   a doCL_main error is detected during a call to one of */
/*   the extended math functions.                       */
/********************************************************/
static void DoCL_mainErrorMessage(
  UDFContext *context,
  UDFValue *returnValue)
  {
   Environment *theEnv = context->environment;

   CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"DOMAIN_ERROR")->header);
   CL_PrintErrorID(theEnv,"EMATHFUN",1,false);
   CL_WriteString(theEnv,STDERR,"DoCL_main error for '");
   CL_WriteString(theEnv,STDERR,CL_UDFContextFunctionName(context));
   CL_WriteString(theEnv,STDERR,"' function.\n");
   SetCL_HaltExecution(theEnv,true);
   SetCL_EvaluationError(theEnv,true);
   returnValue->floatValue = CL_CreateFloat(theEnv,0.0);
  }

/************************************************************/
/* ArgumentOverflowErrorMessage: Generic error message used */
/*   when an argument overflow is detected during a call to */
/*   one of the extended math functions.                    */
/************************************************************/
static void ArgumentOverflowErrorMessage(
  UDFContext *context,
  UDFValue *returnValue)
  {
   Environment *theEnv = context->environment;

   CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"ARGUMENT_OVERFLOW")->header);
   CL_PrintErrorID(theEnv,"EMATHFUN",2,false);
   CL_WriteString(theEnv,STDERR,"Argument overflow for '");
   CL_WriteString(theEnv,STDERR,CL_UDFContextFunctionName(context));
   CL_WriteString(theEnv,STDERR,"' function.\n");
   SetCL_HaltExecution(theEnv,true);
   SetCL_EvaluationError(theEnv,true);
   returnValue->floatValue = CL_CreateFloat(theEnv,0.0);
  }

/************************************************************/
/* SingularityErrorMessage: Generic error message used when */
/*   a singularity is detected during a call to one of the  */
/*   extended math functions.                               */
/************************************************************/
static void SingularityErrorMessage(
  UDFContext *context,
  UDFValue *returnValue)
  {
   Environment *theEnv = context->environment;

   CL_SetErrorValue(theEnv,&CL_CreateSymbol(theEnv,"SINGULARITY_AT_ASYMPTOTE")->header);
   CL_PrintErrorID(theEnv,"EMATHFUN",3,false);
   CL_WriteString(theEnv,STDERR,"Singularity at asymptote in '");
   CL_WriteString(theEnv,STDERR,CL_UDFContextFunctionName(context));
   CL_WriteString(theEnv,STDERR,"' function.\n");
   SetCL_HaltExecution(theEnv,true);
   SetCL_EvaluationError(theEnv,true);
   returnValue->floatValue = CL_CreateFloat(theEnv,0.0);
  }

/*************************************/
/* CL_CosFunction: H/L access routine   */
/*   for the cos function.           */
/*************************************/
void CL_CosFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,cos(CVCoerceToFloat(returnValue)));
  }

/*************************************/
/* CL_SinFunction: H/L access routine   */
/*   for the sin function.           */
/*************************************/
void CL_SinFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,sin(CVCoerceToFloat(returnValue)));
  }

/*************************************/
/* CL_TanFunction: H/L access routine   */
/*   for the tan function.           */
/*************************************/
void CL_TanFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double tv;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   tv = cos(CVCoerceToFloat(returnValue));
   if ((tv < SMALLEST_ALLOWED_NUMBER) && (tv > -SMALLEST_ALLOWED_NUMBER))
     {
      SingularityErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,sin(CVCoerceToFloat(returnValue)) / tv);
  }

/*************************************/
/* CL_SecFunction: H/L access routine   */
/*   for the sec function.           */
/*************************************/
void CL_SecFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double tv;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   tv = cos(CVCoerceToFloat(returnValue));
   if ((tv < SMALLEST_ALLOWED_NUMBER) && (tv > -SMALLEST_ALLOWED_NUMBER))
     {
      SingularityErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,1.0 / tv);
  }

/*************************************/
/* CL_CscFunction: H/L access routine   */
/*   for the csc function.           */
/*************************************/
void CL_CscFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double tv;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   tv = sin(CVCoerceToFloat(returnValue));
   if ((tv < SMALLEST_ALLOWED_NUMBER) && (tv > -SMALLEST_ALLOWED_NUMBER))
     {
      SingularityErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,1.0 / tv);
  }

/*************************************/
/* CL_CotFunction: H/L access routine   */
/*   for the cot function.           */
/*************************************/
void CL_CotFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double tv;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

    tv = sin(CVCoerceToFloat(returnValue));
    if ((tv < SMALLEST_ALLOWED_NUMBER) && (tv > -SMALLEST_ALLOWED_NUMBER))
      {
       SingularityErrorMessage(context,returnValue);
       return;
      }

    returnValue->floatValue = CL_CreateFloat(theEnv,cos(CVCoerceToFloat(returnValue)) / tv);
  }

/**************************************/
/* CL_AcosFunction: H/L access routine   */
/*   for the acos function.           */
/**************************************/
void CL_AcosFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;

   CL_ClearErrorValue(theEnv);
   
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);

   if ((num > 1.0) || (num < -1.0))
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

    returnValue->floatValue = CL_CreateFloat(theEnv,acos(num));
  }

/**************************************/
/* CL_AsinFunction: H/L access routine   */
/*   for the asin function.           */
/**************************************/
void CL_AsinFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if ((num > 1.0) || (num < -1.0))
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,asin(num));
  }

/**************************************/
/* CL_AtanFunction: H/L access routine   */
/*   for the atan function.           */
/**************************************/
void CL_AtanFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,atan(CVCoerceToFloat(returnValue)));
  }

/**************************************/
/* CL_AsecFunction: H/L access routine   */
/*   for the asec function.           */
/**************************************/
void CL_AsecFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if ((num < 1.0) && (num > -1.0))
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   num = 1.0 / num;
   returnValue->floatValue = CL_CreateFloat(theEnv,acos(num));
  }

/**************************************/
/* CL_AcscFunction: H/L access routine   */
/*   for the acsc function.           */
/**************************************/
void CL_AcscFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if ((num < 1.0) && (num > -1.0))
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   num = 1.0 / num;
   returnValue->floatValue = CL_CreateFloat(theEnv,asin(num));
  }

/**************************************/
/* CL_AcotFunction: H/L access routine   */
/*   for the acot function.           */
/**************************************/
void CL_AcotFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if (TestProximity(num,1e-25) == true)
     {
      returnValue->floatValue = CL_CreateFloat(theEnv,PID2);
      return;
     }

   num = 1.0 / num;
   returnValue->floatValue = CL_CreateFloat(theEnv,atan(num));
  }

/**************************************/
/* CL_CoshFunction: H/L access routine   */
/*   for the cosh function.           */
/**************************************/
void CL_CoshFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,cosh(CVCoerceToFloat(returnValue)));
  }

/**************************************/
/* CL_SinhFunction: H/L access routine   */
/*   for the sinh function.           */
/**************************************/
void CL_SinhFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,sinh(CVCoerceToFloat(returnValue)));
  }

/**************************************/
/* CL_TanhFunction: H/L access routine   */
/*   for the tanh function.           */
/**************************************/
void CL_TanhFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,tanh(CVCoerceToFloat(returnValue)));
  }

/**************************************/
/* CL_SechFunction: H/L access routine   */
/*   for the sech function.           */
/**************************************/
void CL_SechFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,1.0 / cosh(CVCoerceToFloat(returnValue)));
  }

/**************************************/
/* CL_CschFunction: H/L access routine   */
/*   for the csch function.           */
/**************************************/
void CL_CschFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if (num == 0.0)
     {
      SingularityErrorMessage(context,returnValue);
      return;
     }
   else if (TestProximity(num,1e-25) == true)
     {
      ArgumentOverflowErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,1.0 / sinh(num));
  }

/**************************************/
/* CL_CothFunction: H/L access routine   */
/*   for the coth function.           */
/**************************************/
void CL_CothFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if (num == 0.0)
     {
      SingularityErrorMessage(context,returnValue);
      return;
     }
   else if (TestProximity(num,1e-25) == true)
     {
      ArgumentOverflowErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,1.0 / tanh(num));
  }

/***************************************/
/* CL_AcoshFunction: H/L access routine   */
/*   for the acosh function.           */
/***************************************/
void CL_AcoshFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if (num < 1.0)
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,genacosh(num));
  }

/***************************************/
/* CL_AsinhFunction: H/L access routine   */
/*   for the asinh function.           */
/***************************************/
void CL_AsinhFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,genasinh(CVCoerceToFloat(returnValue)));
  }

/***************************************/
/* CL_AtanhFunction: H/L access routine   */
/*   for the atanh function.           */
/***************************************/
void CL_AtanhFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if ((num >= 1.0) || (num <= -1.0))
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,genatanh(num));
  }

/***************************************/
/* CL_AsechFunction: H/L access routine   */
/*   for the asech function.           */
/***************************************/
void CL_AsechFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if ((num > 1.0) || (num <= 0.0))
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,genasech(num));
  }

/***************************************/
/* CL_AcschFunction: H/L access routine   */
/*   for the acsch function.           */
/***************************************/
void CL_AcschFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if (num == 0.0)
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,genacsch(num));
  }

/***************************************/
/* CL_AcothFunction: H/L access routine   */
/*   for the acoth function.           */
/***************************************/
void CL_AcothFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if ((num <= 1.0) && (num >= -1.0))
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,genacoth(num));
  }

/*************************************/
/* CL_ExpFunction: H/L access routine   */
/*   for the exp function.           */
/*************************************/
void CL_ExpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,exp(CVCoerceToFloat(returnValue)));
  }

/*************************************/
/* CL_LogFunction: H/L access routine   */
/*   for the log function.           */
/*************************************/
void CL_LogFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if (num < 0.0)
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }
   else if (num == 0.0)
     {
      ArgumentOverflowErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,log(num));
  }

/***************************************/
/* CL_Log10Function: H/L access routine   */
/*   for the log10 function.           */
/***************************************/
void CL_Log10Function(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if (num < 0.0)
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }
   else if (num == 0.0)
     {
      ArgumentOverflowErrorMessage(context,returnValue);
      return;
     }

    returnValue->floatValue = CL_CreateFloat(theEnv,log10(num));
   }

/**************************************/
/* CL_SqrtFunction: H/L access routine   */
/*   for the sqrt function.           */
/**************************************/
void CL_SqrtFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double num;
   
   CL_ClearErrorValue(theEnv);

   if (! SingleNumberCheck(context,returnValue))
     { return; }

   num = CVCoerceToFloat(returnValue);
   if (num < 0.00000)
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   returnValue->floatValue = CL_CreateFloat(theEnv,sqrt(num));
  }

/*************************************/
/* CL_PowFunction: H/L access routine   */
/*   for the pow function.           */
/*************************************/
void CL_PowFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue value1, value2;
   double num1, num2;
   
   CL_ClearErrorValue(theEnv);

   /*==================================*/
   /* Check for two numeric arguments. */
   /*==================================*/

   if (! CL_UDFNthArgument(context,1,NUMBER_BITS,&value1))
     { return; }

   if (! CL_UDFNthArgument(context,2,NUMBER_BITS,&value2))
     { return; }

    /*=====================*/
    /* DoCL_main error check. */
    /*=====================*/

    num1 = CVCoerceToFloat(&value1);
    num2 = CVCoerceToFloat(&value2);

    if (((num1 == 0.0) && (num2 <= 0.0)) ||
       ((num1 < 0.0) && (dtrunc(num2) != num2)))
     {
      DoCL_mainErrorMessage(context,returnValue);
      return;
     }

   /*============================*/
   /* Compute and set the value. */
   /*============================*/

   returnValue->floatValue = CL_CreateFloat(theEnv,pow(num1,num2));
  }

/*************************************/
/* CL_ModFunction: H/L access routine   */
/*   for the mod function.           */
/*************************************/
void CL_ModFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item1, item2;
   double fnum1, fnum2;
   long long lnum1, lnum2;

   /*==================================*/
   /* Check for two numeric arguments. */
   /*==================================*/

   if (! CL_UDFNthArgument(context,1,NUMBER_BITS,&item1))
     { return; }

   if (! CL_UDFNthArgument(context,2,NUMBER_BITS,&item2))
     { return; }

   /*===========================*/
   /* Check for divide by zero. */
   /*===========================*/

   if ((CVIsType(&item2,INTEGER_BIT) ? (item2.integerValue->contents == 0L) : false) ||
       (CVIsType(&item2,FLOAT_BIT) ? (item2.floatValue->contents == 0.0) : false))
     {
      CL_DivideByZeroErrorMessage(theEnv,"mod");
      SetCL_EvaluationError(theEnv,true);
      returnValue->integerValue = CL_CreateInteger(theEnv,0);
      return;
     }

   /*===========================*/
   /* Compute the return value. */
   /*===========================*/

   if (CVIsType(&item1,FLOAT_BIT) || CVIsType(&item2,FLOAT_BIT))
     {
      fnum1 = CVCoerceToFloat(&item1);
      fnum2 = CVCoerceToFloat(&item2);
      returnValue->floatValue = CL_CreateFloat(theEnv,fnum1 - (dtrunc(fnum1 / fnum2) * fnum2));
     }
   else
     {
      lnum1 = item1.integerValue->contents;
      lnum2 = item2.integerValue->contents;
      
      if ((lnum1 == LLONG_MIN) && (lnum2 == -1))
        {
         CL_ArgumentOverUnderflowErrorMessage(theEnv,"mod",true);
         SetCL_EvaluationError(theEnv,true);
         returnValue->integerValue = CL_CreateInteger(theEnv,0);
         return;
        }

      returnValue->integerValue = CL_CreateInteger(theEnv,lnum1 - (lnum1 / lnum2) * lnum2);
     }
  }

/************************************/
/* CL_PiFunction: H/L access routine   */
/*   for the pi function.           */
/************************************/
void CL_PiFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->floatValue = CL_CreateFloat(theEnv,acos(-1.0));
  }

/****************************************/
/* CL_DegRadFunction: H/L access routine   */
/*   for the deg-rad function.          */
/****************************************/
void CL_DegRadFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,CVCoerceToFloat(returnValue) * PI / 180.0);
  }

/****************************************/
/* CL_RadDegFunction: H/L access routine   */
/*   for the rad-deg function.          */
/****************************************/
void CL_RadDegFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,CVCoerceToFloat(returnValue) * 180.0 / PI);
  }

/*****************************************/
/* CL_DegGradFunction: H/L access routine   */
/*   for the deg-grad function.          */
/*****************************************/
void CL_DegGradFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,CVCoerceToFloat(returnValue) / 0.9);
  }

/*****************************************/
/* CL_GradDegFunction: H/L access routine   */
/*   for the grad-deg function.          */
/*****************************************/
void CL_GradDegFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   if (! SingleNumberCheck(context,returnValue))
     { return; }

   returnValue->floatValue = CL_CreateFloat(theEnv,CVCoerceToFloat(returnValue) * 0.9);
  }

/***************************************/
/* CL_RoundFunction: H/L access routine   */
/*   for the round function.           */
/***************************************/
void CL_RoundFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   /*======================================*/
   /* Check that the argument is a number. */
   /*======================================*/

   if (! CL_UDFNthArgument(context,1,NUMBER_BITS,returnValue))
     { return; }

   /*==============================*/
   /* Round float type to integer. */
   /*==============================*/

   if (CVIsType(returnValue,FLOAT_BIT))
     { returnValue->integerValue = CL_CreateInteger(theEnv,(long long) ceil(returnValue->floatValue->contents - 0.5)); }
  }

/*******************************************/
/* genacosh: Generic routine for computing */
/*   the hyperbolic arccosine.             */
/*******************************************/
static double genacosh(
  double num)
  {
   return(log(num + sqrt(num * num - 1.0)));
  }

/*******************************************/
/* genasinh: Generic routine for computing */
/*   the hyperbolic arcsine.               */
/*******************************************/
static double genasinh(
  double num)
  {
   return(log(num + sqrt(num * num + 1.0)));
  }

/*******************************************/
/* genatanh: Generic routine for computing */
/*   the hyperbolic arctangent.            */
/*******************************************/
static double genatanh(
  double num)
  {
   return((0.5) * log((1.0 + num) / (1.0 - num)));
  }

/*******************************************/
/* genasech: Generic routine for computing */
/*   the hyperbolic arcsecant.             */
/*******************************************/
static double genasech(
  double num)
  {
   return(log(1.0 / num + sqrt(1.0 / (num * num) - 1.0)));
  }

/*******************************************/
/* genacsch: Generic routine for computing */
/*   the hyperbolic arccosecant.           */
/*******************************************/
static double genacsch(
  double num)
  {
   return(log(1.0 / num + sqrt(1.0 / (num * num) + 1.0)));
  }

/*******************************************/
/* genacoth: Generic routine for computing */
/*   the hyperbolic arccotangent.          */
/*******************************************/
static double genacoth(
  double num)
  {
   return((0.5) * log((num + 1.0) / (num - 1.0)));
  }

#endif

