   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*                FACT BSAVE/BLOAD MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    fact pattern network.                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Changed integer type/precision.                */
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

#if DEFTEMPLATE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "envrnmnt.h"
#include "factmngr.h"
#include "memalloc.h"
#include "moduldef.h"
#include "pattern.h"
#include "reteutil.h"
#include "rulebin.h"
#include "tmpltdef.h"

#include "factbin.h"

/********************************************/
/* INTERNAL DATA STRUCTURES AND DEFINITIONS */
/********************************************/

struct bsaveFactPatternNode
{
  struct bsavePatternNodeHeader header;
  unsigned short whichSlot;
  unsigned short whichField;
  unsigned short leaveFields;
  unsigned long networkTest;
  unsigned long nextLevel;
  unsigned long lastLevel;
  unsigned long leftNode;
  unsigned long rightNode;
};

#define BSAVE_FIND         0
#define BSAVE_PATTERNS     1

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
static void CL_BsaveDriver (Environment *, int, FILE *,
			    struct factPatternNode *);
static void CL_BsaveFind (Environment *);
static void CL_BsaveStorage (Environment *, FILE *);
static void CL_BsaveFactPatterns (Environment *, FILE *);
static void CL_BsavePatternNode (Environment *, struct factPatternNode *,
				 FILE *);
#endif
static void CL_BloadStorage (Environment *);
static void CL_BloadBinaryItem (Environment *);
static void UpdateFactPatterns (Environment *, void *, unsigned long);
static void CL_Clear_Bload (Environment *);
static void DeallocateFact_BloadData (Environment *);

/*****************************************************/
/* CL_FactBinarySetup: Initializes the binary load/save */
/*   feature for the fact pattern network.           */
/*****************************************************/
void
CL_FactBinarySetup (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, FACTBIN_DATA,
			      sizeof (struct factBinaryData),
			      DeallocateFact_BloadData);

#if BLOAD_AND_BSAVE
  CL_AddBinaryItem (theEnv, "facts", 0, CL_BsaveFind, NULL,
		    CL_BsaveStorage, CL_BsaveFactPatterns,
		    CL_BloadStorage, CL_BloadBinaryItem, CL_Clear_Bload);
#endif
#if BLOAD || BLOAD_ONLY
  CL_AddBinaryItem (theEnv, "facts", 0, NULL, NULL, NULL, NULL,
		    CL_BloadStorage, CL_BloadBinaryItem, CL_Clear_Bload);
#endif
}

/****************************************************/
/* DeallocateFact_BloadData: Deallocates environment */
/*    data for the fact bsave functionality.        */
/****************************************************/
static void
DeallocateFact_BloadData (Environment * theEnv)
{
  size_t space;
  unsigned long i;

  for (i = 0; i < FactBinaryData (theEnv)->NumberOfPatterns; i++)
    {
      CL_DestroyAlphaMemory (theEnv,
			     &FactBinaryData (theEnv)->FactPatternArray[i].
			     header, false);
    }

  space =
    FactBinaryData (theEnv)->NumberOfPatterns *
    sizeof (struct factPatternNode);
  if (space != 0)
    CL_genfree (theEnv, FactBinaryData (theEnv)->FactPatternArray, space);
}

#if BLOAD_AND_BSAVE

/*********************************************************/
/* CL_BsaveFind: Counts the number of data structures which */
/*   must be saved in the binary image for the fact      */
/*   pattern network in the current environment.         */
/*********************************************************/
static void
CL_BsaveFind (Environment * theEnv)
{
  Deftemplate *theDeftemplate;
  Defmodule *theModule;

   /*=======================================================*/
  /* If a binary image is already loaded, then temporarily */
  /* save the count values since these will be overwritten */
  /* in the process of saving the binary image.            */
   /*=======================================================*/

  CL_Save_BloadCount (theEnv, FactBinaryData (theEnv)->NumberOfPatterns);

   /*=======================================*/
  /* Set the count of fact pattern network */
  /* data structures to zero.              */
   /*=======================================*/

  FactBinaryData (theEnv)->NumberOfPatterns = 0L;

   /*===========================*/
  /* Loop through each module. */
   /*===========================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      /*===============================*/
      /* Set the current module to the */
      /* module being examined.        */
      /*===============================*/

      CL_SetCurrentModule (theEnv, theModule);

      /*=====================================================*/
      /* Loop through each deftemplate in the current module */
      /* and count the number of data structures which must  */
      /* be saved for its pattern network.                   */
      /*=====================================================*/

      for (theDeftemplate = CL_GetNextDeftemplate (theEnv, NULL);
	   theDeftemplate != NULL;
	   theDeftemplate = CL_GetNextDeftemplate (theEnv, theDeftemplate))
	{
	  CL_BsaveDriver (theEnv, BSAVE_FIND, NULL,
			  theDeftemplate->patternNetwork);
	}
    }
}

/**********************************************************/
/* CL_BsaveDriver: Binary save driver routine which handles  */
/*   both finding/marking the data structures to be saved */
/*   and saving the data structures to a file.            */
/**********************************************************/
static void
CL_BsaveDriver (Environment * theEnv,
		int action, FILE * fp, struct factPatternNode *thePattern)
{
  while (thePattern != NULL)
    {
      switch (action)
	{
	case BSAVE_FIND:
	  thePattern->bsaveID = FactBinaryData (theEnv)->NumberOfPatterns++;
	  break;

	case BSAVE_PATTERNS:
	  CL_BsavePatternNode (theEnv, thePattern, fp);
	  break;

	default:
	  break;
	}

      if (thePattern->nextLevel == NULL)
	{
	  while (thePattern->rightNode == NULL)
	    {
	      thePattern = thePattern->lastLevel;
	      if (thePattern == NULL)
		return;
	    }
	  thePattern = thePattern->rightNode;
	}
      else
	{
	  thePattern = thePattern->nextLevel;
	}
    }
}

/*********************************************************/
/* CL_BsaveStorage: CL_Writes out storage requirements for all */
/*   factPatternNode data structures to the binary file  */
/*********************************************************/
static void
CL_BsaveStorage (Environment * theEnv, FILE * fp)
{
  size_t space;

  space = sizeof (long);
  CL_Gen_Write (&space, sizeof (size_t), fp);
  CL_Gen_Write (&FactBinaryData (theEnv)->NumberOfPatterns, sizeof (long),
		fp);
}

/*****************************************************/
/* CL_BsaveFactPatterns: CL_Writes out all factPatternNode */
/*    data structures to the binary file.            */
/*****************************************************/
static void
CL_BsaveFactPatterns (Environment * theEnv, FILE * fp)
{
  size_t space;
  Deftemplate *theDeftemplate;
  Defmodule *theModule;

   /*========================================*/
  /* CL_Write out the amount of space taken up */
  /* by the factPatternNode data structures */
  /* in the binary image.                   */
   /*========================================*/

  space =
    FactBinaryData (theEnv)->NumberOfPatterns *
    sizeof (struct bsaveFactPatternNode);
  CL_Gen_Write (&space, sizeof (size_t), fp);

   /*===========================*/
  /* Loop through each module. */
   /*===========================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      /*=====================================================*/
      /* Loop through each deftemplate in the current module */
      /* and save its fact pattern network to the file.      */
      /*=====================================================*/

      CL_SetCurrentModule (theEnv, theModule);
      for (theDeftemplate = CL_GetNextDeftemplate (theEnv, NULL);
	   theDeftemplate != NULL;
	   theDeftemplate = CL_GetNextDeftemplate (theEnv, theDeftemplate))
	{
	  CL_BsaveDriver (theEnv, BSAVE_PATTERNS, fp,
			  theDeftemplate->patternNetwork);
	}
    }

   /*=============================================================*/
  /* If a binary image was already loaded when the bsave command */
  /* was issued, then restore the counts indicating the number   */
  /* of factPatternNode data structures in the binary image      */
  /* (these were overwritten by the binary save).                */
   /*=============================================================*/

  Restore_BloadCount (theEnv, &FactBinaryData (theEnv)->NumberOfPatterns);
}

/******************************************************/
/* CL_BsavePatternNode: CL_Writes out a single fact pattern */
/*   node to the binary image save file.              */
/******************************************************/
static void
CL_BsavePatternNode (Environment * theEnv,
		     struct factPatternNode *thePattern, FILE * fp)
{
  struct bsaveFactPatternNode tempNode;

  CL_Assign_BsavePatternHeaderValues (theEnv, &tempNode.header,
				      &thePattern->header);

  tempNode.whichField = thePattern->whichField;
  tempNode.leaveFields = thePattern->leaveFields;
  tempNode.whichSlot = thePattern->whichSlot;
  tempNode.networkTest =
    CL_HashedExpressionIndex (theEnv, thePattern->networkTest);
  tempNode.nextLevel = CL_BsaveFactPatternIndex (thePattern->nextLevel);
  tempNode.lastLevel = CL_BsaveFactPatternIndex (thePattern->lastLevel);
  tempNode.leftNode = CL_BsaveFactPatternIndex (thePattern->leftNode);
  tempNode.rightNode = CL_BsaveFactPatternIndex (thePattern->rightNode);

  CL_Gen_Write (&tempNode, sizeof (struct bsaveFactPatternNode), fp);
}

#endif /* BLOAD_AND_BSAVE */

/*****************************************************/
/* CL_BloadStorage: Allocates storage requirements for  */
/*   the factPatternNodes used by this binary image. */
/*****************************************************/
static void
CL_BloadStorage (Environment * theEnv)
{
  size_t space;

   /*=========================================*/
  /* Dete_rmine the number of factPatternNode */
  /* data structures to be read.             */
   /*=========================================*/

  CL_GenReadBinary (theEnv, &space, sizeof (size_t));
  CL_GenReadBinary (theEnv, &FactBinaryData (theEnv)->NumberOfPatterns,
		    sizeof (long));

   /*===================================*/
  /* Allocate the space needed for the */
  /* factPatternNode data structures.  */
   /*===================================*/

  if (FactBinaryData (theEnv)->NumberOfPatterns == 0)
    {
      FactBinaryData (theEnv)->FactPatternArray = NULL;
      return;
    }

  space =
    FactBinaryData (theEnv)->NumberOfPatterns *
    sizeof (struct factPatternNode);
  FactBinaryData (theEnv)->FactPatternArray =
    (struct factPatternNode *) CL_genalloc (theEnv, space);
}

