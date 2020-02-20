   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/29/18             */
   /*                                                     */
   /*             BASIC MATH FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for numerous basic math        */
/*   functions including +, *, -, /, integer, float, div,    */
/*   abs,set-auto-float-dividend, get-auto-float-dividend,   */
/*   min, and max.                                           */
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
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.31: Fix for overflow error in div function.        */
/*                                                           */
/*      6.40: Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_GetCL_HaltExecution and       */
/*            SetCL_HaltExecution functions.                    */
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

#include <stdio.h>

#include "setup.h"

#include "argacces.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "prntutil.h"
#include "router.h"

#include "bmathfun.h"

/***************************************************************/
/* CL_BasicMathFunctionDefinitions: Defines basic math functions. */
/***************************************************************/
void CL_BasicMathFunctionDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   CL_AddUDF(theEnv,"+","ld",2,UNBOUNDED,"ld",CL_AdditionFunction,"CL_AdditionFunction",NULL);
   CL_AddUDF(theEnv,"*","ld",2,UNBOUNDED,"ld",CL_MultiplicationFunction,"CL_MultiplicationFunction",NULL);
   CL_AddUDF(theEnv,"-","ld",2,UNBOUNDED,"ld",CL_SubtractionFunction,"CL_SubtractionFunction",NULL);
   CL_AddUDF(theEnv,"/","d",2,UNBOUNDED,"ld",CL_DivisionFunction,"CL_DivisionFunction",NULL);
   CL_AddUDF(theEnv,"div","l",2,UNBOUNDED,"ld",CL_DivFunction,"CL_DivFunction",NULL);
   CL_AddUDF(theEnv,"integer","l",1,1,"ld",CL_IntegerFunction,"CL_IntegerFunction",NULL);
   CL_AddUDF(theEnv,"float","d",1,1,"ld",CL_FloatFunction,"CL_FloatFunction",NULL);
   CL_AddUDF(theEnv,"abs","ld",1,1,"ld",CL_AbsFunction,"CL_AbsFunction",NULL);
   CL_AddUDF(theEnv,"min","ld",1,UNBOUNDED,"ld",CL_MinFunction,"CL_MinFunction",NULL);
   CL_AddUDF(theEnv,"max","ld",1,UNBOUNDED,"ld",CL_MaxFunction,"CL_MaxFunction",NULL);
#endif
  }

/**********************************/
/* CL_AdditionFunction: H/L access   */
/*   routine for the + function.  */
/**********************************/
void CL_AdditionFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double ftotal = 0.0;
   long long ltotal = 0LL;
   bool useFloatTotal = false;
   UDFValue theArg;

   /*=================================================*/
   /* Loop through each of the arguments adding it to */
   /* a running total. If a floating point number is  */
   /* encountered, then do all subsequent operations  */
   /* using floating point values.                    */
   /*=================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&theArg))
        { return; }

      if (useFloatTotal)
        { ftotal += CVCoerceToFloat(&theArg); }
      else
        {
         if (CVIsType(&theArg,INTEGER_BIT))
           { ltotal += theArg.integerValue->contents; }
         else
           {
            ftotal = (double) ltotal + CVCoerceToFloat(&theArg);
            useFloatTotal = true;
           }
        }
     }

   /*======================================================*/
   /* If a floating point number was in the argument list, */
   /* then return a float, otherwise return an integer.    */
   /*======================================================*/

   if (useFloatTotal)
     { returnValue->floatValue = CL_CreateFloat(theEnv,ftotal); }
   else
     { returnValue->integerValue = CL_CreateInteger(theEnv,ltotal); }
  }

/****************************************/
/* CL_MultiplicationFunction: CLIPS access */
/*   routine for the * function.        */
/****************************************/
void CL_MultiplicationFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double ftotal = 1.0;
   long long ltotal = 1LL;
   bool useFloatTotal = false;
   UDFValue theArg;

   /*===================================================*/
   /* Loop through each of the arguments multiplying it */
   /* by a running product. If a floating point number  */
   /* is encountered, then do all subsequent operations */
   /* using floating point values.                      */
   /*===================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&theArg))
        { return; }

      if (useFloatTotal)
        { ftotal *= CVCoerceToFloat(&theArg); }
      else
        {
         if (CVIsType(&theArg,INTEGER_BIT))
           { ltotal *= theArg.integerValue->contents; }
         else
           {
            ftotal = (double) ltotal * CVCoerceToFloat(&theArg);
            useFloatTotal = true;
           }
        }
     }

   /*======================================================*/
   /* If a floating point number was in the argument list, */
   /* then return a float, otherwise return an integer.    */
   /*======================================================*/

   if (useFloatTotal)
     { returnValue->floatValue = CL_CreateFloat(theEnv,ftotal); }
   else
     { returnValue->integerValue = CL_CreateInteger(theEnv,ltotal); }
  }

/*************************************/
/* CL_SubtractionFunction: CLIPS access */
/*   routine for the - function.     */
/*************************************/
void CL_SubtractionFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double ftotal = 0.0;
   long long ltotal = 0LL;
   bool useFloatTotal = false;
   UDFValue theArg;

   /*=================================================*/
   /* Get the first argument. This number which will  */
   /* be the starting total from which all subsequent */
   /* arguments will subtracted.                      */
   /*=================================================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&theArg))
     { return; }

   if (CVIsType(&theArg,INTEGER_BIT))
     { ltotal = theArg.integerValue->contents; }
   else
     {
      ftotal = CVCoerceToFloat(&theArg);
      useFloatTotal = true;
     }

   /*===================================================*/
   /* Loop through each of the arguments subtracting it */
   /* from a running total. If a floating point number  */
   /* is encountered, then do all subsequent operations */
   /* using floating point values.                      */
   /*===================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&theArg))
        { return; }

      if (useFloatTotal)
        { ftotal -= CVCoerceToFloat(&theArg); }
      else
        {
         if (CVIsType(&theArg,INTEGER_BIT))
           { ltotal -= theArg.integerValue->contents; }
         else
           {
            ftotal = (double) ltotal - theArg.floatValue->contents;
            useFloatTotal = true;
           }
        }
     }

   /*======================================================*/
   /* If a floating point number was in the argument list, */
   /* then return a float, otherwise return an integer.    */
   /*======================================================*/

   if (useFloatTotal)
     { returnValue->floatValue = CL_CreateFloat(theEnv,ftotal); }
   else
     { returnValue->integerValue = CL_CreateInteger(theEnv,ltotal); }
  }

/***********************************/
/* CL_DivisionFunction:  CLIPS access */
/*   routine for the / function.   */
/***********************************/
void CL_DivisionFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   double ftotal = 1.0;
   double theNumber;
   UDFValue theArg;

   /*===================================================*/
   /* Get the first argument. This number which will be */
   /* the starting product from which all subsequent    */
   /* arguments will divide. If the auto float dividend */
   /* feature is enable, then this number is converted  */
   /* to a float if it is an integer.                   */
   /*===================================================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&theArg))
     { return; }

   ftotal = CVCoerceToFloat(&theArg);

   /*====================================================*/
   /* Loop through each of the arguments dividing it     */
   /* into a running product. If a floating point number */
   /* is encountered, then do all subsequent operations  */
   /* using floating point values. Each argument is      */
   /* checked to prevent a divide by zero error.         */
   /*====================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&theArg))
        { return; }

      theNumber = CVCoerceToFloat(&theArg);

      if (theNumber == 0.0)
        {
         CL_DivideByZeroErrorMessage(theEnv,"/");
         SetCL_EvaluationError(theEnv,true);
         returnValue->floatValue = CL_CreateFloat(theEnv,1.0);
         return;
        }

      ftotal /= theNumber;
     }

   /*======================================================*/
   /* If a floating point number was in the argument list, */
   /* then return a float, otherwise return an integer.    */
   /*======================================================*/

   returnValue->floatValue = CL_CreateFloat(theEnv,ftotal);
  }

/*************************************/
/* CL_DivFunction: H/L access routine   */
/*   for the div function.           */
/*************************************/
void CL_DivFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   long long total = 1LL;
   UDFValue theArg;
   long long theNumber;

   /*===================================================*/
   /* Get the first argument. This number which will be */
   /* the starting product from which all subsequent    */
   /* arguments will divide.                            */
   /*===================================================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&theArg))
     { return; }
   total = CVCoerceToInteger(&theArg);

   /*=====================================================*/
   /* Loop through each of the arguments dividing it into */
   /* a running product. Floats are converted to integers */
   /* and each argument is checked to prevent a divide by */
   /* zero error.                                         */
   /*=====================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&theArg))
        { return; }

      theNumber = CVCoerceToInteger(&theArg);

      if (theNumber == 0LL)
        {
         CL_DivideByZeroErrorMessage(theEnv,"div");
         SetCL_EvaluationError(theEnv,true);
         returnValue->integerValue = CL_CreateInteger(theEnv,1L);
         return;
        }

      if ((total == LLONG_MIN) && (theNumber == -1))
        {
         CL_ArgumentOverUnderflowErrorMessage(theEnv,"div",true);
         SetCL_EvaluationError(theEnv,true);
         returnValue->integerValue = CL_CreateInteger(theEnv,1L);
         return;
        }
      
      total /= theNumber;
     }

   /*======================================================*/
   /* The result of the div function is always an integer. */
   /*======================================================*/

   returnValue->integerValue = CL_CreateInteger(theEnv,total);
  }

