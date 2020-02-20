   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/19/18             */
   /*                                                     */
   /*              PREDICATE FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for several predicate          */
/*   functions including not, and, or, eq, neq, <=, >=, <,   */
/*   >, =, <>, symbolp, stringp, lexemep, numberp, integerp, */
/*   floatp, oddp, evenp, multifieldp, sequencep, and        */
/*   pointerp.                                               */
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
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Removed the wordp and sequencep functions.     */
/*                                                           */
/*            Deprecated the pointerp function and added     */
/*            the external-addressp function.                */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#include "argacces.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "multifld.h"
#include "router.h"

#include "prdctfun.h"

/**************************************************/
/* CL_PredicateFunctionDefinitions: Defines standard */
/*   math and predicate functions.                */
/**************************************************/
void CL_PredicateFunctionDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   CL_AddUDF(theEnv,"not","b",1,1,NULL,CL_NotFunction,"CL_NotFunction",NULL);
   CL_AddUDF(theEnv,"and","b",2,UNBOUNDED ,NULL,CL_AndFunction,"CL_AndFunction",NULL);
   CL_AddUDF(theEnv,"or","b",2,UNBOUNDED ,NULL,CL_OrFunction,"CL_OrFunction",NULL);

   CL_AddUDF(theEnv,"eq","b",2,UNBOUNDED,NULL,CL_EqFunction,"CL_EqFunction",NULL);
   CL_AddUDF(theEnv,"neq","b",2,UNBOUNDED,NULL,CL_NeqFunction,"CL_NeqFunction",NULL);

   CL_AddUDF(theEnv,"<=","b",2,UNBOUNDED ,"ld",CL_LessThanOrEqualFunction,"CL_LessThanOrEqualFunction",NULL);
   CL_AddUDF(theEnv,">=","b",2,UNBOUNDED ,"ld",CL_GreaterThanOrEqualFunction,"CL_GreaterThanOrEqualFunction",NULL);
   CL_AddUDF(theEnv,"<","b",2,UNBOUNDED ,"ld",CL_LessThanFunction,"CL_LessThanFunction",NULL);
   CL_AddUDF(theEnv,">","b",2,UNBOUNDED ,"ld",CL_GreaterThanFunction,"CL_GreaterThanFunction",NULL);
   CL_AddUDF(theEnv,"=","b",2,UNBOUNDED ,"ld",CL_NumericEqualFunction,"CL_NumericEqualFunction",NULL);
   CL_AddUDF(theEnv,"<>","b",2,UNBOUNDED ,"ld",CL_NumericNotEqualFunction,"CL_NumericNotEqualFunction",NULL);
   CL_AddUDF(theEnv,"!=","b",2,UNBOUNDED ,"ld",CL_NumericNotEqualFunction,"CL_NumericNotEqualFunction",NULL);

   CL_AddUDF(theEnv,"symbolp","b",1,1,NULL,CL_SymbolpFunction,"CL_SymbolpFunction",NULL);
   CL_AddUDF(theEnv,"stringp","b",1,1,NULL,CL_StringpFunction,"CL_StringpFunction",NULL);
   CL_AddUDF(theEnv,"lexemep","b",1,1,NULL,CL_LexemepFunction,"CL_LexemepFunction",NULL);
   CL_AddUDF(theEnv,"numberp","b",1,1,NULL,CL_NumberpFunction,"CL_NumberpFunction",NULL);
   CL_AddUDF(theEnv,"integerp","b",1,1,NULL,CL_IntegerpFunction,"CL_IntegerpFunction",NULL);
   CL_AddUDF(theEnv,"floatp","b",1,1,NULL,CL_FloatpFunction,"CL_FloatpFunction",NULL);
   CL_AddUDF(theEnv,"oddp","b",1,1,"l",CL_OddpFunction,"CL_OddpFunction",NULL);
   CL_AddUDF(theEnv,"evenp","b",1,1,"l",CL_EvenpFunction,"CL_EvenpFunction",NULL);
   CL_AddUDF(theEnv,"multifieldp","b",1,1,NULL,CL_MultifieldpFunction,"CL_MultifieldpFunction",NULL);
   CL_AddUDF(theEnv,"pointerp","b",1,1,NULL,CL_ExternalAddresspFunction,"CL_ExternalAddresspFunction",NULL);
   CL_AddUDF(theEnv,"external-addressp","b",1,1,NULL,CL_ExternalAddresspFunction,"CL_ExternalAddresspFunction",NULL);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

/************************************/
/* CL_EqFunction: H/L access routine   */
/*   for the eq function.           */
/************************************/
void CL_EqFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item, nextItem;
   unsigned int numArgs, i;
   struct expr *theExpression;

   /*====================================*/
   /* DeteCL_rmine the number of arguments. */
   /*====================================*/

   numArgs = CL_UDFArgumentCount(context);
   if (numArgs == 0)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==============================================*/
   /* Get the value of the first argument against  */
   /* which subsequent arguments will be compared. */
   /*==============================================*/

   theExpression = GetFirstArgument();
   CL_EvaluateExpression(theEnv,theExpression,&item);

   /*=====================================*/
   /* Compare all arguments to the first. */
   /* If any are the same, return FALSE.  */
   /*=====================================*/

   theExpression = GetNextArgument(theExpression);
   for (i = 2 ; i <= numArgs ; i++)
     {
      CL_EvaluateExpression(theEnv,theExpression,&nextItem);

      if (nextItem.header->type != item.header->type)
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }

      if (nextItem.header->type == MULTIFIELD_TYPE)
        {
         if (MultifieldCL_DOsEqual(&nextItem,&item) == false)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else if (nextItem.value != item.value)
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }

      theExpression = GetNextArgument(theExpression);
     }

   /*=====================================*/
   /* All of the arguments were different */
   /* from the first. Return TRUE.        */
   /*=====================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*************************************/
/* CL_NeqFunction: H/L access routine   */
/*   for the neq function.           */
/*************************************/
void CL_NeqFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item, nextItem;
   unsigned int numArgs, i;
   struct expr *theExpression;

   /*====================================*/
   /* DeteCL_rmine the number of arguments. */
   /*====================================*/

   numArgs = CL_UDFArgumentCount(context);
   if (numArgs == 0)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==============================================*/
   /* Get the value of the first argument against  */
   /* which subsequent arguments will be compared. */
   /*==============================================*/

   theExpression = GetFirstArgument();
   CL_EvaluateExpression(theEnv,theExpression,&item);

   /*=====================================*/
   /* Compare all arguments to the first. */
   /* If any are different, return FALSE. */
   /*=====================================*/

   for (i = 2, theExpression = GetNextArgument(theExpression);
        i <= numArgs;
        i++, theExpression = GetNextArgument(theExpression))
     {
      CL_EvaluateExpression(theEnv,theExpression,&nextItem);
      if (nextItem.header->type != item.header->type)
        { continue; }
      else if (nextItem.header->type == MULTIFIELD_TYPE)
        {
         if (MultifieldCL_DOsEqual(&nextItem,&item) == true)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else if (nextItem.value == item.value)
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   /*=====================================*/
   /* All of the arguments were identical */
   /* to the first. Return TRUE.          */
   /*=====================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*****************************************/
/* CL_StringpFunction: H/L access routine   */
/*   for the stringp function.           */
/*****************************************/
void CL_StringpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,STRING_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*****************************************/
/* CL_SymbolpFunction: H/L access routine   */
/*   for the symbolp function.           */
/*****************************************/
void CL_SymbolpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,SYMBOL_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*****************************************/
/* CL_LexemepFunction: H/L access routine   */
/*   for the lexemep function.           */
/*****************************************/
void CL_LexemepFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,LEXEME_BITS))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*****************************************/
/* CL_NumberpFunction: H/L access routine   */
/*   for the numberp function.           */
/*****************************************/
void CL_NumberpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,NUMBER_BITS))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/****************************************/
/* CL_FloatpFunction: H/L access routine   */
/*   for the floatp function.           */
/****************************************/
void CL_FloatpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,FLOAT_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/******************************************/
/* CL_IntegerpFunction: H/L access routine   */
/*   for the integerp function.           */
/******************************************/
void CL_IntegerpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,INTEGER_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*********************************************/
/* CL_MultifieldpFunction: H/L access routine   */
/*   for the multifieldp function.           */
/*********************************************/
void CL_MultifieldpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,MULTIFIELD_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/************************************************/
/* CL_ExternalAddresspFunction: H/L access routine */
/*   for the external-addressp function.        */
/************************************************/
void CL_ExternalAddresspFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&item))
     { return; }

   if (CVIsType(&item,EXTERNAL_ADDRESS_BIT))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/***********************************/
/* CL_NotFunction: H/L access routine */
/*   for the not function.         */
/***********************************/
void CL_NotFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&theArg))
     { return; }

   if (theArg.value == FalseSymbol(theEnv))
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*************************************/
/* CL_AndFunction: H/L access routine   */
/*   for the and function.           */
/*************************************/
void CL_AndFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,ANY_TYPE_BITS,&theArg))
        { return; }

      if (theArg.value == FalseSymbol(theEnv))
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }
     }

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/************************************/
/* CL_OrFunction: H/L access routine   */
/*   for the or function.           */
/************************************/
void CL_OrFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,ANY_TYPE_BITS,&theArg))
        { return; }

      if (theArg.value != FalseSymbol(theEnv))
        {
         returnValue->lexemeValue = TrueSymbol(theEnv);
         return;
        }
     }

   returnValue->lexemeValue = FalseSymbol(theEnv);
  }

/*****************************************/
/* CL_LessThanOrEqualFunction: H/L access   */
/*   routine for the <= function.        */
/*****************************************/
void CL_LessThanOrEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*====================================================*/
   /* Compare each of the subsequent arguments to its    */
   /* predecessor. If any is greater, then return FALSE. */
   /*====================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents > rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) > CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }

      rv1.value = rv2.value;
     }

   /*======================================*/
   /* Each argument was less than or equal */
   /* to its predecessor. Return TRUE.     */
   /*======================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/********************************************/
/* CL_GreaterThanOrEqualFunction: H/L access   */
/*   routine for the >= function.           */
/********************************************/
void CL_GreaterThanOrEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*===================================================*/
   /* Compare each of the subsequent arguments to its   */
   /* predecessor. If any is lesser, then return false. */
   /*===================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents < rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) < CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }

      rv1.value = rv2.value;
     }

   /*=========================================*/
   /* Each argument was greater than or equal */
   /* to its predecessor. Return TRUE.        */
   /*=========================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/**********************************/
/* CL_LessThanFunction: H/L access   */
/*   routine for the < function.  */
/**********************************/
void CL_LessThanFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*==========================================*/
   /* Compare each of the subsequent arguments */
   /* to its predecessor. If any is greater or */
   /* equal, then return FALSE.                */
   /*==========================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents >= rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) >= CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }

      rv1.value = rv2.value;
     }

   /*=================================*/
   /* Each argument was less than its */
   /* predecessor. Return TRUE.       */
   /*=================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*************************************/
/* CL_GreaterThanFunction: H/L access   */
/*   routine for the > function.     */
/*************************************/
void CL_GreaterThanFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*==========================================*/
   /* Compare each of the subsequent arguments */
   /* to its predecessor. If any is lesser or  */
   /* equal, then return FALSE.                */
   /*==========================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents <= rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) <= CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }

      rv1.value = rv2.value;
     }

   /*================================*/
   /* Each argument was greater than */
   /* its predecessor. Return TRUE.  */
   /*================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/**************************************/
/* CL_NumericEqualFunction: H/L access   */
/*   routine for the = function.      */
/**************************************/
void CL_NumericEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*=================================================*/
   /* Compare each of the subsequent arguments to the */
   /* first. If any is unequal, then return FALSE.    */
   /*=================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents != rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) != CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
     }

   /*=================================*/
   /* All arguments were equal to the */
   /* first argument. Return TRUE.    */
   /*=================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*****************************************/
/* CL_NumericNotEqualFunction: H/L access   */
/*   routine for the <> function.        */
/*****************************************/
void CL_NumericNotEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&rv1))
     { return; }

   /*=================================================*/
   /* Compare each of the subsequent arguments to the */
   /* first. If any is equal, then return FALSE.      */
   /*=================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! CL_UDFNextArgument(context,NUMBER_BITS,&rv2))
        { return; }

      if (CVIsType(&rv1,INTEGER_BIT) && CVIsType(&rv2,INTEGER_BIT))
        {
         if (rv1.integerValue->contents == rv2.integerValue->contents)
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
      else
        {
         if (CVCoerceToFloat(&rv1) == CVCoerceToFloat(&rv2))
           {
            returnValue->lexemeValue = FalseSymbol(theEnv);
            return;
           }
        }
     }

   /*===================================*/
   /* All arguments were unequal to the */
   /* first argument. Return TRUE.      */
   /*===================================*/

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/**************************************/
/* CL_OddpFunction: H/L access routine   */
/*   for the oddp function.           */
/**************************************/
void CL_OddpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;
   long long num, halfnum;

   /*===========================================*/
   /* Check for the correct types of arguments. */
   /*===========================================*/

   if (! CL_UDFFirstArgument(context,INTEGER_BIT,&item))
     { return; }

   /*===========================*/
   /* Compute the return value. */
   /*===========================*/

   num = item.integerValue->contents;
   halfnum = (num / 2) * 2;

   if (num == halfnum) returnValue->lexemeValue = FalseSymbol(theEnv);
   else returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/***************************************/
/* CL_EvenpFunction: H/L access routine   */
/*   for the evenp function.           */
/***************************************/
void CL_EvenpFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue item;
   long long num, halfnum;

   /*===========================================*/
   /* Check for the correct types of arguments. */
   /*===========================================*/

   if (! CL_UDFFirstArgument(context,INTEGER_BIT,&item))
     { return; }

   /*===========================*/
   /* Compute the return value. */
   /*===========================*/

   num = item.integerValue->contents;;
   halfnum = (num / 2) * 2;

   if (num != halfnum) returnValue->lexemeValue = FalseSymbol(theEnv);
   else returnValue->lexemeValue = TrueSymbol(theEnv);
  }



