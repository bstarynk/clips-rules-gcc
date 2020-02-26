   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*           DEFTEMPLATE BSAVE/BLOAD MODULE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    deftemplate construct.                                 */
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
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Removed initial-fact support.                  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "cstrnbin.h"
#include "envrnmnt.h"
#include "factbin.h"
#include "factmngr.h"
#include "memalloc.h"
#include "tmpltdef.h"
#include "tmpltpsr.h"
#include "tmpltutl.h"

#include "tmpltbin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
static void CL_BsaveFind (Environment *);
static void CL_BsaveStorage (Environment *, FILE *);
static void CL_BsaveBinaryItem (Environment *, FILE *);
#endif
static void CL_BloadStorage (Environment *);
static void CL_BloadBinaryItem (Environment *);
static void Update_DeftemplateModule (Environment *, void *, unsigned long);
static void UpdateDeftemplate (Environment *, void *, unsigned long);
static void UpdateDeftemplateSlot (Environment *, void *, unsigned long);
static void CL_Clear_Bload (Environment *);
static void DeallocateDeftemplate_BloadData (Environment *);

/***********************************************/
/* CL_DeftemplateBinarySetup: Installs the binary */
/*   save/load feature for deftemplates.       */
/***********************************************/
void
CL_DeftemplateBinarySetup (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, TMPLTBIN_DATA,
			      sizeof (struct deftemplateBinaryData),
			      DeallocateDeftemplate_BloadData);
#if BLOAD_AND_BSAVE
  CL_AddBinaryItem (theEnv, "deftemplate", 0, CL_BsaveFind, NULL,
		    CL_BsaveStorage, CL_BsaveBinaryItem,
		    CL_BloadStorage, CL_BloadBinaryItem, CL_Clear_Bload);
#endif
#if (BLOAD || BLOAD_ONLY)
  CL_AddBinaryItem (theEnv, "deftemplate", 0, NULL, NULL, NULL, NULL,
		    CL_BloadStorage, CL_BloadBinaryItem, CL_Clear_Bload);
#endif
}

/***********************************************************/
/* DeallocateDeftemplate_BloadData: Deallocates environment */
/*    data for the deftemplate bsave functionality.        */
/***********************************************************/
static void
DeallocateDeftemplate_BloadData (Environment * theEnv)
{
  size_t space;

  space =
    DeftemplateBinaryData (theEnv)->NumberOfTemplateModules *
    sizeof (struct deftemplateModule);
  if (space != 0)
    CL_genfree (theEnv, DeftemplateBinaryData (theEnv)->ModuleArray, space);

  space =
    DeftemplateBinaryData (theEnv)->NumberOfDeftemplates *
    sizeof (Deftemplate);
  if (space != 0)
    CL_genfree (theEnv, DeftemplateBinaryData (theEnv)->DeftemplateArray,
		space);

  space =
    DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots *
    sizeof (struct templateSlot);
  if (space != 0)
    CL_genfree (theEnv, DeftemplateBinaryData (theEnv)->SlotArray, space);
}

#if BLOAD_AND_BSAVE

/**************************************************************/
/* CL_BsaveFind: Counts the number of data structures which must */
/*   be saved in the binary image for the deftemplates in the */
/*   current environment.                                     */
/**************************************************************/
static void
CL_BsaveFind (Environment * theEnv)
{
  Deftemplate *theDeftemplate;
  struct templateSlot *theSlot;
  Defmodule *theModule;

   /*=======================================================*/
  /* If a binary image is already loaded, then temporarily */
  /* save the count values since these will be overwritten */
  /* in the process of saving the binary image.            */
   /*=======================================================*/

  CL_Save_BloadCount (theEnv,
		      DeftemplateBinaryData (theEnv)->NumberOfDeftemplates);
  CL_Save_BloadCount (theEnv,
		      DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots);
  CL_Save_BloadCount (theEnv,
		      DeftemplateBinaryData
		      (theEnv)->NumberOfTemplateModules);

   /*==================================================*/
  /* Set the count of deftemplates, deftemplate slots */
  /* and deftemplate module data structures to zero.  */
   /*==================================================*/

  DeftemplateBinaryData (theEnv)->NumberOfDeftemplates = 0;
  DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots = 0;
  DeftemplateBinaryData (theEnv)->NumberOfTemplateModules = 0;

   /*===========================*/
  /* Loop through each module. */
   /*===========================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      /*============================================*/
      /* Set the current module to the module being */
      /* examined and increment the number of       */
      /* deftemplate modules encountered.           */
      /*============================================*/

      CL_SetCurrentModule (theEnv, theModule);
      DeftemplateBinaryData (theEnv)->NumberOfTemplateModules++;

      /*======================================================*/
      /* Loop through each deftemplate in the current module. */
      /*======================================================*/

      for (theDeftemplate = CL_GetNextDeftemplate (theEnv, NULL);
	   theDeftemplate != NULL;
	   theDeftemplate = CL_GetNextDeftemplate (theEnv, theDeftemplate))
	{
	 /*======================================================*/
	  /* Initialize the construct header for the binary save. */
	 /*======================================================*/

	  CL_MarkConstructHeaderNeededItems (&theDeftemplate->header,
					     DeftemplateBinaryData
					     (theEnv)->NumberOfDeftemplates++);

	 /*=============================================================*/
	  /* Loop through each slot in the deftemplate, incrementing the */
	  /* slot count and marking the slot names as needed symbols.    */
	 /*=============================================================*/

	  for (theSlot = theDeftemplate->slotList;
	       theSlot != NULL; theSlot = theSlot->next)
	    {
	      DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots++;
	      theSlot->slotName->neededSymbol = true;
	    }
	}

    }
}

/*********************************************************/
/* CL_BsaveStorage: CL_Writes out the storage requirements for */
/*    all deftemplate structures to the binary file.     */
/*********************************************************/
static void
CL_BsaveStorage (Environment * theEnv, FILE * fp)
{
  size_t space;

   /*========================================================================*/
  /* Three data structures are saved as part of a deftemplate binary image: */
  /* the deftemplate data structure, the deftemplateModule data structure,  */
  /* and the templateSlot data structure. The data structures associated    */
  /* with default values and constraints are not save with the deftemplate  */
  /* portion of the binary image.                                           */
   /*========================================================================*/

  space = sizeof (long) * 3;
  CL_Gen_Write (&space, sizeof (size_t), fp);
  CL_Gen_Write (&DeftemplateBinaryData (theEnv)->NumberOfDeftemplates,
		sizeof (long), fp);
  CL_Gen_Write (&DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots,
		sizeof (long), fp);
  CL_Gen_Write (&DeftemplateBinaryData (theEnv)->NumberOfTemplateModules,
		sizeof (long), fp);
}

/***********************************************/
/* CL_BsaveBinaryItem: CL_Writes out all deftemplate */
/*   structures to the binary file.            */
/***********************************************/
static void
CL_BsaveBinaryItem (Environment * theEnv, FILE * fp)
{
  size_t space;
  Deftemplate *theDeftemplate;
  struct bsaveDeftemplate tempDeftemplate;
  struct templateSlot *theSlot;
  struct bsaveTemplateSlot tempTemplateSlot;
  struct bsave_DeftemplateModule tempTemplateModule;
  Defmodule *theModule;
  struct deftemplateModule *theModuleItem;

   /*============================================================*/
  /* CL_Write out the amount of space taken up by the deftemplate, */
  /* deftemplateModule, and templateSlot data structures in the */
  /* binary image.                                              */
   /*============================================================*/

  space =
    (DeftemplateBinaryData (theEnv)->NumberOfDeftemplates *
     sizeof (struct bsaveDeftemplate)) +
    (DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots *
     sizeof (struct bsaveTemplateSlot)) +
    (DeftemplateBinaryData (theEnv)->NumberOfTemplateModules *
     sizeof (struct bsave_DeftemplateModule));
  CL_Gen_Write (&space, sizeof (size_t), fp);

   /*===================================================*/
  /* CL_Write out each deftemplate module data structure. */
   /*===================================================*/

  DeftemplateBinaryData (theEnv)->NumberOfDeftemplates = 0;
  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      CL_SetCurrentModule (theEnv, theModule);

      theModuleItem = (struct deftemplateModule *)
	CL_GetModuleItem (theEnv, NULL,
			  CL_FindModuleItem (theEnv,
					     "deftemplate")->moduleIndex);
      CL_Assign_BsaveDefmdlItemHdrVals (&tempTemplateModule.header,
					&theModuleItem->header);
      CL_Gen_Write (&tempTemplateModule,
		    sizeof (struct bsave_DeftemplateModule), fp);
    }

   /*============================================*/
  /* CL_Write out each deftemplate data structure. */
   /*============================================*/

  DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots = 0;
  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      CL_SetCurrentModule (theEnv, theModule);

      for (theDeftemplate = CL_GetNextDeftemplate (theEnv, NULL);
	   theDeftemplate != NULL;
	   theDeftemplate = CL_GetNextDeftemplate (theEnv, theDeftemplate))
	{
	  CL_Assign_BsaveConstructHeaderVals (&tempDeftemplate.header,
					      &theDeftemplate->header);
	  tempDeftemplate.implied = theDeftemplate->implied;
	  tempDeftemplate.numberOfSlots = theDeftemplate->numberOfSlots;
	  tempDeftemplate.patternNetwork =
	    CL_BsaveFactPatternIndex (theDeftemplate->patternNetwork);

	  if (theDeftemplate->slotList != NULL)
	    {
	      tempDeftemplate.slotList =
		DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots;
	    }
	  else
	    {
	      tempDeftemplate.slotList = ULONG_MAX;
	    }

	  CL_Gen_Write (&tempDeftemplate, sizeof (struct bsaveDeftemplate),
			fp);

	  DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots +=
	    theDeftemplate->numberOfSlots;
	}
    }

   /*=============================================*/
  /* CL_Write out each templateSlot data structure. */
   /*=============================================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      CL_SetCurrentModule (theEnv, theModule);

      for (theDeftemplate = CL_GetNextDeftemplate (theEnv, NULL);
	   theDeftemplate != NULL;
	   theDeftemplate = CL_GetNextDeftemplate (theEnv, theDeftemplate))
	{
	  for (theSlot = theDeftemplate->slotList;
	       theSlot != NULL; theSlot = theSlot->next)
	    {
	      tempTemplateSlot.constraints =
		ConstraintIndex (theSlot->constraints);
	      tempTemplateSlot.slotName = theSlot->slotName->bucket;
	      tempTemplateSlot.multislot = theSlot->multislot;
	      tempTemplateSlot.noDefault = theSlot->noDefault;
	      tempTemplateSlot.defaultPresent = theSlot->defaultPresent;
	      tempTemplateSlot.defaultDynamic = theSlot->defaultDynamic;
	      tempTemplateSlot.defaultList =
		CL_HashedExpressionIndex (theEnv, theSlot->defaultList);
	      tempTemplateSlot.facetList =
		CL_HashedExpressionIndex (theEnv, theSlot->facetList);

	      if (theSlot->next != NULL)
		tempTemplateSlot.next = 0L;
	      else
		tempTemplateSlot.next = ULONG_MAX;

	      CL_Gen_Write (&tempTemplateSlot,
			    sizeof (struct bsaveTemplateSlot), fp);
	    }
	}
    }

   /*=============================================================*/
  /* If a binary image was already loaded when the bsave command */
  /* was issued, then restore the counts indicating the number   */
  /* of deftemplates, deftemplate modules, and deftemplate slots */
  /* in the binary image (these were overwritten by the binary   */
  /* save).                                                      */
   /*=============================================================*/

  Restore_BloadCount (theEnv,
		      &DeftemplateBinaryData (theEnv)->NumberOfDeftemplates);
  Restore_BloadCount (theEnv,
		      &DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots);
  Restore_BloadCount (theEnv,
		      &DeftemplateBinaryData
		      (theEnv)->NumberOfTemplateModules);
}

