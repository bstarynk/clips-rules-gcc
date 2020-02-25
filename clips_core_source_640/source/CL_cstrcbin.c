   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*          CONSTRUCT BINARY LOAD/SAVE MODULE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Binary load/save functions for construct         */
/*   headers.                                                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
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

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE

#include "bload.h"
#include "envrnmnt.h"

#if BLOAD_AND_BSAVE
#include "bsave.h"
#endif

#include "moduldef.h"

#include "cstrcbin.h"

#if BLOAD_AND_BSAVE

/***************************************************
  NAME         : CL_MarkConstructHeaderNeededItems
  DESCRIPTION  : Marks symbols and other ephemerals
                 needed by a construct header, and
                 sets the binary-save id for the
                 construct
  INPUTS       : 1) The construct header
                 2) The binary-save id to assign
  RETURNS      : Nothing useful
  SIDE EFFECTS : Id set and items marked
  NOTES        : None
 ***************************************************/
void
CL_MarkConstructHeaderNeededItems (ConstructHeader * theConstruct,
				   unsigned long the_BsaveID)
{
  theConstruct->name->neededSymbol = true;
  theConstruct->bsaveID = the_BsaveID;
}

/******************************************************
  NAME         : CL_Assign_BsaveConstructHeaderVals
  DESCRIPTION  : Assigns value to the construct
                 header for saving in the binary file
  INPUTS       : 1) The binary-save buffer for the
                    construct header values
                 2) The actual construct header
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary-save buffer for construct
                 header written with appropriate values
  NOTES        : Assumes that module items for this
                 construct were saved in the same
                 order as the defmodules.
                 The defmodule binary-save id is
                 used for the whichModule id of
                 this construct.
 ******************************************************/
void
CL_Assign_BsaveConstructHeaderVals (struct bsaveConstructHeader
				    *the_BsaveConstruct,
				    ConstructHeader * theConstruct)
{
  if (theConstruct->name != NULL)
    {
      the_BsaveConstruct->name = theConstruct->name->bucket;
    }
  else
    {
      the_BsaveConstruct->name = ULONG_MAX;
    }

  if ((theConstruct->whichModule != NULL) &&
      (theConstruct->whichModule->theModule != NULL))
    {
      the_BsaveConstruct->whichModule =
	theConstruct->whichModule->theModule->header.bsaveID;
    }
  else
    {
      the_BsaveConstruct->whichModule = ULONG_MAX;
    }

  if (theConstruct->next != NULL)
    the_BsaveConstruct->next = theConstruct->next->bsaveID;
  else
    the_BsaveConstruct->next = ULONG_MAX;
}

#endif /* BLOAD_AND_BSAVE */

/***************************************************
  NAME         : CL_UpdateConstructHeader
  DESCRIPTION  : Dete_rmines field values for
                 construct header from binary-load
                 buffer
  INPUTS       : 1) The binary-load data for the
                    construct header
                 2) The actual construct header
                 3) The size of a defmodule item for
                    this construct
                 4) The array of all defmodule items
                    for this construct
                 5) The size of this construct
                 6) The array of these constructs
  RETURNS      : Nothing useful
  SIDE EFFECTS : Header values set
  NOTES        : None
 ***************************************************/
void
CL_UpdateConstructHeader (Environment * theEnv,
			  struct bsaveConstructHeader *the_BsaveConstruct,
			  ConstructHeader * theConstruct,
			  ConstructType theType,
			  size_t itemModuleSize,
			  void *itemModuleArray,
			  size_t itemSize, void *itemArray)
{
  size_t moduleOffset, itemOffset;

  if (the_BsaveConstruct->whichModule != ULONG_MAX)
    {
      moduleOffset = itemModuleSize * the_BsaveConstruct->whichModule;
      theConstruct->whichModule =
	(struct defmoduleItemHeader *) &((char *)
					 itemModuleArray)[moduleOffset];
    }
  else
    {
      theConstruct->whichModule = NULL;
    }

  if (the_BsaveConstruct->name != ULONG_MAX)
    {
      theConstruct->name = SymbolPointer (the_BsaveConstruct->name);
      IncrementLexemeCount (theConstruct->name);
    }
  else
    {
      theConstruct->name = NULL;
    }

  if (the_BsaveConstruct->next != ULONG_MAX)
    {
      itemOffset = itemSize * the_BsaveConstruct->next;
      theConstruct->next =
	(ConstructHeader *) & ((char *) itemArray)[itemOffset];
    }
  else
    {
      theConstruct->next = NULL;
    }

  theConstruct->constructType = theType;
  theConstruct->env = theEnv;
  theConstruct->ppFo_rm = NULL;
  theConstruct->bsaveID = 0L;
  theConstruct->usrData = NULL;
}

/*******************************************************
  NAME         : CL_UnmarkConstructHeader
  DESCRIPTION  : CL_Releases any ephemerals (symbols, etc.)
                 of a construct header for removal
  INPUTS       : The construct header
  RETURNS      : Nothing useful
  SIDE EFFECTS : Busy counts fo ephemerals decremented
  NOTES        : None
 *******************************************************/
void
CL_UnmarkConstructHeader (Environment * theEnv,
			  ConstructHeader * theConstruct)
{
  CL_ReleaseLexeme (theEnv, theConstruct->name);
}

#endif /* BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE */
