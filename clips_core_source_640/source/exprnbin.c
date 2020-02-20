   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*             EXPRESSION BSAVE/BLOAD MODULE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    expression data structure.                             */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "constrct.h"
#include "dffctdef.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "moduldef.h"

#if DEFRULE_CONSTRUCT
#include "network.h"
#endif

#if DEFGENERIC_CONSTRUCT
#include "genrcbin.h"
#endif

#if DEFFUNCTION_CONSTRUCT
#include "dffnxbin.h"
#endif

#if DEFTEMPLATE_CONSTRUCT
#include "factmngr.h"
#include "tmpltbin.h"
#endif

#if DEFGLOBAL_CONSTRUCT
#include "globlbin.h"
#endif

#if OBJECT_SYSTEM
#include "objbin.h"
#include "insfun.h"
#include "inscom.h"
#endif

#include "exprnbin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                        UpdateExpression(Environment *,void *,unsigned long);

/***********************************************************/
/* CL_AllocateExpressions: DeteCL_rmines the amount of space     */
/*   required for loading the binary image of expressions  */
/*   and allocates that amount of space.                   */
/***********************************************************/
void CL_AllocateExpressions(
  Environment *theEnv)
  {
   size_t space;

   CL_GenReadBinary(theEnv,&ExpressionData(theEnv)->NumberOfExpressions,sizeof(long));
   if (ExpressionData(theEnv)->NumberOfExpressions == 0L)
     ExpressionData(theEnv)->ExpressionArray = NULL;
   else
     {
      space = ExpressionData(theEnv)->NumberOfExpressions * sizeof(struct expr);
      ExpressionData(theEnv)->ExpressionArray = (struct expr *) CL_genalloc(theEnv,space);
     }
  }

/**********************************************/
/* CL_RefreshExpressions: CL_Refreshes the pointers */
/*   used by the expression binary image.     */
/**********************************************/
void CL_RefreshExpressions(
  Environment *theEnv)
  {
   if (ExpressionData(theEnv)->ExpressionArray == NULL) return;

   CL_BloadandCL_Refresh(theEnv,ExpressionData(theEnv)->NumberOfExpressions,
                   sizeof(BSAVE_EXPRESSION),UpdateExpression);
  }

/*********************************************************
  NAME         : UpdateExpression
  DESCRIPTION  : Given a bloaded expression buffer,
                   this routine refreshes the pointers
                   in the expression array
  INPUTS       : 1) a bloaded expression buffer
                 2) the index of the expression to refresh
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expression updated
  NOTES        : None
 *********************************************************/
