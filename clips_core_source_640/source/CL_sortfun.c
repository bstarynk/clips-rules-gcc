   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*                SORT FUNCTIONS MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for sorting functions.         */
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
/*      6.24: The sort function leaks memory when called     */
/*            with a multifield value of length zero.        */
/*            DR0864                                         */
/*                                                           */
/*      6.30: Added environment cleanup call function        */
/*            Deallocate_SortFunctionData.                    */
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
/*************************************************************/

#include "setup.h"

#include "argacces.h"
#include "dffnxfun.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "multifld.h"
#include "sysdep.h"

#include "sortfun.h"

#define SORTFUN_DATA 7

struct sortFunctionData
  {
   struct expr *SortComparisonFunction;
  };

#define CL_SortFunctionData(theEnv) ((struct sortFunctionData *) GetEnvironmentData(theEnv,SORTFUN_DATA))

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    Do_MergeSort(Environment *,UDFValue *,UDFValue *,size_t,
                                              size_t,size_t,size_t,
                                              bool (*)(Environment *,UDFValue *,UDFValue *));
   static bool                    DefaultCompareSwapFunction(Environment *,UDFValue *,UDFValue *);
   static void                    Deallocate_SortFunctionData(Environment *);

/****************************************/
/* CL_SortFunctionDefinitions: Initializes */
/*   the sorting functions.             */
/****************************************/
void CL_SortFunctionDefinitions(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,SORTFUN_DATA,sizeof(struct sortFunctionData),Deallocate_SortFunctionData);
#if ! RUN_TIME
   CL_AddUDF(theEnv,"sort","bm",1,UNBOUNDED,"*;y",CL_SortFunction,"CL_SortFunction",NULL);
#endif
  }

/*******************************************************/
/* Deallocate_SortFunctionData: Deallocates environment */
/*    data for the sort function.                      */
/*******************************************************/
static void Deallocate_SortFunctionData(
  Environment *theEnv)
  {
   CL_ReturnExpression(theEnv,CL_SortFunctionData(theEnv)->SortComparisonFunction);
  }

/********************************/
/* DefaultCompareSwapFunction:  */
/********************************/
static bool DefaultCompareSwapFunction(
  Environment *theEnv,
  UDFValue *item1,
  UDFValue *item2)
  {
   UDFValue returnValue;

   CL_SortFunctionData(theEnv)->SortComparisonFunction->argList = CL_GenConstant(theEnv,item1->header->type,item1->value);
   CL_SortFunctionData(theEnv)->SortComparisonFunction->argList->nextArg = CL_GenConstant(theEnv,item2->header->type,item2->value);
   CL_ExpressionInstall(theEnv,CL_SortFunctionData(theEnv)->SortComparisonFunction);
   CL_EvaluateExpression(theEnv,CL_SortFunctionData(theEnv)->SortComparisonFunction,&returnValue);
   CL_ExpressionDeinstall(theEnv,CL_SortFunctionData(theEnv)->SortComparisonFunction);
   CL_ReturnExpression(theEnv,CL_SortFunctionData(theEnv)->SortComparisonFunction->argList);
   CL_SortFunctionData(theEnv)->SortComparisonFunction->argList = NULL;

   if (returnValue.value == FalseSymbol(theEnv))
     { return false; }

   return true;
  }

/************************************/
/* CL_SortFunction: H/L access routine */
/*   for the rest$ function.        */
/************************************/
void CL_SortFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   unsigned argumentCount, i;
   unsigned long k = 0;
   size_t j;
   UDFValue *theArguments, *theArguments2;
   UDFValue theArg;
   Multifield *theMultifield, *tempMultifield;
   const char *functionName;
   struct expr *functionReference;
   size_t argumentSize = 0;
   struct functionDefinition *fptr;
#if DEFFUNCTION_CONSTRUCT
   Deffunction *dptr;
