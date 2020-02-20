   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/18/16             */
   /*                                                     */
   /*                 CONSTRAINT MODULE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides functions for creating and removing     */
/*   constraint records, adding them to the contraint hash   */
/*   table, and enabling and disabling static and dynamic    */
/*   constraint checking.                                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian Dantes                                         */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Added allowed-classes slot facet.              */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "setup.h"

#include "argacces.h"
#include "constant.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "multifld.h"
#include "router.h"
#include "scanner.h"

#include "constrnt.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static void                     InstallConstraintRecord(Environment *,CONSTRAINT_RECORD *);
   static bool                     ConstraintCompare(struct constraintRecord *,struct constraintRecord *);
#endif
   static void                     ReturnConstraintRecord(Environment *,CONSTRAINT_RECORD *);
   static void                     DeinstallConstraintRecord(Environment *,CONSTRAINT_RECORD *);
   static void                     DeallocateConstraintData(Environment *);

/*****************************************************/
/* CL_InitializeConstraints: Initializes the constraint */
/*   hash table to NULL and defines the static and   */
/*   dynamic constraint access functions.            */
/*****************************************************/
void CL_InitializeConstraints(
  Environment *theEnv)
  {
#if (! RUN_TIME) && (! BLOAD_ONLY)
   int i;
#endif

   CL_AllocateEnvironmentData(theEnv,CONSTRAINT_DATA,sizeof(struct constraintData),DeallocateConstraintData);

#if (! RUN_TIME) && (! BLOAD_ONLY)

    ConstraintData(theEnv)->ConstraintHashtable = (struct constraintRecord **)
                          CL_gm2(theEnv,sizeof (struct constraintRecord *) *
                                    SIZE_CONSTRAINT_HASH);

    if (ConstraintData(theEnv)->ConstraintHashtable == NULL) CL_ExitRouter(theEnv,EXIT_FAILURE);

    for (i = 0; i < SIZE_CONSTRAINT_HASH; i++) ConstraintData(theEnv)->ConstraintHashtable[i] = NULL;
#endif

#if (! RUN_TIME)
   CL_AddUDF(theEnv,"get-dynamic-constraint-checking","b",0,0,NULL,CL_GDCCommand,"CL_GDCCommand",NULL);
   CL_AddUDF(theEnv,"set-dynamic-constraint-checking","b",1,1,NULL,CL_SDCCommand,"CL_SDCCommand",NULL);
#endif
  }

/*****************************************************/
/* DeallocateConstraintData: Deallocates environment */
/*    data for constraints.                          */
/*****************************************************/
static void DeallocateConstraintData(
  Environment *theEnv)
  {
#if ! RUN_TIME
   struct constraintRecord *tmpPtr, *nextPtr;
   int i;

   for (i = 0; i < SIZE_CONSTRAINT_HASH; i++)
     {
      tmpPtr = ConstraintData(theEnv)->ConstraintHashtable[i];
      while (tmpPtr != NULL)
        {
         nextPtr = tmpPtr->next;
         ReturnConstraintRecord(theEnv,tmpPtr);
         tmpPtr = nextPtr;
        }
     }

   CL_rm(theEnv,ConstraintData(theEnv)->ConstraintHashtable,
      sizeof(struct constraintRecord *) * SIZE_CONSTRAINT_HASH);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
   if (ConstraintData(theEnv)->NumberOfConstraints != 0)
     {
      CL_genfree(theEnv,ConstraintData(theEnv)->ConstraintArray,
              (sizeof(CONSTRAINT_RECORD) * ConstraintData(theEnv)->NumberOfConstraints));
     }
#endif
  }

/*************************************************************/
/* ReturnConstraintRecord: Frees the data structures used by */
/*   a constraint record. If the returnOnlyFields argument   */
/*   is false, then the constraint record is also freed.     */
/*************************************************************/
static void ReturnConstraintRecord(
  Environment *theEnv,
  CONSTRAINT_RECORD *constraints)
  {
   if (constraints == NULL) return;

   if (! constraints->installed)
     {
      CL_ReturnExpression(theEnv,constraints->classList);
      CL_ReturnExpression(theEnv,constraints->restrictionList);
      CL_ReturnExpression(theEnv,constraints->maxValue);
      CL_ReturnExpression(theEnv,constraints->minValue);
      CL_ReturnExpression(theEnv,constraints->minFields);
      CL_ReturnExpression(theEnv,constraints->maxFields);
     }

   ReturnConstraintRecord(theEnv,constraints->multifield);

   rtn_struct(theEnv,constraintRecord,constraints);
  }

/***************************************************/
/* DeinstallConstraintRecord: Decrements the count */
/*   values of all occurrences of primitive data   */
/*   types found in a constraint record.           */
/***************************************************/
static void DeinstallConstraintRecord(
  Environment *theEnv,
  CONSTRAINT_RECORD *constraints)
  {
   if (constraints->installed)
     {
      CL_RemoveHashedExpression(theEnv,constraints->classList);
      CL_RemoveHashedExpression(theEnv,constraints->restrictionList);
      CL_RemoveHashedExpression(theEnv,constraints->maxValue);
      CL_RemoveHashedExpression(theEnv,constraints->minValue);
      CL_RemoveHashedExpression(theEnv,constraints->minFields);
      CL_RemoveHashedExpression(theEnv,constraints->maxFields);
     }
   else
     {
      CL_ExpressionDeinstall(theEnv,constraints->classList);
      CL_ExpressionDeinstall(theEnv,constraints->restrictionList);
      CL_ExpressionDeinstall(theEnv,constraints->maxValue);
      CL_ExpressionDeinstall(theEnv,constraints->minValue);
      CL_ExpressionDeinstall(theEnv,constraints->minFields);
      CL_ExpressionDeinstall(theEnv,constraints->maxFields);
     }

   if (constraints->multifield != NULL)
     { DeinstallConstraintRecord(theEnv,constraints->multifield); }
  }