/*****************************************/
/* CL_IntegerFunction: H/L access routine   */
/*   for the integer function.           */
/*****************************************/
void CL_IntegerFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   /*======================================*/
   /* Check that the argument is a number. */
   /*======================================*/

   if (! CL_UDFNthArgument(context,1,NUMBER_BITS,returnValue))
     { return; }

   /*============================================*/
   /* Convert a float type to integer, otherwise */
   /* return the argument unchanged.             */
   /*============================================*/

   if (CVIsType(returnValue,FLOAT_BIT))
     { returnValue->integerValue = CL_CreateInteger(theEnv,CVCoerceToInteger(returnValue)); }
  }

/***************************************/
/* CL_FloatFunction: H/L access routine   */
/*   for the float function.           */
/***************************************/
void CL_FloatFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   /*======================================*/
   /* Check that the argument is a number. */
   /*======================================*/

   if (! CL_UDFNthArgument(context,1,NUMBER_BITS,returnValue))
     { return; }

   /*=============================================*/
   /* Convert an integer type to float, otherwise */
   /* return the argument unchanged.              */
   /*=============================================*/

   if (CVIsType(returnValue,INTEGER_BIT))
     { returnValue->floatValue = CL_CreateFloat(theEnv,CVCoerceToFloat(returnValue)); }
  }

/*************************************/
/* CL_AbsFunction: H/L access routine   */
/*   for the abs function.           */
/*************************************/
void CL_AbsFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   /*======================================*/
   /* Check that the argument is a number. */
   /*======================================*/

   if (! CL_UDFNthArgument(context,1,NUMBER_BITS,returnValue))
     { return; }

   /*==========================================*/
   /* Return the absolute value of the number. */
   /*==========================================*/

   if (CVIsType(returnValue,INTEGER_BIT))
     {
      long long lv = returnValue->integerValue->contents;
      if (lv < 0L)
        { returnValue->integerValue = CL_CreateInteger(theEnv,-lv); }
     }
   else
     {
      double dv = returnValue->floatValue->contents;
      if (dv < 0.0)
        { returnValue->floatValue = CL_CreateFloat(theEnv,-dv); }
     }
  }

/*************************************/
/* CL_MinFunction: H/L access routine   */
/*   for the min function.           */
/*************************************/
void CL_MinFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue nextPossible;

   /*============================================*/
   /* Check that the first argument is a number. */
   /*============================================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,returnValue))
     { return; }

   /*===========================================================*/
   /* Loop through the reCL_maining arguments, first checking each */
   /* argument to see that it is a number, and then deteCL_rmining */
   /* if the argument is less than the previous arguments and   */
   /* is thus the minimum value.                                */
   /*===========================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&nextPossible))
        { return; }

      /*=============================================*/
      /* If either argument is a float, convert both */
      /* to floats. Otherwise compare two integers.  */
      /*=============================================*/

      if (CVIsType(returnValue,FLOAT_BIT) || CVIsType(&nextPossible,FLOAT_BIT))
        {
         if (CVCoerceToFloat(returnValue) > CVCoerceToFloat(&nextPossible))
           { returnValue->value = nextPossible.value; }
        }
      else
        {
         if (returnValue->integerValue->contents > nextPossible.integerValue->contents)
           { returnValue->value = nextPossible.value; }
        }
     }
  }

/*************************************/
/* CL_MaxFunction: H/L access routine   */
/*   for the max function.           */
/*************************************/
void CL_MaxFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue nextPossible;

   /*============================================*/
   /* Check that the first argument is a number. */
   /*============================================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,returnValue))
     { return; }

   /*===========================================================*/
   /* Loop through the reCL_maining arguments, first checking each */
   /* argument to see that it is a number, and then deteCL_rmining */
   /* if the argument is greater than the previous arguments    */
   /* and is thus the maximum value.                            */
   /*===========================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&nextPossible))
        { return; }

      /*=============================================*/
      /* If either argument is a float, convert both */
      /* to floats. Otherwise compare two integers.  */
      /*=============================================*/

      if (CVIsType(returnValue,FLOAT_BIT) || CVIsType(&nextPossible,FLOAT_BIT))
        {
         if (CVCoerceToFloat(returnValue) < CVCoerceToFloat(&nextPossible))
           { returnValue->value = nextPossible.value; }
        }
      else
        {
         if (returnValue->integerValue->contents < nextPossible.integerValue->contents)
           { returnValue->value = nextPossible.value; }
        }
     }
  }