#endif

   /*==================================*/
   /* Set up the default return value. */
   /*==================================*/

   returnValue->lexemeValue = FalseSymbol(theEnv);

   /*=============================================*/
   /* Verify that the comparison function exists. */
   /*=============================================*/

   if (! CL_UDFNthArgument(context,1,SYMBOL_BIT,&theArg))
     { return; }

   functionName = theArg.lexemeValue->contents;
   functionReference = CL_FunctionReferenceExpression(theEnv,functionName);
   if (functionReference == NULL)
     {
      CL_ExpectedTypeError1(theEnv,"sort",1,"function name, deffunction name, or defgeneric name");
      return;
     }

   /*======================================*/
   /* For an external function, verify the */
   /* correct number of arguments.         */
   /*======================================*/

   if (functionReference->type == FCALL)
     {
      fptr = functionReference->functionValue;
      if ((CL_GetMinimumArgs(fptr) > 2) ||
          (CL_GetMaximumArgs(fptr) == 0) ||
          (CL_GetMaximumArgs(fptr) == 1))
        {
         CL_ExpectedTypeError1(theEnv,"sort",1,"function name expecting two arguments");
         CL_ReturnExpression(theEnv,functionReference);
         return;
        }
     }

   /*=======================================*/
   /* For a deffunction, verify the correct */
   /* number of arguments.                  */
   /*=======================================*/

#if DEFFUNCTION_CONSTRUCT
   if (functionReference->type == PCALL)
     {
      dptr = (Deffunction *) functionReference->value;
      if ((dptr->minNumberOfParameters > 2) ||
          (dptr->maxNumberOfParameters == 0) ||
          (dptr->maxNumberOfParameters == 1))
        {
         CL_ExpectedTypeError1(theEnv,"sort",1,"deffunction name expecting two arguments");
         CL_ReturnExpression(theEnv,functionReference);
         return;
        }
     }
#endif

   /*=====================================*/
   /* If there are no items to be sorted, */
   /* then return an empty multifield.    */
   /*=====================================*/

   argumentCount = CL_UDFArgumentCount(context);

   if (argumentCount == 1)
     {
      CL_SetMultifieldErrorValue(theEnv,returnValue);
      CL_ReturnExpression(theEnv,functionReference);
      return;
     }

   /*=====================================*/
   /* Retrieve the arguments to be sorted */
   /* and dete_rmine how many there are.   */
   /*=====================================*/

   theArguments = (UDFValue *) CL_genalloc(theEnv,(argumentCount - 1) * sizeof(UDFValue));

   for (i = 2; i <= argumentCount; i++)
     {
      CL_UDFNthArgument(context,i,ANY_TYPE_BITS,&theArguments[i-2]);

      if (theArguments[i-2].header->type == MULTIFIELD_TYPE)
        { argumentSize += theArguments[i-2].range; }
      else
        { argumentSize++; }
     }

   if (argumentSize == 0)
     {
      CL_genfree(theEnv,theArguments,(argumentCount - 1) * sizeof(UDFValue)); /* Bug Fix */
      CL_SetMultifieldErrorValue(theEnv,returnValue);
      CL_ReturnExpression(theEnv,functionReference);
      return;
     }

   /*====================================*/
   /* Pack all of the items to be sorted */
   /* into a data object array.          */
   /*====================================*/

   theArguments2 = (UDFValue *) CL_genalloc(theEnv,argumentSize * sizeof(UDFValue));

   for (i = 2; i <= argumentCount; i++)
     {
      if (theArguments[i-2].header->type == MULTIFIELD_TYPE)
        {
         tempMultifield = theArguments[i-2].multifieldValue;
         for (j = theArguments[i-2].begin; j < (theArguments[i-2].begin + theArguments[i-2].range); j++, k++)
           {
            theArguments2[k].value = tempMultifield->contents[j].value;
           }
        }
      else
        {
         theArguments2[k].value = theArguments[i-2].value;
         k++;
        }
     }

   CL_genfree(theEnv,theArguments,(argumentCount - 1) * sizeof(UDFValue));

   functionReference->nextArg = CL_SortFunctionData(theEnv)->SortComparisonFunction;
   CL_SortFunctionData(theEnv)->SortComparisonFunction = functionReference;

   for (i = 0; i < argumentSize; i++)
     { CL_RetainUDFV(theEnv,&theArguments2[i]); }

   CL_MergeSort(theEnv,argumentSize,theArguments2,DefaultCompareSwapFunction);

   for (i = 0; i < argumentSize; i++)
     { CL_ReleaseUDFV(theEnv,&theArguments2[i]); }

   CL_SortFunctionData(theEnv)->SortComparisonFunction = CL_SortFunctionData(theEnv)->SortComparisonFunction->nextArg;
   functionReference->nextArg = NULL;
   CL_ReturnExpression(theEnv,functionReference);

   theMultifield = CL_CreateMultifield(theEnv,argumentSize);

   for (i = 0; i < argumentSize; i++)
     {
      theMultifield->contents[i].value = theArguments2[i].value;
     }

   CL_genfree(theEnv,theArguments2,argumentSize * sizeof(UDFValue));

   returnValue->begin = 0;
   returnValue->range = argumentSize;
   returnValue->value = theMultifield;
  }

/*******************************************/
/* CL_MergeSort: Sorts a list of fields       */
/*   according to user specified criteria. */
/*******************************************/
void CL_MergeSort(
  Environment *theEnv,
  size_t listSize,
  UDFValue *theList,
  bool (*swapFunction)(Environment *,UDFValue *,UDFValue *))
  {
   UDFValue *tempList;
   size_t middle;

   if (listSize <= 1) return;

   /*==============================*/
   /* Create the temporary storage */
   /* needed for the merge sort.   */
   /*==============================*/

   tempList = (UDFValue *) CL_genalloc(theEnv,listSize * sizeof(UDFValue));

   /*=====================================*/
   /* Call the merge sort driver routine. */
   /*=====================================*/

   middle = (listSize + 1) / 2;
   Do_MergeSort(theEnv,theList,tempList,0,middle-1,middle,listSize - 1,swapFunction);

   /*==================================*/
   /* Deallocate the temporary storage */
   /* needed by the merge sort.        */
   /*==================================*/

   CL_genfree(theEnv,tempList,listSize * sizeof(UDFValue));
  }


/******************************************************/
/* Do_MergeSort: Driver routine for perfo_rming a merge */
/*   sort on an array of UDFValue structures.       */
/******************************************************/
static void Do_MergeSort(
  Environment *theEnv,
  UDFValue *theList,
  UDFValue *tempList,
  size_t s1,
  size_t e1,
  size_t s2,
  size_t e2,
  bool (*swapFunction)(Environment *,UDFValue *,UDFValue *))
  {
   UDFValue temp;
   size_t middle, size;
   size_t c1, c2, mergePoint;

   /* Sort the two subareas before merging them. */

   if (s1 == e1)
     { /* List doesn't need to be merged. */ }
   else if ((s1 + 1) == e1)
     {
      if ((*swapFunction)(theEnv,&theList[s1],&theList[e1]))
        {
         CL_TransferDataObjectValues(&temp,&theList[s1]);
         CL_TransferDataObjectValues(&theList[s1],&theList[e1]);
         CL_TransferDataObjectValues(&theList[e1],&temp);
        }
     }
   else
     {
      size = ((e1 - s1) + 1);
      middle = s1 + ((size + 1) / 2);
      Do_MergeSort(theEnv,theList,tempList,s1,middle-1,middle,e1,swapFunction);
     }

   if (s2 == e2)
     { /* List doesn't need to be merged. */ }
   else if ((s2 + 1) == e2)
     {
      if ((*swapFunction)(theEnv,&theList[s2],&theList[e2]))
        {
         CL_TransferDataObjectValues(&temp,&theList[s2]);
         CL_TransferDataObjectValues(&theList[s2],&theList[e2]);
         CL_TransferDataObjectValues(&theList[e2],&temp);
        }
     }
   else
     {
      size = ((e2 - s2) + 1);
      middle = s2 + ((size + 1) / 2);
      Do_MergeSort(theEnv,theList,tempList,s2,middle-1,middle,e2,swapFunction);
     }

   /*======================*/
   /* Merge the two areas. */
   /*======================*/

   mergePoint = s1;
   c1 = s1;
   c2 = s2;

   while (mergePoint <= e2)
     {
      if (c1 > e1)
        {
         CL_TransferDataObjectValues(&tempList[mergePoint],&theList[c2]);
         c2++;
         mergePoint++;
        }
      else if (c2 > e2)
        {
         CL_TransferDataObjectValues(&tempList[mergePoint],&theList[c1]);
         c1++;
         mergePoint++;
        }
      else if ((*swapFunction)(theEnv,&theList[c1],&theList[c2]))
        {
         CL_TransferDataObjectValues(&tempList[mergePoint],&theList[c2]);
         c2++;
         mergePoint++;
        }
      else
        {
         CL_TransferDataObjectValues(&tempList[mergePoint],&theList[c1]);
         c1++;
         mergePoint++;
        }
     }

   /*=======================================*/
   /* Copy them back to the original array. */
   /*=======================================*/

   for (c1 = s1; c1 <= e2; c1++)
     { CL_TransferDataObjectValues(&theList[c1],&tempList[c1]); }
  }