#endif /* BLOAD_AND_BSAVE */

/****************************************************/
/* CL_BloadStorage: Allocates storage requirements for */
/*   the deftemplates used by this binary image.    */
/****************************************************/
static void
CL_BloadStorage (Environment * theEnv)
{
  size_t space;

   /*=========================================================*/
  /* Dete_rmine the number of deftemplate, deftemplateModule, */
  /* and templateSlot data structures to be read.            */
   /*=========================================================*/

  CL_GenReadBinary (theEnv, &space, sizeof (size_t));
  CL_GenReadBinary (theEnv,
		    &DeftemplateBinaryData (theEnv)->NumberOfDeftemplates,
		    sizeof (long));
  CL_GenReadBinary (theEnv,
		    &DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots,
		    sizeof (long));
  CL_GenReadBinary (theEnv,
		    &DeftemplateBinaryData (theEnv)->NumberOfTemplateModules,
		    sizeof (long));

   /*====================================*/
  /* Allocate the space needed for the  */
  /* deftemplateModule data structures. */
   /*====================================*/

  if (DeftemplateBinaryData (theEnv)->NumberOfTemplateModules == 0)
    {
      DeftemplateBinaryData (theEnv)->DeftemplateArray = NULL;
      DeftemplateBinaryData (theEnv)->SlotArray = NULL;
      DeftemplateBinaryData (theEnv)->ModuleArray = NULL;
      return;
    }

  space =
    DeftemplateBinaryData (theEnv)->NumberOfTemplateModules *
    sizeof (struct deftemplateModule);
  DeftemplateBinaryData (theEnv)->ModuleArray =
    (struct deftemplateModule *) CL_genalloc (theEnv, space);

   /*===================================*/
  /* Allocate the space needed for the */
  /* deftemplate data structures.      */
   /*===================================*/

  if (DeftemplateBinaryData (theEnv)->NumberOfDeftemplates == 0)
    {
      DeftemplateBinaryData (theEnv)->DeftemplateArray = NULL;
      DeftemplateBinaryData (theEnv)->SlotArray = NULL;
      return;
    }

  space =
    DeftemplateBinaryData (theEnv)->NumberOfDeftemplates *
    sizeof (Deftemplate);
  DeftemplateBinaryData (theEnv)->DeftemplateArray =
    (Deftemplate *) CL_genalloc (theEnv, space);

   /*===================================*/
  /* Allocate the space needed for the */
  /* templateSlot data structures.     */
   /*===================================*/

  if (DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots == 0)
    {
      DeftemplateBinaryData (theEnv)->SlotArray = NULL;
      return;
    }

  space =
    DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots *
    sizeof (struct templateSlot);
  DeftemplateBinaryData (theEnv)->SlotArray =
    (struct templateSlot *) CL_genalloc (theEnv, space);
}

