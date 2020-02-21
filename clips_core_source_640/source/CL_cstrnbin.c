   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*            CONSTRAINT BLOAD/BSAVE MODULE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for      */
/*    constraint records.                                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added allowed-classes slot facet.              */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"
#include "bload.h"

#if BLOAD_AND_BSAVE
#include "bsave.h"
#endif

#include "cstrnbin.h"

/*******************/
/* DATA STRUCTURES */
/*******************/

struct bsaveConstraintRecord
  {
   unsigned int anyAllowed : 1;
   unsigned int symbolsAllowed : 1;
   unsigned int stringsAllowed : 1;
   unsigned int floatsAllowed : 1;
   unsigned int integersAllowed : 1;
   unsigned int instanceNamesAllowed : 1;
   unsigned int instanceAddressesAllowed : 1;
   unsigned int externalAddressesAllowed : 1;
   unsigned int factAddressesAllowed : 1;
   unsigned int anyRestriction : 1;
   unsigned int symbolRestriction : 1;
   unsigned int stringRestriction : 1;
   unsigned int numberRestriction : 1;
   unsigned int floatRestriction : 1;
   unsigned int integerRestriction : 1;
   unsigned int classRestriction : 1;
   unsigned int instanceNameRestriction : 1;
   unsigned int multifieldsAllowed : 1;
   unsigned int singlefieldsAllowed : 1;
   unsigned long classList;
   unsigned long restrictionList;
   unsigned long minValue;
   unsigned long maxValue;
   unsigned long minFields;
   unsigned long maxFields;
  };

typedef struct bsaveConstraintRecord BSAVE_CONSTRAINT_RECORD;

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                    CopyToCL_BsaveConstraintRecord(Environment *,CONSTRAINT_RECORD *,BSAVE_CONSTRAINT_RECORD *);
#endif
   static void                    CopyFromCL_BsaveConstraintRecord(Environment *,void *,unsigned long);

#if BLOAD_AND_BSAVE

/**************************************************/
/* CL_WriteNeededConstraints: CL_Writes the constraints */
/*   in the constraint table to the binary image  */
/*   currently being saved.                       */
/**************************************************/
void CL_WriteNeededConstraints(
  Environment *theEnv,
  FILE *fp)
  {
   int i;
   unsigned short theIndex = 0;
   unsigned long numberOfUsedConstraints = 0;
   CONSTRAINT_RECORD *tmpPtr;
   BSAVE_CONSTRAINT_RECORD bsaveConstraints;

   /*================================*/
   /* Get the number of constraints. */
   /*================================*/

   for (i = 0; i < SIZE_CONSTRAINT_HASH; i++)
     {
      for (tmpPtr = ConstraintData(theEnv)->ConstraintHashtable[i];
           tmpPtr != NULL;
           tmpPtr = tmpPtr->next)
        {
         tmpPtr->bsaveIndex = theIndex++;
         numberOfUsedConstraints++;
        }
     }

   /*=============================================*/
   /* If dynamic constraint checking is disabled, */
   /* then no constraints are saved.              */
   /*=============================================*/

   if ((! CL_GetDynamicConstraintChecking(theEnv)) && (numberOfUsedConstraints != 0))
     {
      numberOfUsedConstraints = 0;
      CL_PrintWarningID(theEnv,"CSTRNBIN",1,false);
      CL_WriteString(theEnv,STDWRN,"Constraints are not saved with a binary image\n");
      CL_WriteString(theEnv,STDWRN,"  when dynamic constraint checking is disabled.\n");
     }

   /*============================================*/
   /* CL_Write out the number of constraints in the */
   /* constraint table followed by each of the   */
   /* constraints in the constraint table.       */
   /*============================================*/

   CL_GenCL_Write(&numberOfUsedConstraints,sizeof(unsigned long),fp);
   if (numberOfUsedConstraints == 0) return;

   for (i = 0 ; i < SIZE_CONSTRAINT_HASH; i++)
     {
      for (tmpPtr = ConstraintData(theEnv)->ConstraintHashtable[i];
           tmpPtr != NULL;
           tmpPtr = tmpPtr->next)
        {
         CopyToCL_BsaveConstraintRecord(theEnv,tmpPtr,&bsaveConstraints);
         CL_GenCL_Write(&bsaveConstraints,sizeof(BSAVE_CONSTRAINT_RECORD),fp);
        }
     }
  }

/****************************************************/
/* CopyToCL_BsaveConstraintRecord: Copies a constraint */
/*   record to the data structure used for storing  */
/*   constraints in a binary image.                 */
/****************************************************/
static void CopyToCL_BsaveConstraintRecord(
  Environment *theEnv,
  CONSTRAINT_RECORD *constraints,
  BSAVE_CONSTRAINT_RECORD *bsaveConstraints)
  {
   bsaveConstraints->anyAllowed = constraints->anyAllowed;
   bsaveConstraints->symbolsAllowed = constraints->symbolsAllowed;
   bsaveConstraints->stringsAllowed = constraints->stringsAllowed;
   bsaveConstraints->floatsAllowed = constraints->floatsAllowed;
   bsaveConstraints->integersAllowed = constraints->integersAllowed;
   bsaveConstraints->instanceNamesAllowed = constraints->instanceNamesAllowed;
   bsaveConstraints->instanceAddressesAllowed = constraints->instanceAddressesAllowed;
   bsaveConstraints->externalAddressesAllowed = constraints->externalAddressesAllowed;
   bsaveConstraints->multifieldsAllowed = constraints->multifieldsAllowed;
   bsaveConstraints->singlefieldsAllowed = constraints->singlefieldsAllowed;
   bsaveConstraints->factAddressesAllowed = constraints->factAddressesAllowed;
   bsaveConstraints->anyRestriction = constraints->anyRestriction;
   bsaveConstraints->symbolRestriction = constraints->symbolRestriction;
   bsaveConstraints->stringRestriction = constraints->stringRestriction;
   bsaveConstraints->floatRestriction = constraints->floatRestriction;
   bsaveConstraints->integerRestriction = constraints->integerRestriction;
   bsaveConstraints->classRestriction = constraints->classRestriction;
   bsaveConstraints->instanceNameRestriction = constraints->instanceNameRestriction;

   bsaveConstraints->restrictionList = CL_HashedExpressionIndex(theEnv,constraints->restrictionList);
   bsaveConstraints->classList = CL_HashedExpressionIndex(theEnv,constraints->classList);
   bsaveConstraints->minValue = CL_HashedExpressionIndex(theEnv,constraints->minValue);
   bsaveConstraints->maxValue = CL_HashedExpressionIndex(theEnv,constraints->maxValue);
   bsaveConstraints->minFields = CL_HashedExpressionIndex(theEnv,constraints->minFields);
   bsaveConstraints->maxFields = CL_HashedExpressionIndex(theEnv,constraints->maxFields);
  }

#endif /* BLOAD_AND_BSAVE */

/********************************************************/
/* CL_ReadNeededConstraints: Reads in the constraints used */
/*   by the binary image currently being loaded.        */
/********************************************************/
void CL_ReadNeededConstraints(
  Environment *theEnv)
  {
   CL_GenReadBinary(theEnv,&ConstraintData(theEnv)->NumberOfConstraints,sizeof(unsigned long));
   if (ConstraintData(theEnv)->NumberOfConstraints == 0) return;

   ConstraintData(theEnv)->ConstraintArray = (CONSTRAINT_RECORD *)
           CL_genalloc(theEnv,(sizeof(CONSTRAINT_RECORD) * ConstraintData(theEnv)->NumberOfConstraints));

   CL_BloadandCL_Refresh(theEnv,ConstraintData(theEnv)->NumberOfConstraints,sizeof(BSAVE_CONSTRAINT_RECORD),
                   CopyFromCL_BsaveConstraintRecord);
  }

/*****************************************************/
/* CopyFromCL_BsaveConstraintRecord: Copies values to a */
/*   constraint record from the data structure used  */
/*   for storing constraints in a binary image.      */
/*****************************************************/
static void CopyFromCL_BsaveConstraintRecord(
  Environment *theEnv,
  void *buf,
  unsigned long theIndex)
  {
   BSAVE_CONSTRAINT_RECORD *bsaveConstraints;
   CONSTRAINT_RECORD *constraints;

   bsaveConstraints = (BSAVE_CONSTRAINT_RECORD *) buf;
   constraints = (CONSTRAINT_RECORD *) &ConstraintData(theEnv)->ConstraintArray[theIndex];

   constraints->anyAllowed = bsaveConstraints->anyAllowed;
   constraints->symbolsAllowed = bsaveConstraints->symbolsAllowed;
   constraints->stringsAllowed = bsaveConstraints->stringsAllowed;
   constraints->floatsAllowed = bsaveConstraints->floatsAllowed;
   constraints->integersAllowed = bsaveConstraints->integersAllowed;
   constraints->instanceNamesAllowed = bsaveConstraints->instanceNamesAllowed;
   constraints->instanceAddressesAllowed = bsaveConstraints->instanceAddressesAllowed;
   constraints->externalAddressesAllowed = bsaveConstraints->externalAddressesAllowed;
   constraints->voidAllowed = false;
   constraints->multifieldsAllowed = bsaveConstraints->multifieldsAllowed;
   constraints->singlefieldsAllowed = bsaveConstraints->singlefieldsAllowed;
   constraints->factAddressesAllowed = bsaveConstraints->factAddressesAllowed;
   constraints->anyRestriction = bsaveConstraints->anyRestriction;
   constraints->symbolRestriction = bsaveConstraints->symbolRestriction;
   constraints->stringRestriction = bsaveConstraints->stringRestriction;
   constraints->floatRestriction = bsaveConstraints->floatRestriction;
   constraints->integerRestriction = bsaveConstraints->integerRestriction;
   constraints->classRestriction = bsaveConstraints->classRestriction;
   constraints->instanceNameRestriction = bsaveConstraints->instanceNameRestriction;

   constraints->restrictionList = HashedExpressionPointer(bsaveConstraints->restrictionList);
   constraints->classList = HashedExpressionPointer(bsaveConstraints->classList);
   constraints->minValue = HashedExpressionPointer(bsaveConstraints->minValue);
   constraints->maxValue = HashedExpressionPointer(bsaveConstraints->maxValue);
   constraints->minFields = HashedExpressionPointer(bsaveConstraints->minFields);
   constraints->maxFields = HashedExpressionPointer(bsaveConstraints->maxFields);
   constraints->multifield = NULL;
  }

/********************************************************/
/* CL_ClearCL_BloadedConstraints: CL_Releases memory associated  */
/*   with constraints loaded from binary image          */
/********************************************************/
void CL_ClearCL_BloadedConstraints(
  Environment *theEnv)
  {
   if (ConstraintData(theEnv)->NumberOfConstraints != 0)
     {
      CL_genfree(theEnv,ConstraintData(theEnv)->ConstraintArray,
                     (sizeof(CONSTRAINT_RECORD) * ConstraintData(theEnv)->NumberOfConstraints));
      ConstraintData(theEnv)->NumberOfConstraints = 0;
     }
  }

#endif /* (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */




