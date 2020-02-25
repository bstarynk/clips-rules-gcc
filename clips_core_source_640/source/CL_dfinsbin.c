   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Binary CL_Load/CL_Save Functions for Definstances      */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
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

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFINSTANCES_CONSTRUCT && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include "bload.h"
#include "bsave.h"
#include "cstrcbin.h"
#include "defins.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "modulbin.h"

#include "dfinsbin.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */

/* =========================================
   *****************************************
               MACROS AND TYPES
   =========================================
   ***************************************** */
typedef struct bsave_DefinstancesModule
{
  struct bsaveDefmoduleItemHeader header;
} BSAVE_DEFINSTANCES_MODULE;

typedef struct bsaveDefinstances
{
  struct bsaveConstructHeader header;
  unsigned long mkinstance;
} BSAVE_DEFINSTANCES;

/* =========================================
   *****************************************
      INTERNALLY VISIBLE FUNCTION HEADERS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE
static void CL_BsaveDefinstancesFind (Environment *);
static void MarkDefinstancesItems (Environment *, ConstructHeader *, void *);
static void CL_BsaveDefinstancesExpressions (Environment *, FILE *);
static void CL_BsaveDefinstancesExpression (Environment *, ConstructHeader *,
					    void *);
static void CL_BsaveStorageDefinstances (Environment *, FILE *);
static void CL_BsaveDefinstancesDriver (Environment *, FILE *);
static void CL_BsaveDefinstances (Environment *, ConstructHeader *, void *);
#endif

static void CL_BloadStorageDefinstances (Environment *);
static void CL_BloadDefinstances (Environment *);
static void Update_DefinstancesModule (Environment *, void *, unsigned long);
static void UpdateDefinstances (Environment *, void *, unsigned long);
static void CL_ClearDefinstances_Bload (Environment *);
static void DeallocateDefinstancesBinaryData (Environment *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***********************************************************
  NAME         : CL_SetupDefinstances_Bload
  DESCRIPTION  : Initializes data structures and
                   routines for binary loads of definstances
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Routines defined and structures initialized
  NOTES        : None
 ***********************************************************/
void
CL_SetupDefinstances_Bload (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, DFINSBIN_DATA,
			      sizeof (struct definstancesBinaryData),
			      DeallocateDefinstancesBinaryData);
#if BLOAD_AND_BSAVE
  CL_AddBinaryItem (theEnv, "definstances", 0, CL_BsaveDefinstancesFind,
		    CL_BsaveDefinstancesExpressions,
		    CL_BsaveStorageDefinstances, CL_BsaveDefinstancesDriver,
		    CL_BloadStorageDefinstances, CL_BloadDefinstances,
		    CL_ClearDefinstances_Bload);
#else
  CL_AddBinaryItem (theEnv, "definstances", 0, NULL, NULL, NULL, NULL,
		    CL_BloadStorageDefinstances, CL_BloadDefinstances,
		    CL_ClearDefinstances_Bload);
#endif
}

/*************************************************************/
/* DeallocateDefinstancesBinaryData: Deallocates environment */
/*    data for the definstances binary functionality.        */
/*************************************************************/
static void
DeallocateDefinstancesBinaryData (Environment * theEnv)
{
  size_t space;

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) && (! RUN_TIME)
  space =
    DefinstancesBinaryData (theEnv)->DefinstancesCount *
    sizeof (struct definstances);
  if (space != 0)
    CL_genfree (theEnv, DefinstancesBinaryData (theEnv)->DefinstancesArray,
		space);

  space =
    DefinstancesBinaryData (theEnv)->ModuleCount *
    sizeof (struct definstancesModule);
  if (space != 0)
    CL_genfree (theEnv, DefinstancesBinaryData (theEnv)->ModuleArray, space);
#endif
}

/***************************************************
  NAME         : CL_Bload_DefinstancesModuleRef
  DESCRIPTION  : Returns a pointer to the
                 appropriate definstances module
  INPUTS       : The index of the module
  RETURNS      : A pointer to the module
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void *
CL_Bload_DefinstancesModuleRef (Environment * theEnv, unsigned long theIndex)
{
  return ((void *) &DefinstancesBinaryData (theEnv)->ModuleArray[theIndex]);
}

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if BLOAD_AND_BSAVE

/***************************************************************************
  NAME         : CL_BsaveDefinstancesFind
  DESCRIPTION  : For all definstances, this routine marks all
                   the needed symbols.
                 Also, it also counts the number of
                   expression structures needed.
                 Also, counts total number of definstances.
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : ExpressionCount (a global from BSAVE.C) is incremented
                   for every expression needed
                 Symbols are marked in their structures
  NOTES        : Also sets bsaveIndex for each definstances (assumes
                   definstances will be bsaved in order of binary list)
 ***************************************************************************/
static void
CL_BsaveDefinstancesFind (Environment * theEnv)
{
  CL_Save_BloadCount (theEnv, DefinstancesBinaryData (theEnv)->ModuleCount);
  CL_Save_BloadCount (theEnv,
		      DefinstancesBinaryData (theEnv)->DefinstancesCount);
  DefinstancesBinaryData (theEnv)->DefinstancesCount = 0L;

  DefinstancesBinaryData (theEnv)->ModuleCount =
    CL_GetNumberOfDefmodules (theEnv);

  CL_DoForAllConstructs (theEnv, MarkDefinstancesItems,
			 DefinstancesData (theEnv)->
			 CL_DefinstancesModuleIndex, false, NULL);
}


/***************************************************
  NAME         : MarkDefinstancesItems
  DESCRIPTION  : Marks the needed items for
                 a definstances bsave
  INPUTS       : 1) The definstances
                 2) User data buffer (ignored)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Needed items marked
  NOTES        : None
 ***************************************************/
static void
MarkDefinstancesItems (Environment * theEnv,
		       ConstructHeader * theDefinstances, void *userBuffer)
{
#if MAC_XCD
#pragma unused(userBuffer)
#endif

  CL_MarkConstructHeaderNeededItems (theDefinstances,
				     DefinstancesBinaryData (theEnv)->
				     DefinstancesCount++);
  ExpressionData (theEnv)->ExpressionCount +=
    CL_ExpressionSize (((Definstances *) theDefinstances)->mkinstance);
  CL_MarkNeededItems (theEnv, ((Definstances *) theDefinstances)->mkinstance);
}

/***************************************************
  NAME         : CL_BsaveDefinstancesExpressions
  DESCRIPTION  : CL_Writes out all expressions needed
                   by deffunctyions
  INPUTS       : The file pointer of the binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : File updated
  NOTES        : None
 ***************************************************/
static void
CL_BsaveDefinstancesExpressions (Environment * theEnv, FILE * fp)
{
  CL_DoForAllConstructs (theEnv, CL_BsaveDefinstancesExpression,
			 DefinstancesData (theEnv)->
			 CL_DefinstancesModuleIndex, false, fp);
}

/***************************************************
  NAME         : CL_BsaveDefinstancesExpression
  DESCRIPTION  : CL_Saves the needed expressions for
                 a definstances bsave
  INPUTS       : 1) The definstances
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expressions saved
  NOTES        : None
 ***************************************************/
static void
CL_BsaveDefinstancesExpression (Environment * theEnv,
				ConstructHeader * theDefinstances,
				void *userBuffer)
{
  CL_BsaveExpression (theEnv, ((Definstances *) theDefinstances)->mkinstance,
		      (FILE *) userBuffer);
}

/***********************************************************
  NAME         : CL_BsaveStorageDefinstances
  DESCRIPTION  : CL_Writes out number of each type of structure
                   required for definstances
                 Space required for counts (unsigned long)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 ***********************************************************/
static void
CL_BsaveStorageDefinstances (Environment * theEnv, FILE * fp)
{
  size_t space;

  space = sizeof (unsigned long) * 2;
  CL_Gen_Write (&space, sizeof (size_t), fp);
  CL_Gen_Write (&DefinstancesBinaryData (theEnv)->ModuleCount,
		sizeof (unsigned long), fp);
  CL_Gen_Write (&DefinstancesBinaryData (theEnv)->DefinstancesCount,
		sizeof (unsigned long), fp);
}

/*************************************************************************************
  NAME         : CL_BsaveDefinstancesDriver
  DESCRIPTION  : CL_Writes out definstances in binary fo_rmat
                 Space required (unsigned long)
                 All definstances (sizeof(Definstances) * Number of definstances)
  INPUTS       : File pointer of binary file
  RETURNS      : Nothing useful
  SIDE EFFECTS : Binary file adjusted
  NOTES        : None
 *************************************************************************************/
static void
CL_BsaveDefinstancesDriver (Environment * theEnv, FILE * fp)
{
  size_t space;
  Defmodule *theModule;
  DEFINSTANCES_MODULE *theModuleItem;
  BSAVE_DEFINSTANCES_MODULE dummy_mitem;

  space =
    ((sizeof (BSAVE_DEFINSTANCES_MODULE) *
      DefinstancesBinaryData (theEnv)->ModuleCount) +
     (sizeof (BSAVE_DEFINSTANCES) *
      DefinstancesBinaryData (theEnv)->DefinstancesCount));
  CL_Gen_Write (&space, sizeof (size_t), fp);

  /* =================================
     CL_Write out each definstances module
     ================================= */
  DefinstancesBinaryData (theEnv)->DefinstancesCount = 0L;
  theModule = CL_GetNextDefmodule (theEnv, NULL);
  while (theModule != NULL)
    {
      theModuleItem = (DEFINSTANCES_MODULE *)
	CL_GetModuleItem (theEnv, theModule,
			  CL_FindModuleItem (theEnv,
					     "definstances")->moduleIndex);
      CL_Assign_BsaveDefmdlItemHdrVals (&dummy_mitem.header,
					&theModuleItem->header);
      CL_Gen_Write (&dummy_mitem, sizeof (BSAVE_DEFINSTANCES_MODULE), fp);
      theModule = CL_GetNextDefmodule (theEnv, theModule);
    }

   /*==============================*/
  /* CL_Write out each definstances. */
   /*==============================*/

  CL_DoForAllConstructs (theEnv, CL_BsaveDefinstances,
			 DefinstancesData (theEnv)->
			 CL_DefinstancesModuleIndex, false, fp);

  Restore_BloadCount (theEnv, &DefinstancesBinaryData (theEnv)->ModuleCount);
  Restore_BloadCount (theEnv,
		      &DefinstancesBinaryData (theEnv)->DefinstancesCount);
}

/***************************************************
  NAME         : CL_BsaveDefinstances
  DESCRIPTION  : CL_Bsaves a definstances
  INPUTS       : 1) The definstances
                 2) Output data file pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Definstances saved
  NOTES        : None
 ***************************************************/
static void
CL_BsaveDefinstances (Environment * theEnv,
		      ConstructHeader * theDefinstances, void *userBuffer)
{
  Definstances *dptr = (Definstances *) theDefinstances;
  BSAVE_DEFINSTANCES dummy_df;

  CL_Assign_BsaveConstructHeaderVals (&dummy_df.header, &dptr->header);
  if (dptr->mkinstance != NULL)
    {
      dummy_df.mkinstance = ExpressionData (theEnv)->ExpressionCount;
      ExpressionData (theEnv)->ExpressionCount +=
	CL_ExpressionSize (dptr->mkinstance);
    }
  else
    dummy_df.mkinstance = ULONG_MAX;
  CL_Gen_Write (&dummy_df, sizeof (BSAVE_DEFINSTANCES), (FILE *) userBuffer);
}

#endif

/***********************************************************************
  NAME         : CL_BloadStorageDefinstances
  DESCRIPTION  : This routine space required for definstances
                   structures and allocates space for them
  INPUTS       : Nothing
  RETURNS      : Nothing useful
  SIDE EFFECTS : Arrays allocated and set
  NOTES        : This routine makes no attempt to reset any pointers
                   within the structures
 ***********************************************************************/
static void
CL_BloadStorageDefinstances (Environment * theEnv)
{
  size_t space;

  CL_GenReadBinary (theEnv, &space, sizeof (size_t));
  if (space == 0L)
    return;
  CL_GenReadBinary (theEnv, &DefinstancesBinaryData (theEnv)->ModuleCount,
		    sizeof (unsigned long));
  CL_GenReadBinary (theEnv,
		    &DefinstancesBinaryData (theEnv)->DefinstancesCount,
		    sizeof (unsigned long));
  if (DefinstancesBinaryData (theEnv)->ModuleCount == 0L)
    {
      DefinstancesBinaryData (theEnv)->ModuleArray = NULL;
      DefinstancesBinaryData (theEnv)->DefinstancesArray = NULL;
      return;
    }

  space =
    (DefinstancesBinaryData (theEnv)->ModuleCount *
     sizeof (DEFINSTANCES_MODULE));
  DefinstancesBinaryData (theEnv)->ModuleArray =
    (DEFINSTANCES_MODULE *) CL_genalloc (theEnv, space);

  if (DefinstancesBinaryData (theEnv)->DefinstancesCount == 0L)
    {
      DefinstancesBinaryData (theEnv)->DefinstancesArray = NULL;
      return;
    }

  space =
    (DefinstancesBinaryData (theEnv)->DefinstancesCount *
     sizeof (Definstances));
  DefinstancesBinaryData (theEnv)->DefinstancesArray =
    (Definstances *) CL_genalloc (theEnv, space);
}

/*********************************************************************
  NAME         : CL_BloadDefinstances
  DESCRIPTION  : This routine reads definstances info_rmation from
                   a binary file
                 This routine moves through the definstances
                   binary array updating pointers
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pointers reset from array indices
  NOTES        : Assumes all loading is finished
 ********************************************************************/
static void
CL_BloadDefinstances (Environment * theEnv)
{
  size_t space;

  CL_GenReadBinary (theEnv, &space, sizeof (size_t));
  CL_Bloadand_Refresh (theEnv, DefinstancesBinaryData (theEnv)->ModuleCount,
		       sizeof (BSAVE_DEFINSTANCES_MODULE),
		       Update_DefinstancesModule);
  CL_Bloadand_Refresh (theEnv,
		       DefinstancesBinaryData (theEnv)->DefinstancesCount,
		       sizeof (BSAVE_DEFINSTANCES), UpdateDefinstances);
}

/*******************************************************
  NAME         : Update_DefinstancesModule
  DESCRIPTION  : Updates definstances module with binary
                 load data - sets pointers from
                 offset info_rmation
  INPUTS       : 1) A pointer to the bloaded data
                 2) The index of the binary array
                    element to update
  RETURNS      : Nothing useful
  SIDE EFFECTS : Definstances moudle pointers updated
  NOTES        : None
 *******************************************************/
static void
Update_DefinstancesModule (Environment * theEnv,
			   void *buf, unsigned long obji)
{
  BSAVE_DEFINSTANCES_MODULE *bdptr;

  bdptr = (BSAVE_DEFINSTANCES_MODULE *) buf;
  CL_UpdateDefmoduleItemHeader (theEnv, &bdptr->header,
				&DefinstancesBinaryData (theEnv)->
				ModuleArray[obji].header,
				sizeof (Definstances),
				DefinstancesBinaryData (theEnv)->
				DefinstancesArray);
}

/***************************************************
  NAME         : UpdateDefinstances
  DESCRIPTION  : Updates definstances with binary
                 load data - sets pointers from
                 offset info_rmation
  INPUTS       : 1) A pointer to the bloaded data
                 2) The index of the binary array
                    element to update
  RETURNS      : Nothing useful
  SIDE EFFECTS : Definstances pointers upadted
  NOTES        : None
 ***************************************************/
static void
UpdateDefinstances (Environment * theEnv, void *buf, unsigned long obji)
{
  BSAVE_DEFINSTANCES *bdptr;
  Definstances *dfiptr;

  bdptr = (BSAVE_DEFINSTANCES *) buf;
  dfiptr =
    (Definstances *) & DefinstancesBinaryData (theEnv)->
    DefinstancesArray[obji];

  CL_UpdateConstructHeader (theEnv, &bdptr->header, &dfiptr->header,
			    DEFINSTANCES, sizeof (DEFINSTANCES_MODULE),
			    DefinstancesBinaryData (theEnv)->ModuleArray,
			    sizeof (Definstances),
			    DefinstancesBinaryData (theEnv)->
			    DefinstancesArray);
  dfiptr->mkinstance = ExpressionPointer (bdptr->mkinstance);
  dfiptr->busy = 0;
}

/***************************************************************
  NAME         : CL_ClearDefinstances_Bload
  DESCRIPTION  : CL_Release all binary-loaded definstances
                   structure arrays
                 CL_Resets definstances list to NULL
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Memory cleared
  NOTES        : Definstances name symbol counts decremented
 ***************************************************************/
static void
CL_ClearDefinstances_Bload (Environment * theEnv)
{
  unsigned long i;
  size_t space;

  space =
    (sizeof (DEFINSTANCES_MODULE) *
     DefinstancesBinaryData (theEnv)->ModuleCount);
  if (space == 0L)
    return;
  CL_genfree (theEnv, DefinstancesBinaryData (theEnv)->ModuleArray, space);
  DefinstancesBinaryData (theEnv)->ModuleArray = NULL;
  DefinstancesBinaryData (theEnv)->ModuleCount = 0L;

  for (i = 0; i < DefinstancesBinaryData (theEnv)->DefinstancesCount; i++)
    CL_UnmarkConstructHeader (theEnv,
			      &DefinstancesBinaryData (theEnv)->
			      DefinstancesArray[i].header);
  space =
    (sizeof (Definstances) *
     DefinstancesBinaryData (theEnv)->DefinstancesCount);
  if (space == 0)
    return;
  CL_genfree (theEnv, DefinstancesBinaryData (theEnv)->DefinstancesArray,
	      space);
  DefinstancesBinaryData (theEnv)->DefinstancesArray = NULL;
  DefinstancesBinaryData (theEnv)->DefinstancesCount = 0L;
}

#endif
