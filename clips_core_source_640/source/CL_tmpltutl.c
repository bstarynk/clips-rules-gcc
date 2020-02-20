   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*            DEFTEMPLATE UTILITIES MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides utility routines for deftemplates.      */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Added support for templates CL_maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added additional arguments to                  */
/*            CL_InvalidDeftemplateSlotMessage function.        */
/*                                                           */
/*            Added additional arguments to                  */
/*            CL_PrintTemplateFact function.                    */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            CL_Watch facts for modify command only prints     */
/*            changed slots.                                 */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "constrct.h"
#include "cstrnchk.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "modulutl.h"
#include "multifld.h"
#include "prntutil.h"
#include "router.h"
#include "sysdep.h"
#include "tmpltbsc.h"
#include "tmpltdef.h"
#include "tmpltfun.h"
#include "tmpltpsr.h"
#include "watch.h"

#include "tmpltutl.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                     PrintTemplateSlot(Environment *,const char *,struct templateSlot *,CLIPSValue *);
   static struct templateSlot     *GetNextTemplateSlotToPrint(Environment *,struct fact *,struct templateSlot *,int *,int,const char *);

/********************************************************/
/* CL_InvalidDeftemplateSlotMessage: Generic error message */
/*   for use when a specified slot name isn't defined   */
/*   in its corresponding deftemplate.                  */
/********************************************************/
void CL_InvalidDeftemplateSlotMessage(
  Environment *theEnv,
  const char *slotName,
  const char *deftemplateName,
  bool printCR)
  {
   CL_PrintErrorID(theEnv,"TMPLTDEF",1,printCR);
   CL_WriteString(theEnv,STDERR,"Invalid slot '");
   CL_WriteString(theEnv,STDERR,slotName);
   CL_WriteString(theEnv,STDERR,"' not defined in corresponding deftemplate '");
   CL_WriteString(theEnv,STDERR,deftemplateName);
   CL_WriteString(theEnv,STDERR,"'.\n");
  }

/**********************************************************/
/* CL_SingleFieldCL_SlotCardinalityError: Generic error message */
/*   used when an attempt is made to placed a multifield  */
/*   value into a single field slot.                      */
/**********************************************************/
void CL_SingleFieldCL_SlotCardinalityError(
  Environment *theEnv,
  const char *slotName)
  {
   CL_PrintErrorID(theEnv,"TMPLTDEF",2,true);
   CL_WriteString(theEnv,STDERR,"The single field slot '");
   CL_WriteString(theEnv,STDERR,slotName);
   CL_WriteString(theEnv,STDERR,"' can only contain a single field value.\n");
  }

/**********************************************************************/
/* CL_MultiIntoSingleFieldSlotError: DeteCL_rmines if a multifield value is */
/*   being placed into a single field slot of a deftemplate fact.     */
/**********************************************************************/
void CL_MultiIntoSingleFieldSlotError(
  Environment *theEnv,
  struct templateSlot *theSlot,
  Deftemplate *theDeftemplate)
  {
   CL_PrintErrorID(theEnv,"TMPLTFUN",1,true);
   CL_WriteString(theEnv,STDERR,"Attempted to assert a multifield value ");
   CL_WriteString(theEnv,STDERR,"into the single field slot ");
   if (theSlot != NULL)
     {
      CL_WriteString(theEnv,STDERR,"'");
      CL_WriteString(theEnv,STDERR,theSlot->slotName->contents);
      CL_WriteString(theEnv,STDERR,"'");
     }
   else
     { CL_WriteString(theEnv,STDERR,"<<unknown>>"); }
   CL_WriteString(theEnv,STDERR," of deftemplate ");
   if (theDeftemplate != NULL)
     {
      CL_WriteString(theEnv,STDERR,"'");
      CL_WriteString(theEnv,STDERR,theDeftemplate->header.name->contents);
      CL_WriteString(theEnv,STDERR,"'");
     }
   else
     { CL_WriteString(theEnv,STDERR,"<<unknown>>"); }
   CL_WriteString(theEnv,STDERR,".\n");

   SetCL_EvaluationError(theEnv,true);
  }

/**************************************************************/
/* CL_CheckTemplateFact: Checks a fact to see if it violates any */
/*   deftemplate type, allowed-..., or range specifications.  */
/**************************************************************/
void CL_CheckTemplateFact(
  Environment *theEnv,
  Fact *theFact)
  {
   CLIPSValue *sublist;
   int i;
   Deftemplate *theDeftemplate;
   struct templateSlot *slotPtr;
   UDFValue theData;
   char thePlace[20];
   ConstraintViolationType rv;

   if (! CL_GetDynamicConstraintChecking(theEnv)) return;

   sublist = theFact->theProposition.contents;

   /*========================================================*/
   /* If the deftemplate corresponding to the first field of */
   /* of the fact cannot be found, then the fact cannot be   */
   /* checked against the deftemplate foCL_rmat.                */
   /*========================================================*/

   theDeftemplate = theFact->whichDeftemplate;
   if (theDeftemplate == NULL) return;
   if (theDeftemplate->implied) return;

   /*=============================================*/
   /* Check each of the slots of the deftemplate. */
   /*=============================================*/

   i = 0;
   for (slotPtr = theDeftemplate->slotList;
        slotPtr != NULL;
        slotPtr = slotPtr->next)
     {
      /*================================================*/
      /* Store the slot value in the appropriate foCL_rmat */
      /* for a call to the constraint checking routine. */
      /*================================================*/

      if (slotPtr->multislot == false)
        {
         theData.value = sublist[i].value;
         i++;
        }
      else
        {
         theData.value = (void *) sublist[i].value;
         theData.begin = 0;
         theData.range = sublist[i].multifieldValue->length;
         i++;
        }

      /*=============================================*/
      /* Call the constraint checking routine to see */
      /* if a constraint violation occurred.         */
      /*=============================================*/

      rv = CL_ConstraintCheckDataObject(theEnv,&theData,slotPtr->constraints);
      if (rv != NO_VIOLATION)
        {
         CL_gensprintf(thePlace,"fact f-%lld",theFact->factIndex);

         CL_PrintErrorID(theEnv,"CSTRNCHK",1,true);
         CL_WriteString(theEnv,STDERR,"Slot value ");
         CL_WriteUDFValue(theEnv,STDERR,&theData);
         CL_ConstraintViolationErrorMessage(theEnv,NULL,thePlace,false,0,slotPtr->slotName,
                                         0,rv,slotPtr->constraints,true);
         SetCL_HaltExecution(theEnv,true);
         return;
        }
     }

   return;
  }

/***********************************************************************/
/* CL_CheckRHSCL_SlotTypes: Checks the validity of a change to a slot as the */
/*   result of an assert, modify, or duplicate command. This checking  */
/*   is perfoCL_rmed statically (i.e. when the command is being parsed).  */
/***********************************************************************/
bool CL_CheckRHSCL_SlotTypes(
  Environment *theEnv,
  struct expr *rhsSlots,
  struct templateSlot *slotPtr,
  const char *thePlace)
  {
   ConstraintViolationType rv;
   const char *theName;

   rv = CL_ConstraintCheckExpressionChain(theEnv,rhsSlots,slotPtr->constraints);
   if (rv != NO_VIOLATION)
     {
      if (rv != CARDINALITY_VIOLATION) theName = "A literal slot value";
      else theName = "Literal slot values";
      CL_ConstraintViolationErrorMessage(theEnv,theName,thePlace,true,0,
                                      slotPtr->slotName,0,rv,slotPtr->constraints,true);
      return false;
     }

   return true;
  }

/*********************************************************/
/* CL_GetNthSlot: Given a deftemplate and an integer index, */
/*   returns the nth slot of a deftemplate.              */
/*********************************************************/
struct templateSlot *CL_GetNthSlot(
  Deftemplate *theDeftemplate,
  long long position)
  {
   struct templateSlot *slotPtr;
   long long i = 0;

   slotPtr = theDeftemplate->slotList;
   while (slotPtr != NULL)
     {
      if (i == position) return slotPtr;
      slotPtr = slotPtr->next;
      i++;
     }

   return NULL;
  }

/*******************************************************/
/* CL_FindSlotPosition: Finds the position of a specified */
/*   slot in a deftemplate structure.                  */
/*******************************************************/
int CL_FindSlotPosition(
  Deftemplate *theDeftemplate,
  CLIPSLexeme *name)
  {
   struct templateSlot *slotPtr;
   int position;

   for (slotPtr = theDeftemplate->slotList, position = 1;
        slotPtr != NULL;
        slotPtr = slotPtr->next, position++)
     {
      if (slotPtr->slotName == name)
        { return(position); }
     }

   return 0;
  }

/**********************/
/* PrintTemplateSlot: */
/**********************/
static void PrintTemplateSlot(
  Environment *theEnv,
  const char *logicalName,
  struct templateSlot *slotPtr,
  CLIPSValue *slotValue)
  {
   CL_WriteString(theEnv,logicalName,"(");
   CL_WriteString(theEnv,logicalName,slotPtr->slotName->contents);

   /*======================================================*/
   /* Print the value of the slot for a single field slot. */
   /*======================================================*/

   if (slotPtr->multislot == false)
     {
      CL_WriteString(theEnv,logicalName," ");
      CL_PrintAtom(theEnv,logicalName,((TypeHeader *) slotValue->value)->type,slotValue->value);
     }

   /*==========================================================*/
   /* Else print the value of the slot for a multi field slot. */
   /*==========================================================*/

   else
     {
      struct multifield *theSegment;

      theSegment = (Multifield *) slotValue->value;
      if (theSegment->length > 0)
        {
         CL_WriteString(theEnv,logicalName," ");
         CL_PrintMultifieldDriver(theEnv,logicalName,theSegment,0,theSegment->length,false);
        }
     }

   /*============================================*/
   /* Print the closing parenthesis of the slot. */
   /*============================================*/

   CL_WriteString(theEnv,logicalName,")");
  }

/********************************/
/* GetNextTemplateSloteToPrint: */
/********************************/
static struct templateSlot *GetNextTemplateSlotToPrint(
  Environment *theEnv,
  struct fact *theFact,
  struct templateSlot *slotPtr,
  int *position,
  int ignoreDefaults,
  const char *changeMap)
  {
   UDFValue tempDO;
   CLIPSValue *sublist;

   sublist = theFact->theProposition.contents;
   if (slotPtr == NULL)
     { slotPtr = theFact->whichDeftemplate->slotList; }
   else
     {
      slotPtr = slotPtr->next;
      (*position)++;
     }

   while (slotPtr != NULL)
     {
      if ((changeMap != NULL) && (TestBitMap(changeMap,*position) == 0))
        {
         (*position)++;
         slotPtr = slotPtr->next;
         continue;
        }

      if (ignoreDefaults && (slotPtr->defaultDynamic == false))
        {
         CL_DeftemplateSlotDefault(theEnv,theFact->whichDeftemplate,slotPtr,&tempDO,true);

         if (slotPtr->multislot == false)
           {
            if (tempDO.value == sublist[*position].value)
              {
               (*position)++;
               slotPtr = slotPtr->next;
               continue;
              }
           }
         else if (CL_MultifieldsEqual((Multifield *) tempDO.value,
                                   (Multifield *) sublist[*position].value))
           {
            (*position)++;
            slotPtr = slotPtr->next;
            continue;
           }
        }

      return slotPtr;
     }

   return NULL;
  }

/**********************************************************/
/* CL_PrintTemplateFact: Prints a fact using the deftemplate */
/*   foCL_rmat. Returns true if the fact was printed using   */
/*   this foCL_rmat, otherwise false.                        */
/**********************************************************/
void CL_PrintTemplateFact(
  Environment *theEnv,
  const char *logicalName,
  Fact *theFact,
  bool separateLines,
  bool ignoreDefaults,
  const char *changeMap)
  {
   CLIPSValue *sublist;
   int i;
   Deftemplate *theDeftemplate;
   struct templateSlot *slotPtr, *lastPtr = NULL;
   bool slotPrinted = false;

   /*==============================*/
   /* Initialize some infoCL_rmation. */
   /*==============================*/

   theDeftemplate = theFact->whichDeftemplate;
   sublist = theFact->theProposition.contents;

   /*=============================================*/
   /* Print the relation name of the deftemplate. */
   /*=============================================*/

   CL_WriteString(theEnv,logicalName,"(");
   CL_WriteString(theEnv,logicalName,theDeftemplate->header.name->contents);

   /*===================================================*/
   /* Print each of the field slots of the deftemplate. */
   /*===================================================*/

   i = 0;
   slotPtr = GetNextTemplateSlotToPrint(theEnv,theFact,lastPtr,&i,
                                        ignoreDefaults,changeMap);

   if ((changeMap != NULL) &&
       (theFact->whichDeftemplate->slotList != slotPtr))
     { CL_WriteString(theEnv,logicalName," ..."); }

   while (slotPtr != NULL)
     {
      /*===========================================*/
      /* Print the opening parenthesis of the slot */
      /* and the slot name.                        */
      /*===========================================*/

      if (! slotPrinted)
        {
         slotPrinted = true;
         CL_WriteString(theEnv,logicalName," ");
        }

      if (separateLines)
        { CL_WriteString(theEnv,logicalName,"\n   "); }

      /*====================================*/
      /* Print the slot name and its value. */
      /*====================================*/

      PrintTemplateSlot(theEnv,logicalName,slotPtr,&sublist[i]);

      /*===========================*/
      /* Move on to the next slot. */
      /*===========================*/

      lastPtr = slotPtr;
      slotPtr = GetNextTemplateSlotToPrint(theEnv,theFact,lastPtr,&i,
                                           ignoreDefaults,changeMap);

      if ((changeMap != NULL) && (lastPtr->next != slotPtr))
        { CL_WriteString(theEnv,logicalName," ..."); }

      if (slotPtr != NULL) CL_WriteString(theEnv,logicalName," ");
     }

   CL_WriteString(theEnv,logicalName,")");
  }

/***************************************************************************/
/* CL_UpdateDeftemplateScope: Updates the scope flag of all the deftemplates. */
/***************************************************************************/
void CL_UpdateDeftemplateScope(
  Environment *theEnv)
  {
   Deftemplate *theDeftemplate;
   unsigned int moduleCount;
   Defmodule *theModule;
   struct defmoduleItemHeader *theItem;

   /*==================================*/
   /* Loop through all of the modules. */
   /*==================================*/

   for (theModule = CL_GetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = CL_GetNextDefmodule(theEnv,theModule))
     {
      /*======================================================*/
      /* Loop through each of the deftemplates in the module. */
      /*======================================================*/

      theItem = (struct defmoduleItemHeader *)
                CL_GetModuleItem(theEnv,theModule,DeftemplateData(theEnv)->CL_DeftemplateModuleIndex);

      for (theDeftemplate = (Deftemplate *) theItem->firstItem;
           theDeftemplate != NULL ;
           theDeftemplate = CL_GetNextDeftemplate(theEnv,theDeftemplate))
        {
         /*=======================================*/
         /* If the deftemplate can be seen by the */
         /* current module, then it is in scope.  */
         /*=======================================*/

         if (CL_FindImportedConstruct(theEnv,"deftemplate",theModule,
                                   theDeftemplate->header.name->contents,
                                   &moduleCount,true,NULL) != NULL)
           { theDeftemplate->inScope = true; }
         else
           { theDeftemplate->inScope = false; }
        }
     }
  }

/****************************************************************/
/* CL_FindSlot: Finds a specified slot in a deftemplate structure. */
/****************************************************************/
struct templateSlot *CL_FindSlot(
  Deftemplate *theDeftemplate,
  CLIPSLexeme *name,
  unsigned short *whichOne)
  {
   struct templateSlot *slotPtr;

   if (whichOne != NULL) *whichOne = 0;
   slotPtr = theDeftemplate->slotList;
   while (slotPtr != NULL)
     {
      if (slotPtr->slotName == name)
        { return(slotPtr); }
      if (whichOne != NULL) (*whichOne)++;
      slotPtr = slotPtr->next;
     }

   return NULL;
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/************************************************************/
/* CL_CreateImpliedDeftemplate: Creates an implied deftemplate */
/*   and adds it to the list of deftemplates.               */
/************************************************************/
Deftemplate *CL_CreateImpliedDeftemplate(
  Environment *theEnv,
  CLIPSLexeme *deftemplateName,
  bool setFlag)
  {
   Deftemplate *newDeftemplate;

   newDeftemplate = get_struct(theEnv,deftemplate);
   newDeftemplate->header.name = deftemplateName;
   newDeftemplate->header.ppFoCL_rm = NULL;
   newDeftemplate->header.usrData = NULL;
   newDeftemplate->header.constructType = DEFTEMPLATE;
   newDeftemplate->header.env = theEnv;
   newDeftemplate->slotList = NULL;
   newDeftemplate->implied = setFlag;
   newDeftemplate->numberOfSlots = 0;
   newDeftemplate->inScope = 1;
   newDeftemplate->patternNetwork = NULL;
   newDeftemplate->factList = NULL;
   newDeftemplate->lastFact = NULL;
   newDeftemplate->busyCount = 0;
   newDeftemplate->watch = false;
   newDeftemplate->header.next = NULL;

#if DEBUGGING_FUNCTIONS
   if (CL_GetCL_WatchItem(theEnv,"facts") == 1)
     { CL_DeftemplateSetCL_Watch(newDeftemplate,true); }
#endif

   newDeftemplate->header.whichModule = (struct defmoduleItemHeader *)
                                        CL_GetModuleItem(theEnv,NULL,DeftemplateData(theEnv)->CL_DeftemplateModuleIndex);

   CL_AddConstructToModule(&newDeftemplate->header);
   CL_InstallDeftemplate(theEnv,newDeftemplate);

   return(newDeftemplate);
  }

#endif

#endif /* DEFTEMPLATE_CONSTRUCT */