/******************************************/
/* CL_RemoveConstraint: Removes a constraint */
/*   from the constraint hash table.      */
/******************************************/
void CL_RemoveConstraint(
  Environment *theEnv,
  struct constraintRecord *theConstraint)
  {
   struct constraintRecord *tmpPtr, *prevPtr = NULL;

   if (theConstraint == NULL) return;

   /*========================================*/
   /* If the bucket value is less than zero, */
   /* then the constraint wasn't stored in   */
   /* the hash table.                        */
   /*========================================*/

   if (! theConstraint->installed)
     {
      ReturnConstraintRecord(theEnv,theConstraint);
      return;
     }

   /*================================*/
   /* Find and remove the constraint */
   /* from the contraint hash table. */
   /*================================*/

   tmpPtr = ConstraintData(theEnv)->ConstraintHashtable[theConstraint->bucket];
   while (tmpPtr != NULL)
     {
      if (tmpPtr == theConstraint)
        {
         theConstraint->count--;
         if (theConstraint->count == 0)
           {
            if (prevPtr == NULL)
              { ConstraintData(theEnv)->ConstraintHashtable[theConstraint->bucket] = theConstraint->next; }
            else
              { prevPtr->next = theConstraint->next; }
            DeinstallConstraintRecord(theEnv,theConstraint);
            ReturnConstraintRecord(theEnv,theConstraint);
           }
         return;
        }

      prevPtr = tmpPtr;
      tmpPtr = tmpPtr->next;
     }

   return;
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/***********************************/
/* CL_HashConstraint: Returns a hash  */
/*   value for a given constraint. */
/***********************************/
unsigned long CL_HashConstraint(
  struct constraintRecord *theConstraint)
  {
   unsigned short i = 0;
   unsigned long count = 0;
   unsigned long hashValue;
   struct expr *tmpPtr;

   count +=
      (theConstraint->anyAllowed * 17) +
      (theConstraint->symbolsAllowed * 5) +
      (theConstraint->stringsAllowed * 23) +
      (theConstraint->floatsAllowed * 19) +
      (theConstraint->integersAllowed * 29) +
      (theConstraint->instanceNamesAllowed * 31) +
      (theConstraint->instanceAddressesAllowed * 17);

   count +=
      (theConstraint->externalAddressesAllowed * 29) +
      (theConstraint->voidAllowed * 29) +
      (theConstraint->multifieldsAllowed * 29) +
      (theConstraint->factAddressesAllowed * 79) +
      (theConstraint->anyRestriction * 59) +
      (theConstraint->symbolRestriction * 61);

   count +=
      (theConstraint->stringRestriction * 3) +
      (theConstraint->floatRestriction * 37) +
      (theConstraint->integerRestriction * 9) +
      (theConstraint->classRestriction * 11) +
      (theConstraint->instanceNameRestriction * 7);

   for (tmpPtr = theConstraint->classList; tmpPtr != NULL; tmpPtr = tmpPtr->nextArg)
     { count += CL_GetAtomicHashValue(tmpPtr->type,tmpPtr->value,i++); }

   for (tmpPtr = theConstraint->restrictionList; tmpPtr != NULL; tmpPtr = tmpPtr->nextArg)
     { count += CL_GetAtomicHashValue(tmpPtr->type,tmpPtr->value,i++); }

   for (tmpPtr = theConstraint->minValue; tmpPtr != NULL; tmpPtr = tmpPtr->nextArg)
     { count += CL_GetAtomicHashValue(tmpPtr->type,tmpPtr->value,i++); }

   for (tmpPtr = theConstraint->maxValue; tmpPtr != NULL; tmpPtr = tmpPtr->nextArg)
     { count += CL_GetAtomicHashValue(tmpPtr->type,tmpPtr->value,i++); }

   for (tmpPtr = theConstraint->minFields; tmpPtr != NULL; tmpPtr = tmpPtr->nextArg)
     { count += CL_GetAtomicHashValue(tmpPtr->type,tmpPtr->value,i++); }

   for (tmpPtr = theConstraint->maxFields; tmpPtr != NULL; tmpPtr = tmpPtr->nextArg)
     { count += CL_GetAtomicHashValue(tmpPtr->type,tmpPtr->value,i++); }

   if (theConstraint->multifield != NULL)
     { count += CL_HashConstraint(theConstraint->multifield); }

   hashValue = count % SIZE_CONSTRAINT_HASH;

   return hashValue;
  }

/**********************************************/
/* ConstraintCompare: Compares two constraint */
/*   records and returns true if they are     */
/*   identical, otherwise false.              */
/**********************************************/
static bool ConstraintCompare(
  struct constraintRecord *constraint1,
  struct constraintRecord *constraint2)
  {
   struct expr *tmpPtr1, *tmpPtr2;

   if ((constraint1->anyAllowed != constraint2->anyAllowed) ||
       (constraint1->symbolsAllowed != constraint2->symbolsAllowed) ||
       (constraint1->stringsAllowed != constraint2->stringsAllowed) ||
       (constraint1->floatsAllowed != constraint2->floatsAllowed) ||
       (constraint1->integersAllowed != constraint2->integersAllowed) ||
       (constraint1->instanceNamesAllowed != constraint2->instanceNamesAllowed) ||
       (constraint1->instanceAddressesAllowed != constraint2->instanceAddressesAllowed) ||
       (constraint1->externalAddressesAllowed != constraint2->externalAddressesAllowed) ||
       (constraint1->voidAllowed != constraint2->voidAllowed) ||
       (constraint1->multifieldsAllowed != constraint2->multifieldsAllowed) ||
       (constraint1->singlefieldsAllowed != constraint2->singlefieldsAllowed) ||
       (constraint1->factAddressesAllowed != constraint2->factAddressesAllowed) ||
       (constraint1->anyRestriction != constraint2->anyRestriction) ||
       (constraint1->symbolRestriction != constraint2->symbolRestriction) ||
       (constraint1->stringRestriction != constraint2->stringRestriction) ||
       (constraint1->floatRestriction != constraint2->floatRestriction) ||
       (constraint1->integerRestriction != constraint2->integerRestriction) ||
       (constraint1->classRestriction != constraint2->classRestriction) ||
       (constraint1->instanceNameRestriction != constraint2->instanceNameRestriction))
     { return false; }

   for (tmpPtr1 = constraint1->classList, tmpPtr2 = constraint2->classList;
        (tmpPtr1 != NULL) && (tmpPtr2 != NULL);
        tmpPtr1 = tmpPtr1->nextArg, tmpPtr2 = tmpPtr2->nextArg)
     {
      if ((tmpPtr1->type != tmpPtr2->type) || (tmpPtr1->value != tmpPtr2->value))
        { return false; }
     }
   if (tmpPtr1 != tmpPtr2) return false;

   for (tmpPtr1 = constraint1->restrictionList, tmpPtr2 = constraint2->restrictionList;
        (tmpPtr1 != NULL) && (tmpPtr2 != NULL);
        tmpPtr1 = tmpPtr1->nextArg, tmpPtr2 = tmpPtr2->nextArg)
     {
      if ((tmpPtr1->type != tmpPtr2->type) || (tmpPtr1->value != tmpPtr2->value))
        { return false; }
     }
   if (tmpPtr1 != tmpPtr2) return false;

   for (tmpPtr1 = constraint1->minValue, tmpPtr2 = constraint2->minValue;
        (tmpPtr1 != NULL) && (tmpPtr2 != NULL);
        tmpPtr1 = tmpPtr1->nextArg, tmpPtr2 = tmpPtr2->nextArg)
     {
      if ((tmpPtr1->type != tmpPtr2->type) || (tmpPtr1->value != tmpPtr2->value))
        { return false; }
     }
   if (tmpPtr1 != tmpPtr2) return false;

   for (tmpPtr1 = constraint1->maxValue, tmpPtr2 = constraint2->maxValue;
        (tmpPtr1 != NULL) && (tmpPtr2 != NULL);
        tmpPtr1 = tmpPtr1->nextArg, tmpPtr2 = tmpPtr2->nextArg)
     {
      if ((tmpPtr1->type != tmpPtr2->type) || (tmpPtr1->value != tmpPtr2->value))
        { return false; }
     }
   if (tmpPtr1 != tmpPtr2) return false;

   for (tmpPtr1 = constraint1->minFields, tmpPtr2 = constraint2->minFields;
        (tmpPtr1 != NULL) && (tmpPtr2 != NULL);
        tmpPtr1 = tmpPtr1->nextArg, tmpPtr2 = tmpPtr2->nextArg)
     {
      if ((tmpPtr1->type != tmpPtr2->type) || (tmpPtr1->value != tmpPtr2->value))
        { return false; }
     }
   if (tmpPtr1 != tmpPtr2) return false;

   for (tmpPtr1 = constraint1->maxFields, tmpPtr2 = constraint2->maxFields;
        (tmpPtr1 != NULL) && (tmpPtr2 != NULL);
        tmpPtr1 = tmpPtr1->nextArg, tmpPtr2 = tmpPtr2->nextArg)
     {
      if ((tmpPtr1->type != tmpPtr2->type) || (tmpPtr1->value != tmpPtr2->value))
        { return false; }
     }
   if (tmpPtr1 != tmpPtr2) return false;

   if (((constraint1->multifield == NULL) && (constraint2->multifield != NULL)) ||
       ((constraint1->multifield != NULL) && (constraint2->multifield == NULL)))
     { return false; }
   else if (constraint1->multifield == constraint2->multifield)
     { return true; }

   return(ConstraintCompare(constraint1->multifield,constraint2->multifield));
  }

/************************************/
/* CL_AddConstraint: Adds a constraint */
/*   to the constraint hash table.  */
/************************************/
struct constraintRecord *CL_AddConstraint(
  Environment *theEnv,
  struct constraintRecord *theConstraint)
  {
   struct constraintRecord *tmpPtr;
   unsigned long hashValue;

   if (theConstraint == NULL) return NULL;

   hashValue = CL_HashConstraint(theConstraint);

   for (tmpPtr = ConstraintData(theEnv)->ConstraintHashtable[hashValue];
        tmpPtr != NULL;
        tmpPtr = tmpPtr->next)
     {
      if (ConstraintCompare(theConstraint,tmpPtr))
        {
         tmpPtr->count++;
         ReturnConstraintRecord(theEnv,theConstraint);
         return tmpPtr;
        }
     }

   InstallConstraintRecord(theEnv,theConstraint);
   theConstraint->count = 1;
   theConstraint->bucket = (unsigned int) hashValue;
   theConstraint->installed = true;
   theConstraint->next = ConstraintData(theEnv)->ConstraintHashtable[hashValue];
   ConstraintData(theEnv)->ConstraintHashtable[hashValue] = theConstraint;
   return theConstraint;
  }

/*************************************************/
/* InstallConstraintRecord: Increments the count */
/*   values of all occurrences of primitive data */
/*   types found in a constraint record.         */
/*************************************************/
static void InstallConstraintRecord(
  Environment *theEnv,
  CONSTRAINT_RECORD *constraints)
  {
   struct expr *tempExpr;

   tempExpr = CL_AddHashedExpression(theEnv,constraints->classList);
   CL_ReturnExpression(theEnv,constraints->classList);
   constraints->classList = tempExpr;

   tempExpr = CL_AddHashedExpression(theEnv,constraints->restrictionList);
   CL_ReturnExpression(theEnv,constraints->restrictionList);
   constraints->restrictionList = tempExpr;

   tempExpr = CL_AddHashedExpression(theEnv,constraints->maxValue);
   CL_ReturnExpression(theEnv,constraints->maxValue);
   constraints->maxValue = tempExpr;

   tempExpr = CL_AddHashedExpression(theEnv,constraints->minValue);
   CL_ReturnExpression(theEnv,constraints->minValue);
   constraints->minValue = tempExpr;

   tempExpr = CL_AddHashedExpression(theEnv,constraints->minFields);
   CL_ReturnExpression(theEnv,constraints->minFields);
   constraints->minFields = tempExpr;

   tempExpr = CL_AddHashedExpression(theEnv,constraints->maxFields);
   CL_ReturnExpression(theEnv,constraints->maxFields);
   constraints->maxFields = tempExpr;

   if (constraints->multifield != NULL)
     { InstallConstraintRecord(theEnv,constraints->multifield); }
  }

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

/**********************************************/
/* CL_SDCCommand: H/L access routine for the     */
/*   set-dynamic-constraint-checking command. */
/**********************************************/
void CL_SDCCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theArg;

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_GetDynamicConstraintChecking(theEnv));

   if (! CL_UDFFirstArgument(context,ANY_TYPE_BITS,&theArg))
     { return; }

   CL_SetDynamicConstraintChecking(theEnv,theArg.value != FalseSymbol(theEnv));
  }

/**********************************************/
/* CL_GDCCommand: H/L access routine for the     */
/*   get-dynamic-constraint-checking command. */
/**********************************************/
void CL_GDCCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_GetDynamicConstraintChecking(theEnv));
  }

/******************************************************/
/* CL_SetDynamicConstraintChecking: C access routine     */
/*   for the set-dynamic-constraint-checking command. */
/******************************************************/
bool CL_SetDynamicConstraintChecking(
  Environment *theEnv,
  bool value)
  {
   bool ov;
   ov = ConstraintData(theEnv)->DynamicConstraintChecking;
   ConstraintData(theEnv)->DynamicConstraintChecking = value;
   return(ov);
  }

/******************************************************/
/* CL_GetDynamicConstraintChecking: C access routine     */
/*   for the get-dynamic-constraint-checking command. */
/******************************************************/
bool CL_GetDynamicConstraintChecking(
  Environment *theEnv)
  {
   return(ConstraintData(theEnv)->DynamicConstraintChecking);
  }

