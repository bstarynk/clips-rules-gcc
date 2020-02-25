   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*                 DEFTEMPLATE MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Defines basic deftemplate primitive functions    */
/*   such as allocating and deallocating, traversing, and    */
/*   finding deftemplate data structures.                    */
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
/*            Corrected code to remove run-time program      */
/*            compiler warnings.                             */
/*                                                           */
/*      6.30: Added code for deftemplate run time            */
/*            initialization of hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
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
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>

#include "cstrccom.h"
#include "cstrnchk.h"
#include "envrnmnt.h"
#include "exprnops.h"
#include "memalloc.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "network.h"
#include "pattern.h"
#include "router.h"
#include "tmpltbsc.h"
#include "tmpltfun.h"
#include "tmpltpsr.h"
#include "tmpltutl.h"

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#include "tmpltbin.h"
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "tmpltcmp.h"
#endif

#include "tmpltdef.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void *AllocateModule (Environment *);
static void ReturnModule (Environment *, void *);
static void ReturnDeftemplate (Environment *, Deftemplate *);
static void Initialize_DeftemplateModules (Environment *);
static void DeallocateDeftemplateData (Environment *);
static void DestroyDeftemplateAction (Environment *, ConstructHeader *,
				      void *);
static void DestroyDeftemplate (Environment *, Deftemplate *);
#if RUN_TIME
static void CL_RuntimeDeftemplateAction (Environment *, ConstructHeader *,
					 void *);
static void SearchForHashedPatternNodes (Environment *,
					 struct factPatternNode *);
#endif

/******************************************************************/
/* CL_InitializeDeftemplates: Initializes the deftemplate construct. */
/******************************************************************/
void
CL_InitializeDeftemplates (Environment * theEnv)
{
  struct entityRecord deftemplatePtrRecord = { "DEFTEMPLATE_PTR",
    DEFTEMPLATE_PTR, 1, 0, 0,
    NULL,
    NULL, NULL,
    NULL,
    NULL,
    (EntityBusyCountFunction *) CL_DecrementDeftemplateBusyCount,
    (EntityBusyCountFunction *) CL_IncrementDeftemplateBusyCount,
    NULL, NULL, NULL, NULL, NULL
  };
  CL_AllocateEnvironmentData (theEnv, DEFTEMPLATE_DATA,
			      sizeof (struct deftemplateData),
			      DeallocateDeftemplateData);

  memcpy (&DeftemplateData (theEnv)->DeftemplatePtrRecord,
	  &deftemplatePtrRecord, sizeof (struct entityRecord));

  Initialize_Facts (theEnv);

  Initialize_DeftemplateModules (theEnv);

  CL_DeftemplateBasicCommands (theEnv);

  CL_DeftemplateFunctions (theEnv);

  DeftemplateData (theEnv)->DeftemplateConstruct =
    CL_AddConstruct (theEnv, "deftemplate", "deftemplates",
		     CL_ParseDeftemplate,
		     (CL_FindConstructFunction *) CL_FindDeftemplate,
		     CL_GetConstructNamePointer, CL_GetConstructPPFo_rm,
		     CL_GetConstructModuleItem,
		     (GetNextConstructFunction *) CL_GetNextDeftemplate,
		     CL_SetNextConstruct,
		     (IsConstructDeletableFunction *)
		     CL_DeftemplateIsDeletable,
		     (DeleteConstructFunction *) CL_Undeftemplate,
		     (FreeConstructFunction *) ReturnDeftemplate);

  CL_InstallPrimitive (theEnv,
		       (EntityRecord *) & DeftemplateData (theEnv)->
		       DeftemplatePtrRecord, DEFTEMPLATE_PTR);
}

/******************************************************/
/* DeallocateDeftemplateData: Deallocates environment */
/*    data for the deftemplate construct.             */
/******************************************************/
static void
DeallocateDeftemplateData (Environment * theEnv)
{
#if ! RUN_TIME
  struct deftemplateModule *theModuleItem;
  Defmodule *theModule;
#endif
#if BLOAD || BLOAD_AND_BSAVE
  if (CL_Bloaded (theEnv))
    return;
#endif

  CL_DoForAllConstructs (theEnv, DestroyDeftemplateAction,
			 DeftemplateData (theEnv)->CL_DeftemplateModuleIndex,
			 false, NULL);

#if ! RUN_TIME
  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      theModuleItem = (struct deftemplateModule *)
	CL_GetModuleItem (theEnv, theModule,
			  DeftemplateData (theEnv)->
			  CL_DeftemplateModuleIndex);
      rtn_struct (theEnv, deftemplateModule, theModuleItem);
    }
#endif
}

/*****************************************************/
/* DestroyDeftemplateAction: Action used to remove   */
/*   deftemplates as a result of CL_DestroyEnvironment. */
/*****************************************************/
static void
DestroyDeftemplateAction (Environment * theEnv,
			  ConstructHeader * theConstruct, void *buffer)
{
#if MAC_XCD
#pragma unused(buffer)
#endif
  Deftemplate *theDeftemplate = (Deftemplate *) theConstruct;

  if (theDeftemplate == NULL)
    return;

  DestroyDeftemplate (theEnv, theDeftemplate);
}


/*************************************************************/
/* Initialize_DeftemplateModules: Initializes the deftemplate */
/*   construct for use with the defmodule construct.         */
/*************************************************************/
static void
Initialize_DeftemplateModules (Environment * theEnv)
{
  DeftemplateData (theEnv)->CL_DeftemplateModuleIndex =
    CL_RegisterModuleItem (theEnv, "deftemplate", AllocateModule,
			   ReturnModule,
#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
			   CL_Bload_DeftemplateModuleReference,
#else
			   NULL,
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
			   CL_DeftemplateCModuleReference,
#else
			   NULL,
#endif
			   (CL_FindConstructFunction *)
			   CL_FindDeftemplateInModule);

#if (! BLOAD_ONLY) && (! RUN_TIME) && DEFMODULE_CONSTRUCT
  CL_AddPortConstructItem (theEnv, "deftemplate", SYMBOL_TOKEN);
#endif
}

/***************************************************/
/* AllocateModule: Allocates a deftemplate module. */
/***************************************************/
static void *
AllocateModule (Environment * theEnv)
{
  return ((void *) get_struct (theEnv, deftemplateModule));
}

/*************************************************/
/* ReturnModule: Deallocates a deftemplate module. */
/*************************************************/
static void
ReturnModule (Environment * theEnv, void *theItem)
{
  CL_FreeConstructHeaderModule (theEnv,
				(struct defmoduleItemHeader *) theItem,
				DeftemplateData (theEnv)->
				DeftemplateConstruct);
  rtn_struct (theEnv, deftemplateModule, theItem);
}

/****************************************************************/
/* Get_DeftemplateModuleItem: Returns a pointer to the defmodule */
/*  item for the specified deftemplate or defmodule.            */
/****************************************************************/
struct deftemplateModule *
Get_DeftemplateModuleItem (Environment * theEnv, Defmodule * theModule)
{
  return ((struct deftemplateModule *)
	  CL_GetConstructModuleItemByIndex (theEnv, theModule,
					    DeftemplateData (theEnv)->
					    CL_DeftemplateModuleIndex));
}

/***************************************************/
/* CL_FindDeftemplate: Searches for a deftemplate in  */
/*   the list of deftemplates. Returns a pointer   */
/*   to the deftemplate if  found, otherwise NULL. */
/***************************************************/
Deftemplate *
CL_FindDeftemplate (Environment * theEnv, const char *deftemplateName)
{
  return (Deftemplate *) CL_FindNamedConstructInModuleOrImports (theEnv,
								 deftemplateName,
								 DeftemplateData
								 (theEnv)->
								 DeftemplateConstruct);
}

/*******************************************************/
/* CL_FindDeftemplateInModule: Searches for a deftemplate */
/*   in the list of deftemplates. Returns a pointer    */
/*   to the deftemplate if  found, otherwise NULL.     */
/*******************************************************/
Deftemplate *
CL_FindDeftemplateInModule (Environment * theEnv, const char *deftemplateName)
{
  return (Deftemplate *) CL_FindNamedConstructInModule (theEnv,
							deftemplateName,
							DeftemplateData
							(theEnv)->
							DeftemplateConstruct);
}

/***********************************************************************/
/* CL_GetNextDeftemplate: If passed a NULL pointer, returns the first     */
/*   deftemplate in the ListOfDeftemplates. Otherwise returns the next */
/*   deftemplate following the deftemplate passed as an argument.      */
/***********************************************************************/
Deftemplate *
CL_GetNextDeftemplate (Environment * theEnv, Deftemplate * deftemplatePtr)
{
  return (Deftemplate *) CL_GetNextConstructItem (theEnv,
						  &deftemplatePtr->header,
						  DeftemplateData (theEnv)->
						  CL_DeftemplateModuleIndex);
}

/**********************************************************/
/* CL_DeftemplateIsDeletable: Returns true if a particular   */
/*   deftemplate can be deleted, otherwise returns false. */
/**********************************************************/
bool
CL_DeftemplateIsDeletable (Deftemplate * theDeftemplate)
{
  Environment *theEnv = theDeftemplate->header.env;

  if (!CL_ConstructsDeletable (theEnv))
    {
      return false;
    }

  if (theDeftemplate->busyCount > 0)
    return false;
  if (theDeftemplate->patternNetwork != NULL)
    return false;

  return true;
}

/**************************************************************/
/* ReturnDeftemplate: Returns the data structures associated  */
/*   with a deftemplate construct to the pool of free memory. */
/**************************************************************/
static void
ReturnDeftemplate (Environment * theEnv, Deftemplate * theDeftemplate)
{
#if (! BLOAD_ONLY) && (! RUN_TIME)
  struct templateSlot *slotPtr;

  if (theDeftemplate == NULL)
    return;

   /*====================================================================*/
  /* If a template is redefined, then we want to save its debug status. */
   /*====================================================================*/

#if DEBUGGING_FUNCTIONS
  DeftemplateData (theEnv)->DeletedTemplateDebugFlags = 0;
  if (theDeftemplate->watch)
    BitwiseSet (DeftemplateData (theEnv)->DeletedTemplateDebugFlags, 0);
#endif

   /*===========================================*/
  /* Free storage used by the templates slots. */
   /*===========================================*/

  slotPtr = theDeftemplate->slotList;
  while (slotPtr != NULL)
    {
      CL_ReleaseLexeme (theEnv, slotPtr->slotName);
      CL_RemoveHashedExpression (theEnv, slotPtr->defaultList);
      slotPtr->defaultList = NULL;
      CL_RemoveHashedExpression (theEnv, slotPtr->facetList);
      slotPtr->facetList = NULL;
      CL_RemoveConstraint (theEnv, slotPtr->constraints);
      slotPtr->constraints = NULL;
      slotPtr = slotPtr->next;
    }

  CL_ReturnSlots (theEnv, theDeftemplate->slotList);

   /*==================================*/
  /* Free storage used by the header. */
   /*==================================*/

  CL_DeinstallConstructHeader (theEnv, &theDeftemplate->header);

  rtn_struct (theEnv, deftemplate, theDeftemplate);
#endif
}

/**************************************************************/
/* DestroyDeftemplate: Returns the data structures associated */
/*   with a deftemplate construct to the pool of free memory. */
/**************************************************************/
static void
DestroyDeftemplate (Environment * theEnv, Deftemplate * theDeftemplate)
{
#if (! BLOAD_ONLY) && (! RUN_TIME)
  struct templateSlot *slotPtr, *nextSlot;
#endif
  if (theDeftemplate == NULL)
    return;

#if (! BLOAD_ONLY) && (! RUN_TIME)
  slotPtr = theDeftemplate->slotList;

  while (slotPtr != NULL)
    {
      nextSlot = slotPtr->next;
      rtn_struct (theEnv, templateSlot, slotPtr);
      slotPtr = nextSlot;
    }
#endif

  CL_DestroyFactPatternNetwork (theEnv, theDeftemplate->patternNetwork);

   /*==================================*/
  /* Free storage used by the header. */
   /*==================================*/

#if (! BLOAD_ONLY) && (! RUN_TIME)
  CL_DeinstallConstructHeader (theEnv, &theDeftemplate->header);

  rtn_struct (theEnv, deftemplate, theDeftemplate);
#endif
}

/***********************************************/
/* CL_ReturnSlots: Returns the slot structures of */
/*   a deftemplate to free memory.             */
/***********************************************/
void
CL_ReturnSlots (Environment * theEnv, struct templateSlot *slotPtr)
{
#if (! BLOAD_ONLY) && (! RUN_TIME)
  struct templateSlot *nextSlot;

  while (slotPtr != NULL)
    {
      nextSlot = slotPtr->next;
      CL_ReturnExpression (theEnv, slotPtr->defaultList);
      CL_ReturnExpression (theEnv, slotPtr->facetList);
      CL_RemoveConstraint (theEnv, slotPtr->constraints);
      rtn_struct (theEnv, templateSlot, slotPtr);
      slotPtr = nextSlot;
    }
#endif
}

/*************************************************/
/* CL_DecrementDeftemplateBusyCount: Decrements the */
/*   busy count of a deftemplate data structure. */
/*************************************************/
void
CL_DecrementDeftemplateBusyCount (Environment * theEnv,
				  Deftemplate * theTemplate)
{
  if (!ConstructData (theEnv)->CL_ClearInProgress)
    theTemplate->busyCount--;
}

/*************************************************/
/* CL_IncrementDeftemplateBusyCount: Increments the */
/*   busy count of a deftemplate data structure. */
/*************************************************/
void
CL_IncrementDeftemplateBusyCount (Environment * theEnv,
				  Deftemplate * theTemplate)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif

  theTemplate->busyCount++;
}

/*******************************************************************/
/* CL_GetNextFactInTemplate: If passed a NULL pointer, returns the    */
/*   first fact in the template's fact-list. Otherwise returns the */
/*   next template fact following the fact passed as an argument.  */
/*******************************************************************/
Fact *
CL_GetNextFactInTemplate (Deftemplate * theTemplate, Fact * factPtr)
{
  if (factPtr == NULL)
    {
      return (theTemplate->factList);
    }

  if (factPtr->garbage)
    return NULL;

  return (factPtr->nextTemplateFact);
}

#if ! RUN_TIME

/******************************/
/* CL_CreateDeftemplateScopeMap: */
/******************************/
void *
CL_CreateDeftemplateScopeMap (Environment * theEnv,
			      Deftemplate * theDeftemplate)
{
  unsigned short scopeMapSize;
  char *scopeMap;
  const char *templateName;
  Defmodule *matchModule, *theModule;
  unsigned long moduleID;
  unsigned int count;
  void *theBitMap;

  templateName = theDeftemplate->header.name->contents;
  matchModule = theDeftemplate->header.whichModule->theModule;

  scopeMapSize =
    (sizeof (char) *
     ((CL_GetNumberOfDefmodules (theEnv) / BITS_PER_BYTE) + 1));
  scopeMap = (char *) CL_gm2 (theEnv, scopeMapSize);

  CL_ClearBitString ((void *) scopeMap, scopeMapSize);
  CL_SaveCurrentModule (theEnv);
  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      CL_SetCurrentModule (theEnv, theModule);
      moduleID = theModule->header.bsaveID;
      if (CL_FindImportedConstruct (theEnv, "deftemplate", matchModule,
				    templateName, &count, true, NULL) != NULL)
	SetBitMap (scopeMap, moduleID);
    }
  CL_RestoreCurrentModule (theEnv);
  theBitMap = CL_AddBitMap (theEnv, scopeMap, scopeMapSize);
  IncrementBitMapCount (theBitMap);
  CL_rm (theEnv, scopeMap, scopeMapSize);
  return (theBitMap);
}

#endif

#if RUN_TIME

/**************************************************/
/* CL_RuntimeDeftemplateAction: Action to be applied */
/*   to each deftemplate construct when a runtime */
/*   initialization occurs.                       */
/**************************************************/
static void
CL_RuntimeDeftemplateAction (Environment * theEnv,
			     ConstructHeader * theConstruct, void *buffer)
{
#if MAC_XCD
#pragma unused(buffer)
#endif
  Deftemplate *theDeftemplate = (Deftemplate *) theConstruct;

  theDeftemplate->header.env = theEnv;
  SearchForHashedPatternNodes (theEnv, theDeftemplate->patternNetwork);
}

/********************************/
/* SearchForHashedPatternNodes: */
/********************************/
static void
SearchForHashedPatternNodes (Environment * theEnv,
			     struct factPatternNode *theNode)
{
  while (theNode != NULL)
    {
      if ((theNode->lastLevel != NULL)
	  && (theNode->lastLevel->header.selector))
	{
	  CL_AddHashedPatternNode (theEnv, theNode->lastLevel, theNode,
				   theNode->networkTest->type,
				   theNode->networkTest->value);
	}

      SearchForHashedPatternNodes (theEnv, theNode->nextLevel);

      theNode = theNode->rightNode;
    }
}

/*********************************/
/* Deftemplate_RunTimeInitialize: */
/*********************************/
void
Deftemplate_RunTimeInitialize (Environment * theEnv)
{
  CL_DoForAllConstructs (theEnv, CL_RuntimeDeftemplateAction,
			 DeftemplateData (theEnv)->CL_DeftemplateModuleIndex,
			 true, NULL);
}

#endif /* RUN_TIME */

/*##################################*/
/* Additional Environment Functions */
/*##################################*/

const char *
CL_DeftemplateModule (Deftemplate * theDeftemplate)
{
  return CL_GetConstructModuleName (&theDeftemplate->header);
}

const char *
CL_DeftemplateName (Deftemplate * theDeftemplate)
{
  return CL_GetConstructNameString (&theDeftemplate->header);
}

const char *
CL_DeftemplatePPFo_rm (Deftemplate * theDeftemplate)
{
  return CL_GetConstructPPFo_rm (&theDeftemplate->header);
}

#endif /* DEFTEMPLATE_CONSTRUCT */