/************************************************************/
/* CL_BloadBinaryItem: CL_Loads and refreshes the factPatternNode */
/*   data structures used by this binary image.             */
/************************************************************/
static void
CL_BloadBinaryItem (Environment * theEnv)
{
  size_t space;
  unsigned long i;

   /*======================================================*/
  /* Read in the amount of space used by the binary image */
  /* (this is used to skip the construct in the event it  */
  /* is not available in the version being run).          */
   /*======================================================*/

  CL_GenReadBinary (theEnv, &space, sizeof (size_t));

   /*=============================================*/
  /* Read in the factPatternNode data structures */
  /* and refresh the pointers.                   */
   /*=============================================*/

  CL_Bloadand_Refresh (theEnv, FactBinaryData (theEnv)->NumberOfPatterns,
		       sizeof (struct bsaveFactPatternNode),
		       UpdateFactPatterns);

  for (i = 0; i < FactBinaryData (theEnv)->NumberOfPatterns; i++)
    {
      if ((FactBinaryData (theEnv)->FactPatternArray[i].lastLevel != NULL) &&
	  (FactBinaryData (theEnv)->FactPatternArray[i].lastLevel->header.
	   selector))
	{
	  CL_AddHashedPatternNode (theEnv,
				   FactBinaryData (theEnv)->
				   FactPatternArray[i].lastLevel,
				   &FactBinaryData (theEnv)->
				   FactPatternArray[i],
				   FactBinaryData (theEnv)->
				   FactPatternArray[i].networkTest->type,
				   FactBinaryData (theEnv)->
				   FactPatternArray[i].networkTest->value);
	}
    }
}

/*************************************************/
/* UpdateFactPatterns: CL_Bload refresh routine for */
/*   the factPatternNode structure.              */
/*************************************************/
static void
UpdateFactPatterns (Environment * theEnv, void *buf, unsigned long obji)
{
  struct bsaveFactPatternNode *bp;

  bp = (struct bsaveFactPatternNode *) buf;

  CL_UpdatePatternNodeHeader (theEnv,
			      &FactBinaryData (theEnv)->
			      FactPatternArray[obji].header, &bp->header);

  FactBinaryData (theEnv)->FactPatternArray[obji].bsaveID = 0L;
  FactBinaryData (theEnv)->FactPatternArray[obji].whichField = bp->whichField;
  FactBinaryData (theEnv)->FactPatternArray[obji].leaveFields =
    bp->leaveFields;
  FactBinaryData (theEnv)->FactPatternArray[obji].whichSlot = bp->whichSlot;

  FactBinaryData (theEnv)->FactPatternArray[obji].networkTest =
    HashedExpressionPointer (bp->networkTest);
  FactBinaryData (theEnv)->FactPatternArray[obji].rightNode =
    CL_BloadFactPatternPointer (bp->rightNode);
  FactBinaryData (theEnv)->FactPatternArray[obji].nextLevel =
    CL_BloadFactPatternPointer (bp->nextLevel);
  FactBinaryData (theEnv)->FactPatternArray[obji].lastLevel =
    CL_BloadFactPatternPointer (bp->lastLevel);
  FactBinaryData (theEnv)->FactPatternArray[obji].leftNode =
    CL_BloadFactPatternPointer (bp->leftNode);
}

/***************************************************/
/* CL_Clear_Bload:  Fact pattern network clear routine */
/*   when a binary load is in effect.              */
/***************************************************/
static void
CL_Clear_Bload (Environment * theEnv)
{
  size_t space;
  unsigned long i;

  for (i = 0; i < FactBinaryData (theEnv)->NumberOfPatterns; i++)
    {
      if ((FactBinaryData (theEnv)->FactPatternArray[i].lastLevel != NULL) &&
	  (FactBinaryData (theEnv)->FactPatternArray[i].lastLevel->header.
	   selector))
	{
	  CL_RemoveHashedPatternNode (theEnv,
				      FactBinaryData (theEnv)->
				      FactPatternArray[i].lastLevel,
				      &FactBinaryData (theEnv)->
				      FactPatternArray[i],
				      FactBinaryData (theEnv)->
				      FactPatternArray[i].networkTest->type,
				      FactBinaryData (theEnv)->
				      FactPatternArray[i].networkTest->value);
	}
    }


  space =
    FactBinaryData (theEnv)->NumberOfPatterns *
    sizeof (struct factPatternNode);
  if (space != 0)
    CL_genfree (theEnv, FactBinaryData (theEnv)->FactPatternArray, space);
  FactBinaryData (theEnv)->NumberOfPatterns = 0;
}

#endif /* DEFTEMPLATE_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME) */
