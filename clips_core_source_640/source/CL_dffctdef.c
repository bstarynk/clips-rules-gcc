   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/06/16             */
   /*                                                     */
   /*              DEFFACTS DEFINITION MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Defines basic deffacts primitive functions such  */
/*   as allocating and deallocating, traversing, and finding */
/*   deffacts data structures.                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warning.                              */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
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

#if DEFFACTS_CONSTRUCT

#include <stdio.h>

#include "dffctbsc.h"
#include "dffctpsr.h"
#include "envrnmnt.h"
#include "memalloc.h"

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#include "dffctbin.h"
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "dffctcmp.h"
#endif

#include "dffctdef.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void *AllocateModule (Environment *);
static void ReturnModule (Environment *, void *);
static void ReturnDeffacts (Environment *, Deffacts *);
static void Initialize_DeffactsModules (Environment *);
static void DeallocateDeffactsData (Environment *);
#if ! RUN_TIME
static void DestroyDeffactsAction (Environment *, ConstructHeader *, void *);
#else
static void CL_RuntimeDeffactsAction (Environment *, ConstructHeader *,
				      void *);
#endif

/***********************************************************/
/* CL_InitializeDeffacts: Initializes the deffacts construct. */
/***********************************************************/
void
CL_InitializeDeffacts (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, DEFFACTS_DATA,
			      sizeof (struct deffactsData),
			      DeallocateDeffactsData);

  Initialize_DeffactsModules (theEnv);

  CL_DeffactsBasicCommands (theEnv);

  DeffactsData (theEnv)->DeffactsConstruct =
    CL_AddConstruct (theEnv, "deffacts", "deffacts", CL_ParseDeffacts,
		     (CL_FindConstructFunction *) CL_FindDeffacts,
		     CL_GetConstructNamePointer, CL_GetConstructPPFo_rm,
		     CL_GetConstructModuleItem,
		     (GetNextConstructFunction *) CL_GetNextDeffacts,
		     CL_SetNextConstruct,
		     (IsConstructDeletableFunction *) CL_DeffactsIsDeletable,
		     (DeleteConstructFunction *) CL_Undeffacts,
		     (FreeConstructFunction *) ReturnDeffacts);
}

/***************************************************/
/* DeallocateDeffactsData: Deallocates environment */
/*    data for the deffacts construct.             */
/***************************************************/
static void
DeallocateDeffactsData (Environment * theEnv)
{
#if ! RUN_TIME
  struct deffactsModule *theModuleItem;
  Defmodule *theModule;

#if BLOAD || BLOAD_AND_BSAVE
  if (CL_Bloaded (theEnv))
    return;
#endif

  CL_DoForAllConstructs (theEnv,
			 DestroyDeffactsAction,
			 DeffactsData (theEnv)->CL_DeffactsModuleIndex,
			 false, NULL);

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      theModuleItem = (struct deffactsModule *)
	CL_GetModuleItem (theEnv, theModule,
			  DeffactsData (theEnv)->CL_DeffactsModuleIndex);
      rtn_struct (theEnv, deffactsModule, theModuleItem);
    }
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
}

#if ! RUN_TIME
/*********************************************************/
/* DestroyDeffactsAction: Action used to remove deffacts */
/*   as a result of CL_DestroyEnvironment.                  */
/*********************************************************/
static void
DestroyDeffactsAction (Environment * theEnv,
		       ConstructHeader * theConstruct, void *buffer)
{
#if MAC_XCD
#pragma unused(buffer)
#endif
#if (! BLOAD_ONLY) && (! RUN_TIME)
  Deffacts *theDeffacts = (Deffacts *) theConstruct;

  if (theDeffacts == NULL)
    return;

  CL_ReturnPackedExpression (theEnv, theDeffacts->assertList);

  CL_DestroyConstructHeader (theEnv, &theDeffacts->header);

  rtn_struct (theEnv, deffacts, theDeffacts);
#else
#if MAC_XCD
#pragma unused(theEnv,theConstruct)
#endif
#endif
}
#endif

#if RUN_TIME

/***********************************************/
/* CL_RuntimeDeffactsAction: Action to be applied */
/*   to each deffacts construct when a runtime */
/*   initialization occurs.                    */
/***********************************************/
static void
CL_RuntimeDeffactsAction (Environment * theEnv,
			  ConstructHeader * theConstruct, void *buffer)
{
#if MAC_XCD
#pragma unused(buffer)
#endif
  Deffacts *theDeffacts = (Deffacts *) theConstruct;

  theDeffacts->header.env = theEnv;
}

/******************************/
/* Deffacts_RunTimeInitialize: */
/******************************/
void
Deffacts_RunTimeInitialize (Environment * theEnv)
{
  CL_DoForAllConstructs (theEnv, CL_RuntimeDeffactsAction,
			 DeffactsData (theEnv)->CL_DeffactsModuleIndex, true,
			 NULL);
}

#endif

/*******************************************************/
/* Initialize_DeffactsModules: Initializes the deffacts */
/*   construct for use with the defmodule construct.   */
/*******************************************************/
static void
Initialize_DeffactsModules (Environment * theEnv)
{
  DeffactsData (theEnv)->CL_DeffactsModuleIndex =
    CL_RegisterModuleItem (theEnv, "deffacts", AllocateModule, ReturnModule,
#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
			   CL_Bload_DeffactsModuleReference,
#else
			   NULL,
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
			   CL_DeffactsCModuleReference,
#else
			   NULL,
#endif
			   (CL_FindConstructFunction *)
			   CL_FindDeffactsInModule);
}

/************************************************/
/* AllocateModule: Allocates a deffacts module. */
/************************************************/
static void *
AllocateModule (Environment * theEnv)
{
  return ((void *) get_struct (theEnv, deffactsModule));
}

/************************************************/
/* ReturnModule: Deallocates a deffacts module. */
/************************************************/
static void
ReturnModule (Environment * theEnv, void *theItem)
{
  CL_FreeConstructHeaderModule (theEnv,
				(struct defmoduleItemHeader *) theItem,
				DeffactsData (theEnv)->DeffactsConstruct);
  rtn_struct (theEnv, deffactsModule, theItem);
}

/*************************************************************/
/* Get_DeffactsModuleItem: Returns a pointer to the defmodule */
/*  item for the specified deffacts or defmodule.            */
/*************************************************************/
struct deffactsModule *
Get_DeffactsModuleItem (Environment * theEnv, Defmodule * theModule)
{
  return ((struct deffactsModule *)
	  CL_GetConstructModuleItemByIndex (theEnv, theModule,
					    DeffactsData (theEnv)->
					    CL_DeffactsModuleIndex));
}

/************************************************/
/* CL_FindDeffacts: Searches for a deffact in the  */
/*   list of deffacts. Returns a pointer to the */
/*   deffact if found, otherwise NULL.          */
/************************************************/
Deffacts *
CL_FindDeffacts (Environment * theEnv, const char *deffactsName)
{
  return (Deffacts *) CL_FindNamedConstructInModuleOrImports (theEnv,
							      deffactsName,
							      DeffactsData
							      (theEnv)->
							      DeffactsConstruct);
}

/************************************************/
/* CL_FindDeffactsInModule: Searches for a deffact */
/*   in the list of deffacts. Returns a pointer */
/*   to the deffact if found, otherwise NULL.   */
/************************************************/
Deffacts *
CL_FindDeffactsInModule (Environment * theEnv, const char *deffactsName)
{
  return (Deffacts *) CL_FindNamedConstructInModule (theEnv, deffactsName,
						     DeffactsData (theEnv)->
						     DeffactsConstruct);
}

/*********************************************************/
/* CL_GetNextDeffacts: If passed a NULL pointer, returns    */
/*   the first deffacts in the ListOfDeffacts. Otherwise */
/*   returns the next deffacts following the deffacts    */
/*   passed as an argument.                              */
/*********************************************************/
Deffacts *
CL_GetNextDeffacts (Environment * theEnv, Deffacts * deffactsPtr)
{
  return (Deffacts *) CL_GetNextConstructItem (theEnv, &deffactsPtr->header,
					       DeffactsData (theEnv)->
					       CL_DeffactsModuleIndex);
}

/*******************************************************/
/* CL_DeffactsIsDeletable: Returns true if a particular   */
/*   deffacts can be deleted, otherwise returns false. */
/*******************************************************/
bool
CL_DeffactsIsDeletable (Deffacts * theDeffacts)
{
  Environment *theEnv = theDeffacts->header.env;

  if (!CL_ConstructsDeletable (theEnv))
    {
      return false;
    }

  if (ConstructData (theEnv)->CL_ResetInProgress)
    return false;

  return true;
}

/***********************************************************/
/* ReturnDeffacts: Returns the data structures associated  */
/*   with a deffacts construct to the pool of free memory. */
/***********************************************************/
static void
ReturnDeffacts (Environment * theEnv, Deffacts * theDeffacts)
{
#if (! BLOAD_ONLY) && (! RUN_TIME)
  if (theDeffacts == NULL)
    return;

  CL_ExpressionDeinstall (theEnv, theDeffacts->assertList);
  CL_ReturnPackedExpression (theEnv, theDeffacts->assertList);

  CL_DeinstallConstructHeader (theEnv, &theDeffacts->header);

  rtn_struct (theEnv, deffacts, theDeffacts);
#endif
}

/*##################################*/
/* Additional Environment Functions */
/*##################################*/

const char *
CL_DeffactsModule (Deffacts * theDeffacts)
{
  return CL_GetConstructModuleName (&theDeffacts->header);
}

const char *
CL_DeffactsName (Deffacts * theDeffacts)
{
  return CL_GetConstructNameString (&theDeffacts->header);
}

const char *
CL_DeffactsPPFo_rm (Deffacts * theDeffacts)
{
  return CL_GetConstructPPFo_rm (&theDeffacts->header);
}


#endif /* DEFFACTS_CONSTRUCT */
