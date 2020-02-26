   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/02/18             */
   /*                                                     */
   /*                 FACT MANAGER MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides core routines for CL_maintaining the fact  */
/*   list including assert/retract operations, data          */
/*   structure creation/deletion, printing, slot access,     */
/*   and other utility functions.                            */
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
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            CL_AssignFactSlotDefaults function does not       */
/*            properly handle defaults for multifield slots. */
/*            DR0869                                         */
/*                                                           */
/*            Support for ppfact command.                    */
/*                                                           */
/*      6.30: Callback function support for assertion,       */
/*            retraction, and modification of facts.         */
/*                                                           */
/*            Updates to fact pattern entity record.         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Removed unused global variables.               */
/*                                                           */
/*            Added code to prevent a clear command from     */
/*            being executed during fact assertions via      */
/*            JoinOperationInProgress mechanism.             */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
/*                                                           */
/*      6.31: Added NULL check for slotName in function      */
/*            Env_GetFactSlot. Return value of FALSE now      */
/*            returned if garbage flag set for fact.         */
/*                                                           */
/*            Added constraint checking for slot value in    */
/*            Env_PutFactSlot function.                       */
/*                                                           */
/*            Calling Env_FactExistp for a fact that has      */
/*            been created, but not asserted now returns     */
/*            FALSE.                                         */
/*                                                           */
/*            Calling Env_Retract for a fact that has been    */
/*            created, but not asserted now returns FALSE.   */
/*                                                           */
/*            Calling Env_AssignFactSlotDefaults or           */
/*            Env_PutFactSlot for a fact that has been        */
/*            asserted now returns FALSE.                    */
/*                                                           */
/*            CL_Retracted and existing facts cannot be         */
/*            asserted.                                      */
/*                                                           */
/*            Crash bug fix for modifying fact with invalid  */
/*            slot name.                                     */
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
/*            Removed initial-fact support.                  */
/*                                                           */
/*            CL_Watch facts for modify command only prints     */
/*            changed slots.                                 */
/*                                                           */
/*            Modify command preserves fact id and address.  */
/*                                                           */
/*            CL_Assert returns duplicate fact. FALSE is now    */
/*            returned only if an error occurs.              */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT

#include "commline.h"
#include "default.h"
#include "engine.h"
#include "factbin.h"
#include "factcmp.h"
#include "factcom.h"
#include "factfun.h"
#include "factmch.h"
#include "factqury.h"
#include "factrhs.h"
#include "lgcldpnd.h"
#include "memalloc.h"
#include "multifld.h"
#include "retract.h"
#include "prntutil.h"
#include "router.h"
#include "strngrtr.h"
#include "sysdep.h"
#include "tmpltbsc.h"
#include "tmpltfun.h"
#include "tmpltutl.h"
#include "utility.h"
#include "watch.h"
#include "cstrnchk.h"

#include "factmngr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void CL_Reset_Facts (Environment *, void *);
static bool CL_Clear_FactsReady (Environment *, void *);
static void RemoveGarbage_Facts (Environment *, void *);
static void DeallocateFactData (Environment *);
static bool CL_RetractCallback (Fact *, Environment *);

/**************************************************************/
/* Initialize_Facts: Initializes the fact data representation. */
/*   CL_Facts are only available when both the defrule and       */
/*   deftemplate constructs are available.                    */
/**************************************************************/
void
Initialize_Facts (Environment * theEnv)
{
  struct patternEntityRecord factInfo =
    { {"FACT_ADDRESS_TYPE", FACT_ADDRESS_TYPE, 1, 0, 0,
       (Entity_PrintFunction *) CL_PrintFactIdentifier,
       (Entity_PrintFunction *) CL_PrintFactIdentifierInLongFo_rm,
       (bool (*)(void *, Environment *)) CL_RetractCallback,
       NULL,
       (void *(*)(void *, void *)) CL_GetNextFact,
       (EntityBusyCountFunction *) CL_DecrementFactCallback,
       (EntityBusyCountFunction *) CL_IncrementFactCallback,
       NULL, NULL, NULL, NULL, NULL},
  (void (*)(Environment *, void *)) CL_DecrementFactBasisCount,
  (void (*)(Environment *, void *)) CL_IncrementFactBasisCount,
  (void (*)(Environment *, void *)) CL_MatchFactFunction,
  NULL,
  (bool (*)(Environment *, void *)) CL_FactIsDeleted
  };

  Fact dummyFact = { {{{FACT_ADDRESS_TYPE}, NULL, NULL, 0, 0L}},
  NULL, NULL, -1L, 0, 1,
  NULL, NULL, NULL, NULL, NULL,
  {{MULTIFIELD_TYPE}, 1, 0UL, NULL, {{{NULL}}}}
  };

  CL_AllocateEnvironmentData (theEnv, FACTS_DATA, sizeof (struct factsData),
			      DeallocateFactData);

  memcpy (&FactData (theEnv)->FactInfo, &factInfo,
	  sizeof (struct patternEntityRecord));
  dummyFact.patternHeader.theInfo = &FactData (theEnv)->FactInfo;
  memcpy (&FactData (theEnv)->DummyFact, &dummyFact, sizeof (struct fact));
  FactData (theEnv)->LastModuleIndex = -1;

   /*=========================================*/
  /* Initialize the fact hash table (used to */
  /* quickly dete_rmine if a fact exists).    */
   /*=========================================*/

  CL_InitializeFactHashTable (theEnv);

   /*============================================*/
  /* Initialize the fact callback functions for */
  /* use with the reset and clear commands.     */
   /*============================================*/

  CL_Add_ResetFunction (theEnv, "facts", CL_Reset_Facts, 60, NULL);
  CL_Add_ClearReadyFunction (theEnv, "facts", CL_Clear_FactsReady, 0, NULL);

   /*=============================*/
  /* Initialize periodic garbage */
  /* collection for facts.       */
   /*=============================*/

  CL_AddCleanupFunction (theEnv, "facts", RemoveGarbage_Facts, 0, NULL);

   /*===================================*/
  /* Initialize fact pattern matching. */
   /*===================================*/

  CL_InitializeFactPatterns (theEnv);

   /*==================================*/
  /* Initialize the facts keyword for */
  /* use with the watch command.      */
   /*==================================*/

#if DEBUGGING_FUNCTIONS
  CL_Add_WatchItem (theEnv, "facts", 0, &FactData (theEnv)->CL_Watch_Facts,
		    80, CL_Deftemplate_WatchAccess,
		    CL_Deftemplate_WatchPrint);
#endif

   /*=========================================*/
  /* Initialize fact commands and functions. */
   /*=========================================*/

  CL_FactCommandDefinitions (theEnv);
  CL_FactFunctionDefinitions (theEnv);

   /*==============================*/
  /* Initialize fact set queries. */
   /*==============================*/

#if FACT_SET_QUERIES
  CL_SetupFactQuery (theEnv);
#endif

   /*==================================*/
  /* Initialize fact patterns for use */
  /* with the bload/bsave commands.   */
   /*==================================*/

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
  CL_FactBinarySetup (theEnv);
#endif

   /*===================================*/
  /* Initialize fact patterns for use  */
  /* with the constructs-to-c command. */
   /*===================================*/

#if CONSTRUCT_COMPILER && (! RUN_TIME)
  CL_FactPatternsCompilerSetup (theEnv);
#endif
}

/***********************************/
/* DeallocateFactData: Deallocates */
/*   environment data for facts.   */
/***********************************/
static void
DeallocateFactData (Environment * theEnv)
{
  struct factHashEntry *tmpFHEPtr, *nextFHEPtr;
  Fact *tmpFactPtr, *nextFactPtr;
  unsigned long i;
  struct patternMatch *theMatch, *tmpMatch;

  for (i = 0; i < FactData (theEnv)->FactHashTableSize; i++)
    {
      tmpFHEPtr = FactData (theEnv)->FactHashTable[i];

      while (tmpFHEPtr != NULL)
	{
	  nextFHEPtr = tmpFHEPtr->next;
	  rtn_struct (theEnv, factHashEntry, tmpFHEPtr);
	  tmpFHEPtr = nextFHEPtr;
	}
    }

  CL_rm (theEnv, FactData (theEnv)->FactHashTable,
	 sizeof (struct factHashEntry *) *
	 FactData (theEnv)->FactHashTableSize);

  tmpFactPtr = FactData (theEnv)->FactList;
  while (tmpFactPtr != NULL)
    {
      nextFactPtr = tmpFactPtr->nextFact;

      theMatch = (struct patternMatch *) tmpFactPtr->list;
      while (theMatch != NULL)
	{
	  tmpMatch = theMatch->next;
	  rtn_struct (theEnv, patternMatch, theMatch);
	  theMatch = tmpMatch;
	}

      ReturnEntity_Dependencies (theEnv, (struct patternEntity *) tmpFactPtr);

      CL_ReturnFact (theEnv, tmpFactPtr);
      tmpFactPtr = nextFactPtr;
    }

  tmpFactPtr = FactData (theEnv)->Garbage_Facts;
  while (tmpFactPtr != NULL)
    {
      nextFactPtr = tmpFactPtr->nextFact;
      CL_ReturnFact (theEnv, tmpFactPtr);
      tmpFactPtr = nextFactPtr;
    }

  CL_DeallocateCallListWithArg (theEnv,
				FactData (theEnv)->ListOf_AssertFunctions);
  CL_DeallocateCallListWithArg (theEnv,
				FactData (theEnv)->ListOf_RetractFunctions);
  CL_DeallocateModifyCallList (theEnv,
			       FactData (theEnv)->ListOfModifyFunctions);
}

/**********************************************/
/* CL_PrintFactWithIdentifier: Displays a single */
/*   fact preceded by its fact identifier.    */
/**********************************************/
void
CL_PrintFactWithIdentifier (Environment * theEnv,
			    const char *logicalName,
			    Fact * factPtr, const char *changeMap)
{
  char printSpace[20];

  CL_gensprintf (printSpace, "f-%-5lld ", factPtr->factIndex);
  CL_WriteString (theEnv, logicalName, printSpace);
  CL_PrintFact (theEnv, logicalName, factPtr, false, false, changeMap);
}

/****************************************************/
/* CL_PrintFactIdentifier: Displays a fact identifier. */
/****************************************************/
void
CL_PrintFactIdentifier (Environment * theEnv,
			const char *logicalName, Fact * factPtr)
{
  char printSpace[20];

  CL_gensprintf (printSpace, "f-%lld", factPtr->factIndex);
  CL_WriteString (theEnv, logicalName, printSpace);
}

/********************************************/
/* CL_PrintFactIdentifierInLongFo_rm: Display a */
/*   fact identifier in a longer fo_rmat.    */
/********************************************/
void
CL_PrintFactIdentifierInLongFo_rm (Environment * theEnv,
				   const char *logicalName, Fact * factPtr)
{
  if (PrintUtilityData (theEnv)->AddressesToStrings)
    CL_WriteString (theEnv, logicalName, "\"");
  if (factPtr != &FactData (theEnv)->DummyFact)
    {
      CL_WriteString (theEnv, logicalName, "<Fact-");
      CL_WriteInteger (theEnv, logicalName, factPtr->factIndex);
      CL_WriteString (theEnv, logicalName, ">");
    }
  else
    {
      CL_WriteString (theEnv, logicalName, "<Dummy Fact>");
    }

  if (PrintUtilityData (theEnv)->AddressesToStrings)
    CL_WriteString (theEnv, logicalName, "\"");
}

/*******************************************/
/* CL_DecrementFactBasisCount: Decrements the */
/*   partial match busy count of a fact    */
/*******************************************/
void
CL_DecrementFactBasisCount (Environment * theEnv, Fact * factPtr)
{
  Multifield *theSegment;
  size_t i;

  CL_ReleaseFact (factPtr);

  if (factPtr->basisSlots != NULL)
    {
      theSegment = factPtr->basisSlots;
      factPtr->basisSlots->busyCount--;
    }
  else
    {
      theSegment = &factPtr->theProposition;
    }

  for (i = 0; i < theSegment->length; i++)
    {
      CL_AtomDeinstall (theEnv, theSegment->contents[i].header->type,
			theSegment->contents[i].value);
    }

  if ((factPtr->basisSlots != NULL) && (factPtr->basisSlots->busyCount == 0))
    {
      CL_ReturnMultifield (theEnv, factPtr->basisSlots);
      factPtr->basisSlots = NULL;
    }
}