static void UpdateExpression(
  Environment *theEnv,
  void *buf,
  unsigned long obji)
  {
   BSAVE_EXPRESSION *bexp;
   unsigned long theIndex;

   bexp = (BSAVE_EXPRESSION *) buf;
   ExpressionData(theEnv)->ExpressionArray[obji].type = bexp->type;
   switch(bexp->type)
     {
      case FCALL:
        ExpressionData(theEnv)->ExpressionArray[obji].value = CL_BloadData(theEnv)->FunctionArray[bexp->value];
        break;

      case GCALL:
#if DEFGENERIC_CONSTRUCT
        ExpressionData(theEnv)->ExpressionArray[obji].value = GenericPointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;

      case PCALL:
#if DEFFUNCTION_CONSTRUCT
        ExpressionData(theEnv)->ExpressionArray[obji].value = DeffunctionPointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;

      case DEFTEMPLATE_PTR:
#if DEFTEMPLATE_CONSTRUCT
        ExpressionData(theEnv)->ExpressionArray[obji].value = DeftemplatePointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;

     case DEFCLASS_PTR:
#if OBJECT_SYSTEM
        ExpressionData(theEnv)->ExpressionArray[obji].value = DefclassPointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;

      case DEFGLOBAL_PTR:

#if DEFGLOBAL_CONSTRUCT
        ExpressionData(theEnv)->ExpressionArray[obji].value = DefglobalPointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;


      case INTEGER_TYPE:
        ExpressionData(theEnv)->ExpressionArray[obji].value = SymbolData(theEnv)->IntegerArray[bexp->value];
        IncrementIntegerCount(ExpressionData(theEnv)->ExpressionArray[obji].integerValue);
        break;

      case FLOAT_TYPE:
        ExpressionData(theEnv)->ExpressionArray[obji].value = SymbolData(theEnv)->FloatArray[bexp->value];
        IncrementFloatCount(ExpressionData(theEnv)->ExpressionArray[obji].floatValue);
        break;

      case INSTANCE_NAME_TYPE:
#if ! OBJECT_SYSTEM
        ExpressionData(theEnv)->ExpressionArray[obji].type = SYMBOL_TYPE;
#endif
      case GBL_VARIABLE:
      case SYMBOL_TYPE:
      case STRING_TYPE:
        ExpressionData(theEnv)->ExpressionArray[obji].value = SymbolData(theEnv)->SymbolArray[bexp->value];
        IncrementLexemeCount(ExpressionData(theEnv)->ExpressionArray[obji].lexemeValue);
        break;

#if DEFTEMPLATE_CONSTRUCT
      case FACT_ADDRESS_TYPE:
        ExpressionData(theEnv)->ExpressionArray[obji].value = &FactData(theEnv)->DummyFact;
        CL_RetainFact((Fact *) ExpressionData(theEnv)->ExpressionArray[obji].value);
        break;
#endif

#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS_TYPE:
        ExpressionData(theEnv)->ExpressionArray[obji].value = &InstanceData(theEnv)->DummyInstance;
        CL_RetainInstance((Instance *) ExpressionData(theEnv)->ExpressionArray[obji].value);
        break;
#endif

      case EXTERNAL_ADDRESS_TYPE:
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
        break;

      case VOID_TYPE:
        break;

      default:
        if (CL_EvaluationData(theEnv)->PrimitivesArray[bexp->type] == NULL) break;
        if (CL_EvaluationData(theEnv)->PrimitivesArray[bexp->type]->bitMap)
          {
           ExpressionData(theEnv)->ExpressionArray[obji].value = SymbolData(theEnv)->BitMapArray[bexp->value];
           IncrementBitMapCount((CLIPSBitMap *) ExpressionData(theEnv)->ExpressionArray[obji].value);
          }
        break;
     }

   theIndex = bexp->nextArg;
   if (theIndex == ULONG_MAX)
     { ExpressionData(theEnv)->ExpressionArray[obji].nextArg = NULL; }
   else
     { ExpressionData(theEnv)->ExpressionArray[obji].nextArg = (struct expr *) &ExpressionData(theEnv)->ExpressionArray[theIndex]; }

   theIndex = bexp->argList;
   if (theIndex == ULONG_MAX)
     { ExpressionData(theEnv)->ExpressionArray[obji].argList = NULL; }
   else
     { ExpressionData(theEnv)->ExpressionArray[obji].argList = (struct expr *) &ExpressionData(theEnv)->ExpressionArray[theIndex]; }
  }

/*********************************************/
/* CL_ClearCL_BloadedExpressions: CL_Clears the space */
/*   utilized by an expression binary image. */
/*********************************************/
void CL_ClearCL_BloadedExpressions(
  Environment *theEnv)
  {
   unsigned long i;
   size_t space;

   /*===============================================*/
   /* Update the busy counts of atomic data values. */
   /*===============================================*/

   for (i = 0; i < ExpressionData(theEnv)->NumberOfExpressions; i++)
     {
      switch (ExpressionData(theEnv)->ExpressionArray[i].type)
        {
         case SYMBOL_TYPE          :
         case STRING_TYPE          :
         case INSTANCE_NAME_TYPE   :
         case GBL_VARIABLE    :
           CL_ReleaseLexeme(theEnv,ExpressionData(theEnv)->ExpressionArray[i].lexemeValue);
           break;
         case FLOAT_TYPE           :
           CL_ReleaseFloat(theEnv,ExpressionData(theEnv)->ExpressionArray[i].floatValue);
           break;
         case INTEGER_TYPE         :
           CL_ReleaseInteger(theEnv,ExpressionData(theEnv)->ExpressionArray[i].integerValue);
           break;

#if DEFTEMPLATE_CONSTRUCT
         case FACT_ADDRESS_TYPE    :
           CL_ReleaseFact((Fact *) ExpressionData(theEnv)->ExpressionArray[i].value);
           break;
#endif

#if OBJECT_SYSTEM
         case INSTANCE_ADDRESS_TYPE :
           CL_ReleaseInstance((Instance *) ExpressionData(theEnv)->ExpressionArray[i].value);
           break;
#endif

         case VOID_TYPE:
           break;

         default:
           if (CL_EvaluationData(theEnv)->PrimitivesArray[ExpressionData(theEnv)->ExpressionArray[i].type] == NULL) break;
           if (CL_EvaluationData(theEnv)->PrimitivesArray[ExpressionData(theEnv)->ExpressionArray[i].type]->bitMap)
             { CL_DecrementBitMapReferenceCount(theEnv,(CLIPSBitMap *) ExpressionData(theEnv)->ExpressionArray[i].value); }
           break;
        }
     }

   /*===================================*/
   /* Free the binary expression array. */
   /*===================================*/

   space = ExpressionData(theEnv)->NumberOfExpressions * sizeof(struct expr);
   if (space != 0) CL_genfree(theEnv,ExpressionData(theEnv)->ExpressionArray,space);
   ExpressionData(theEnv)->ExpressionArray = 0;
  }


#if BLOAD_AND_BSAVE

/***************************************************
  NAME         : CL_FindHashedExpressions
  DESCRIPTION  : Sets the bsave expression array
                 indices for hashed expression nodes
                 and marks the items needed by
                 these expressions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Atoms marked and ids set
  NOTES        : None
 ***************************************************/
void CL_FindHashedExpressions(
  Environment *theEnv)
  {
   unsigned i;
   EXPRESSION_HN *exphash;

   for (i = 0 ; i < EXPRESSION_HASH_SIZE ; i++)
     for (exphash = ExpressionData(theEnv)->ExpressionHashTable[i] ; exphash != NULL ; exphash = exphash->next)
       {
        CL_MarkNeededItems(theEnv,exphash->exp);
        exphash->bsaveID = ExpressionData(theEnv)->ExpressionCount;
        ExpressionData(theEnv)->ExpressionCount += CL_ExpressionSize(exphash->exp);
       }
  }

/***************************************************
  NAME         : CL_BsaveHashedExpressions
  DESCRIPTION  : CL_Writes out hashed expressions
  INPUTS       : CL_Bsave file stream pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expressions written
  NOTES        : None
 ***************************************************/
void CL_BsaveHashedExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   unsigned i;
   EXPRESSION_HN *exphash;

   for (i = 0 ; i < EXPRESSION_HASH_SIZE ; i++)
     for (exphash = ExpressionData(theEnv)->ExpressionHashTable[i] ; exphash != NULL ; exphash = exphash->next)
       CL_BsaveExpression(theEnv,exphash->exp,fp);
  }

/***************************************************************/
/* CL_BsaveConstructExpressions: CL_Writes all expression needed by  */
/*   constructs for this binary image to the binary save file. */
/***************************************************************/
void CL_BsaveConstructExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   struct BinaryItem *biPtr;

   for (biPtr = CL_BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     {
      if (biPtr->expressionFunction != NULL)
        { (*biPtr->expressionFunction)(theEnv,fp); }
     }
  }

/***************************************/
/* CL_BsaveExpression: Recursively saves  */
/*   an expression to the binary file. */
/***************************************/
void CL_BsaveExpression(
  Environment *theEnv,
  struct expr *testPtr,
  FILE *fp)
  {
   BSAVE_EXPRESSION newTest;
   unsigned long newIndex;

   while (testPtr != NULL)
     {
      ExpressionData(theEnv)->ExpressionCount++;

      /*================*/
      /* Copy the type. */
      /*================*/

      newTest.type = testPtr->type;

      /*=======================================*/
      /* Convert the argList slot to an index. */
      /*=======================================*/

      if (testPtr->argList == NULL)
        { newTest.argList = ULONG_MAX; }
      else
        { newTest.argList = ExpressionData(theEnv)->ExpressionCount; }

      /*========================================*/
      /* Convert the nextArg slot to an index. */
      /*========================================*/

      if (testPtr->nextArg == NULL)
        { newTest.nextArg = ULONG_MAX; }
      else
        {
         newIndex = ExpressionData(theEnv)->ExpressionCount + CL_ExpressionSize(testPtr->argList);
         newTest.nextArg = newIndex;
        }

      /*=========================*/
      /* Convert the value slot. */
      /*=========================*/

      switch(testPtr->type)
        {
         case FLOAT_TYPE:
           newTest.value = testPtr->floatValue->bucket;
           break;

         case INTEGER_TYPE:
           newTest.value = testPtr->integerValue->bucket;
           break;

         case FCALL:
           newTest.value = testPtr->functionValue->bsaveIndex;
           break;

         case GCALL:
#if DEFGENERIC_CONSTRUCT
           if (testPtr->value != NULL)
             newTest.value = testPtr->constructValue->bsaveID;
           else
#endif
             newTest.value = ULONG_MAX;
           break;

         case PCALL:
#if DEFFUNCTION_CONSTRUCT
           if (testPtr->value != NULL)
             newTest.value = testPtr->constructValue->bsaveID;
           else
#endif
             newTest.value = ULONG_MAX;
           break;

         case DEFTEMPLATE_PTR:
#if DEFTEMPLATE_CONSTRUCT
           if (testPtr->value != NULL)
             newTest.value = testPtr->constructValue->bsaveID;
           else
#endif
             newTest.value = ULONG_MAX;
           break;

         case DEFCLASS_PTR:
#if OBJECT_SYSTEM
           if (testPtr->value != NULL)
             newTest.value = testPtr->constructValue->bsaveID;
           else
#endif
             newTest.value = ULONG_MAX;
           break;

         case DEFGLOBAL_PTR:
#if DEFGLOBAL_CONSTRUCT
           if (testPtr->value != NULL)
             newTest.value = testPtr->constructValue->bsaveID;
           else
#endif
             newTest.value = ULONG_MAX;
           break;

#if OBJECT_SYSTEM
         case INSTANCE_NAME_TYPE:
#endif
         case SYMBOL_TYPE:
         case GBL_VARIABLE:
         case STRING_TYPE:
           newTest.value = testPtr->lexemeValue->bucket;
           break;

         case FACT_ADDRESS_TYPE:
         case INSTANCE_ADDRESS_TYPE:
         case EXTERNAL_ADDRESS_TYPE:
           newTest.value = ULONG_MAX;
           break;

         case VOID_TYPE:
           break;

         default:
           if (CL_EvaluationData(theEnv)->PrimitivesArray[testPtr->type] == NULL) break;
           if (CL_EvaluationData(theEnv)->PrimitivesArray[testPtr->type]->bitMap)
             { newTest.value = ((CLIPSBitMap *) testPtr->value)->bucket; }
           break;
        }

     /*===========================*/
     /* CL_Write out the expression. */
     /*===========================*/

     CL_GenCL_Write(&newTest,sizeof(BSAVE_EXPRESSION),fp);

     /*==========================*/
     /* CL_Write out argument list. */
     /*==========================*/

     if (testPtr->argList != NULL)
       {
        CL_BsaveExpression(theEnv,testPtr->argList,fp);
       }

     testPtr = testPtr->nextArg;
    }
  }

#endif /* BLOAD_AND_BSAVE */

#endif /* (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) */