/********************************************************/
/* CL_BloadBinaryItem: CL_Loads and refreshes the deftemplate */
/*   constructs used by this binary image.              */
/********************************************************/
static void
CL_BloadBinaryItem (Environment * theEnv)
{
  size_t space;

   /*======================================================*/
  /* Read in the amount of space used by the binary image */
  /* (this is used to skip the construct in the event it  */
  /* is not available in the version being run).          */
   /*======================================================*/

  CL_GenReadBinary (theEnv, &space, sizeof (size_t));

   /*===============================================*/
  /* Read in the deftemplateModule data structures */
  /* and refresh the pointers.                     */
   /*===============================================*/

  CL_Bloadand_Refresh (theEnv,
		       DeftemplateBinaryData
		       (theEnv)->NumberOfTemplateModules,
		       sizeof (struct bsave_DeftemplateModule),
		       Update_DeftemplateModule);

   /*===============================================*/
  /* Read in the deftemplateModule data structures */
  /* and refresh the pointers.                     */
   /*===============================================*/

  CL_Bloadand_Refresh (theEnv,
		       DeftemplateBinaryData (theEnv)->NumberOfDeftemplates,
		       sizeof (struct bsaveDeftemplate), UpdateDeftemplate);

   /*==========================================*/
  /* Read in the templateSlot data structures */
  /* and refresh the pointers.                */
   /*==========================================*/

  CL_Bloadand_Refresh (theEnv,
		       DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots,
		       sizeof (struct bsaveTemplateSlot),
		       UpdateDeftemplateSlot);
}

/**************************************************/
/* Update_DeftemplateModule: CL_Bload refresh routine */
/*   for deftemplateModule data structures.       */
/**************************************************/
static void
Update_DeftemplateModule (Environment * theEnv, void *buf, unsigned long obji)
{
  struct bsave_DeftemplateModule *bdmPtr;

  bdmPtr = (struct bsave_DeftemplateModule *) buf;
  CL_UpdateDefmoduleItemHeader (theEnv, &bdmPtr->header,
				&DeftemplateBinaryData (theEnv)->ModuleArray
				[obji].header, sizeof (Deftemplate),
				(void *)
				DeftemplateBinaryData
				(theEnv)->DeftemplateArray);
}

/********************************************/
/* UpdateDeftemplate: CL_Bload refresh routine */
/*   for deftemplate data structures.       */
/********************************************/
static void
UpdateDeftemplate (Environment * theEnv, void *buf, unsigned long obji)
{
  Deftemplate *theDeftemplate;
  struct bsaveDeftemplate *bdtPtr;

  bdtPtr = (struct bsaveDeftemplate *) buf;
  theDeftemplate = &DeftemplateBinaryData (theEnv)->DeftemplateArray[obji];

  CL_UpdateConstructHeader (theEnv, &bdtPtr->header, &theDeftemplate->header,
			    DEFTEMPLATE, sizeof (struct deftemplateModule),
			    DeftemplateBinaryData (theEnv)->ModuleArray,
			    sizeof (Deftemplate),
			    DeftemplateBinaryData (theEnv)->DeftemplateArray);

  if (bdtPtr->slotList != ULONG_MAX)
    {
      theDeftemplate->slotList =
	(struct templateSlot *)
	&DeftemplateBinaryData (theEnv)->SlotArray[bdtPtr->slotList];
    }
  else
    {
      theDeftemplate->slotList = NULL;
    }

  if (bdtPtr->patternNetwork != ULONG_MAX)
    {
      theDeftemplate->patternNetwork =
	(struct factPatternNode *)
	CL_BloadFactPatternPointer (bdtPtr->patternNetwork);
    }
  else
    {
      theDeftemplate->patternNetwork = NULL;
    }

  theDeftemplate->implied = bdtPtr->implied;
#if DEBUGGING_FUNCTIONS
  theDeftemplate->watch = FactData (theEnv)->CL_Watch_Facts;
#endif
  theDeftemplate->inScope = false;
  theDeftemplate->numberOfSlots = bdtPtr->numberOfSlots;
  theDeftemplate->factList = NULL;
  theDeftemplate->lastFact = NULL;
}

/************************************************/
/* UpdateDeftemplateSlot: CL_Bload refresh routine */
/*   for templateSlot data structures.          */
/************************************************/
static void
UpdateDeftemplateSlot (Environment * theEnv, void *buf, unsigned long obji)
{
  struct templateSlot *theSlot;
  struct bsaveTemplateSlot *btsPtr;

  btsPtr = (struct bsaveTemplateSlot *) buf;
  theSlot =
    (struct templateSlot *) &DeftemplateBinaryData (theEnv)->SlotArray[obji];

  theSlot->slotName = SymbolPointer (btsPtr->slotName);
  IncrementLexemeCount (theSlot->slotName);
  theSlot->defaultList = HashedExpressionPointer (btsPtr->defaultList);
  theSlot->facetList = HashedExpressionPointer (btsPtr->facetList);
  theSlot->constraints = ConstraintPointer (btsPtr->constraints);

  theSlot->multislot = btsPtr->multislot;
  theSlot->noDefault = btsPtr->noDefault;
  theSlot->defaultPresent = btsPtr->defaultPresent;
  theSlot->defaultDynamic = btsPtr->defaultDynamic;

  if (btsPtr->next != ULONG_MAX)
    {
      theSlot->next =
	(struct templateSlot *)
	&DeftemplateBinaryData (theEnv)->SlotArray[obji + 1];
    }
  else
    {
      theSlot->next = NULL;
    }
}

/*****************************************/
/* CL_Clear_Bload: Deftemplate clear routine */
/*   when a binary load is in effect.    */
/*****************************************/
static void
CL_Clear_Bload (Environment * theEnv)
{
  size_t space;
  unsigned long i;

   /*=============================================*/
  /* Decrement in use counters for atomic values */
  /* contained in the construct headers.         */
   /*=============================================*/

  for (i = 0; i < DeftemplateBinaryData (theEnv)->NumberOfDeftemplates; i++)
    {
      CL_UnmarkConstructHeader (theEnv,
				&DeftemplateBinaryData
				(theEnv)->DeftemplateArray[i].header);
    }

   /*=======================================*/
  /* Decrement in use counters for symbols */
  /* used as slot names.                   */
   /*=======================================*/

  for (i = 0; i < DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots; i++)
    {
      CL_ReleaseLexeme (theEnv,
			DeftemplateBinaryData (theEnv)->
			SlotArray[i].slotName);
    }

   /*======================================================================*/
  /* Deallocate the space used for the deftemplateModule data structures. */
   /*======================================================================*/

  space =
    DeftemplateBinaryData (theEnv)->NumberOfTemplateModules *
    sizeof (struct deftemplateModule);
  if (space != 0)
    CL_genfree (theEnv, DeftemplateBinaryData (theEnv)->ModuleArray, space);
  DeftemplateBinaryData (theEnv)->NumberOfTemplateModules = 0;

   /*================================================================*/
  /* Deallocate the space used for the deftemplate data structures. */
   /*================================================================*/

  space =
    DeftemplateBinaryData (theEnv)->NumberOfDeftemplates *
    sizeof (Deftemplate);
  if (space != 0)
    CL_genfree (theEnv, DeftemplateBinaryData (theEnv)->DeftemplateArray,
		space);
  DeftemplateBinaryData (theEnv)->NumberOfDeftemplates = 0;

   /*=================================================================*/
  /* Deallocate the space used for the templateSlot data structures. */
   /*=================================================================*/

  space =
    DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots *
    sizeof (struct templateSlot);
  if (space != 0)
    CL_genfree (theEnv, DeftemplateBinaryData (theEnv)->SlotArray, space);
  DeftemplateBinaryData (theEnv)->NumberOfTemplateSlots = 0;
}

/************************************************************/
/* CL_Bload_DeftemplateModuleReference: Returns the deftemplate */
/*   module pointer for use with the bload function.        */
/************************************************************/
void *
CL_Bload_DeftemplateModuleReference (Environment * theEnv,
				     unsigned long theIndex)
{
  return ((void *) &DeftemplateBinaryData (theEnv)->ModuleArray[theIndex]);
}

#endif /* DEFTEMPLATE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */
