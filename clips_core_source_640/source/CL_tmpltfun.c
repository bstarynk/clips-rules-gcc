   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  09/20/17             */
   /*                                                     */
   /*             DEFTEMPLATE FUNCTIONS MODULE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the modify and duplicate functions.   */
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
/*      6.24: Added deftemplate-slot-names,                  */
/*            deftemplate-slot-default-value,                */
/*            deftemplate-slot-cardinality,                  */
/*            deftemplate-slot-allowed-values,               */
/*            deftemplate-slot-range,                        */
/*            deftemplate-slot-types,                        */
/*            deftemplate-slot-multip,                       */
/*            deftemplate-slot-singlep,                      */
/*            deftemplate-slot-existp, and                   */
/*            deftemplate-slot-defaultp functions.           */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for deftemplate slot facets.           */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added deftemplate-slot-facet-existp and        */
/*            deftemplate-slot-facet-value functions.        */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Support for modify callback function.          */
/*                                                           */
/*            Added additional argument to function          */
/*            CheckDeftemplateAndSlotArguments to specify    */
/*            the expected number of arguments.              */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            Increment/Decrement_ClearReadyLocks API.        */
/*                                                           */
/*      6.31: Error messages are now generated when modify   */
/*            and duplicate functions are given a retracted  */
/*            fact.                                          */
/*                                                           */
/*      6.40: Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
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
/*            Callbacks must be environment aware.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*            CL_Watch facts for modify command only prints     */
/*            changed slots.                                 */
/*                                                           */
/*            Modify command preserves fact id and address.  */
/*                                                           */
/*            CL_Assert returns duplicate fact. FALSE is now    */
/*            returned only if an error occurs.              */
/*                                                           */
/*            For the modify command, specifying the fact    */
/*            using a fact-index is no longer limited to     */
/*            top-level commands.                            */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "commline.h"
#include "constant.h"
#include "cstrnchk.h"
#include "default.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "factmngr.h"
#include "factrhs.h"
#include "memalloc.h"
#include "modulutl.h"
#include "multifld.h"
#include "pprint.h"
#include "prcdrpsr.h"
#include "prntutil.h"
#include "reorder.h"
#include "router.h"
#include "scanner.h"
#include "symbol.h"
#include "sysdep.h"
#include "tmpltdef.h"
#include "tmpltlhs.h"
#include "tmpltrhs.h"
#include "tmpltutl.h"
#include "utility.h"

#include "tmpltfun.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static CLIPSLexeme            *CheckDeftemplateAndSlotArguments(UDFContext *,Deftemplate **);
   static void                    FreeTemplateValueArray(Environment *,CLIPSValue *,Deftemplate *);
   static struct expr            *ModAndDupParse(Environment *,struct expr *,const char *,const char *);
#if (! RUN_TIME) && (! BLOAD_ONLY)
   static CLIPSLexeme            *FindTemplateForFactAddress(CLIPSLexeme *,struct lhsParseNode *);
#endif

/****************************************************************/
/* CL_DeftemplateFunctions: Initializes the deftemplate functions. */
/****************************************************************/
void CL_DeftemplateFunctions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   CL_AddUDF(theEnv,"modify","bf",0,UNBOUNDED,"*;lf",CL_ModifyCommand,"CL_ModifyCommand",NULL);
   CL_AddUDF(theEnv,"duplicate","bf",0,UNBOUNDED,"*;lf",CL_DuplicateCommand,"CL_DuplicateCommand",NULL);

   CL_AddUDF(theEnv,"deftemplate-slot-names","bm",1,1,"y",CL_DeftemplateSlotNamesFunction,"CL_DeftemplateSlotNamesFunction",NULL);
   CL_AddUDF(theEnv,"deftemplate-slot-default-value","*",2,2,"y",CL_Deftemplate_SlotDefaultValueFunction,"CL_Deftemplate_SlotDefaultValueFunction",NULL);
   CL_AddUDF(theEnv,"deftemplate-slot-cardinality","*",2,2,"y",CL_Deftemplate_SlotCardinalityFunction,"CL_Deftemplate_SlotCardinalityFunction",NULL);
   CL_AddUDF(theEnv,"deftemplate-slot-allowed-values","*",2,2,"y",CL_Deftemplate_SlotAllowedValuesFunction,"CL_Deftemplate_SlotAllowedValuesFunction",NULL);
   CL_AddUDF(theEnv,"deftemplate-slot-range","*",2,2,"y",CL_Deftemplate_SlotRangeFunction,"CL_Deftemplate_SlotRangeFunction",NULL);
   CL_AddUDF(theEnv,"deftemplate-slot-types","*",2,2,"y",CL_Deftemplate_SlotTypesFunction,"CL_Deftemplate_SlotTypesFunction",NULL);

   CL_AddUDF(theEnv,"deftemplate-slot-multip","b",2,2,"y",CL_DeftemplateSlotMultiPFunction,"CL_DeftemplateSlotMultiPFunction",NULL);
   CL_AddUDF(theEnv,"deftemplate-slot-singlep","b",2,2,"y",CL_DeftemplateSlotSinglePFunction,"CL_DeftemplateSlotSinglePFunction",NULL);
   CL_AddUDF(theEnv,"deftemplate-slot-existp","b",2,2,"y",CL_Deftemplate_SlotExistPFunction,"CL_Deftemplate_SlotExistPFunction",NULL);
   CL_AddUDF(theEnv,"deftemplate-slot-defaultp","y",2,2,"y",CL_Deftemplate_SlotDefaultPFunction,"CL_Deftemplate_SlotDefaultPFunction",NULL);

   CL_AddUDF(theEnv,"deftemplate-slot-facet-existp","b",3,3,"y",CL_DeftemplateSlotFacetExistPFunction,"CL_DeftemplateSlotFacetExistPFunction",NULL);

   CL_AddUDF(theEnv,"deftemplate-slot-facet-value","*",3,3,"y",CL_DeftemplateSlotFacetValueFunction,"CL_DeftemplateSlotFacetValueFunction",NULL);

   CL_FuncSeqOvlFlags(theEnv,"modify",false,false);
   CL_FuncSeqOvlFlags(theEnv,"duplicate",false,false);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif

   CL_AddFunctionParser(theEnv,"modify",CL_ModifyParse);
   CL_AddFunctionParser(theEnv,"duplicate",CL_DuplicateParse);
  }

/***************************/
/* FreeTemplateValueArray: */
/***************************/
static void FreeTemplateValueArray(
  Environment *theEnv,
  CLIPSValue *theValueArray,
  Deftemplate *templatePtr)
  {
   unsigned short i;

   if (theValueArray == NULL) return;

   for (i = 0; i < templatePtr->numberOfSlots; i++)
     {
      if (theValueArray[i].header->type == MULTIFIELD_TYPE)
        { CL_ReturnMultifield(theEnv,theValueArray[i].multifieldValue); }
     }

   CL_rm(theEnv,theValueArray,sizeof(CLIPSValue) * templatePtr->numberOfSlots);
  }

/*************************************************************/
/* CL_ModifyCommand: H/L access routine for the modify command. */
/*************************************************************/
void CL_ModifyCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   long long factNum;
   Fact *oldFact;
   struct expr *testPtr;
   UDFValue computeResult;
   Deftemplate *templatePtr;
   struct templateSlot *slotPtr;
   size_t i;
   long long position;
   int replacementCount = 0;
   bool found;
   CLIPSValue *theValueArray;
   char *changeMap;

   /*===================================================*/
   /* Set the default return value to the symbol FALSE. */
   /*===================================================*/

   returnValue->lexemeValue = FalseSymbol(theEnv);

   /*==================================================*/
   /* CL_Evaluate the first argument which is used to get */
   /* a pointer to the fact to be modified/duplicated. */
   /*==================================================*/

   testPtr = GetFirstArgument();
   Increment_ClearReadyLocks(theEnv);
   CL_EvaluateExpression(theEnv,testPtr,&computeResult);
   Decrement_ClearReadyLocks(theEnv);

   /*==============================================================*/
   /* If an integer is supplied, then treat it as a fact-index and */
   /* search the fact-list for the fact with that fact-index.      */
   /*==============================================================*/

   if (computeResult.header->type == INTEGER_TYPE)
     {
      factNum = computeResult.integerValue->contents;
      if (factNum < 0)
        {
         CL_ExpectedTypeError2(theEnv,"modify",1);
         Set_EvaluationError(theEnv,true);
         return;
        }

      oldFact = CL_GetNextFact(theEnv,NULL);
      while (oldFact != NULL)
        {
         if (oldFact->factIndex == factNum)
           { break; }
         else
           { oldFact = oldFact->nextFact; }
        }

      if (oldFact == NULL)
        {
         char tempBuffer[20];
         CL_gensprintf(tempBuffer,"f-%lld",factNum);
         CL_CantFindItemErrorMessage(theEnv,"fact",tempBuffer,false);
         return;
        }
     }

   /*==========================================*/
   /* Otherwise, if a pointer is supplied then */
   /* no lookup is required.                   */
   /*==========================================*/

   else if (computeResult.header->type == FACT_ADDRESS_TYPE)
     { oldFact = computeResult.factValue; }

   /*===========================================*/
   /* Otherwise, the first argument is invalid. */
   /*===========================================*/

   else
     {
      CL_ExpectedTypeError2(theEnv,"modify",1);
      Set_EvaluationError(theEnv,true);
      return;
     }

   /*=====================================*/
   /* CL_Retracted facts cannot be modified. */
   /*=====================================*/
   
   if (oldFact->garbage)
     {
      CL_Fact_RetractedErrorMessage(theEnv,oldFact);
      return;
     }
     
   /*==================================*/
   /* See if it is a deftemplate fact. */
   /*==================================*/

   templatePtr = oldFact->whichDeftemplate;

   if (templatePtr->implied) return;

   /*========================================================*/
   /* Create a data object array to hold the updated values. */
   /*========================================================*/
   
   if (templatePtr->numberOfSlots == 0)
     {
      theValueArray = NULL;
      changeMap = NULL;
     }
   else
     {
      theValueArray = (CLIPSValue *) CL_gm2(theEnv,sizeof(void *) * templatePtr->numberOfSlots);
      changeMap = (char *) CL_gm2(theEnv,CountToBitMapSize(templatePtr->numberOfSlots));
      CL_ClearBitString((void *) changeMap,CountToBitMapSize(templatePtr->numberOfSlots));
     }

   /*================================================================*/
   /* Duplicate the values from the old fact (skipping multifields). */
   /*================================================================*/

   for (i = 0; i < oldFact->theProposition.length; i++)
     { theValueArray[i].voidValue = VoidConstant(theEnv); }

   /*========================*/
   /* Start replacing slots. */
   /*========================*/

   testPtr = testPtr->nextArg;
   while (testPtr != NULL)
     {
      /*============================================================*/
      /* If the slot identifier is an integer, then the slot was    */
      /* previously identified and its position within the template */
      /* was stored. Otherwise, the position of the slot within the */
      /* deftemplate has to be dete_rmined by comparing the name of  */
      /* the slot against the list of slots for the deftemplate.    */
      /*============================================================*/

      if (testPtr->type == INTEGER_TYPE)
        { position = testPtr->integerValue->contents; }
      else
        {
         found = false;
         position = 0;
         slotPtr = templatePtr->slotList;
         while (slotPtr != NULL)
           {
            if (slotPtr->slotName == testPtr->lexemeValue)
              {
               found = true;
               slotPtr = NULL;
              }
            else
              {
               slotPtr = slotPtr->next;
               position++;
              }
           }

         if (! found)
           {
            CL_InvalidDeftemplateSlotMessage(theEnv,testPtr->lexemeValue->contents,
                                          templatePtr->header.name->contents,true);
            Set_EvaluationError(theEnv,true);
            FreeTemplateValueArray(theEnv,theValueArray,templatePtr);
            if (changeMap != NULL)
              { CL_rm(theEnv,(void *) changeMap,CountToBitMapSize(templatePtr->numberOfSlots)); }
            return;
           }
        }

      /*===================================================*/
      /* If a single field slot is being replaced, then... */
      /*===================================================*/

      if (oldFact->theProposition.contents[position].header->type != MULTIFIELD_TYPE)
        {
         /*======================================================*/
         /* If the list of values to store in the slot is empty  */
         /* or contains more than one member than an error has   */
         /* occured because a single field slot can only contain */
         /* a single value.                                      */
         /*======================================================*/

         if ((testPtr->argList == NULL) ? true : (testPtr->argList->nextArg != NULL))
           {
            CL_MultiIntoSingleFieldSlotError(theEnv,CL_GetNthSlot(templatePtr,position),templatePtr);
            FreeTemplateValueArray(theEnv,theValueArray,templatePtr);
            if (changeMap != NULL)
              { CL_rm(theEnv,(void *) changeMap,CountToBitMapSize(templatePtr->numberOfSlots)); }
            return;
           }

         /*===================================================*/
         /* CL_Evaluate the expression to be stored in the slot. */
         /*===================================================*/

         Increment_ClearReadyLocks(theEnv);
         CL_EvaluateExpression(theEnv,testPtr->argList,&computeResult);
         Set_EvaluationError(theEnv,false);
         Decrement_ClearReadyLocks(theEnv);

         /*====================================================*/
         /* If the expression evaluated to a multifield value, */
         /* then an error occured since a multifield value can */
         /* not be stored in a single field slot.              */
         /*====================================================*/

         if (computeResult.header->type == MULTIFIELD_TYPE)
           {
            CL_MultiIntoSingleFieldSlotError(theEnv,CL_GetNthSlot(templatePtr,position),templatePtr);
            FreeTemplateValueArray(theEnv,theValueArray,templatePtr);
            if (changeMap != NULL)
              { CL_rm(theEnv,(void *) changeMap,CountToBitMapSize(templatePtr->numberOfSlots)); }
            return;
           }

         /*=============================*/
         /* Store the value in the slot */
         /*=============================*/

         if (oldFact->theProposition.contents[position].value != computeResult.value)
           {
            replacementCount++;
            theValueArray[position].value = computeResult.value;
            if (changeMap != NULL)
              { SetBitMap(changeMap,position); }
           }
        }

      /*=================================*/
      /* Else replace a multifield slot. */
      /*=================================*/

      else
        {
         /*======================================*/
         /* Dete_rmine the new value of the slot. */
         /*======================================*/

         Increment_ClearReadyLocks(theEnv);
         CL_StoreInMultifield(theEnv,&computeResult,testPtr->argList,false);
         Set_EvaluationError(theEnv,false);
         Decrement_ClearReadyLocks(theEnv);

         /*=============================*/
         /* Store the value in the slot */
         /*=============================*/

         if ((oldFact->theProposition.contents[position].header->type != computeResult.header->type) ||
             (! CL_MultifieldsEqual((Multifield *) oldFact->theProposition.contents[position].value,(Multifield *) computeResult.value)))
           {
            theValueArray[position].value = computeResult.value;
            replacementCount++;
            if (changeMap != NULL)
              { SetBitMap(changeMap,position); }
           }
         else
           { CL_ReturnMultifield(theEnv,computeResult.multifieldValue); }
        }

      testPtr = testPtr->nextArg;
     }

   /*==================================*/
   /* If no slots have changed, then a */
   /* retract/assert is not perfo_rmed. */
   /*==================================*/

   if (replacementCount == 0)
     {
      if (theValueArray != NULL)
        { CL_rm(theEnv,theValueArray,sizeof(void *) * templatePtr->numberOfSlots); }
      if (changeMap != NULL)
        { CL_rm(theEnv,(void *) changeMap,CountToBitMapSize(templatePtr->numberOfSlots)); }
      
      returnValue->value = oldFact;
      return;
     }
    
   /*=========================================*/
   /* Replace the old values with the values. */
   /*=========================================*/
   
   if ((oldFact = CL_ReplaceFact(theEnv,oldFact,theValueArray,changeMap)) != NULL)
     { returnValue->factValue = oldFact; }

   /*=============================*/
   /* Free the data object array. */
   /*=============================*/

   if (theValueArray != NULL)
     { CL_rm(theEnv,theValueArray,sizeof(void *) * templatePtr->numberOfSlots); }

   if (changeMap != NULL)
     { CL_rm(theEnv,(void *) changeMap,CountToBitMapSize(templatePtr->numberOfSlots)); }

   return;
  }

/****************/
/* CL_ReplaceFact: */
/****************/
Fact *CL_ReplaceFact(
  Environment *theEnv,
  Fact *oldFact,
  CLIPSValue *theValueArray,
  char *changeMap)
  {
   size_t i;
   Fact *theFact;
   Fact *factListPosition, *templatePosition;
   
   /*===============================================*/
   /* Call registered modify notification functions */
   /* for the existing version of the fact.         */
   /*===============================================*/

   if (FactData(theEnv)->ListOfModifyFunctions != NULL)
     {
      Modify_CallFunctionItem *theModifyFunction;
      
      for (theModifyFunction = FactData(theEnv)->ListOfModifyFunctions;
           theModifyFunction != NULL;
           theModifyFunction = theModifyFunction->next)
        {
         (*theModifyFunction->func)(theEnv,oldFact,NULL,theModifyFunction->context);
        }
     }

   /*==========================================*/
   /* Remember the position of the fact before */
   /* it is retracted so this can be restored  */
   /* when the modified fact is asserted.      */
   /*==========================================*/

   factListPosition = oldFact->previousFact;
   templatePosition = oldFact->previousTemplateFact;
   
   /*===================*/
   /* CL_Retract the fact. */
   /*===================*/

   CL_RetractDriver(theEnv,oldFact,true,changeMap);
   oldFact->garbage = false;

   /*======================================*/
   /* Copy the new values to the old fact. */
   /*======================================*/

   for (i = 0; i < oldFact->theProposition.length; i++)
     {
      if (theValueArray[i].voidValue != VoidConstant(theEnv))
        {
         CL_AtomDeinstall(theEnv,oldFact->theProposition.contents[i].header->type,oldFact->theProposition.contents[i].value);
         
         if (oldFact->theProposition.contents[i].header->type == MULTIFIELD_TYPE)
           {
            Multifield *theSegment = oldFact->theProposition.contents[i].multifieldValue;
            if (theSegment->busyCount == 0)
              { CL_ReturnMultifield(theEnv,theSegment); }
            else
              { CL_AddToMultifieldList(theEnv,theSegment); }
           }

         oldFact->theProposition.contents[i].value = theValueArray[i].value;
         
         CL_AtomInstall(theEnv,oldFact->theProposition.contents[i].header->type,oldFact->theProposition.contents[i].value);
        }
     }

   /*======================*/
   /* CL_Assert the new fact. */
   /*======================*/

   theFact = CL_AssertDriver(oldFact,oldFact->factIndex,factListPosition,templatePosition,changeMap);

   /*===============================================*/
   /* Call registered modify notification functions */
   /* for the new version of the fact.              */
   /*===============================================*/

   if (FactData(theEnv)->ListOfModifyFunctions != NULL)
     {
      Modify_CallFunctionItem *theModifyFunction;

      for (theModifyFunction = FactData(theEnv)->ListOfModifyFunctions;
           theModifyFunction != NULL;
           theModifyFunction = theModifyFunction->next)
        {
         (*theModifyFunction->func)(theEnv,NULL,theFact,theModifyFunction->context);
        }
     }
     
   return theFact;
  }

/*******************************************************************/
/* CL_DuplicateCommand: H/L access routine for the duplicate command. */
/*******************************************************************/
void CL_DuplicateCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   long long factNum;
   Fact *oldFact, *newFact, *theFact;
   struct expr *testPtr;
   UDFValue computeResult;
   Deftemplate *templatePtr;
   struct templateSlot *slotPtr;
   size_t i;
   long long position;
   bool found;

   /*===================================================*/
   /* Set the default return value to the symbol FALSE. */
   /*===================================================*/

   returnValue->lexemeValue = FalseSymbol(theEnv);

   /*==================================================*/
   /* CL_Evaluate the first argument which is used to get */
   /* a pointer to the fact to be modified/duplicated. */
   /*==================================================*/

   testPtr = GetFirstArgument();
   Increment_ClearReadyLocks(theEnv);
   CL_EvaluateExpression(theEnv,testPtr,&computeResult);
   Decrement_ClearReadyLocks(theEnv);

   /*==============================================================*/
   /* If an integer is supplied, then treat it as a fact-index and */
   /* search the fact-list for the fact with that fact-index.      */
   /*==============================================================*/

   if (computeResult.header->type == INTEGER_TYPE)
     {
      factNum = computeResult.integerValue->contents;
      if (factNum < 0)
        {
         CL_ExpectedTypeError2(theEnv,"duplicate",1);
         Set_EvaluationError(theEnv,true);
         return;
        }

      oldFact = CL_GetNextFact(theEnv,NULL);
      while (oldFact != NULL)
        {
         if (oldFact->factIndex == factNum)
           { break; }
         else
           { oldFact = oldFact->nextFact; }
        }

      if (oldFact == NULL)
        {
         char tempBuffer[20];
         CL_gensprintf(tempBuffer,"f-%lld",factNum);
         CL_CantFindItemErrorMessage(theEnv,"fact",tempBuffer,false);
         return;
        }
     }

   /*==========================================*/
   /* Otherwise, if a pointer is supplied then */
   /* no lookup is required.                   */
   /*==========================================*/

   else if (computeResult.header->type == FACT_ADDRESS_TYPE)
     { oldFact = computeResult.factValue; }

   /*===========================================*/
   /* Otherwise, the first argument is invalid. */
   /*===========================================*/

   else
     {
      CL_ExpectedTypeError2(theEnv,"duplicate",1);
      Set_EvaluationError(theEnv,true);
      return;
     }

   /*=======================================*/
   /* CL_Retracted facts cannot be duplicated. */
   /*=======================================*/
   
   if (oldFact->garbage)
     {
      CL_Fact_RetractedErrorMessage(theEnv,oldFact);
      return;
     }

   /*==================================*/
   /* See if it is a deftemplate fact. */
   /*==================================*/

   templatePtr = oldFact->whichDeftemplate;

   if (templatePtr->implied) return;

   /*================================================================*/
   /* Duplicate the values from the old fact (skipping multifields). */
   /*================================================================*/

   newFact = CL_CreateFactBySize(theEnv,oldFact->theProposition.length);
   newFact->whichDeftemplate = templatePtr;
   for (i = 0; i < oldFact->theProposition.length; i++)
     {
      if (oldFact->theProposition.contents[i].header->type != MULTIFIELD_TYPE)
        { newFact->theProposition.contents[i].value = oldFact->theProposition.contents[i].value; }
      else
        { newFact->theProposition.contents[i].value = NULL; }
     }

   /*========================*/
   /* Start replacing slots. */
   /*========================*/

   testPtr = testPtr->nextArg;
   while (testPtr != NULL)
     {
      /*============================================================*/
      /* If the slot identifier is an integer, then the slot was    */
      /* previously identified and its position within the template */
      /* was stored. Otherwise, the position of the slot within the */
      /* deftemplate has to be dete_rmined by comparing the name of  */
      /* the slot against the list of slots for the deftemplate.    */
      /*============================================================*/

      if (testPtr->type == INTEGER_TYPE)
        { position = testPtr->integerValue->contents; }
      else
        {
         found = false;
         position = 0;
         slotPtr = templatePtr->slotList;
         while (slotPtr != NULL)
           {
            if (slotPtr->slotName == testPtr->lexemeValue)
              {
               found = true;
               slotPtr = NULL;
              }
            else
              {
               slotPtr = slotPtr->next;
               position++;
              }
           }

         if (! found)
           {
            CL_InvalidDeftemplateSlotMessage(theEnv,testPtr->lexemeValue->contents,
                                          templatePtr->header.name->contents,true);
            Set_EvaluationError(theEnv,true);
            CL_ReturnFact(theEnv,newFact);
            return;
           }
        }

      /*===================================================*/
      /* If a single field slot is being replaced, then... */
      /*===================================================*/

      if (newFact->theProposition.contents[position].value != NULL)
        {
         /*======================================================*/
         /* If the list of values to store in the slot is empty  */
         /* or contains more than one member than an error has   */
         /* occured because a single field slot can only contain */
         /* a single value.                                      */
         /*======================================================*/

         if ((testPtr->argList == NULL) ? true : (testPtr->argList->nextArg != NULL))
           {
            CL_MultiIntoSingleFieldSlotError(theEnv,CL_GetNthSlot(templatePtr,position),templatePtr);
            CL_ReturnFact(theEnv,newFact);
            return;
           }

         /*===================================================*/
         /* CL_Evaluate the expression to be stored in the slot. */
         /*===================================================*/

         Increment_ClearReadyLocks(theEnv);
         CL_EvaluateExpression(theEnv,testPtr->argList,&computeResult);
         Set_EvaluationError(theEnv,false);
         Decrement_ClearReadyLocks(theEnv);

         /*====================================================*/
         /* If the expression evaluated to a multifield value, */
         /* then an error occured since a multifield value can */
         /* not be stored in a single field slot.              */
         /*====================================================*/

         if (computeResult.header->type == MULTIFIELD_TYPE)
           {
            CL_ReturnFact(theEnv,newFact);
            CL_MultiIntoSingleFieldSlotError(theEnv,CL_GetNthSlot(templatePtr,position),templatePtr);
            return;
           }

         /*=============================*/
         /* Store the value in the slot */
         /*=============================*/

         newFact->theProposition.contents[position].value = computeResult.value;
        }

      /*=================================*/
      /* Else replace a multifield slot. */
      /*=================================*/

      else
        {
         /*======================================*/
         /* Dete_rmine the new value of the slot. */
         /*======================================*/

         Increment_ClearReadyLocks(theEnv);
         CL_StoreInMultifield(theEnv,&computeResult,testPtr->argList,false);
         Set_EvaluationError(theEnv,false);
         Decrement_ClearReadyLocks(theEnv);

         /*=============================*/
         /* Store the value in the slot */
         /*=============================*/

         newFact->theProposition.contents[position].value = computeResult.value;
        }

      testPtr = testPtr->nextArg;
     }

   /*=====================================*/
   /* Copy the multifield values from the */
   /* old fact that were not replaced.    */
   /*=====================================*/

   for (i = 0; i < oldFact->theProposition.length; i++)
     {
      if ((oldFact->theProposition.contents[i].header->type == MULTIFIELD_TYPE) &&
          (newFact->theProposition.contents[i].value == NULL))

        {
         newFact->theProposition.contents[i].value =
            CL_CopyMultifield(theEnv,oldFact->theProposition.contents[i].multifieldValue);
        }
     }

   /*===============================*/
   /* Perfo_rm the duplicate action. */
   /*===============================*/

   theFact = CL_AssertDriver(newFact,0,NULL,NULL,NULL);

   /*========================================*/
   /* The asserted fact is the return value. */
   /*========================================*/

   if (theFact != NULL)
     {
      returnValue->begin = 0;
      returnValue->range = theFact->theProposition.length;
      returnValue->value = theFact;
     }

   return;
  }

/****************************************************/
/* CL_DeftemplateSlotNamesFunction: H/L access routine */
/*   for the deftemplate-slot-names function.       */
/****************************************************/
void CL_DeftemplateSlotNamesFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *deftemplateName;
   Deftemplate *theDeftemplate;
   CLIPSValue cv;

   /*=============================================*/
   /* Set up the default return value for errors. */
   /*=============================================*/

   returnValue->value = FalseSymbol(theEnv);

   /*=======================================*/
   /* Get the reference to the deftemplate. */
   /*=======================================*/

   deftemplateName = CL_GetConstructName(context,"deftemplate-slot-names","deftemplate name");
   if (deftemplateName == NULL) return;

   theDeftemplate = CL_FindDeftemplate(theEnv,deftemplateName);
   if (theDeftemplate == NULL)
     {
      CL_CantFindItemErrorMessage(theEnv,"deftemplate",deftemplateName,true);
      return;
     }

   /*=====================*/
   /* Get the slot names. */
   /*=====================*/

   CL_DeftemplateSlotNames(theDeftemplate,&cv);
   CL_CLIPSToUDFValue(&cv,returnValue);
  }

/**********************************************/
/* CL_DeftemplateSlotNames: C access routine for */
/*   the deftemplate-slot-names function.     */
/**********************************************/
void CL_DeftemplateSlotNames(
  Deftemplate *theDeftemplate,
  CLIPSValue *returnValue)
  {
   Multifield *theList;
   struct templateSlot *theSlot;
   unsigned long count;
   Environment *theEnv = theDeftemplate->header.env;

   /*===============================================*/
   /* If we're dealing with an implied deftemplate, */
   /* then the only slot names is "implied."        */
   /*===============================================*/

   if (theDeftemplate->implied)
     {
      theList = CL_CreateMultifield(theEnv,1);
      theList->contents[0].lexemeValue = CL_CreateSymbol(theEnv,"implied");
      returnValue->value = theList;
      return;
     }

   /*=================================*/
   /* Count the number of slot names. */
   /*=================================*/

   for (count = 0, theSlot = theDeftemplate->slotList;
        theSlot != NULL;
        count++, theSlot = theSlot->next)
     { /* Do Nothing */ }

   /*=============================================================*/
   /* Create a multifield value in which to store the slot names. */
   /*=============================================================*/

   theList = CL_CreateMultifield(theEnv,count);
   returnValue->value = theList;

   /*===============================================*/
   /* Store the slot names in the multifield value. */
   /*===============================================*/

   for (count = 0, theSlot = theDeftemplate->slotList;
        theSlot != NULL;
        count++, theSlot = theSlot->next)
     {
      theList->contents[count].lexemeValue = theSlot->slotName;
     }
  }

/*******************************************************/
/* CL_Deftemplate_SlotDefaultPFunction: H/L access routine */
/*   for the deftemplate-slot-defaultp function.       */
/*******************************************************/
void CL_Deftemplate_SlotDefaultPFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;
   int defaultType;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*===============================*/
   /* Does the slot have a default? */
   /*===============================*/

   defaultType = CL_Deftemplate_SlotDefaultP(theDeftemplate,slotName->contents);

   if (defaultType == STATIC_DEFAULT)
     { returnValue->lexemeValue = CL_CreateSymbol(theEnv,"static"); }
   else if (defaultType == DYNAMIC_DEFAULT)
     { returnValue->lexemeValue = CL_CreateSymbol(theEnv,"dynamic"); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*************************************************/
/* CL_Deftemplate_SlotDefaultP: C access routine for */
/*   the deftemplate-slot-defaultp function.     */
/*************************************************/
DefaultType CL_Deftemplate_SlotDefaultP(
  Deftemplate *theDeftemplate,
  const char *slotName)
  {
   struct templateSlot *theSlot;
   Environment *theEnv = theDeftemplate->header.env;

   /*==================================================*/
   /* Make sure the slot exists (the symbol implied is */
   /* used for the implied slot of an ordered fact).   */
   /*==================================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") == 0)
        {
         return STATIC_DEFAULT;
        }
      else
        {
         Set_EvaluationError(theEnv,true);
         CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                       theDeftemplate->header.name->contents,false);
         return NO_DEFAULT;
        }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     {
      Set_EvaluationError(theEnv,true);
      CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                    theDeftemplate->header.name->contents,false);
      return NO_DEFAULT;
     }

   /*======================================*/
   /* Return the default type of the slot. */
   /*======================================*/

   if (theSlot->noDefault)
     { return NO_DEFAULT; }
   else if (theSlot->defaultDynamic)
     { return DYNAMIC_DEFAULT; }

   return STATIC_DEFAULT;
  }

/*************************************************************/
/* CL_Deftemplate_SlotDefaultValueFunction: H/L access routine   */
/*   for the deftemplate-slot-default-value function.        */
/*************************************************************/
void CL_Deftemplate_SlotDefaultValueFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;
   CLIPSValue cv;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*=========================================*/
   /* Get the deftemplate slot default value. */
   /*=========================================*/

   CL_Deftemplate_SlotDefaultValue(theDeftemplate,slotName->contents,&cv);
   CL_CLIPSToUDFValue(&cv,returnValue);
  }

/******************************************************/
/* CL_Deftemplate_SlotDefaultValue: C access routine for  */
/*   the deftemplate-slot-default-value function.     */
/******************************************************/
bool CL_Deftemplate_SlotDefaultValue(
  Deftemplate *theDeftemplate,
  const char *slotName,
  CLIPSValue *theValue)
  {
   struct templateSlot *theSlot;
   UDFValue tempDO;
   Environment *theEnv = theDeftemplate->header.env;

   /*=============================================*/
   /* Set up the default return value for errors. */
   /*=============================================*/

   theValue->value = FalseSymbol(theEnv);

   /*==================================================*/
   /* Make sure the slot exists (the symbol implied is */
   /* used for the implied slot of an ordered fact).   */
   /*==================================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") == 0)
        {
         theValue->value = CL_CreateMultifield(theEnv,0L);
         return true;
        }
      else
        {
         Set_EvaluationError(theEnv,true);
         CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                       theDeftemplate->header.name->contents,false);
         return false;
        }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     {
      Set_EvaluationError(theEnv,true);
      CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                    theDeftemplate->header.name->contents,false);
      return false;
     }

   /*=======================================*/
   /* Return the default value of the slot. */
   /*=======================================*/

   if (theSlot->noDefault)
     { theValue->value = CL_CreateSymbol(theEnv,"?NONE"); }
   else if (CL_DeftemplateSlotDefault(theEnv,theDeftemplate,theSlot,&tempDO,true))
     {
      CL_No_rmalizeMultifield(theEnv,&tempDO);
      theValue->value = tempDO.value;
     }
   else
     { return false; }

   return true;
  }

/**********************************************************/
/* CL_Deftemplate_SlotCardinalityFunction: H/L access routine */
/*   for the deftemplate-slot-cardinality function.       */
/**********************************************************/
void CL_Deftemplate_SlotCardinalityFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;
   CLIPSValue cv;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      CL_SetMultifieldErrorValue(theEnv,returnValue);
      return;
     }

   /*=======================================*/
   /* Get the deftemplate slot cardinality. */
   /*=======================================*/

   CL_Deftemplate_SlotCardinality(theDeftemplate,slotName->contents,&cv);
   CL_CLIPSToUDFValue(&cv,returnValue);
  }

/****************************************************/
/* CL_Deftemplate_SlotCardinality: C access routine for */
/*   the deftemplate-slot-cardinality function.     */
/****************************************************/
bool CL_Deftemplate_SlotCardinality(
  Deftemplate *theDeftemplate,
  const char *slotName,
  CLIPSValue *returnValue)
  {
   struct templateSlot *theSlot;
   Environment *theEnv = theDeftemplate->header.env;

   /*===============================================*/
   /* If we're dealing with an implied deftemplate, */
   /* then the only slot names is "implied."        */
   /*===============================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") == 0)
        {
         returnValue->value = CL_CreateMultifield(theEnv,2L);
         returnValue->multifieldValue->contents[0].integerValue = SymbolData(theEnv)->Zero;
         returnValue->multifieldValue->contents[1].lexemeValue = SymbolData(theEnv)->PositiveInfinity;
         return true;
        }
      else
        {
         returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
         Set_EvaluationError(theEnv,true);
         CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                       theDeftemplate->header.name->contents,false);
         return false;
        }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     {
      returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
      Set_EvaluationError(theEnv,true);
      CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                    theDeftemplate->header.name->contents,false);
      return false;
     }

   /*=====================================*/
   /* Return the cardinality of the slot. */
   /*=====================================*/

   if (theSlot->multislot == 0)
     {
      returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
      return true;
     }

   returnValue->value = CL_CreateMultifield(theEnv,2L);

   if (theSlot->constraints != NULL)
     {
      returnValue->multifieldValue->contents[0].value = theSlot->constraints->minFields->value;
      returnValue->multifieldValue->contents[1].value = theSlot->constraints->maxFields->value;
     }
   else
     {
      returnValue->multifieldValue->contents[0].integerValue = SymbolData(theEnv)->Zero;
      returnValue->multifieldValue->contents[1].lexemeValue = SymbolData(theEnv)->PositiveInfinity;
     }
     
   return true;
  }

/************************************************************/
/* CL_Deftemplate_SlotAllowedValuesFunction: H/L access routine */
/*   for the deftemplate-slot-allowed-values function.      */
/************************************************************/
void CL_Deftemplate_SlotAllowedValuesFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;
   CLIPSValue result;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      CL_SetMultifieldErrorValue(theEnv,returnValue);
      return;
     }

   /*==========================================*/
   /* Get the deftemplate slot allowed values. */
   /*==========================================*/

   CL_Deftemplate_SlotAllowedValues(theDeftemplate,slotName->contents,&result);
   CL_CLIPSToUDFValue(&result,returnValue);
  }

/*******************************************************/
/* CL_Deftemplate_SlotAllowedValues: C access routine      */
/*   for the deftemplate-slot-allowed-values function. */
/*******************************************************/
bool CL_Deftemplate_SlotAllowedValues(
  Deftemplate *theDeftemplate,
  const char *slotName,
  CLIPSValue *returnValue)
  {
   struct templateSlot *theSlot;
   int i;
   Expression *theExp;
   Environment *theEnv = theDeftemplate->header.env;

   /*===============================================*/
   /* If we're dealing with an implied deftemplate, */
   /* then the only slot names is "implied."        */
   /*===============================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") == 0)
        {
         returnValue->value = FalseSymbol(theEnv);
         return true;
        }
      else
        {
         returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
         Set_EvaluationError(theEnv,true);
         CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                       theDeftemplate->header.name->contents,false);
         return false;
        }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     {
      returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
      Set_EvaluationError(theEnv,true);
      CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                    theDeftemplate->header.name->contents,false);
      return false;
     }

   /*========================================*/
   /* Return the allowed values of the slot. */
   /*========================================*/

   if ((theSlot->constraints != NULL) ? (theSlot->constraints->restrictionList == NULL) : true)
     {
      returnValue->value = FalseSymbol(theEnv);
      return true;
     }

   returnValue->value = CL_CreateMultifield(theEnv,CL_ExpressionSize(theSlot->constraints->restrictionList));
   i = 0;

   theExp = theSlot->constraints->restrictionList;
   while (theExp != NULL)
     {
      returnValue->multifieldValue->contents[i].value = theExp->value;
      theExp = theExp->nextArg;
      i++;
     }
     
   return true;
  }

/****************************************************/
/* CL_Deftemplate_SlotRangeFunction: H/L access routine */
/*   for the deftemplate-slot-range function.       */
/****************************************************/
void CL_Deftemplate_SlotRangeFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;
   CLIPSValue cv;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      CL_SetMultifieldErrorValue(theEnv,returnValue);
      return;
     }

   /*=================================*/
   /* Get the deftemplate slot range. */
   /*=================================*/

   CL_Deftemplate_SlotRange(theDeftemplate,slotName->contents,&cv);
   CL_CLIPSToUDFValue(&cv,returnValue);
  }

/**********************************************/
/* CL_Deftemplate_SlotRange: C access routine for */
/*   the deftemplate-slot-range function.     */
/**********************************************/
bool CL_Deftemplate_SlotRange(
  Deftemplate *theDeftemplate,
  const char *slotName,
  CLIPSValue *returnValue)
  {
   struct templateSlot *theSlot;
   Environment *theEnv = theDeftemplate->header.env;

   /*===============================================*/
   /* If we're dealing with an implied deftemplate, */
   /* then the only slot names is "implied."        */
   /*===============================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") == 0)
        {
         returnValue->value = CL_CreateMultifield(theEnv,2L);
         returnValue->multifieldValue->contents[0].lexemeValue =
            SymbolData(theEnv)->NegativeInfinity;
         returnValue->multifieldValue->contents[1].lexemeValue =
            SymbolData(theEnv)->PositiveInfinity;
         return true;
        }
      else
        {
         returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
         Set_EvaluationError(theEnv,true);
         CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                       theDeftemplate->header.name->contents,false);
         return false;
        }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     {
      returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
      Set_EvaluationError(theEnv,true);
      CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                    theDeftemplate->header.name->contents,false);
      return false;
     }

   /*===============================*/
   /* Return the range of the slot. */
   /*===============================*/

   if ((theSlot->constraints == NULL) ? false :
       (theSlot->constraints->anyAllowed || theSlot->constraints->floatsAllowed ||
        theSlot->constraints->integersAllowed))
     {
      returnValue->value = CL_CreateMultifield(theEnv,2L);
      returnValue->multifieldValue->contents[0].value = theSlot->constraints->minValue->value;
      returnValue->multifieldValue->contents[1].value = theSlot->constraints->maxValue->value;
     }
   else
     { returnValue->value = FalseSymbol(theEnv); }
     
   return true;
  }

/****************************************************/
/* CL_Deftemplate_SlotTypesFunction: H/L access routine */
/*   for the deftemplate-slot-types function.       */
/****************************************************/
void CL_Deftemplate_SlotTypesFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;
   CLIPSValue cv;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      CL_SetMultifieldErrorValue(theEnv,returnValue);
      return;
     }

   /*=================================*/
   /* Get the deftemplate slot types. */
   /*=================================*/

   CL_Deftemplate_SlotTypes(theDeftemplate,slotName->contents,&cv);
   CL_CLIPSToUDFValue(&cv,returnValue);
  }

/**********************************************/
/* CL_Deftemplate_SlotTypes: C access routine for */
/*   the deftemplate-slot-types function.     */
/**********************************************/
bool CL_Deftemplate_SlotTypes(
  Deftemplate *theDeftemplate,
  const char *slotName,
  CLIPSValue *returnValue)
  {
   struct templateSlot *theSlot = NULL;
   unsigned int numTypes, i;
   bool allTypes = false;
   Environment *theEnv = theDeftemplate->header.env;

   /*===============================================*/
   /* If we're dealing with an implied deftemplate, */
   /* then the only slot name is "implied."         */
   /*===============================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") != 0)
        {
         returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
         Set_EvaluationError(theEnv,true);
         CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                       theDeftemplate->header.name->contents,false);
         return false;
        }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     {
      returnValue->multifieldValue = CL_CreateMultifield(theEnv,0L);
      Set_EvaluationError(theEnv,true);
      CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                    theDeftemplate->header.name->contents,false);
      return false;
     }

   /*==============================================*/
   /* If the slot has no constraint info_rmation or */
   /* there is no type restriction, then all types */
   /* are allowed for the slot.                    */
   /*==============================================*/

   if ((theDeftemplate->implied) ||
       ((theSlot->constraints != NULL) ? theSlot->constraints->anyAllowed : true))
     {
#if OBJECT_SYSTEM
      numTypes = 8;
#else
      numTypes = 6;
#endif
      allTypes = true;
     }

   /*==============================================*/
   /* Otherwise count the number of types allowed. */
   /*==============================================*/

   else
     {
      numTypes = theSlot->constraints->symbolsAllowed +
                 theSlot->constraints->stringsAllowed +
                 theSlot->constraints->floatsAllowed +
                 theSlot->constraints->integersAllowed +
                 theSlot->constraints->instanceNamesAllowed +
                 theSlot->constraints->instanceAddressesAllowed +
                 theSlot->constraints->externalAddressesAllowed +
                 theSlot->constraints->factAddressesAllowed;
     }

   /*========================================*/
   /* Return the allowed types for the slot. */
   /*========================================*/

   returnValue->value = CL_CreateMultifield(theEnv,numTypes);

   i = 0;

   if (allTypes || theSlot->constraints->floatsAllowed)
     {
      returnValue->multifieldValue->contents[i++].lexemeValue = CL_CreateSymbol(theEnv,"FLOAT");
     }

   if (allTypes || theSlot->constraints->integersAllowed)
     {
      returnValue->multifieldValue->contents[i++].lexemeValue = CL_CreateSymbol(theEnv,"INTEGER");
     }

   if (allTypes || theSlot->constraints->symbolsAllowed)
     {
      returnValue->multifieldValue->contents[i++].lexemeValue = CL_CreateSymbol(theEnv,"SYMBOL");
     }

   if (allTypes || theSlot->constraints->stringsAllowed)
     {
      returnValue->multifieldValue->contents[i++].lexemeValue = CL_CreateSymbol(theEnv,"STRING");
     }

   if (allTypes || theSlot->constraints->externalAddressesAllowed)
     {
      returnValue->multifieldValue->contents[i++].lexemeValue = CL_CreateSymbol(theEnv,"EXTERNAL-ADDRESS");
     }

   if (allTypes || theSlot->constraints->factAddressesAllowed)
     {
      returnValue->multifieldValue->contents[i++].lexemeValue = CL_CreateSymbol(theEnv,"FACT-ADDRESS");
     }

#if OBJECT_SYSTEM
   if (allTypes || theSlot->constraints->instanceAddressesAllowed)
     {
      returnValue->multifieldValue->contents[i++].lexemeValue = CL_CreateSymbol(theEnv,"INSTANCE-ADDRESS");
     }

   if (allTypes || theSlot->constraints->instanceNamesAllowed)
     {
      returnValue->multifieldValue->contents[i++].lexemeValue = CL_CreateSymbol(theEnv,"INSTANCE-NAME");
     }
#endif

   return true;
  }

/*****************************************************/
/* CL_DeftemplateSlotMultiPFunction: H/L access routine */
/*   for the deftemplate-slot-multip function.       */
/*****************************************************/
void CL_DeftemplateSlotMultiPFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*================================*/
   /* Is the slot a multifield slot? */
   /*================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_DeftemplateSlotMultiP(theDeftemplate,slotName->contents));
  }

/***********************************************/
/* CL_DeftemplateSlotMultiP: C access routine for */
/*   the deftemplate-slot-multip function.     */
/***********************************************/
bool CL_DeftemplateSlotMultiP(
  Deftemplate *theDeftemplate,
  const char *slotName)
  {
   struct templateSlot *theSlot;
   Environment *theEnv = theDeftemplate->header.env;

   /*===============================================*/
   /* If we're dealing with an implied deftemplate, */
   /* then the only slot names is "implied."        */
   /*===============================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") == 0)
        { return true; }
      else
        {
         Set_EvaluationError(theEnv,true);
         CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                       theDeftemplate->header.name->contents,false);
         return false;
        }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     {
      Set_EvaluationError(theEnv,true);
      CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                    theDeftemplate->header.name->contents,false);
      return false;
     }

   /*================================*/
   /* Is the slot a multifield slot? */
   /*================================*/

   return(theSlot->multislot);
  }

/******************************************************/
/* CL_DeftemplateSlotSinglePFunction: H/L access routine */
/*   for the deftemplate-slot-singlep function.       */
/******************************************************/
void CL_DeftemplateSlotSinglePFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*==================================*/
   /* Is the slot a single field slot? */
   /*==================================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_DeftemplateSlotSingleP(theDeftemplate,slotName->contents));
  }

/************************************************/
/* CL_DeftemplateSlotSingleP: C access routine for */
/*   the deftemplate-slot-singlep function.     */
/************************************************/
bool CL_DeftemplateSlotSingleP(
  Deftemplate *theDeftemplate,
  const char *slotName)
  {
   struct templateSlot *theSlot;
   Environment *theEnv = theDeftemplate->header.env;

   /*===============================================*/
   /* If we're dealing with an implied deftemplate, */
   /* then the only slot names is "implied."        */
   /*===============================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") == 0)
        { return false; }
      else
        {
         Set_EvaluationError(theEnv,true);
         CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                       theDeftemplate->header.name->contents,false);
         return false;
        }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     {
      Set_EvaluationError(theEnv,true);
      CL_InvalidDeftemplateSlotMessage(theEnv,slotName,
                                    theDeftemplate->header.name->contents,false);
      return false;
     }

   /*==================================*/
   /* Is the slot a single field slot? */
   /*==================================*/

   return(! theSlot->multislot);
  }

/*****************************************************/
/* CL_Deftemplate_SlotExistPFunction: H/L access routine */
/*   for the deftemplate-slot-existp function.       */
/*****************************************************/
void CL_Deftemplate_SlotExistPFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*======================*/
   /* Does the slot exist? */
   /*======================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_Deftemplate_SlotExistP(theDeftemplate,slotName->contents));
  }

/***********************************************/
/* CL_Deftemplate_SlotExistP: C access routine for */
/*   the deftemplate-slot-existp function.     */
/***********************************************/
bool CL_Deftemplate_SlotExistP(
  Deftemplate *theDeftemplate,
  const char *slotName)
  {
   Environment *theEnv = theDeftemplate->header.env;

   /*===============================================*/
   /* If we're dealing with an implied deftemplate, */
   /* then the only slot names is "implied."        */
   /*===============================================*/

   if (theDeftemplate->implied)
     {
      if (strcmp(slotName,"implied") == 0)
        { return true; }
      else
        { return false; }
     }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if (CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL) == NULL)
     { return false; }

   /*==================*/
   /* The slot exists. */
   /*==================*/

   return true;
  }

/**********************************************************/
/* CL_DeftemplateSlotFacetExistPFunction: H/L access routine */
/*   for the deftemplate-slot-facet-existp function.      */
/**********************************************************/
void CL_DeftemplateSlotFacetExistPFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;
   UDFValue facetName;

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*============================*/
   /* Get the name of the facet. */
   /*============================*/

   if (! CL_UDFNextArgument(context,SYMBOL_BIT,&facetName))
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*======================*/
   /* Does the slot exist? */
   /*======================*/

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_DeftemplateSlotFacetExistP(theEnv,theDeftemplate,slotName->contents,facetName.lexemeValue->contents));
  }

/****************************************************/
/* CL_DeftemplateSlotFacetExistP: C access routine for */
/*   the deftemplate-slot-facet-existp function.    */
/****************************************************/
bool CL_DeftemplateSlotFacetExistP(
  Environment *theEnv,
  Deftemplate *theDeftemplate,
  const char *slotName,
  const char *facetName)
  {
   struct templateSlot *theSlot;
   CLIPSLexeme *facetHN;
   struct expr *tempFacet;

   /*=================================================*/
   /* An implied deftemplate doesn't have any facets. */
   /*=================================================*/

   if (theDeftemplate->implied)
     { return false; }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     { return false; }

   /*=======================*/
   /* Search for the facet. */
   /*=======================*/

   facetHN = CL_FindSymbolHN(theEnv,facetName,SYMBOL_BIT);
   for (tempFacet = theSlot->facetList;
        tempFacet != NULL;
        tempFacet = tempFacet->nextArg)
     {
      if (tempFacet->value == facetHN)
        { return true; }
     }

   /*===========================*/
   /* The facet does not exist. */
   /*===========================*/

   return false;
  }

/*********************************************************/
/* CL_DeftemplateSlotFacetValueFunction: H/L access routine */
/*   for the deftemplate-slot-facet-value function.      */
/*********************************************************/
void CL_DeftemplateSlotFacetValueFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Deftemplate *theDeftemplate;
   CLIPSLexeme *slotName;
   UDFValue facetName;

   /*=============================================*/
   /* Set up the default return value for errors. */
   /*=============================================*/

   returnValue->lexemeValue = FalseSymbol(theEnv);

   /*===================================================*/
   /* Retrieve the deftemplate and slot name arguments. */
   /*===================================================*/

   slotName = CheckDeftemplateAndSlotArguments(context,&theDeftemplate);
   if (slotName == NULL)
     { return; }

   /*============================*/
   /* Get the name of the facet. */
   /*============================*/

   if (! CL_UDFNthArgument(context,3,SYMBOL_BIT,&facetName))
     { return; }

   /*===========================*/
   /* Retrieve the facet value. */
   /*===========================*/

   CL_DeftemplateSlotFacetValue(theEnv,theDeftemplate,slotName->contents,facetName.lexemeValue->contents,returnValue);
  }

/****************************************************/
/* CL_DeftemplateSlotFacetValue: C access routine      */
/*   for the deftemplate-slot-facet-value function. */
/****************************************************/
bool CL_DeftemplateSlotFacetValue(
  Environment *theEnv,
  Deftemplate *theDeftemplate,
  const char *slotName,
  const char *facetName,
  UDFValue *rv)
  {
   struct templateSlot *theSlot;
   CLIPSLexeme *facetHN;
   struct expr *tempFacet;

   /*=================================================*/
   /* An implied deftemplate doesn't have any facets. */
   /*=================================================*/

   if (theDeftemplate->implied)
     { return false; }

   /*============================================*/
   /* Otherwise search for the slot name in the  */
   /* list of slots defined for the deftemplate. */
   /*============================================*/

   else if ((theSlot = CL_FindSlot(theDeftemplate,CL_CreateSymbol(theEnv,slotName),NULL)) == NULL)
     { return false; }

   /*=======================*/
   /* Search for the facet. */
   /*=======================*/

   facetHN = CL_FindSymbolHN(theEnv,facetName,SYMBOL_BIT);
   for (tempFacet = theSlot->facetList;
        tempFacet != NULL;
        tempFacet = tempFacet->nextArg)
     {
      if (tempFacet->value == facetHN)
        {
         CL_EvaluateExpression(theEnv,tempFacet->argList,rv);
         return true;
        }
     }

   /*===========================*/
   /* The facet does not exist. */
   /*===========================*/

   return false;
  }

/************************************************************/
/* CheckDeftemplateAndSlotArguments: Checks the deftemplate */
/*   and slot arguments for various functions.              */
/************************************************************/
static CLIPSLexeme *CheckDeftemplateAndSlotArguments(
  UDFContext *context,
  Deftemplate **theDeftemplate)
  {
   UDFValue theArg;
   const char *deftemplateName;
   Environment *theEnv = context->environment;

   /*=======================================*/
   /* Get the reference to the deftemplate. */
   /*=======================================*/

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theArg))
     { return NULL; }

   deftemplateName = theArg.lexemeValue->contents;

   *theDeftemplate = CL_FindDeftemplate(theEnv,deftemplateName);
   if (*theDeftemplate == NULL)
     {
      CL_CantFindItemErrorMessage(theEnv,"deftemplate",deftemplateName,true);
      return NULL;
     }

   /*===========================*/
   /* Get the name of the slot. */
   /*===========================*/

   if (! CL_UDFNextArgument(context,SYMBOL_BIT,&theArg))
     { return NULL; }

   return theArg.lexemeValue;
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/***************************************************************/
/* CL_UpdateModifyDuplicate: Changes the modify/duplicate command */
/*   found on the RHS of a rule such that the positions of the */
/*   slots for replacement are stored rather than the slot     */
/*   name which allows quicker replacement of slots. This      */
/*   substitution can only take place when the deftemplate     */
/*   type is known (i.e. if a fact-index is used you don't     */
/*   know which type of deftemplate is going to be replaced    */
/*   until you actually do the replacement of slots).          */
/***************************************************************/
bool CL_UpdateModifyDuplicate(
  Environment *theEnv,
  struct expr *top,
  const char *name,
  void *vTheLHS)
  {
   struct expr *functionArgs, *tempArg;
   CLIPSLexeme *templateName;
   Deftemplate *theDeftemplate;
   struct templateSlot *slotPtr;

   /*========================================*/
   /* Dete_rmine the fact-address or index to */
   /* be retracted by the modify command.    */
   /*========================================*/

   functionArgs = top->argList;
   if (functionArgs->type == SF_VARIABLE)
     {
      if (CL_SearchParsedBindNames(theEnv,functionArgs->lexemeValue) != 0)
        { return true; }
      templateName = FindTemplateForFactAddress(functionArgs->lexemeValue,
                                                (struct lhsParseNode *) vTheLHS);
      if (templateName == NULL) return true;
     }
   else
     { return true; }

   /*========================================*/
   /* Make sure that the fact being modified */
   /* has a corresponding deftemplate.       */
   /*========================================*/

   theDeftemplate = (Deftemplate *)
                    CL_LookupConstruct(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,
                                    templateName->contents,
                                    false);

   if (theDeftemplate == NULL) return true;

   if (theDeftemplate->implied) return true;

   /*=============================================================*/
   /* Make sure all the slot names are valid for the deftemplate. */
   /*=============================================================*/

   tempArg = functionArgs->nextArg;
   while (tempArg != NULL)
     {
      /*======================*/
      /* Does the slot exist? */
      /*======================*/

      if ((slotPtr = CL_FindSlot(theDeftemplate,tempArg->lexemeValue,NULL)) == NULL)
        {
         CL_InvalidDeftemplateSlotMessage(theEnv,tempArg->lexemeValue->contents,
                                       theDeftemplate->header.name->contents,true);
         return false;
        }

      /*=========================================================*/
      /* Is a multifield value being put in a single field slot? */
      /*=========================================================*/

      if (slotPtr->multislot == false)
        {
         if (tempArg->argList == NULL)
           {
            CL_SingleField_SlotCardinalityError(theEnv,slotPtr->slotName->contents);
            return false;
           }
         else if (tempArg->argList->nextArg != NULL)
           {
            CL_SingleField_SlotCardinalityError(theEnv,slotPtr->slotName->contents);
            return false;
           }
         else if (tempArg->argList->type == FCALL)
           {
            if ((ExpressionUnknownFunctionType(tempArg->argList) & SINGLEFIELD_BITS) == 0)
              {
               CL_SingleField_SlotCardinalityError(theEnv,slotPtr->slotName->contents);
               return false;
              }
           }
         else if (tempArg->argList->type == MF_VARIABLE)
           {
            CL_SingleField_SlotCardinalityError(theEnv,slotPtr->slotName->contents);
            return false;
           }
        }

      /*======================================*/
      /* Are the slot restrictions satisfied? */
      /*======================================*/

      if (CL_CheckRHS_SlotTypes(theEnv,tempArg->argList,slotPtr,name) == 0)
        return false;

      /*=============================================*/
      /* Replace the slot with the integer position. */
      /*=============================================*/

      tempArg->type = INTEGER_TYPE;
      tempArg->value = CL_CreateInteger(theEnv,(long long) (CL_FindSlotPosition(theDeftemplate,tempArg->lexemeValue) - 1));

      tempArg = tempArg->nextArg;
     }

   return true;
  }

/**************************************************/
/* FindTemplateForFactAddress: Searches for the   */
/*   deftemplate name associated with the pattern */
/*   to which a fact address has been bound.      */
/**************************************************/
static CLIPSLexeme *FindTemplateForFactAddress(
  CLIPSLexeme *factAddress,
  struct lhsParseNode *theLHS)
  {
   struct lhsParseNode *thePattern = NULL;

   /*===============================================*/
   /* Look through the LHS patterns for the pattern */
   /* which is bound to the fact address used by    */
   /* the modify/duplicate function.                */
   /*===============================================*/

   while (theLHS != NULL)
     {
      if (theLHS->value == (void *) factAddress)
        {
         thePattern = theLHS;
         theLHS = NULL;
        }
      else
        { theLHS = theLHS->bottom; }
     }

   if (thePattern == NULL) return NULL;

   /*=====================================*/
   /* Verify that just a symbol is stored */
   /* as the first field of the pattern.  */
   /*=====================================*/

   thePattern = thePattern->right;
   if ((thePattern->pnType != SF_WILDCARD_NODE) || (thePattern->bottom == NULL))
     { return NULL; }

   thePattern = thePattern->bottom;
   if ((thePattern->pnType != SYMBOL_NODE) ||
            (thePattern->right != NULL) ||
            (thePattern->bottom != NULL))
    { return NULL; }

   /*==============================*/
   /* Return the deftemplate name. */
   /*==============================*/

   return thePattern->lexemeValue;
  }
#endif

/*******************************************/
/* CL_ModifyParse: Parses the modify command. */
/*******************************************/
struct expr *CL_ModifyParse(
  Environment *theEnv,
  struct expr *top,
  const char *logicalName)
  {
   return ModAndDupParse(theEnv,top,logicalName,"modify");
  }

/*************************************************/
/* CL_DuplicateParse: Parses the duplicate command. */
/*************************************************/
struct expr *CL_DuplicateParse(
  Environment *theEnv,
  struct expr *top,
  const char *logicalName)
  {
   return ModAndDupParse(theEnv,top,logicalName,"duplicate");
  }

/*************************************************************/
/* ModAndDupParse: Parses the modify and duplicate commands. */
/*************************************************************/
static struct expr *ModAndDupParse(
  Environment *theEnv,
  struct expr *top,
  const char *logicalName,
  const char *name)
  {
   bool error = false;
   struct token theToken;
   struct expr *nextOne, *tempSlot;
   struct expr *newField, *firstField, *lastField;
   bool printError;
   bool done;

   /*==================================================================*/
   /* Parse the fact-address or index to the modify/duplicate command. */
   /*==================================================================*/

   CL_SavePPBuffer(theEnv," ");
   CL_GetToken(theEnv,logicalName,&theToken);

   if ((theToken.tknType == SF_VARIABLE_TOKEN) || (theToken.tknType == GBL_VARIABLE_TOKEN))
     { nextOne = CL_GenConstant(theEnv,CL_TokenTypeToType(theToken.tknType),theToken.value); }
   else if (theToken.tknType == INTEGER_TOKEN)
     { nextOne = CL_GenConstant(theEnv,INTEGER_TYPE,theToken.value); }
   else if (theToken.tknType == LEFT_PARENTHESIS_TOKEN)
     {
      nextOne = CL_Function1Parse(theEnv,logicalName);
      if (nextOne == NULL)
        {
         CL_ReturnExpression(theEnv,top);
         return NULL;
        }
     }
   else
     {
      CL_ExpectedTypeError2(theEnv,name,1);
      CL_ReturnExpression(theEnv,top);
      return NULL;
     }

   top->argList = nextOne;
   nextOne = top->argList;

   /*=======================================================*/
   /* Parse the re_maining modify/duplicate slot specifiers. */
   /*=======================================================*/

   CL_GetToken(theEnv,logicalName,&theToken);
   while (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
     {
      CL_PPBackup(theEnv);
      CL_SavePPBuffer(theEnv," ");
      CL_SavePPBuffer(theEnv,theToken.printFo_rm);

      /*=================================================*/
      /* Slot definition begins with a left parenthesis. */
      /*=================================================*/

      if (theToken.tknType != LEFT_PARENTHESIS_TOKEN)
        {
         CL_SyntaxErrorMessage(theEnv,"duplicate/modify function");
         CL_ReturnExpression(theEnv,top);
         return NULL;
        }

      /*=================================*/
      /* The slot name must be a symbol. */
      /*=================================*/

      CL_GetToken(theEnv,logicalName,&theToken);
      if (theToken.tknType != SYMBOL_TOKEN)
        {
         CL_SyntaxErrorMessage(theEnv,"duplicate/modify function");
         CL_ReturnExpression(theEnv,top);
         return NULL;
        }

      /*=================================*/
      /* Check for duplicate slot names. */
      /*=================================*/

      for (tempSlot = top->argList->nextArg;
           tempSlot != NULL;
           tempSlot = tempSlot->nextArg)
        {
         if (tempSlot->value == theToken.value)
           {
            CL_AlreadyParsedErrorMessage(theEnv,"slot ",theToken.lexemeValue->contents);
            CL_ReturnExpression(theEnv,top);
            return NULL;
           }
        }

      /*=========================================*/
      /* Add the slot name to the list of slots. */
      /*=========================================*/

      nextOne->nextArg = CL_GenConstant(theEnv,SYMBOL_TYPE,theToken.value);
      nextOne = nextOne->nextArg;

      /*====================================================*/
      /* Get the values to be stored in the specified slot. */
      /*====================================================*/

      firstField = NULL;
      lastField = NULL;
      done = false;
      while (! done)
        {
         CL_SavePPBuffer(theEnv," ");
         newField = Get_AssertArgument(theEnv,logicalName,&theToken,&error,
                                      RIGHT_PARENTHESIS_TOKEN,false,&printError);

         if (error)
           {
            if (printError) CL_SyntaxErrorMessage(theEnv,"deftemplate pattern");
            CL_ReturnExpression(theEnv,top);
            return NULL;
           }

         if (newField == NULL)
           { done = true; }

         if (lastField == NULL)
           { firstField = newField; }
         else
           { lastField->nextArg = newField; }
         lastField = newField;
        }

      /*================================================*/
      /* Slot definition ends with a right parenthesis. */
      /*================================================*/

      if (theToken.tknType != RIGHT_PARENTHESIS_TOKEN)
        {
         CL_SyntaxErrorMessage(theEnv,"duplicate/modify function");
         CL_ReturnExpression(theEnv,top);
         CL_ReturnExpression(theEnv,firstField);
         return NULL;
        }
      else
        {
         CL_PPBackup(theEnv);
         CL_PPBackup(theEnv);
         CL_SavePPBuffer(theEnv,")");
        }

      nextOne->argList = firstField;

      CL_GetToken(theEnv,logicalName,&theToken);
     }

   /*================================================*/
   /* Return the parsed modify/duplicate expression. */
   /*================================================*/

   return top;
  }

#endif /* DEFTEMPLATE_CONSTRUCT */