/*******************************************/
/* CL_IncrementFactBasisCount: Increments the */
/*   partial match busy count of a fact.   */
/*******************************************/
void
CL_IncrementFactBasisCount (Environment * theEnv, Fact * factPtr)
{
  Multifield *theSegment;
  size_t i;

  CL_RetainFact (factPtr);

  theSegment = &factPtr->theProposition;

  if (theSegment->length != 0)
    {
      if (factPtr->basisSlots != NULL)
	{
	  factPtr->basisSlots->busyCount++;
	}
      else
	{
	  factPtr->basisSlots = CL_CopyMultifield (theEnv, theSegment);
	  factPtr->basisSlots->busyCount = 1;
	}
      theSegment = factPtr->basisSlots;
    }

  for (i = 0; i < theSegment->length; i++)
    {
      CL_AtomInstall (theEnv, theSegment->contents[i].header->type,
		      theSegment->contents[i].value);
    }
}

/******************/
/* CL_FactIsDeleted: */
/******************/
bool
CL_FactIsDeleted (Environment * theEnv, Fact * theFact)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif

  return theFact->garbage;
}

/**************************************************/
/* CL_PrintFact: Displays the printed representation */
/*   of a fact containing the relation name and   */
/*   all of the fact's slots or fields.           */
/**************************************************/
void
CL_PrintFact (Environment * theEnv,
	      const char *logicalName,
	      Fact * factPtr,
	      bool separateLines, bool ignoreDefaults, const char *changeMap)
{
  Multifield *theMultifield;

   /*=========================================*/
  /* Print a deftemplate (non-ordered) fact. */
   /*=========================================*/

  if (factPtr->whichDeftemplate->implied == false)
    {
      CL_PrintTemplateFact (theEnv, logicalName, factPtr, separateLines,
			    ignoreDefaults, changeMap);
      return;
    }

   /*==============================*/
  /* Print an ordered fact (which */
  /* has an implied deftemplate). */
   /*==============================*/

  CL_WriteString (theEnv, logicalName, "(");

  CL_WriteString (theEnv, logicalName,
		  factPtr->whichDeftemplate->header.name->contents);

  theMultifield = factPtr->theProposition.contents[0].multifieldValue;
  if (theMultifield->length != 0)
    {
      CL_WriteString (theEnv, logicalName, " ");
      CL_PrintMultifieldDriver (theEnv, logicalName, theMultifield, 0,
				theMultifield->length, false);
    }

  CL_WriteString (theEnv, logicalName, ")");
}

/*********************************************/
/* CL_MatchFactFunction: Filters a fact through */
/*   the appropriate fact pattern network.   */
/*********************************************/
void
CL_MatchFactFunction (Environment * theEnv, Fact * theFact)
{
  CL_FactPatternMatch (theEnv, theFact,
		       theFact->whichDeftemplate->patternNetwork, 0, 0, NULL,
		       NULL);
}

/**********************************************/
/* CL_RetractDriver: Driver routine for CL_Retract. */
/**********************************************/
CL_RetractError
CL_RetractDriver (Environment * theEnv,
		  Fact * theFact, bool modifyOperation, char *changeMap)
{
  Deftemplate *theTemplate = theFact->whichDeftemplate;
  struct callFunctionItemWithArg *the_RetractFunction;

  FactData (theEnv)->retractError = RE_NO_ERROR;

   /*===========================================*/
  /* CL_Retracting a retracted fact does nothing. */
   /*===========================================*/

  if (theFact->garbage)
    {
      return RE_NO_ERROR;
    }

   /*===========================================*/
  /* A fact can not be retracted while another */
  /* fact is being asserted or retracted.      */
   /*===========================================*/

  if (EngineData (theEnv)->JoinOperationInProgress)
    {
      CL_PrintErrorID (theEnv, "FACTMNGR", 1, true);
      CL_WriteString (theEnv, STDERR,
		      "CL_Facts may not be retracted during pattern-matching.\n");
      Set_EvaluationError (theEnv, true);
      FactData (theEnv)->retractError = RE_COULD_NOT_RETRACT_ERROR;
      return RE_COULD_NOT_RETRACT_ERROR;
    }

   /*====================================*/
  /* A NULL fact pointer indicates that */
  /* all facts should be retracted.     */
   /*====================================*/

  if (theFact == NULL)
    {
      return CL_RetractAll_Facts (theEnv);
    }

   /*=================================================*/
  /* Check to see if the fact has not been asserted. */
   /*=================================================*/

  if (theFact->factIndex == 0)
    {
      CL_SystemError (theEnv, "FACTMNGR", 5);
      CL_ExitRouter (theEnv, EXIT_FAILURE);
    }

   /*===========================================*/
  /* Execute the list of functions that are    */
  /* to be called before each fact retraction. */
   /*===========================================*/

  for (the_RetractFunction = FactData (theEnv)->ListOf_RetractFunctions;
       the_RetractFunction != NULL;
       the_RetractFunction = the_RetractFunction->next)
    {
      (*the_RetractFunction->func) (theEnv, theFact,
				    the_RetractFunction->context);
    }

   /*============================*/
  /* Print retraction output if */
  /* facts are being watched.   */
   /*============================*/

#if DEBUGGING_FUNCTIONS
  if (theFact->whichDeftemplate->watch &&
      (!ConstructData (theEnv)->CL_ClearReadyInProgress) &&
      (!ConstructData (theEnv)->CL_ClearInProgress))
    {
      CL_WriteString (theEnv, STDOUT, "<== ");
      CL_PrintFactWithIdentifier (theEnv, STDOUT, theFact, changeMap);
      CL_WriteString (theEnv, STDOUT, "\n");
    }
#endif

   /*==================================*/
  /* Set the change flag to indicate  */
  /* the fact-list has been modified. */
   /*==================================*/

  FactData (theEnv)->ChangeToFactList = true;

   /*===============================================*/
  /* Remove any links between the fact and partial */
  /* matches in the join network. These links are  */
  /* used to keep track of logical dependencies.   */
   /*===============================================*/

  RemoveEntity_Dependencies (theEnv, (struct patternEntity *) theFact);

   /*===========================================*/
  /* Remove the fact from the fact hash table. */
   /*===========================================*/

  CL_RemoveHashedFact (theEnv, theFact);

   /*=========================================*/
  /* Remove the fact from its template list. */
   /*=========================================*/

  if (theFact == theTemplate->lastFact)
    {
      theTemplate->lastFact = theFact->previousTemplateFact;
    }

  if (theFact->previousTemplateFact == NULL)
    {
      theTemplate->factList = theTemplate->factList->nextTemplateFact;
      if (theTemplate->factList != NULL)
	{
	  theTemplate->factList->previousTemplateFact = NULL;
	}
    }
  else
    {
      theFact->previousTemplateFact->nextTemplateFact =
	theFact->nextTemplateFact;
      if (theFact->nextTemplateFact != NULL)
	{
	  theFact->nextTemplateFact->previousTemplateFact =
	    theFact->previousTemplateFact;
	}
    }

   /*=====================================*/
  /* Remove the fact from the fact list. */
   /*=====================================*/

  if (theFact == FactData (theEnv)->LastFact)
    {
      FactData (theEnv)->LastFact = theFact->previousFact;
    }

  if (theFact->previousFact == NULL)
    {
      FactData (theEnv)->FactList = FactData (theEnv)->FactList->nextFact;
      if (FactData (theEnv)->FactList != NULL)
	{
	  FactData (theEnv)->FactList->previousFact = NULL;
	}
    }
  else
    {
      theFact->previousFact->nextFact = theFact->nextFact;
      if (theFact->nextFact != NULL)
	{
	  theFact->nextFact->previousFact = theFact->previousFact;
	}
    }

   /*===================================================*/
  /* Add the fact to the fact garbage list unless this */
  /* fact is being retract as part of a modify action. */
   /*===================================================*/

  if (!modifyOperation)
    {
      theFact->nextFact = FactData (theEnv)->Garbage_Facts;
      FactData (theEnv)->Garbage_Facts = theFact;
      UtilityData (theEnv)->CurrentGarbageFrame->dirty = true;
    }
  else
    {
      theFact->nextFact = NULL;
    }
  theFact->garbage = true;

   /*===================================================*/
  /* CL_Reset the evaluation error flag since expressions */
  /* will be evaluated as part of the retract.         */
   /*===================================================*/

  Set_EvaluationError (theEnv, false);

   /*===========================================*/
  /* Loop through the list of all the patterns */
  /* that matched the fact and process the     */
  /* retract operation for each one.           */
   /*===========================================*/

  EngineData (theEnv)->JoinOperationInProgress = true;
  CL_Network_Retract (theEnv, (struct patternMatch *) theFact->list);
  theFact->list = NULL;
  EngineData (theEnv)->JoinOperationInProgress = false;

   /*=========================================*/
  /* Free partial matches that were released */
  /* by the retraction of the fact.          */
   /*=========================================*/

  if (EngineData (theEnv)->ExecutingRule == NULL)
    {
      CL_FlushGarbagePartial_Matches (theEnv);
    }

   /*=========================================*/
  /* CL_Retract other facts that were logically */
  /* dependent on the fact just retracted.   */
   /*=========================================*/

  CL_ForceLogical_Retractions (theEnv);

   /*==================================*/
  /* Update busy counts and ephemeral */
  /* garbage info_rmation.             */
   /*==================================*/

  CL_FactDeinstall (theEnv, theFact);

   /*====================================*/
  /* Return the appropriate error code. */
   /*====================================*/

  if (Get_EvaluationError (theEnv))
    {
      FactData (theEnv)->retractError = RE_RULE_NETWORK_ERROR;
      return RE_RULE_NETWORK_ERROR;
    }

  FactData (theEnv)->retractError = RE_NO_ERROR;
  return RE_NO_ERROR;
}

/*******************/
/* CL_RetractCallback */
/*******************/
static bool
CL_RetractCallback (Fact * theFact, Environment * theEnv)
{
  return (CL_RetractDriver (theEnv, theFact, false, NULL) == RE_NO_ERROR);
}

/******************************************************/
/* CL_Retract: C access routine for the retract command. */
/******************************************************/
CL_RetractError
CL_Retract (Fact * theFact)
{
  GCBlock gcb;
  CL_RetractError rv;
  Environment *theEnv;

  if (theFact == NULL)
    {
      return RE_NULL_POINTER_ERROR;
    }

  if (theFact->garbage)
    {
      return RE_NO_ERROR;
    }

  theEnv = theFact->whichDeftemplate->header.env;

   /*=====================================*/
  /* If embedded, clear the error flags. */
   /*=====================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_ResetErrorFlags (theEnv);
    }

  CL_GCBlockStart (theEnv, &gcb);
  rv = CL_RetractDriver (theEnv, theFact, false, NULL);
  CL_GCBlockEnd (theEnv, &gcb);

  return rv;
}

/*******************************************************************/
/* RemoveGarbage_Facts: Returns facts that have been retracted to   */
/*   the pool of available memory. It is necessary to postpone     */
/*   returning the facts to memory because RHS actions retrieve    */
/*   their variable bindings directly from the fact data structure */
/*   and the facts may be in use in other data structures.         */
/*******************************************************************/
static void
RemoveGarbage_Facts (Environment * theEnv, void *context)
{
  Fact *factPtr, *nextPtr, *lastPtr = NULL;

  factPtr = FactData (theEnv)->Garbage_Facts;

  while (factPtr != NULL)
    {
      nextPtr = factPtr->nextFact;

      if (factPtr->patternHeader.busyCount == 0)
	{
	  Multifield *theSegment;
	  size_t i;

	  theSegment = &factPtr->theProposition;
	  for (i = 0; i < theSegment->length; i++)
	    {
	      CL_AtomDeinstall (theEnv, theSegment->contents[i].header->type,
				theSegment->contents[i].value);
	    }

	  CL_ReturnFact (theEnv, factPtr);
	  if (lastPtr == NULL)
	    FactData (theEnv)->Garbage_Facts = nextPtr;
	  else
	    lastPtr->nextFact = nextPtr;
	}
      else
	{
	  lastPtr = factPtr;
	}

      factPtr = nextPtr;
    }
}

/********************************************************/
/* CL_AssertDriver: Driver routine for the assert command. */
/********************************************************/
Fact *
CL_AssertDriver (Fact * theFact,
		 long long reuseIndex,
		 Fact * factListPosition,
		 Fact * templatePosition, char *changeMap)
{
  size_t hashValue;
  size_t length, i;
  CLIPSValue *theField;
  Fact *duplicate;
  struct callFunctionItemWithArg *the_AssertFunction;
  Environment *theEnv = theFact->whichDeftemplate->header.env;

  FactData (theEnv)->assertError = AE_NO_ERROR;

   /*==================================================*/
  /* CL_Retracted and existing facts cannot be asserted. */
   /*==================================================*/

  if (theFact->garbage)
    {
      FactData (theEnv)->assertError = AE_RETRACTED_ERROR;
      return NULL;
    }

  if (reuseIndex != theFact->factIndex)
    {
      CL_SystemError (theEnv, "FACTMNGR", 6);
      CL_ExitRouter (theEnv, EXIT_FAILURE);
    }

   /*==========================================*/
  /* A fact can not be asserted while another */
  /* fact is being asserted or retracted.     */
   /*==========================================*/

  if (EngineData (theEnv)->JoinOperationInProgress)
    {
      FactData (theEnv)->assertError = AE_COULD_NOT_ASSERT_ERROR;
      CL_ReturnFact (theEnv, theFact);
      CL_PrintErrorID (theEnv, "FACTMNGR", 2, true);
      CL_WriteString (theEnv, STDERR,
		      "CL_Facts may not be asserted during pattern-matching.\n");
      return NULL;
    }

   /*=============================================================*/
  /* Replace invalid data types in the fact with the symbol nil. */
   /*=============================================================*/

  length = theFact->theProposition.length;
  theField = theFact->theProposition.contents;

  for (i = 0; i < length; i++)
    {
      if (theField[i].value == VoidConstant (theEnv))
	{
	  theField[i].value = CL_CreateSymbol (theEnv, "nil");
	}
    }

   /*========================================================*/
  /* If fact assertions are being checked for duplications, */
  /* then search the fact list for a duplicate fact.        */
   /*========================================================*/

  hashValue =
    CL_HandleFactDuplication (theEnv, theFact, &duplicate, reuseIndex);
  if (duplicate != NULL)
    return duplicate;

   /*==========================================================*/
  /* If necessary, add logical dependency links between the   */
  /* fact and the partial match which is its logical support. */
   /*==========================================================*/

  if (CL_AddLogical_Dependencies
      (theEnv, (struct patternEntity *) theFact, false) == false)
    {
      if (reuseIndex == 0)
	{
	  CL_ReturnFact (theEnv, theFact);
	}
      else
	{
	  theFact->nextFact = FactData (theEnv)->Garbage_Facts;
	  FactData (theEnv)->Garbage_Facts = theFact;
	  UtilityData (theEnv)->CurrentGarbageFrame->dirty = true;
	  theFact->garbage = true;
	}

      FactData (theEnv)->assertError = AE_COULD_NOT_ASSERT_ERROR;
      return NULL;
    }

   /*======================================*/
  /* Add the fact to the fact hash table. */
   /*======================================*/

  CL_AddHashedFact (theEnv, theFact, hashValue);

   /*================================*/
  /* Add the fact to the fact list. */
   /*================================*/

  if (reuseIndex == 0)
    {
      factListPosition = FactData (theEnv)->LastFact;
    }

  if (factListPosition == NULL)
    {
      theFact->nextFact = FactData (theEnv)->FactList;
      FactData (theEnv)->FactList = theFact;
      theFact->previousFact = NULL;
      if (theFact->nextFact != NULL)
	{
	  theFact->nextFact->previousFact = theFact;
	}
    }
  else
    {
      theFact->nextFact = factListPosition->nextFact;
      theFact->previousFact = factListPosition;
      factListPosition->nextFact = theFact;
      if (theFact->nextFact != NULL)
	{
	  theFact->nextFact->previousFact = theFact;
	}
    }

  if ((FactData (theEnv)->LastFact == NULL) || (theFact->nextFact == NULL))
    {
      FactData (theEnv)->LastFact = theFact;
    }

   /*====================================*/
  /* Add the fact to its template list. */
   /*====================================*/

  if (reuseIndex == 0)
    {
      templatePosition = theFact->whichDeftemplate->lastFact;
    }

  if (templatePosition == NULL)
    {
      theFact->nextTemplateFact = theFact->whichDeftemplate->factList;
      theFact->whichDeftemplate->factList = theFact;
      theFact->previousTemplateFact = NULL;
      if (theFact->nextTemplateFact != NULL)
	{
	  theFact->nextTemplateFact->previousTemplateFact = theFact;
	}
    }
  else
    {
      theFact->nextTemplateFact = templatePosition->nextTemplateFact;
      theFact->previousTemplateFact = templatePosition;
      templatePosition->nextTemplateFact = theFact;
      if (theFact->nextTemplateFact != NULL)
	{
	  theFact->nextTemplateFact->previousTemplateFact = theFact;
	}
    }

  if ((theFact->whichDeftemplate->lastFact == NULL)
      || (theFact->nextTemplateFact == NULL))
    {
      theFact->whichDeftemplate->lastFact = theFact;
    }

   /*==================================*/
  /* Set the fact index and time tag. */
   /*==================================*/

  if (reuseIndex > 0)
    {
      theFact->factIndex = reuseIndex;
    }
  else
    {
      theFact->factIndex = FactData (theEnv)->Next_FactIndex++;
    }

  theFact->patternHeader.timeTag =
    DefruleData (theEnv)->CurrentEntityTimeTag++;

   /*=====================*/
  /* Update busy counts. */
   /*=====================*/

  CL_FactInstall (theEnv, theFact);

  if (reuseIndex == 0)
    {
      Multifield *theSegment = &theFact->theProposition;
      for (i = 0; i < theSegment->length; i++)
	{
	  CL_AtomInstall (theEnv, theSegment->contents[i].header->type,
			  theSegment->contents[i].value);
	}
    }

   /*==========================================*/
  /* Execute the list of functions that are   */
  /* to be called before each fact assertion. */
   /*==========================================*/

  for (the_AssertFunction = FactData (theEnv)->ListOf_AssertFunctions;
       the_AssertFunction != NULL;
       the_AssertFunction = the_AssertFunction->next)
    {
      (*the_AssertFunction->func) (theEnv, theFact,
				   the_AssertFunction->context);
    }

   /*==========================*/
  /* Print assert output if   */
  /* facts are being watched. */
   /*==========================*/

#if DEBUGGING_FUNCTIONS
  if (theFact->whichDeftemplate->watch &&
      (!ConstructData (theEnv)->CL_ClearReadyInProgress) &&
      (!ConstructData (theEnv)->CL_ClearInProgress))
    {
      CL_WriteString (theEnv, STDOUT, "==> ");
      CL_PrintFactWithIdentifier (theEnv, STDOUT, theFact, changeMap);
      CL_WriteString (theEnv, STDOUT, "\n");
    }
#endif

   /*==================================*/
  /* Set the change flag to indicate  */
  /* the fact-list has been modified. */
   /*==================================*/

  FactData (theEnv)->ChangeToFactList = true;

   /*==========================================*/
  /* Check for constraint errors in the fact. */
   /*==========================================*/

  CL_CheckTemplateFact (theEnv, theFact);

   /*===================================================*/
  /* CL_Reset the evaluation error flag since expressions */
  /* will be evaluated as part of the assert .         */
   /*===================================================*/

  Set_EvaluationError (theEnv, false);

   /*=============================================*/
  /* Pattern match the fact using the associated */
  /* deftemplate's pattern network.              */
   /*=============================================*/

  EngineData (theEnv)->JoinOperationInProgress = true;
  CL_FactPatternMatch (theEnv, theFact,
		       theFact->whichDeftemplate->patternNetwork, 0, 0, NULL,
		       NULL);
  EngineData (theEnv)->JoinOperationInProgress = false;

   /*===================================================*/
  /* CL_Retract other facts that were logically dependent */
  /* on the non-existence of the fact just asserted.   */
   /*===================================================*/

  CL_ForceLogical_Retractions (theEnv);

   /*=========================================*/
  /* Free partial matches that were released */
  /* by the assertion of the fact.           */
   /*=========================================*/

  if (EngineData (theEnv)->ExecutingRule == NULL)
    CL_FlushGarbagePartial_Matches (theEnv);

   /*===============================*/
  /* Return a pointer to the fact. */
   /*===============================*/

  if (CL_EvaluationData (theEnv)->CL_EvaluationError)
    {
      FactData (theEnv)->assertError = AE_RULE_NETWORK_ERROR;
    }

  return theFact;
}

/*****************************************************/
/* CL_Assert: C access routine for the assert function. */
/*****************************************************/
Fact *
CL_Assert (Fact * theFact)
{
  return CL_AssertDriver (theFact, 0, NULL, NULL, NULL);
}

/*************************/
/* Get_AssertStringError: */
/*************************/
CL_AssertStringError
Get_AssertStringError (Environment * theEnv)
{
  return FactData (theEnv)->assertStringError;
}

/**************************************/
/* CL_RetractAll_Facts: Loops through the */
/*   fact-list and removes each fact. */
/**************************************/
CL_RetractError
CL_RetractAll_Facts (Environment * theEnv)
{
  CL_RetractError rv;

  while (FactData (theEnv)->FactList != NULL)
    {
      if ((rv = CL_Retract (FactData (theEnv)->FactList)) != RE_NO_ERROR)
	{
	  return rv;
	}
    }

  return RE_NO_ERROR;
}

/*********************************************/
/* CL_CreateFact: Creates a fact data structure */
/*   of the specified deftemplate.           */
/*********************************************/
Fact *
CL_CreateFact (Deftemplate * theDeftemplate)
{
  Fact *newFact;
  unsigned short i;
  Environment *theEnv = theDeftemplate->header.env;

   /*=================================*/
  /* A deftemplate must be specified */
  /* in order to create a fact.      */
   /*=================================*/

  if (theDeftemplate == NULL)
    return NULL;

   /*============================================*/
  /* Create a fact for an explicit deftemplate. */
   /*============================================*/

  if (theDeftemplate->implied == false)
    {
      newFact = CL_CreateFactBySize (theEnv, theDeftemplate->numberOfSlots);
      for (i = 0; i < theDeftemplate->numberOfSlots; i++)
	{
	  newFact->theProposition.contents[i].voidValue =
	    VoidConstant (theEnv);
	}
    }

   /*===========================================*/
  /* Create a fact for an implied deftemplate. */
   /*===========================================*/

  else
    {
      newFact = CL_CreateFactBySize (theEnv, 1);
      newFact->theProposition.contents[0].value =
	CL_CreateUnmanagedMultifield (theEnv, 0L);
    }

   /*===============================*/
  /* Return a pointer to the fact. */
   /*===============================*/

  newFact->whichDeftemplate = theDeftemplate;

  return newFact;
}

/****************************************/
/* CL_GetFactSlot: Returns the slot value  */
/*   from the specified slot of a fact. */
/****************************************/
GetSlotError
CL_GetFactSlot (Fact * theFact, const char *slotName, CLIPSValue * theValue)
{
  Deftemplate *theDeftemplate;
  unsigned short whichSlot;
  Environment *theEnv = theFact->whichDeftemplate->header.env;

  if (theFact == NULL)
    {
      return GSE_NULL_POINTER_ERROR;
    }

  if (theFact->garbage)
    {
      theValue->lexemeValue = FalseSymbol (theEnv);
      return GSE_INVALID_TARGET_ERROR;
    }

   /*===============================================*/
  /* Get the deftemplate associated with the fact. */
   /*===============================================*/

  theDeftemplate = theFact->whichDeftemplate;

   /*==============================================*/
  /* Handle retrieving the slot value from a fact */
  /* having an implied deftemplate. An implied    */
  /* facts has a single multifield slot.          */
   /*==============================================*/

  if (theDeftemplate->implied)
    {
      if (slotName != NULL)
	{
	  if (strcmp (slotName, "implied") != 0)
	    {
	      return GSE_SLOT_NOT_FOUND_ERROR;
	    }
	}

      theValue->value = theFact->theProposition.contents[0].value;
      return GSE_NO_ERROR;
    }

   /*===================================*/
  /* Make sure the slot name requested */
  /* corresponds to a valid slot name. */
   /*===================================*/

  if (slotName == NULL)
    return GSE_NULL_POINTER_ERROR;
  if (CL_FindSlot
      (theDeftemplate, CL_CreateSymbol (theEnv, slotName),
       &whichSlot) == NULL)
    {
      return GSE_SLOT_NOT_FOUND_ERROR;
    }

   /*========================*/
  /* Return the slot value. */
   /*========================*/

  theValue->value = theFact->theProposition.contents[whichSlot].value;

  return GSE_NO_ERROR;
}

/**************************************/
/* CL_PutFactSlot: Sets the slot value   */
/*   of the specified slot of a fact. */
/**************************************/
bool
CL_PutFactSlot (Fact * theFact, const char *slotName, CLIPSValue * theValue)
{
  Deftemplate *theDeftemplate;
  struct templateSlot *theSlot;
  unsigned short whichSlot;
  Environment *theEnv = theFact->whichDeftemplate->header.env;

   /*========================================*/
  /* This function cannot be used on a fact */
  /* that's already been asserted.          */
   /*========================================*/

  if (theFact->factIndex != 0LL)
    {
      return false;
    }

   /*===============================================*/
  /* Get the deftemplate associated with the fact. */
   /*===============================================*/

  theDeftemplate = theFact->whichDeftemplate;

   /*============================================*/
  /* Handle setting the slot value of a fact    */
  /* having an implied deftemplate. An implied  */
  /* facts has a single multifield slot.        */
   /*============================================*/

  if (theDeftemplate->implied)
    {
      if ((slotName != NULL) || (theValue->header->type != MULTIFIELD_TYPE))
	{
	  return false;
	}

      if (theFact->theProposition.contents[0].header->type == MULTIFIELD_TYPE)
	{
	  CL_ReturnMultifield (theEnv,
			       theFact->theProposition.
			       contents[0].multifieldValue);
	}

      theFact->theProposition.contents[0].value =
	CL_CopyMultifield (theEnv, theValue->multifieldValue);

      return true;
    }

   /*===================================*/
  /* Make sure the slot name requested */
  /* corresponds to a valid slot name. */
   /*===================================*/

  if ((theSlot =
       CL_FindSlot (theDeftemplate, CL_CreateSymbol (theEnv, slotName),
		    &whichSlot)) == NULL)
    {
      return false;
    }

   /*=============================================*/
  /* Make sure a single field value is not being */
  /* stored in a multifield slot or vice versa.  */
   /*=============================================*/

  if (((theSlot->multislot == 0)
       && (theValue->header->type == MULTIFIELD_TYPE))
      || ((theSlot->multislot == 1)
	  && (theValue->header->type != MULTIFIELD_TYPE)))
    {
      return false;
    }

   /*=================================*/
  /* Check constraints for the slot. */
   /*=================================*/

  if (theSlot->constraints != NULL)
    {
      if (CL_ConstraintCheckValue
	  (theEnv, theValue->header->type, theValue->value,
	   theSlot->constraints) != NO_VIOLATION)
	{
	  return false;
	}
    }

   /*=====================*/
  /* Set the slot value. */
   /*=====================*/

  if (theFact->theProposition.contents[whichSlot].header->type ==
      MULTIFIELD_TYPE)
    {
      CL_ReturnMultifield (theEnv,
			   theFact->theProposition.
			   contents[whichSlot].multifieldValue);
    }

  if (theValue->header->type == MULTIFIELD_TYPE)
    {
      theFact->theProposition.contents[whichSlot].multifieldValue =
	CL_CopyMultifield (theEnv, theValue->multifieldValue);
    }
  else
    {
      theFact->theProposition.contents[whichSlot].value = theValue->value;
    }

  return true;
}

/*******************************************************/
/* CL_AssignFactSlotDefaults: Sets a fact's slot values   */
/*   to its default value if the value of the slot has */
/*   not yet been set.                                 */
/*******************************************************/
bool
CL_AssignFactSlotDefaults (Fact * theFact)
{
  Deftemplate *theDeftemplate;
  struct templateSlot *slotPtr;
  unsigned short i;
  UDFValue theResult;
  Environment *theEnv = theFact->whichDeftemplate->header.env;

   /*========================================*/
  /* This function cannot be used on a fact */
  /* that's already been asserted.          */
   /*========================================*/

  if (theFact->factIndex != 0LL)
    {
      return false;
    }

   /*===============================================*/
  /* Get the deftemplate associated with the fact. */
   /*===============================================*/

  theDeftemplate = theFact->whichDeftemplate;

   /*================================================*/
  /* The value for the implied multifield slot of   */
  /* an implied deftemplate is set to a multifield  */
  /* of length zero when the fact is created.       */
   /*================================================*/

  if (theDeftemplate->implied)
    return true;

   /*============================================*/
  /* Loop through each slot of the deftemplate. */
   /*============================================*/

  for (i = 0, slotPtr = theDeftemplate->slotList;
       i < theDeftemplate->numberOfSlots; i++, slotPtr = slotPtr->next)
    {
      /*===================================*/
      /* If the slot's value has been set, */
      /* then move on to the next slot.    */
      /*===================================*/

      if (theFact->theProposition.contents[i].value != VoidConstant (theEnv))
	continue;

      /*======================================================*/
      /* Assign the default value for the slot if one exists. */
      /*======================================================*/

      if (CL_DeftemplateSlotDefault
	  (theEnv, theDeftemplate, slotPtr, &theResult, false))
	{
	  theFact->theProposition.contents[i].value = theResult.value;
	}
    }

   /*==========================================*/
  /* Return true to indicate that the default */
  /* values have been successfully set.       */
   /*==========================================*/

  return true;
}

/********************************************************/
/* CL_DeftemplateSlotDefault: Dete_rmines the default value */
/*   for the specified slot of a deftemplate.           */
/********************************************************/
bool
CL_DeftemplateSlotDefault (Environment * theEnv,
			   Deftemplate * theDeftemplate,
			   struct templateSlot *slotPtr,
			   UDFValue * theResult, bool garbageMultifield)
{
   /*================================================*/
  /* The value for the implied multifield slot of an */
  /* implied deftemplate does not have a default.    */
   /*=================================================*/

  if (theDeftemplate->implied)
    return false;

   /*===============================================*/
  /* If the (default ?NONE) attribute was declared */
  /* for the slot, then return false to indicate   */
  /* the default values for the fact couldn't be   */
  /* supplied since this attribute requires that a */
  /* default value can't be used for the slot.     */
   /*===============================================*/

  if (slotPtr->noDefault)
    return false;

   /*==============================================*/
  /* Otherwise if a static default was specified, */
  /* use this as the default value.               */
   /*==============================================*/

  else if (slotPtr->defaultPresent)
    {
      if (slotPtr->multislot)
	{
	  CL_StoreInMultifield (theEnv, theResult, slotPtr->defaultList,
				garbageMultifield);
	}
      else
	{
	  theResult->value = slotPtr->defaultList->value;
	}
    }

   /*================================================*/
  /* Otherwise if a dynamic-default was specified,  */
  /* evaluate it and use this as the default value. */
   /*================================================*/

  else if (slotPtr->defaultDynamic)
    {
      if (!CL_EvaluateAndStoreInDataObject (theEnv, slotPtr->multislot,
					    (Expression *)
					    slotPtr->defaultList, theResult,
					    garbageMultifield))
	{
	  return false;
	}
    }

   /*====================================*/
  /* Otherwise derive the default value */
  /* from the slot's constraints.       */
   /*====================================*/

  else
    {
      CL_DeriveDefaultFromConstraints (theEnv, slotPtr->constraints,
				       theResult, slotPtr->multislot,
				       garbageMultifield);
    }

   /*==========================================*/
  /* Return true to indicate that the default */
  /* values have been successfully set.       */
   /*==========================================*/

  return true;
}

/***************************************************************/
/* CL_Copy_FactSlotValues: Copies the slot values from one fact to */
/*   another. Both facts must have the same relation name.     */
/***************************************************************/
bool
CL_Copy_FactSlotValues (Environment * theEnv,
			Fact * theDestFact, Fact * theSourceFact)
{
  Deftemplate *theDeftemplate;
  struct templateSlot *slotPtr;
  unsigned short i;

   /*===================================*/
  /* Both facts must be the same type. */
   /*===================================*/

  theDeftemplate = theSourceFact->whichDeftemplate;
  if (theDestFact->whichDeftemplate != theDeftemplate)
    {
      return false;
    }

   /*===================================================*/
  /* Loop through each slot of the deftemplate copying */
  /* the source fact value to the destination fact.    */
   /*===================================================*/

  for (i = 0, slotPtr = theDeftemplate->slotList;
       i < theDeftemplate->numberOfSlots; i++, slotPtr = slotPtr->next)
    {
      if (theSourceFact->theProposition.contents[i].header->type !=
	  MULTIFIELD_TYPE)
	{
	  theDestFact->theProposition.contents[i].value =
	    theSourceFact->theProposition.contents[i].value;
	}
      else
	{
	  theDestFact->theProposition.contents[i].value =
	    CL_CopyMultifield (theEnv,
			       theSourceFact->theProposition.
			       contents[i].multifieldValue);
	}
    }

   /*========================================*/
  /* Return true to indicate that fact slot */
  /* values were successfully copied.       */
   /*========================================*/

  return true;
}

/*********************************************/
/* CL_CreateFactBySize: Allocates a fact data   */
/*   structure based on the number of slots. */
/*********************************************/
Fact *
CL_CreateFactBySize (Environment * theEnv, size_t size)
{
  Fact *theFact;
  size_t newSize;

  if (size <= 0)
    newSize = 1;
  else
    newSize = size;

  theFact =
    get_var_struct (theEnv, fact, sizeof (struct clipsValue) * (newSize - 1));

  theFact->patternHeader.header.type = FACT_ADDRESS_TYPE;
  theFact->garbage = false;
  theFact->factIndex = 0LL;
  theFact->patternHeader.busyCount = 0;
  theFact->patternHeader.theInfo = &FactData (theEnv)->FactInfo;
  theFact->patternHeader.dependents = NULL;
  theFact->whichDeftemplate = NULL;
  theFact->nextFact = NULL;
  theFact->previousFact = NULL;
  theFact->previousTemplateFact = NULL;
  theFact->nextTemplateFact = NULL;
  theFact->list = NULL;
  theFact->basisSlots = NULL;

  theFact->theProposition.length = size;
  theFact->theProposition.busyCount = 0;

  return (theFact);
}

/*********************************************/
/* CL_ReturnFact: Returns a fact data structure */
/*   to the pool of free memory.             */
/*********************************************/
void
CL_ReturnFact (Environment * theEnv, Fact * theFact)
{
  Multifield *theSegment, *subSegment;
  size_t newSize, i;

  theSegment = &theFact->theProposition;

  for (i = 0; i < theSegment->length; i++)
    {
      if (theSegment->contents[i].header->type == MULTIFIELD_TYPE)
	{
	  subSegment = theSegment->contents[i].multifieldValue;
	  if (subSegment != NULL)
	    {
	      if (subSegment->busyCount == 0)
		{
		  CL_ReturnMultifield (theEnv, subSegment);
		}
	      else
		{
		  CL_AddToMultifieldList (theEnv, subSegment);
		}
	    }
	}
    }

  if (theFact->theProposition.length == 0)
    newSize = 1;
  else
    newSize = theFact->theProposition.length;

  rtn_var_struct (theEnv, fact, sizeof (struct clipsValue) * (newSize - 1),
		  theFact);
}

/*************************************************************/
/* CL_FactInstall: Increments the fact, deftemplate, and atomic */
/*   data value busy counts associated with the fact.        */
/*************************************************************/
void
CL_FactInstall (Environment * theEnv, Fact * newFact)
{
  FactData (theEnv)->NumberOf_Facts++;
  newFact->whichDeftemplate->busyCount++;
  newFact->patternHeader.busyCount++;
}

/***************************************************************/
/* CL_FactDeinstall: Decrements the fact, deftemplate, and atomic */
/*   data value busy counts associated with the fact.          */
/***************************************************************/
void
CL_FactDeinstall (Environment * theEnv, Fact * newFact)
{
  FactData (theEnv)->NumberOf_Facts--;
  newFact->whichDeftemplate->busyCount--;
  newFact->patternHeader.busyCount--;
}

/***********************************************/
/* CL_IncrementFactCallback: Increments the       */
/*   number of references to a specified fact. */
/***********************************************/
void
CL_IncrementFactCallback (Environment * theEnv, Fact * factPtr)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif
  if (factPtr == NULL)
    return;

  factPtr->patternHeader.busyCount++;
}

/***********************************************/
/* CL_DecrementFactCallback: Decrements the       */
/*   number of references to a specified fact. */
/***********************************************/
void
CL_DecrementFactCallback (Environment * theEnv, Fact * factPtr)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif
  if (factPtr == NULL)
    return;

  factPtr->patternHeader.busyCount--;
}

/****************************************/
/* CL_RetainFact: Increments the number of */
/*   references to a specified fact.    */
/****************************************/
void
CL_RetainFact (Fact * factPtr)
{
  if (factPtr == NULL)
    return;

  factPtr->patternHeader.busyCount++;
}

/*****************************************/
/* CL_ReleaseFact: Decrements the number of */
/*   references to a specified fact.     */
/*****************************************/
void
CL_ReleaseFact (Fact * factPtr)
{
  if (factPtr == NULL)
    return;

  factPtr->patternHeader.busyCount--;
}

/*********************************************************/
/* CL_GetNextFact: If passed a NULL pointer, returns the */
/*   first fact in the fact-list. Otherwise returns the  */
/*   next fact following the fact passed as an argument. */
/*********************************************************/
Fact *
CL_GetNextFact (Environment * theEnv, Fact * factPtr)
{
  if (factPtr == NULL)
    {
      return FactData (theEnv)->FactList;
    }

  if (factPtr->garbage)
    return NULL;

  return factPtr->nextFact;
}

/**************************************************/
/* CL_GetNextFactInScope: Returns the next fact that */
/*   is in scope of the current module. Works in  */
/*   a similar fashion to CL_GetNextFact, but skips  */
/*   facts that are out of scope.                 */
/**************************************************/
Fact *
CL_GetNextFactInScope (Environment * theEnv, Fact * theFact)
{
   /*=======================================================*/
  /* If fact passed as an argument is a NULL pointer, then */
  /* we're just beginning a traversal of the fact list. If */
  /* the module index has changed since that last time the */
  /* fact list was traversed by this routine, then         */
  /* dete_rmine all of the deftemplates that are in scope   */
  /* of the current module.                                */
   /*=======================================================*/

  if (theFact == NULL)
    {
      theFact = FactData (theEnv)->FactList;
      if (FactData (theEnv)->LastModuleIndex !=
	  DefmoduleData (theEnv)->ModuleChangeIndex)
	{
	  CL_UpdateDeftemplateScope (theEnv);
	  FactData (theEnv)->LastModuleIndex =
	    DefmoduleData (theEnv)->ModuleChangeIndex;
	}
    }

   /*==================================================*/
  /* Otherwise, if the fact passed as an argument has */
  /* been retracted, then there's no way to dete_rmine */
  /* the next fact, so return a NULL pointer.         */
   /*==================================================*/

  else if (theFact->garbage)
    {
      return NULL;
    }

   /*==================================================*/
  /* Otherwise, start the search for the next fact in */
  /* scope with the fact immediately following the    */
  /* fact passed as an argument.                      */
   /*==================================================*/

  else
    {
      theFact = theFact->nextFact;
    }

   /*================================================*/
  /* Continue traversing the fact-list until a fact */
  /* is found that's associated with a deftemplate  */
  /* that's in scope.                               */
   /*================================================*/

  while (theFact != NULL)
    {
      if (theFact->whichDeftemplate->inScope)
	return theFact;

      theFact = theFact->nextFact;
    }

  return NULL;
}

/*************************************/
/* CL_FactPPFo_rm: Returns the pretty    */
/*   print representation of a fact. */
/*************************************/
void
CL_FactPPFo_rm (Fact * theFact, String_Builder * theSB, bool ignoreDefaults)
{
  Environment *theEnv = theFact->whichDeftemplate->header.env;

  OpenString_BuilderDestination (theEnv, "CL_FactPPFo_rm", theSB);
  CL_PrintFact (theEnv, "CL_FactPPFo_rm", theFact, true, ignoreDefaults,
		NULL);
  CloseString_BuilderDestination (theEnv, "CL_FactPPFo_rm");
}

/**********************************/
/* CL_FactIndex: C access routine    */
/*   for the fact-index function. */
/**********************************/
long long
CL_FactIndex (Fact * factPtr)
{
  return factPtr->factIndex;
}

/*************************************/
/* CL_AssertString: C access routine    */
/*   for the assert-string function. */
/*************************************/
Fact *
CL_AssertString (Environment * theEnv, const char *theString)
{
  Fact *theFact, *rv;
  GCBlock gcb;
  int danglingConstructs;

  if (theString == NULL)
    {
      FactData (theEnv)->assertStringError = ASE_NULL_POINTER_ERROR;
      return NULL;
    }

  danglingConstructs = ConstructData (theEnv)->DanglingConstructs;

   /*=====================================*/
  /* If embedded, clear the error flags. */
   /*=====================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_ResetErrorFlags (theEnv);
    }

  CL_GCBlockStart (theEnv, &gcb);

  if ((theFact = CL_StringToFact (theEnv, theString)) == NULL)
    {
      FactData (theEnv)->assertStringError = ASE_PARSING_ERROR;
      CL_GCBlockEnd (theEnv, &gcb);
      return NULL;
    }

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      ConstructData (theEnv)->DanglingConstructs = danglingConstructs;
    }

  rv = CL_Assert (theFact);

  CL_GCBlockEnd (theEnv, &gcb);

  switch (FactData (theEnv)->assertError)
    {
    case AE_NO_ERROR:
      FactData (theEnv)->assertStringError = ASE_NO_ERROR;
      break;

    case AE_COULD_NOT_ASSERT_ERROR:
      FactData (theEnv)->assertStringError = ASE_COULD_NOT_ASSERT_ERROR;

    case AE_RULE_NETWORK_ERROR:
      FactData (theEnv)->assertStringError = ASE_RULE_NETWORK_ERROR;
      break;

    case AE_NULL_POINTER_ERROR:
    case AE_RETRACTED_ERROR:
      CL_SystemError (theEnv, "FACTMNGR", 4);
      CL_ExitRouter (theEnv, EXIT_FAILURE);
      break;
    }

  return rv;
}

/******************************************************/
/* CL_GetFactListChanged: Returns the flag indicating    */
/*   whether a change to the fact-list has been made. */
/******************************************************/
bool
CL_GetFactListChanged (Environment * theEnv)
{
  return (FactData (theEnv)->ChangeToFactList);
}

/********************************************************/
/* CL_SetFactListChanged: Sets the flag indicating whether */
/*   a change to the fact-list has been made.           */
/********************************************************/
void
CL_SetFactListChanged (Environment * theEnv, bool value)
{
  FactData (theEnv)->ChangeToFactList = value;
}

/****************************************/
/* GetNumberOf_Facts: Returns the number */
/* of facts in the fact-list.           */
/****************************************/
unsigned long
GetNumberOf_Facts (Environment * theEnv)
{
  return (FactData (theEnv)->NumberOf_Facts);
}

/***********************************************************/
/* CL_Reset_Facts: CL_Reset function for facts. Sets the starting */
/*   fact index to zero and removes all facts.             */
/***********************************************************/
static void
CL_Reset_Facts (Environment * theEnv, void *context)
{
   /*====================================*/
  /* Initialize the fact index to zero. */
   /*====================================*/

  FactData (theEnv)->Next_FactIndex = 1L;

   /*======================================*/
  /* Remove all facts from the fact list. */
   /*======================================*/

  CL_RetractAll_Facts (theEnv);
}

/************************************************************/
/* CL_Clear_FactsReady: CL_Clear ready function for facts. Returns */
/*   true if facts were successfully removed and the clear  */
/*   command can continue, otherwise false.                 */
/************************************************************/
static bool
CL_Clear_FactsReady (Environment * theEnv, void *context)
{
   /*======================================*/
  /* CL_Facts can not be deleted when a join */
  /* operation is already in progress.    */
   /*======================================*/

  if (EngineData (theEnv)->JoinOperationInProgress)
    return false;

   /*====================================*/
  /* Initialize the fact index to zero. */
   /*====================================*/

  FactData (theEnv)->Next_FactIndex = 1L;

   /*======================================*/
  /* Remove all facts from the fact list. */
   /*======================================*/

  CL_RetractAll_Facts (theEnv);

   /*==============================================*/
  /* If for some reason there are any facts still */
  /* re_maining, don't continue with the clear.    */
   /*==============================================*/

  if (CL_GetNextFact (theEnv, NULL) != NULL)
    return false;

   /*=============================*/
  /* Return true to indicate the */
  /* clear command can continue. */
   /*=============================*/

  return true;
}

/***************************************************/
/* CL_FindIndexedFact: Returns a pointer to a fact in */
/*   the fact list with the specified fact index.  */
/***************************************************/
Fact *
CL_FindIndexedFact (Environment * theEnv, long long factIndexSought)
{
  Fact *theFact;

  for (theFact = CL_GetNextFact (theEnv, NULL);
       theFact != NULL; theFact = CL_GetNextFact (theEnv, theFact))
    {
      if (theFact->factIndex == factIndexSought)
	{
	  return (theFact);
	}
    }

  return NULL;
}

/**************************************/
/* CL_Add_AssertFunction: Adds a function */
/*   to the ListOf_AssertFunctions.    */
/**************************************/
bool
CL_Add_AssertFunction (Environment * theEnv,
		       const char *name,
		       Void_CallFunctionWithArg * functionPtr,
		       int priority, void *context)
{
  FactData (theEnv)->ListOf_AssertFunctions =
    CL_AddFunctionToCallListWithArg (theEnv, name, priority, functionPtr,
				     FactData
				     (theEnv)->ListOf_AssertFunctions,
				     context);
  return true;
}

/********************************************/
/* Remove_AssertFunction: Removes a function */
/*   from the ListOf_AssertFunctions.        */
/********************************************/
bool
Remove_AssertFunction (Environment * theEnv, const char *name)
{
  bool found;

  FactData (theEnv)->ListOf_AssertFunctions =
    CL_RemoveFunctionFromCallListWithArg (theEnv, name,
					  FactData
					  (theEnv)->ListOf_AssertFunctions,
					  &found);

  if (found)
    return true;

  return false;
}

/***************************************/
/* CL_Add_RetractFunction: Adds a function */
/*   to the ListOf_RetractFunctions.    */
/***************************************/
bool
CL_Add_RetractFunction (Environment * theEnv,
			const char *name,
			Void_CallFunctionWithArg * functionPtr,
			int priority, void *context)
{
  FactData (theEnv)->ListOf_RetractFunctions =
    CL_AddFunctionToCallListWithArg (theEnv, name, priority, functionPtr,
				     FactData
				     (theEnv)->ListOf_RetractFunctions,
				     context);
  return true;
}

/*********************************************/
/* CL_Remove_RetractFunction: Removes a function */
/*   from the ListOf_RetractFunctions.        */
/*********************************************/
bool
CL_Remove_RetractFunction (Environment * theEnv, const char *name)
{
  bool found;

  FactData (theEnv)->ListOf_RetractFunctions =
    CL_RemoveFunctionFromCallListWithArg (theEnv, name,
					  FactData
					  (theEnv)->ListOf_RetractFunctions,
					  &found);

  if (found)
    return true;

  return false;
}

/**************************************/
/* CL_AddModifyFunction: Adds a function */
/*   to the ListOfModifyFunctions.    */
/**************************************/
bool
CL_AddModifyFunction (Environment * theEnv,
		      const char *name,
		      Modify_CallFunction * functionPtr,
		      int priority, void *context)
{
  FactData (theEnv)->ListOfModifyFunctions =
    CL_AddModifyFunctionToCallList (theEnv, name, priority, functionPtr,
				    FactData (theEnv)->ListOfModifyFunctions,
				    context);

  return true;
}

/********************************************/
/* CL_RemoveModifyFunction: Removes a function */
/*   from the ListOfModifyFunctions.        */
/********************************************/
bool
CL_RemoveModifyFunction (Environment * theEnv, const char *name)
{
  bool found;

  FactData (theEnv)->ListOfModifyFunctions =
    CL_RemoveModifyFunctionFromCallList (theEnv, name,
					 FactData
					 (theEnv)->ListOfModifyFunctions,
					 &found);

  if (found)
    return true;

  return false;
}

/**********************************************************/
/* CL_AddModifyFunctionToCallList: Adds a function to a list */
/*   of functions which are called to perfo_rm certain     */
/*   operations (e.g. clear, reset, and bload functions). */
/**********************************************************/
Modify_CallFunctionItem *
CL_AddModifyFunctionToCallList (Environment * theEnv,
				const char *name,
				int priority,
				Modify_CallFunction * func,
				Modify_CallFunctionItem * head, void *context)
{
  Modify_CallFunctionItem *newPtr, *currentPtr, *lastPtr = NULL;
  char *nameCopy;

  newPtr = get_struct (theEnv, modify_CallFunctionItem);

  nameCopy = (char *) CL_genalloc (theEnv, strlen (name) + 1);
  CL_genstrcpy (nameCopy, name);
  newPtr->name = nameCopy;

  newPtr->func = func;
  newPtr->priority = priority;
  newPtr->context = context;

  if (head == NULL)
    {
      newPtr->next = NULL;
      return (newPtr);
    }

  currentPtr = head;
  while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
    {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
    }

  if (lastPtr == NULL)
    {
      newPtr->next = head;
      head = newPtr;
    }
  else
    {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
    }

  return (head);
}

/********************************************************************/
/* CL_RemoveModifyFunctionFromCallList: Removes a function from a list */
/*   of functions which are called to perfo_rm certain operations    */
/*   (e.g. clear, reset, and bload functions).                      */
/********************************************************************/
Modify_CallFunctionItem *
CL_RemoveModifyFunctionFromCallList (Environment * theEnv,
				     const char *name,
				     Modify_CallFunctionItem * head,
				     bool *found)
{
  Modify_CallFunctionItem *currentPtr, *lastPtr;

  *found = false;
  lastPtr = NULL;
  currentPtr = head;

  while (currentPtr != NULL)
    {
      if (strcmp (name, currentPtr->name) == 0)
	{
	  *found = true;
	  if (lastPtr == NULL)
	    {
	      head = currentPtr->next;
	    }
	  else
	    {
	      lastPtr->next = currentPtr->next;
	    }

	  CL_genfree (theEnv, (void *) currentPtr->name,
		      strlen (currentPtr->name) + 1);
	  rtn_struct (theEnv, modify_CallFunctionItem, currentPtr);
	  return head;
	}

      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
    }

  return head;
}


/***************************************************************/
/* CL_DeallocateModifyCallList: Removes all functions from a list */
/*   of functions which are called to perfo_rm certain          */
/*   operations (e.g. clear, reset, and bload functions).      */
/***************************************************************/
void
CL_DeallocateModifyCallList (Environment * theEnv,
			     Modify_CallFunctionItem * theList)
{
  Modify_CallFunctionItem *tmpPtr, *nextPtr;

  tmpPtr = theList;
  while (tmpPtr != NULL)
    {
      nextPtr = tmpPtr->next;
      CL_genfree (theEnv, (void *) tmpPtr->name, strlen (tmpPtr->name) + 1);
      rtn_struct (theEnv, modify_CallFunctionItem, tmpPtr);
      tmpPtr = nextPtr;
    }
}

/**********************/
/* CL_CreateFact_Builder: */
/**********************/
Fact_Builder *
CL_CreateFact_Builder (Environment * theEnv, const char *deftemplateName)
{
  Fact_Builder *theFB;
  Deftemplate *theDeftemplate;
  int i;

  if (theEnv == NULL)
    return NULL;

  if (deftemplateName != NULL)
    {
      theDeftemplate = CL_FindDeftemplate (theEnv, deftemplateName);
      if (theDeftemplate == NULL)
	{
	  FactData (theEnv)->fact_BuilderError =
	    FBE_DEFTEMPLATE_NOT_FOUND_ERROR;
	  return NULL;
	}

      if (theDeftemplate->implied)
	{
	  FactData (theEnv)->fact_BuilderError =
	    FBE_IMPLIED_DEFTEMPLATE_ERROR;
	  return NULL;
	}
    }
  else
    {
      theDeftemplate = NULL;
    }

  theFB = get_struct (theEnv, fact_Builder);
  theFB->fbEnv = theEnv;
  theFB->fbDeftemplate = theDeftemplate;

  if ((theDeftemplate == NULL) || (theDeftemplate->numberOfSlots == 0))
    {
      theFB->fbValueArray = NULL;
    }
  else
    {
      theFB->fbValueArray =
	(CLIPSValue *) CL_gm2 (theEnv,
			       sizeof (CLIPSValue) *
			       theDeftemplate->numberOfSlots);
      for (i = 0; i < theDeftemplate->numberOfSlots; i++)
	{
	  theFB->fbValueArray[i].voidValue = VoidConstant (theEnv);
	}
    }

  FactData (theEnv)->fact_BuilderError = FBE_NO_ERROR;

  return theFB;
}

/*************************/
/* CL_FBPutSlotCLIPSInteger */
/*************************/
PutSlotError
CL_FBPutSlotCLIPSInteger (Fact_Builder * theFB,
			  const char *slotName, CLIPSInteger * slotValue)
{
  CLIPSValue theValue;

  theValue.integerValue = slotValue;
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/********************/
/* CL_FBPutSlotInteger */
/********************/
PutSlotError
CL_FBPutSlotInteger (Fact_Builder * theFB,
		     const char *slotName, long long longLongValue)
{
  CLIPSValue theValue;

  if (theFB == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.integerValue = CL_CreateInteger (theFB->fbEnv, longLongValue);
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/************************/
/* CL_FBPutSlotCLIPSLexeme */
/************************/
PutSlotError
CL_FBPutSlotCLIPSLexeme (Fact_Builder * theFB,
			 const char *slotName, CLIPSLexeme * slotValue)
{
  CLIPSValue theValue;

  theValue.lexemeValue = slotValue;
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/*******************/
/* CL_FBPutSlotSymbol */
/*******************/
PutSlotError
CL_FBPutSlotSymbol (Fact_Builder * theFB,
		    const char *slotName, const char *stringValue)
{
  CLIPSValue theValue;

  if (theFB == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.lexemeValue = CL_CreateSymbol (theFB->fbEnv, stringValue);
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/*******************/
/* CL_FBPutSlotString */
/*******************/
PutSlotError
CL_FBPutSlotString (Fact_Builder * theFB,
		    const char *slotName, const char *stringValue)
{
  CLIPSValue theValue;

  if (theFB == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.lexemeValue = CL_CreateString (theFB->fbEnv, stringValue);
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/*************************/
/* CL_FBPutSlot_InstanceName */
/*************************/
PutSlotError
CL_FBPutSlot_InstanceName (Fact_Builder * theFB,
			   const char *slotName, const char *stringValue)
{
  CLIPSValue theValue;

  if (theFB == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.lexemeValue = CL_Create_InstanceName (theFB->fbEnv, stringValue);
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/***********************/
/* CL_FBPutSlotCLIPSFloat */
/***********************/
PutSlotError
CL_FBPutSlotCLIPSFloat (Fact_Builder * theFB,
			const char *slotName, CLIPSFloat * slotValue)
{
  CLIPSValue theValue;

  theValue.floatValue = slotValue;
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/******************/
/* CL_FBPutSlotFloat */
/******************/
PutSlotError
CL_FBPutSlotFloat (Fact_Builder * theFB,
		   const char *slotName, double floatValue)
{
  CLIPSValue theValue;

  if (theFB == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.floatValue = CL_CreateFloat (theFB->fbEnv, floatValue);
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/*****************/
/* CL_FBPutSlotFact */
/*****************/
PutSlotError
CL_FBPutSlotFact (Fact_Builder * theFB,
		  const char *slotName, Fact * slotValue)
{
  CLIPSValue theValue;

  theValue.factValue = slotValue;
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/*********************/
/* CL_FBPutSlotInstance */
/*********************/
PutSlotError
CL_FBPutSlotInstance (Fact_Builder * theFB,
		      const char *slotName, Instance * slotValue)
{
  CLIPSValue theValue;

  theValue.instanceValue = slotValue;
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/*********************************/
/* CL_FBPutSlotCLIPSExternalAddress */
/*********************************/
PutSlotError
CL_FBPutSlotCLIPSExternalAddress (Fact_Builder * theFB,
				  const char *slotName,
				  CLIPSExternalAddress * slotValue)
{
  CLIPSValue theValue;

  theValue.externalAddressValue = slotValue;
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/***********************/
/* CL_FBPutSlotMultifield */
/***********************/
PutSlotError
CL_FBPutSlotMultifield (Fact_Builder * theFB,
			const char *slotName, Multifield * slotValue)
{
  CLIPSValue theValue;

  theValue.multifieldValue = slotValue;
  return CL_FBPutSlot (theFB, slotName, &theValue);
}

/**************/
/* CL_FBPutSlot: */
/**************/
PutSlotError
CL_FBPutSlot (Fact_Builder * theFB,
	      const char *slotName, CLIPSValue * slotValue)
{
  Environment *theEnv;
  struct templateSlot *theSlot;
  unsigned short whichSlot;
  CLIPSValue oldValue;
  int i;
  ConstraintViolationType cvType;

   /*==========================*/
  /* Check for NULL pointers. */
   /*==========================*/

  if ((theFB == NULL) || (slotName == NULL) || (slotValue == NULL))
    {
      return PSE_NULL_POINTER_ERROR;
    }

  if ((theFB->fbDeftemplate == NULL) || (slotValue->value == NULL))
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theEnv = theFB->fbEnv;

   /*===================================*/
  /* Make sure the slot name requested */
  /* corresponds to a valid slot name. */
   /*===================================*/

  if ((theSlot =
       CL_FindSlot (theFB->fbDeftemplate,
		    CL_CreateSymbol (theFB->fbEnv, slotName),
		    &whichSlot)) == NULL)
    {
      return PSE_SLOT_NOT_FOUND_ERROR;
    }

   /*=============================================*/
  /* Make sure a single field value is not being */
  /* stored in a multifield slot or vice versa.  */
   /*=============================================*/

  if (((theSlot->multislot == 0)
       && (slotValue->header->type == MULTIFIELD_TYPE))
      || ((theSlot->multislot == 1)
	  && (slotValue->header->type != MULTIFIELD_TYPE)))
    {
      return PSE_CARDINALITY_ERROR;
    }

   /*=================================*/
  /* Check constraints for the slot. */
   /*=================================*/

  if (theSlot->constraints != NULL)
    {
      if ((cvType =
	   CL_ConstraintCheckValue (theEnv, slotValue->header->type,
				    slotValue->value,
				    theSlot->constraints)) != NO_VIOLATION)
	{
	  switch (cvType)
	    {
	    case NO_VIOLATION:
	    case FUNCTION_RETURN_TYPE_VIOLATION:
	      CL_SystemError (theEnv, "FACTMNGR", 2);
	      CL_ExitRouter (theEnv, EXIT_FAILURE);
	      break;

	    case TYPE_VIOLATION:
	      return PSE_TYPE_ERROR;

	    case RANGE_VIOLATION:
	      return PSE_RANGE_ERROR;

	    case ALLOWED_VALUES_VIOLATION:
	      return PSE_ALLOWED_VALUES_ERROR;

	    case CARDINALITY_VIOLATION:
	      return PSE_CARDINALITY_ERROR;

	    case ALLOWED_CLASSES_VIOLATION:
	      return PSE_ALLOWED_CLASSES_ERROR;
	    }
	}
    }

   /*==========================*/
  /* Set up the change array. */
   /*==========================*/

  if (theFB->fbValueArray == NULL)
    {
      theFB->fbValueArray =
	(CLIPSValue *) CL_gm2 (theFB->fbEnv,
			       sizeof (CLIPSValue) *
			       theFB->fbDeftemplate->numberOfSlots);
      for (i = 0; i < theFB->fbDeftemplate->numberOfSlots; i++)
	{
	  theFB->fbValueArray[i].voidValue = theFB->fbEnv->VoidConstant;
	}
    }

   /*=====================*/
  /* Set the slot value. */
   /*=====================*/

  oldValue.value = theFB->fbValueArray[whichSlot].value;

  if (oldValue.header->type == MULTIFIELD_TYPE)
    {
      if (CL_MultifieldsEqual
	  (oldValue.multifieldValue, slotValue->multifieldValue))
	{
	  return PSE_NO_ERROR;
	}
    }
  else
    {
      if (oldValue.value == slotValue->value)
	{
	  return PSE_NO_ERROR;
	}
    }

  CL_Release (theEnv, oldValue.header);

  if (oldValue.header->type == MULTIFIELD_TYPE)
    {
      CL_ReturnMultifield (theEnv, oldValue.multifieldValue);
    }

  if (slotValue->header->type == MULTIFIELD_TYPE)
    {
      theFB->fbValueArray[whichSlot].multifieldValue =
	CL_CopyMultifield (theEnv, slotValue->multifieldValue);
    }
  else
    {
      theFB->fbValueArray[whichSlot].value = slotValue->value;
    }

  CL_Retain (theEnv, theFB->fbValueArray[whichSlot].header);

  return PSE_NO_ERROR;
}

/*************/
/* FB_Assert: */
/*************/
Fact *
FB_Assert (Fact_Builder * theFB)
{
  Environment *theEnv;
  int i;
  Fact *theFact;

  if (theFB == NULL)
    return NULL;
  theEnv = theFB->fbEnv;

  if (theFB->fbDeftemplate == NULL)
    {
      FactData (theEnv)->fact_BuilderError = FBE_NULL_POINTER_ERROR;
      return NULL;
    }

  theFact = CL_CreateFact (theFB->fbDeftemplate);

  for (i = 0; i < theFB->fbDeftemplate->numberOfSlots; i++)
    {
      if (theFB->fbValueArray[i].voidValue != VoidConstant (theEnv))
	{
	  theFact->theProposition.contents[i].value =
	    theFB->fbValueArray[i].value;
	  CL_Release (theEnv, theFB->fbValueArray[i].header);
	  theFB->fbValueArray[i].voidValue = VoidConstant (theEnv);
	}
    }

  CL_AssignFactSlotDefaults (theFact);

  theFact = CL_Assert (theFact);

  switch (FactData (theEnv)->assertError)
    {
    case AE_NO_ERROR:
      FactData (theEnv)->fact_BuilderError = FBE_NO_ERROR;
      break;

    case AE_NULL_POINTER_ERROR:
    case AE_RETRACTED_ERROR:
      CL_SystemError (theEnv, "FACTMNGR", 1);
      CL_ExitRouter (theEnv, EXIT_FAILURE);
      break;

    case AE_COULD_NOT_ASSERT_ERROR:
      FactData (theEnv)->fact_BuilderError = FBE_COULD_NOT_ASSERT_ERROR;
      break;

    case AE_RULE_NETWORK_ERROR:
      FactData (theEnv)->fact_BuilderError = FBE_RULE_NETWORK_ERROR;
      break;
    }

  return theFact;
}

/**************/
/* CL_FBDispose: */
/**************/
void
CL_FBDispose (Fact_Builder * theFB)
{
  Environment *theEnv;

  if (theFB == NULL)
    return;

  theEnv = theFB->fbEnv;

  CL_FBAbort (theFB);

  if (theFB->fbValueArray != NULL)
    {
      CL_rm (theEnv, theFB->fbValueArray,
	     sizeof (CLIPSValue) * theFB->fbDeftemplate->numberOfSlots);
    }

  rtn_struct (theEnv, fact_Builder, theFB);
}

/************/
/* CL_FBAbort: */
/************/
void
CL_FBAbort (Fact_Builder * theFB)
{
  Environment *theEnv;
  GCBlock gcb;
  int i;

  if (theFB == NULL)
    return;

  if (theFB->fbDeftemplate == NULL)
    return;

  theEnv = theFB->fbEnv;

  CL_GCBlockStart (theEnv, &gcb);

  for (i = 0; i < theFB->fbDeftemplate->numberOfSlots; i++)
    {
      CL_Release (theEnv, theFB->fbValueArray[i].header);

      if (theFB->fbValueArray[i].header->type == MULTIFIELD_TYPE)
	{
	  CL_ReturnMultifield (theEnv,
			       theFB->fbValueArray[i].multifieldValue);
	}

      theFB->fbValueArray[i].voidValue = VoidConstant (theEnv);
    }

  CL_GCBlockEnd (theEnv, &gcb);
}

/********************/
/* CL_FBSetDeftemplate */
/********************/
Fact_BuilderError
CL_FBSetDeftemplate (Fact_Builder * theFB, const char *deftemplateName)
{
  Deftemplate *theDeftemplate;
  Environment *theEnv;
  int i;

  if (theFB == NULL)
    {
      return FBE_NULL_POINTER_ERROR;
    }

  theEnv = theFB->fbEnv;

  CL_FBAbort (theFB);

  if (deftemplateName != NULL)
    {
      theDeftemplate = CL_FindDeftemplate (theFB->fbEnv, deftemplateName);

      if (theDeftemplate == NULL)
	{
	  FactData (theEnv)->fact_BuilderError =
	    FBE_DEFTEMPLATE_NOT_FOUND_ERROR;
	  return FBE_DEFTEMPLATE_NOT_FOUND_ERROR;
	}

      if (theDeftemplate->implied)
	{
	  FactData (theEnv)->fact_BuilderError =
	    FBE_IMPLIED_DEFTEMPLATE_ERROR;
	  return FBE_IMPLIED_DEFTEMPLATE_ERROR;
	}
    }
  else
    {
      theDeftemplate = NULL;
    }

  if (theFB->fbValueArray != NULL)
    {
      CL_rm (theEnv, theFB->fbValueArray,
	     sizeof (CLIPSValue) * theFB->fbDeftemplate->numberOfSlots);
    }

  theFB->fbDeftemplate = theDeftemplate;

  if ((theDeftemplate == NULL) || (theDeftemplate->numberOfSlots == 0))
    {
      theFB->fbValueArray = NULL;
    }
  else
    {
      theFB->fbValueArray =
	(CLIPSValue *) CL_gm2 (theEnv,
			       sizeof (CLIPSValue) *
			       theDeftemplate->numberOfSlots);
      for (i = 0; i < theDeftemplate->numberOfSlots; i++)
	{
	  theFB->fbValueArray[i].voidValue = VoidConstant (theEnv);
	}
    }

  FactData (theEnv)->fact_BuilderError = FBE_NO_ERROR;
  return FBE_NO_ERROR;
}

/************/
/* CL_FBError: */
/************/
Fact_BuilderError
CL_FBError (Environment * theEnv)
{
  return FactData (theEnv)->fact_BuilderError;
}

/***********************/
/* CL_CreateFactModifier: */
/***********************/
FactModifier *
CL_CreateFactModifier (Environment * theEnv, Fact * oldFact)
{
  FactModifier *theFM;
  int i;

  if (theEnv == NULL)
    return NULL;

  if (oldFact != NULL)
    {
      if (oldFact->garbage)
	{
	  FactData (theEnv)->factModifierError = FME_RETRACTED_ERROR;
	  return NULL;
	}

      if (oldFact->whichDeftemplate->implied)
	{
	  FactData (theEnv)->factModifierError =
	    FME_IMPLIED_DEFTEMPLATE_ERROR;
	  return NULL;
	}

      CL_RetainFact (oldFact);
    }

  theFM = get_struct (theEnv, factModifier);
  theFM->fmEnv = theEnv;
  theFM->fmOldFact = oldFact;

  if ((oldFact == NULL) || (oldFact->whichDeftemplate->numberOfSlots == 0))
    {
      theFM->fmValueArray = NULL;
      theFM->changeMap = NULL;
    }
  else
    {
      theFM->fmValueArray =
	(CLIPSValue *) CL_gm2 (theEnv,
			       sizeof (CLIPSValue) *
			       oldFact->whichDeftemplate->numberOfSlots);

      for (i = 0; i < oldFact->whichDeftemplate->numberOfSlots; i++)
	{
	  theFM->fmValueArray[i].voidValue = VoidConstant (theEnv);
	}

      theFM->changeMap =
	(char *) CL_gm2 (theEnv,
			 CountToBitMapSize (oldFact->
					    whichDeftemplate->numberOfSlots));
      CL_ClearBitString ((void *) theFM->changeMap,
			 CountToBitMapSize (oldFact->
					    whichDeftemplate->numberOfSlots));
    }

  FactData (theEnv)->factModifierError = FME_NO_ERROR;
  return theFM;
}

/*************************/
/* CL_FMPutSlotCLIPSInteger */
/*************************/
PutSlotError
CL_FMPutSlotCLIPSInteger (FactModifier * theFM,
			  const char *slotName, CLIPSInteger * slotValue)
{
  CLIPSValue theValue;

  theValue.integerValue = slotValue;
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/********************/
/* CL_FMPutSlotInteger */
/********************/
PutSlotError
CL_FMPutSlotInteger (FactModifier * theFM,
		     const char *slotName, long long longLongValue)
{
  CLIPSValue theValue;

  if (theFM == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.integerValue = CL_CreateInteger (theFM->fmEnv, longLongValue);
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/************************/
/* CL_FMPutSlotCLIPSLexeme */
/************************/
PutSlotError
CL_FMPutSlotCLIPSLexeme (FactModifier * theFM,
			 const char *slotName, CLIPSLexeme * slotValue)
{
  CLIPSValue theValue;

  theValue.lexemeValue = slotValue;
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/*******************/
/* CL_FMPutSlotSymbol */
/*******************/
PutSlotError
CL_FMPutSlotSymbol (FactModifier * theFM,
		    const char *slotName, const char *stringValue)
{
  CLIPSValue theValue;

  if (theFM == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.lexemeValue = CL_CreateSymbol (theFM->fmEnv, stringValue);
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/*******************/
/* CL_FMPutSlotString */
/*******************/
PutSlotError
CL_FMPutSlotString (FactModifier * theFM,
		    const char *slotName, const char *stringValue)
{
  CLIPSValue theValue;

  if (theFM == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.lexemeValue = CL_CreateString (theFM->fmEnv, stringValue);
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/*************************/
/* CL_FMPutSlot_InstanceName */
/*************************/
PutSlotError
CL_FMPutSlot_InstanceName (FactModifier * theFM,
			   const char *slotName, const char *stringValue)
{
  CLIPSValue theValue;

  if (theFM == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.lexemeValue = CL_Create_InstanceName (theFM->fmEnv, stringValue);
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/***********************/
/* CL_FMPutSlotCLIPSFloat */
/***********************/
PutSlotError
CL_FMPutSlotCLIPSFloat (FactModifier * theFM,
			const char *slotName, CLIPSFloat * slotValue)
{
  CLIPSValue theValue;

  theValue.floatValue = slotValue;
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/******************/
/* CL_FMPutSlotFloat */
/******************/
PutSlotError
CL_FMPutSlotFloat (FactModifier * theFM,
		   const char *slotName, double floatValue)
{
  CLIPSValue theValue;

  if (theFM == NULL)
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theValue.floatValue = CL_CreateFloat (theFM->fmEnv, floatValue);
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/*****************/
/* CL_FMPutSlotFact */
/*****************/
PutSlotError
CL_FMPutSlotFact (FactModifier * theFM,
		  const char *slotName, Fact * slotValue)
{
  CLIPSValue theValue;

  theValue.factValue = slotValue;
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/*********************/
/* CL_FMPutSlotInstance */
/*********************/
PutSlotError
CL_FMPutSlotInstance (FactModifier * theFM,
		      const char *slotName, Instance * slotValue)
{
  CLIPSValue theValue;

  theValue.instanceValue = slotValue;
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/****************************/
/* CL_FMPutSlotExternalAddress */
/****************************/
PutSlotError
CL_FMPutSlotExternalAddress (FactModifier * theFM,
			     const char *slotName,
			     CLIPSExternalAddress * slotValue)
{
  CLIPSValue theValue;

  theValue.externalAddressValue = slotValue;
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/***********************/
/* CL_FMPutSlotMultifield */
/***********************/
PutSlotError
CL_FMPutSlotMultifield (FactModifier * theFM,
			const char *slotName, Multifield * slotValue)
{
  CLIPSValue theValue;

  theValue.multifieldValue = slotValue;
  return CL_FMPutSlot (theFM, slotName, &theValue);
}

/**************/
/* CL_FMPutSlot: */
/**************/
PutSlotError
CL_FMPutSlot (FactModifier * theFM,
	      const char *slotName, CLIPSValue * slotValue)
{
  Environment *theEnv;
  struct templateSlot *theSlot;
  unsigned short whichSlot;
  CLIPSValue oldValue;
  CLIPSValue oldFactValue;
  int i;
  ConstraintViolationType cvType;

   /*==========================*/
  /* Check for NULL pointers. */
   /*==========================*/

  if ((theFM == NULL) || (slotName == NULL) || (slotValue == NULL))
    {
      return PSE_NULL_POINTER_ERROR;
    }

  if ((theFM->fmOldFact == NULL) || (slotValue->value == NULL))
    {
      return PSE_NULL_POINTER_ERROR;
    }

  theEnv = theFM->fmEnv;

   /*==================================*/
  /* Deleted facts can't be modified. */
   /*==================================*/

  if (theFM->fmOldFact->garbage)
    {
      return PSE_INVALID_TARGET_ERROR;
    }

   /*===================================*/
  /* Make sure the slot name requested */
  /* corresponds to a valid slot name. */
   /*===================================*/

  if ((theSlot =
       CL_FindSlot (theFM->fmOldFact->whichDeftemplate,
		    CL_CreateSymbol (theEnv, slotName), &whichSlot)) == NULL)
    {
      return PSE_SLOT_NOT_FOUND_ERROR;
    }

   /*=============================================*/
  /* Make sure a single field value is not being */
  /* stored in a multifield slot or vice versa.  */
   /*=============================================*/

  if (((theSlot->multislot == 0)
       && (slotValue->header->type == MULTIFIELD_TYPE))
      || ((theSlot->multislot == 1)
	  && (slotValue->header->type != MULTIFIELD_TYPE)))
    {
      return PSE_CARDINALITY_ERROR;
    }

   /*=================================*/
  /* Check constraints for the slot. */
   /*=================================*/

  if (theSlot->constraints != NULL)
    {
      if ((cvType =
	   CL_ConstraintCheckValue (theEnv, slotValue->header->type,
				    slotValue->value,
				    theSlot->constraints)) != NO_VIOLATION)
	{
	  switch (cvType)
	    {
	    case NO_VIOLATION:
	    case FUNCTION_RETURN_TYPE_VIOLATION:
	      CL_SystemError (theEnv, "FACTMNGR", 3);
	      CL_ExitRouter (theEnv, EXIT_FAILURE);
	      break;

	    case TYPE_VIOLATION:
	      return PSE_TYPE_ERROR;

	    case RANGE_VIOLATION:
	      return PSE_RANGE_ERROR;

	    case ALLOWED_VALUES_VIOLATION:
	      return PSE_ALLOWED_VALUES_ERROR;

	    case CARDINALITY_VIOLATION:
	      return PSE_CARDINALITY_ERROR;

	    case ALLOWED_CLASSES_VIOLATION:
	      return PSE_ALLOWED_CLASSES_ERROR;
	    }
	}
    }

   /*===========================*/
  /* Set up the change arrays. */
   /*===========================*/

  if (theFM->fmValueArray == NULL)
    {
      theFM->fmValueArray =
	(CLIPSValue *) CL_gm2 (theFM->fmEnv,
			       sizeof (CLIPSValue) *
			       theFM->fmOldFact->
			       whichDeftemplate->numberOfSlots);
      for (i = 0; i < theFM->fmOldFact->whichDeftemplate->numberOfSlots; i++)
	{
	  theFM->fmValueArray[i].voidValue = theFM->fmEnv->VoidConstant;
	}
    }

  if (theFM->changeMap == NULL)
    {
      theFM->changeMap =
	(char *) CL_gm2 (theFM->fmEnv,
			 CountToBitMapSize (theFM->
					    fmOldFact->whichDeftemplate->
					    numberOfSlots));
      CL_ClearBitString ((void *) theFM->changeMap,
			 CountToBitMapSize (theFM->
					    fmOldFact->whichDeftemplate->
					    numberOfSlots));
    }

   /*=====================*/
  /* Set the slot value. */
   /*=====================*/

  oldValue.value = theFM->fmValueArray[whichSlot].value;
  oldFactValue.value =
    theFM->fmOldFact->theProposition.contents[whichSlot].value;

  if (oldFactValue.header->type == MULTIFIELD_TYPE)
    {
      if (CL_MultifieldsEqual
	  (oldFactValue.multifieldValue, slotValue->multifieldValue))
	{
	  CL_Release (theFM->fmEnv, oldValue.header);
	  if (oldValue.header->type == MULTIFIELD_TYPE)
	    {
	      CL_ReturnMultifield (theFM->fmEnv, oldValue.multifieldValue);
	    }
	  theFM->fmValueArray[whichSlot].voidValue =
	    theFM->fmEnv->VoidConstant;
	  CL_ClearBitMap (theFM->changeMap, whichSlot);
	  return PSE_NO_ERROR;
	}

      if (CL_MultifieldsEqual
	  (oldValue.multifieldValue, slotValue->multifieldValue))
	{
	  return PSE_NO_ERROR;
	}
    }
  else
    {
      if (slotValue->value == oldFactValue.value)
	{
	  CL_Release (theFM->fmEnv, oldValue.header);
	  theFM->fmValueArray[whichSlot].voidValue =
	    theFM->fmEnv->VoidConstant;
	  CL_ClearBitMap (theFM->changeMap, whichSlot);
	  return PSE_NO_ERROR;
	}

      if (oldValue.value == slotValue->value)
	{
	  return PSE_NO_ERROR;
	}
    }

  SetBitMap (theFM->changeMap, whichSlot);

  CL_Release (theFM->fmEnv, oldValue.header);

  if (oldValue.header->type == MULTIFIELD_TYPE)
    {
      CL_ReturnMultifield (theFM->fmEnv, oldValue.multifieldValue);
    }

  if (slotValue->header->type == MULTIFIELD_TYPE)
    {
      theFM->fmValueArray[whichSlot].multifieldValue =
	CL_CopyMultifield (theFM->fmEnv, slotValue->multifieldValue);
    }
  else
    {
      theFM->fmValueArray[whichSlot].value = slotValue->value;
    }

  CL_Retain (theFM->fmEnv, theFM->fmValueArray[whichSlot].header);

  return PSE_NO_ERROR;
}

/*************/
/* CL_FMModify: */
/*************/
Fact *
CL_FMModify (FactModifier * theFM)
{
  Environment *theEnv;
  Fact *rv;

  if (theFM == NULL)
    {
      return NULL;
    }

  theEnv = theFM->fmEnv;

  if (theFM->fmOldFact == NULL)
    {
      FactData (theEnv)->factModifierError = FME_NULL_POINTER_ERROR;
      return NULL;
    }

  if (theFM->fmOldFact->garbage)
    {
      FactData (theEnv)->factModifierError = FME_RETRACTED_ERROR;
      return NULL;
    }

  if (theFM->changeMap == NULL)
    {
      return theFM->fmOldFact;
    }

  if (!CL_BitStringHasBitsSet
      (theFM->changeMap,
       CountToBitMapSize (theFM->fmOldFact->whichDeftemplate->numberOfSlots)))
    {
      return theFM->fmOldFact;
    }

  rv =
    CL_ReplaceFact (theFM->fmEnv, theFM->fmOldFact, theFM->fmValueArray,
		    theFM->changeMap);

  if ((FactData (theEnv)->assertError == AE_RULE_NETWORK_ERROR) ||
      (FactData (theEnv)->retractError == RE_RULE_NETWORK_ERROR))
    {
      FactData (theEnv)->factModifierError = FME_RULE_NETWORK_ERROR;
    }
  else if ((FactData (theEnv)->retractError == RE_COULD_NOT_RETRACT_ERROR) ||
	   (FactData (theEnv)->assertError == AE_COULD_NOT_ASSERT_ERROR))
    {
      FactData (theEnv)->factModifierError = FME_COULD_NOT_MODIFY_ERROR;
    }
  else
    {
      FactData (theEnv)->factModifierError = FME_NO_ERROR;
    }

  CL_FMAbort (theFM);

  if ((rv != NULL) && (rv != theFM->fmOldFact))
    {
      CL_ReleaseFact (theFM->fmOldFact);
      theFM->fmOldFact = rv;
      CL_RetainFact (theFM->fmOldFact);
    }

  return rv;
}

/**************/
/* CL_FMDispose: */
/**************/
void
CL_FMDispose (FactModifier * theFM)
{
  GCBlock gcb;
  Environment *theEnv = theFM->fmEnv;
  int i;

  CL_GCBlockStart (theEnv, &gcb);

   /*========================*/
  /* CL_Clear the value array. */
   /*========================*/

  if (theFM->fmOldFact != NULL)
    {
      for (i = 0; i < theFM->fmOldFact->whichDeftemplate->numberOfSlots; i++)
	{
	  CL_Release (theEnv, theFM->fmValueArray[i].header);

	  if (theFM->fmValueArray[i].header->type == MULTIFIELD_TYPE)
	    {
	      CL_ReturnMultifield (theEnv,
				   theFM->fmValueArray[i].multifieldValue);
	    }
	}
    }

   /*=====================================*/
  /* Return the value and change arrays. */
   /*=====================================*/

  if (theFM->fmValueArray != NULL)
    {
      CL_rm (theEnv, theFM->fmValueArray,
	     sizeof (CLIPSValue) *
	     theFM->fmOldFact->whichDeftemplate->numberOfSlots);
    }

  if (theFM->changeMap != NULL)
    {
      CL_rm (theEnv, (void *) theFM->changeMap,
	     CountToBitMapSize (theFM->fmOldFact->
				whichDeftemplate->numberOfSlots));
    }

   /*====================================*/
  /* Return the FactModifier structure. */
   /*====================================*/

  if (theFM->fmOldFact != NULL)
    {
      CL_ReleaseFact (theFM->fmOldFact);
    }

  rtn_struct (theEnv, factModifier, theFM);

  CL_GCBlockEnd (theEnv, &gcb);
}

/************/
/* CL_FMAbort: */
/************/
void
CL_FMAbort (FactModifier * theFM)
{
  GCBlock gcb;
  Environment *theEnv;
  unsigned int i;

  if (theFM == NULL)
    return;

  if (theFM->fmOldFact == NULL)
    return;

  theEnv = theFM->fmEnv;

  CL_GCBlockStart (theEnv, &gcb);

  for (i = 0; i < theFM->fmOldFact->whichDeftemplate->numberOfSlots; i++)
    {
      CL_Release (theEnv, theFM->fmValueArray[i].header);

      if (theFM->fmValueArray[i].header->type == MULTIFIELD_TYPE)
	{
	  CL_ReturnMultifield (theEnv,
			       theFM->fmValueArray[i].multifieldValue);
	}

      theFM->fmValueArray[i].voidValue = theFM->fmEnv->VoidConstant;
    }

  if (theFM->changeMap != NULL)
    {
      CL_ClearBitString ((void *) theFM->changeMap,
			 CountToBitMapSize (theFM->
					    fmOldFact->whichDeftemplate->
					    numberOfSlots));
    }

  CL_GCBlockEnd (theEnv, &gcb);
}

/**************/
/* CL_FMSetFact: */
/**************/
FactModifierError
CL_FMSetFact (FactModifier * theFM, Fact * oldFact)
{
  Environment *theEnv;
  unsigned short currentSlotCount, newSlotCount;
  unsigned int i;

  if (theFM == NULL)
    {
      return FME_NULL_POINTER_ERROR;
    }

  theEnv = theFM->fmEnv;

   /*=================================================*/
  /* Modifiers can only be created for non-retracted */
  /* deftemplate facts with at least one slot.       */
   /*=================================================*/

  if (oldFact != NULL)
    {
      if (oldFact->garbage)
	{
	  FactData (theEnv)->factModifierError = FME_RETRACTED_ERROR;
	  return FME_RETRACTED_ERROR;
	}

      if (oldFact->whichDeftemplate->implied)
	{
	  FactData (theEnv)->factModifierError =
	    FME_IMPLIED_DEFTEMPLATE_ERROR;
	  return FME_IMPLIED_DEFTEMPLATE_ERROR;
	}
    }

   /*========================*/
  /* CL_Clear the value array. */
   /*========================*/

  if (theFM->fmValueArray != NULL)
    {
      for (i = 0; i < theFM->fmOldFact->whichDeftemplate->numberOfSlots; i++)
	{
	  CL_Release (theEnv, theFM->fmValueArray[i].header);

	  if (theFM->fmValueArray[i].header->type == MULTIFIELD_TYPE)
	    {
	      CL_ReturnMultifield (theEnv,
				   theFM->fmValueArray[i].multifieldValue);
	    }
	}
    }

   /*==================================================*/
  /* Resize the value and change arrays if necessary. */
   /*==================================================*/

  if (theFM->fmOldFact == NULL)
    {
      currentSlotCount = 0;
    }
  else
    {
      currentSlotCount = theFM->fmOldFact->whichDeftemplate->numberOfSlots;
    }

  if (oldFact == NULL)
    {
      newSlotCount = 0;
    }
  else
    {
      newSlotCount = oldFact->whichDeftemplate->numberOfSlots;
    }

  if (newSlotCount != currentSlotCount)
    {
      if (theFM->fmValueArray != NULL)
	{
	  CL_rm (theEnv, theFM->fmValueArray,
		 sizeof (CLIPSValue) * currentSlotCount);
	}

      if (theFM->changeMap != NULL)
	{
	  CL_rm (theEnv, (void *) theFM->changeMap, currentSlotCount);
	}

      if (newSlotCount == 0)
	{
	  theFM->fmValueArray = NULL;
	  theFM->changeMap = NULL;
	}
      else
	{
	  theFM->fmValueArray =
	    (CLIPSValue *) CL_gm2 (theEnv,
				   sizeof (CLIPSValue) * newSlotCount);
	  theFM->changeMap =
	    (char *) CL_gm2 (theEnv, CountToBitMapSize (newSlotCount));
	}
    }

   /*=================================*/
  /* Update the fact being modified. */
   /*=================================*/

  CL_RetainFact (oldFact);
  CL_ReleaseFact (theFM->fmOldFact);
  theFM->fmOldFact = oldFact;

   /*=========================================*/
  /* Initialize the value and change arrays. */
   /*=========================================*/

  for (i = 0; i < newSlotCount; i++)
    {
      theFM->fmValueArray[i].voidValue = theFM->fmEnv->VoidConstant;
    }

  if (newSlotCount != 0)
    {
      CL_ClearBitString ((void *) theFM->changeMap,
			 CountToBitMapSize (newSlotCount));
    }

   /*================================================================*/
  /* Return true to indicate the modifier was successfully created. */
   /*================================================================*/

  FactData (theEnv)->factModifierError = FME_NO_ERROR;
  return FME_NO_ERROR;
}

/************/
/* CL_FMError: */
/************/
FactModifierError
CL_FMError (Environment * theEnv)
{
  return FactData (theEnv)->factModifierError;
}

#endif /* DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT */
