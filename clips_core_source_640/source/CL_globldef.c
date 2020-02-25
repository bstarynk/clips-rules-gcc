   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/13/17             */
   /*                                                     */
   /*                  DEFGLOBAL MODULE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides core routines for the creation and      */
/*   CL_maintenance of the defglobal construct.                 */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
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
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            Use of ?<var>, $?<var>, ?*<var>, and  $?*var*  */
/*            by itself at the command prompt and within     */
/*            the eval function now consistently returns the */
/*            value of  the variable.                        */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFGLOBAL_CONSTRUCT

#include <stdio.h>

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#include "globlbin.h"
#endif
#include "commline.h"
#include "envrnmnt.h"
#include "globlbsc.h"
#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "globlcmp.h"
#endif
#include "globlcom.h"
#include "globlpsr.h"
#include "memalloc.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "multifld.h"
#include "prntutil.h"
#include "router.h"
#include "strngrtr.h"
#include "utility.h"

#include "globldef.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void *AllocateModule (Environment *);
static void ReturnModule (Environment *, void *);
static void ReturnDefglobal (Environment *, Defglobal *);
static void Initialize_DefglobalModules (Environment *);
static bool EntityGetDefglobalValue (Environment *, void *, UDFValue *);
static void IncrementDefglobalBusyCount (Environment *, Defglobal *);
static void DecrementDefglobalBusyCount (Environment *, Defglobal *);
static void DeallocateDefglobalData (Environment *);
static void DestroyDefglobalAction (Environment *, ConstructHeader *, void *);
#if (! BLOAD_ONLY)
static void DestroyDefglobal (Environment *, Defglobal *);
#endif
#if RUN_TIME
static void CL_RuntimeDefglobalAction (Environment *, ConstructHeader *,
				       void *);
#endif

/**************************************************************/
/* CL_InitializeDefglobals: Initializes the defglobal construct. */
/**************************************************************/
void
CL_InitializeDefglobals (Environment * theEnv)
{
  struct entityRecord globalInfo = { "GBL_VARIABLE", GBL_VARIABLE, 0, 0, 0,
    NULL,
    NULL,
    NULL,
    (Entity_EvaluationFunction *) EntityGetDefglobalValue,
    NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL
  };

  struct entityRecord defglobalPtrRecord =
    { "DEFGLOBAL_PTR", DEFGLOBAL_PTR, 0, 0, 0,
    NULL, NULL, NULL,
    (Entity_EvaluationFunction *) CL_QGetDefglobalUDFValue,
    NULL,
    (EntityBusyCountFunction *) DecrementDefglobalBusyCount,
    (EntityBusyCountFunction *) IncrementDefglobalBusyCount,
    NULL, NULL, NULL, NULL, NULL
  };

  CL_AllocateEnvironmentData (theEnv, DEFGLOBAL_DATA,
			      sizeof (struct defglobalData),
			      DeallocateDefglobalData);

  memcpy (&DefglobalData (theEnv)->GlobalInfo, &globalInfo,
	  sizeof (struct entityRecord));
  memcpy (&DefglobalData (theEnv)->DefglobalPtrRecord, &defglobalPtrRecord,
	  sizeof (struct entityRecord));

  DefglobalData (theEnv)->CL_ResetGlobals = true;
  DefglobalData (theEnv)->LastModuleIndex = -1;

  CL_InstallPrimitive (theEnv, &DefglobalData (theEnv)->GlobalInfo,
		       GBL_VARIABLE);
  CL_InstallPrimitive (theEnv, &DefglobalData (theEnv)->GlobalInfo,
		       MF_GBL_VARIABLE);
  CL_InstallPrimitive (theEnv, &DefglobalData (theEnv)->DefglobalPtrRecord,
		       DEFGLOBAL_PTR);

  Initialize_DefglobalModules (theEnv);

  CL_DefglobalBasicCommands (theEnv);
  CL_DefglobalCommandDefinitions (theEnv);

  DefglobalData (theEnv)->DefglobalConstruct =
    CL_AddConstruct (theEnv, "defglobal", "defglobals", CL_ParseDefglobal,
		     (CL_FindConstructFunction *) CL_FindDefglobal,
		     CL_GetConstructNamePointer, CL_GetConstructPPFo_rm,
		     CL_GetConstructModuleItem,
		     (GetNextConstructFunction *) CL_GetNextDefglobal,
		     CL_SetNextConstruct,
		     (IsConstructDeletableFunction *) CL_DefglobalIsDeletable,
		     (DeleteConstructFunction *) CL_Undefglobal,
		     (FreeConstructFunction *) ReturnDefglobal);
}

/****************************************************/
/* DeallocateDefglobalData: Deallocates environment */
/*    data for the defglobal construct.             */
/****************************************************/
static void
DeallocateDefglobalData (Environment * theEnv)
{
#if ! RUN_TIME
  struct defglobalModule *theModuleItem;
  Defmodule *theModule;

#if BLOAD || BLOAD_AND_BSAVE
  if (CL_Bloaded (theEnv))
    return;
#endif

  CL_DoForAllConstructs (theEnv, DestroyDefglobalAction,
			 DefglobalData (theEnv)->CL_DefglobalModuleIndex,
			 false, NULL);

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      theModuleItem = (struct defglobalModule *)
	CL_GetModuleItem (theEnv, theModule,
			  DefglobalData (theEnv)->CL_DefglobalModuleIndex);
      rtn_struct (theEnv, defglobalModule, theModuleItem);
    }
#else
  CL_DoForAllConstructs (theEnv, DestroyDefglobalAction,
			 DefglobalData (theEnv)->CL_DefglobalModuleIndex,
			 false, NULL);
#endif
}

/***************************************************/
/* DestroyDefglobalAction: Action used to remove   */
/*   defglobals as a result of CL_DestroyEnvironment. */
/***************************************************/
static void
DestroyDefglobalAction (Environment * theEnv,
			ConstructHeader * theConstruct, void *buffer)
{
#if MAC_XCD
#pragma unused(buffer)
#endif
#if (! BLOAD_ONLY)
  Defglobal *theDefglobal = (Defglobal *) theConstruct;

  if (theDefglobal == NULL)
    return;

  DestroyDefglobal (theEnv, theDefglobal);
#else
#if MAC_XCD
#pragma unused(theEnv,theConstruct)
#endif
#endif
}

/*********************************************************/
/* Initialize_DefglobalModules: Initializes the defglobal */
/*   construct for use with the defmodule construct.     */
/*********************************************************/
static void
Initialize_DefglobalModules (Environment * theEnv)
{
  DefglobalData (theEnv)->CL_DefglobalModuleIndex =
    CL_RegisterModuleItem (theEnv, "defglobal", AllocateModule, ReturnModule,
#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
			   CL_Bload_DefglobalModuleReference,
#else
			   NULL,
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
			   CL_DefglobalCModuleReference,
#else
			   NULL,
#endif
			   (CL_FindConstructFunction *)
			   CL_FindDefglobalInModule);

#if (! BLOAD_ONLY) && (! RUN_TIME) && DEFMODULE_CONSTRUCT
  CL_AddPortConstructItem (theEnv, "defglobal", SYMBOL_TOKEN);
#endif
}

/*************************************************/
/* AllocateModule: Allocates a defglobal module. */
/*************************************************/
static void *
AllocateModule (Environment * theEnv)
{
  return (void *) get_struct (theEnv, defglobalModule);
}

/*************************************************/
/* ReturnModule: Deallocates a defglobal module. */
/*************************************************/
static void
ReturnModule (Environment * theEnv, void *theItem)
{
  CL_FreeConstructHeaderModule (theEnv,
				(struct defmoduleItemHeader *) theItem,
				DefglobalData (theEnv)->DefglobalConstruct);
  rtn_struct (theEnv, defglobalModule, theItem);
}

/**************************************************************/
/* Get_DefglobalModuleItem: Returns a pointer to the defmodule */
/*  item for the specified defglobal or defmodule.            */
/**************************************************************/
struct defglobalModule *
Get_DefglobalModuleItem (Environment * theEnv, Defmodule * theModule)
{
  return ((struct defglobalModule *)
	  CL_GetConstructModuleItemByIndex (theEnv, theModule,
					    DefglobalData (theEnv)->
					    CL_DefglobalModuleIndex));
}

/**************************************************/
/* CL_FindDefglobal: Searches for a defglobal in the */
/*   list of defglobals. Returns a pointer to the */
/*   defglobal if found, otherwise NULL.          */
/**************************************************/
Defglobal *
CL_FindDefglobal (Environment * theEnv, const char *defglobalName)
{
  return (Defglobal *) CL_FindNamedConstructInModuleOrImports (theEnv,
							       defglobalName,
							       DefglobalData
							       (theEnv)->
							       DefglobalConstruct);
}

/******************************************************/
/* CL_FindDefglobalInModule: Searches for a defglobal in */
/*   the list of defglobals. Returns a pointer to the */
/*   defglobal if found, otherwise NULL.              */
/******************************************************/
Defglobal *
CL_FindDefglobalInModule (Environment * theEnv, const char *defglobalName)
{
  return (Defglobal *) CL_FindNamedConstructInModule (theEnv, defglobalName,
						      DefglobalData (theEnv)->
						      DefglobalConstruct);
}

/*****************************************************************/
/* CL_GetNextDefglobal: If passed a NULL pointer, returns the first */
/*   defglobal in the defglobal list. Otherwise returns the next */
/*   defglobal following the defglobal passed as an argument.    */
/*****************************************************************/
Defglobal *
CL_GetNextDefglobal (Environment * theEnv, Defglobal * defglobalPtr)
{
  return (Defglobal *) CL_GetNextConstructItem (theEnv, &defglobalPtr->header,
						DefglobalData (theEnv)->
						CL_DefglobalModuleIndex);
}

/********************************************************/
/* CL_DefglobalIsDeletable: Returns true if a particular   */
/*   defglobal can be deleted, otherwise returns false. */
/********************************************************/
bool
CL_DefglobalIsDeletable (Defglobal * theDefglobal)
{
  Environment *theEnv = theDefglobal->header.env;

  if (!CL_ConstructsDeletable (theEnv))
    {
      return false;
    }

  if (theDefglobal->busyCount)
    return false;

  return true;
}

/************************************************************/
/* ReturnDefglobal: Returns the data structures associated  */
/*   with a defglobal construct to the pool of free memory. */
/************************************************************/
static void
ReturnDefglobal (Environment * theEnv, Defglobal * theDefglobal)
{
#if (! BLOAD_ONLY) && (! RUN_TIME)
  if (theDefglobal == NULL)
    return;

   /*====================================*/
  /* Return the global's current value. */
   /*====================================*/

  CL_Release (theEnv, theDefglobal->current.header);
  if (theDefglobal->current.header->type == MULTIFIELD_TYPE)
    {
      if (theDefglobal->current.multifieldValue->busyCount == 0)
	{
	  CL_ReturnMultifield (theEnv, theDefglobal->current.multifieldValue);
	}
      else
	{
	  CL_AddToMultifieldList (theEnv,
				  theDefglobal->current.multifieldValue);
	}
    }

   /*================================================*/
  /* Return the expression representing the initial */
  /* value of the defglobal when it was defined.    */
   /*================================================*/

  CL_RemoveHashedExpression (theEnv, theDefglobal->initial);

   /*===============================*/
  /* CL_Release items stored in the   */
  /* defglobal's construct header. */
   /*===============================*/

  CL_DeinstallConstructHeader (theEnv, &theDefglobal->header);

   /*======================================*/
  /* Return the defglobal data structure. */
   /*======================================*/

  rtn_struct (theEnv, defglobal, theDefglobal);

   /*===========================================*/
  /* Set the variable indicating that a change */
  /* has been made to a global variable.       */
   /*===========================================*/

  DefglobalData (theEnv)->ChangeToGlobals = true;
#endif
}

/************************************************************/
/* DestroyDefglobal: Returns the data structures associated  */
/*   with a defglobal construct to the pool of free memory. */
/************************************************************/
#if (! BLOAD_ONLY)
static void
DestroyDefglobal (Environment * theEnv, Defglobal * theDefglobal)
{
  if (theDefglobal == NULL)
    return;

   /*====================================*/
  /* Return the global's current value. */
   /*====================================*/

  if (theDefglobal->current.header->type == MULTIFIELD_TYPE)
    {
      if (theDefglobal->current.multifieldValue->busyCount == 0)
	{
	  CL_ReturnMultifield (theEnv, theDefglobal->current.multifieldValue);
	}
      else
	{
	  CL_AddToMultifieldList (theEnv,
				  theDefglobal->current.multifieldValue);
	}
    }

#if (! RUN_TIME)

   /*===============================*/
  /* CL_Release items stored in the   */
  /* defglobal's construct header. */
   /*===============================*/

  CL_DeinstallConstructHeader (theEnv, &theDefglobal->header);

   /*======================================*/
  /* Return the defglobal data structure. */
   /*======================================*/

  rtn_struct (theEnv, defglobal, theDefglobal);
#endif
}
#endif

/************************************************/
/* CL_QSetDefglobalValue: Lowest level routine for */
/*   setting a defglobal's value.               */
/************************************************/
void
CL_QSetDefglobalValue (Environment * theEnv,
		       Defglobal * theGlobal, UDFValue * vPtr, bool resetVar)
{
   /*====================================================*/
  /* If the new value passed for the defglobal is NULL, */
  /* then reset the defglobal to the initial value it   */
  /* had when it was defined.                           */
   /*====================================================*/

  if (resetVar)
    {
      CL_EvaluateExpression (theEnv, theGlobal->initial, vPtr);
      if (CL_EvaluationData (theEnv)->CL_EvaluationError)
	{
	  vPtr->value = FalseSymbol (theEnv);
	}
    }

   /*==========================================*/
  /* If globals are being watch, then display */
  /* the change to the global variable.       */
   /*==========================================*/

#if DEBUGGING_FUNCTIONS
  if (theGlobal->watch &&
      (!ConstructData (theEnv)->CL_ClearReadyInProgress) &&
      (!ConstructData (theEnv)->CL_ClearInProgress))
    {
      CL_WriteString (theEnv, STDOUT, ":== ?*");
      CL_WriteString (theEnv, STDOUT, theGlobal->header.name->contents);
      CL_WriteString (theEnv, STDOUT, "* ==> ");
      CL_WriteUDFValue (theEnv, STDOUT, vPtr);
      CL_WriteString (theEnv, STDOUT, " <== ");
      CL_WriteCLIPSValue (theEnv, STDOUT, &theGlobal->current);
      CL_WriteString (theEnv, STDOUT, "\n");
    }
#endif

   /*==============================================*/
  /* Remove the old value of the global variable. */
   /*==============================================*/

  CL_Release (theEnv, theGlobal->current.header);
  if (theGlobal->current.header->type == MULTIFIELD_TYPE)
    {
      if (theGlobal->current.multifieldValue->busyCount == 0)
	{
	  CL_ReturnMultifield (theEnv, theGlobal->current.multifieldValue);
	}
      else
	{
	  CL_AddToMultifieldList (theEnv, theGlobal->current.multifieldValue);
	}
    }

   /*===========================================*/
  /* Set the new value of the global variable. */
   /*===========================================*/

  if (vPtr->header->type != MULTIFIELD_TYPE)
    {
      theGlobal->current.value = vPtr->value;
    }
  else
    {
      theGlobal->current.value =
	CL_CopyMultifield (theEnv, vPtr->multifieldValue);
    }
  CL_Retain (theEnv, theGlobal->current.header);

   /*===========================================*/
  /* Set the variable indicating that a change */
  /* has been made to a global variable.       */
   /*===========================================*/

  DefglobalData (theEnv)->ChangeToGlobals = true;

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_CleanCurrentGarbageFrame (theEnv, NULL);
      CL_CallPeriodicTasks (theEnv);
    }
}

/**************************************************************/
/* Q_FindDefglobal: Searches for a defglobal in the list of    */
/*   defglobals. Returns a pointer to the defglobal if found, */
/*   otherwise NULL.                                          */
/**************************************************************/
Defglobal *
Q_FindDefglobal (Environment * theEnv, CLIPSLexeme * defglobalName)
{
  Defglobal *theDefglobal;

  for (theDefglobal = CL_GetNextDefglobal (theEnv, NULL);
       theDefglobal != NULL;
       theDefglobal = CL_GetNextDefglobal (theEnv, theDefglobal))
    {
      if (defglobalName == theDefglobal->header.name)
	return theDefglobal;
    }

  return NULL;
}

/*******************************************************************/
/* CL_DefglobalValueFo_rm: Returns the pretty print representation of  */
/*   the current value of the specified defglobal. For example, if */
/*   the current value of ?*x* is 5, the string "?*x* = 5" would   */
/*   be returned.                                                  */
/*******************************************************************/
void
CL_DefglobalValueFo_rm (Defglobal * theGlobal, String_Builder * theSB)
{
  Environment *theEnv = theGlobal->header.env;

  OpenString_BuilderDestination (theEnv, "GlobalValueFo_rm", theSB);
  CL_WriteString (theEnv, "GlobalValueFo_rm", "?*");
  CL_WriteString (theEnv, "GlobalValueFo_rm",
		  theGlobal->header.name->contents);
  CL_WriteString (theEnv, "GlobalValueFo_rm", "* = ");
  CL_WriteCLIPSValue (theEnv, "GlobalValueFo_rm", &theGlobal->current);
  CloseString_BuilderDestination (theEnv, "GlobalValueFo_rm");
}

/*********************************************************/
/* CL_GetGlobalsChanged: Returns the defglobal change flag. */
/*********************************************************/
bool
CL_GetGlobalsChanged (Environment * theEnv)
{
  return DefglobalData (theEnv)->ChangeToGlobals;
}

/******************************************************/
/* CL_SetGlobalsChanged: Sets the defglobal change flag. */
/******************************************************/
void
CL_SetGlobalsChanged (Environment * theEnv, bool value)
{
  DefglobalData (theEnv)->ChangeToGlobals = value;
}

/*********************************************************/
/* EntityGetDefglobalValue: Returns the value of the     */
/*   specified global variable in the supplied UDFValue. */
/*********************************************************/
static bool
EntityGetDefglobalValue (Environment * theEnv,
			 void *theValue, UDFValue * vPtr)
{
  Defglobal *theGlobal;
  unsigned int count;

   /*===========================================*/
  /* Search for the specified defglobal in the */
  /* modules visible to the current module.    */
   /*===========================================*/

  theGlobal = (Defglobal *)
    CL_FindImportedConstruct (theEnv, "defglobal", NULL,
			      ((CLIPSLexeme *) theValue)->contents, &count,
			      true, NULL);

   /*=============================================*/
  /* If it wasn't found, print an error message. */
   /*=============================================*/

  if (theGlobal == NULL)
    {
      CL_PrintErrorID (theEnv, "GLOBLDEF", 1, false);
      CL_WriteString (theEnv, STDERR, "Global variable ?*");
      CL_WriteString (theEnv, STDERR, ((CLIPSLexeme *) theValue)->contents);
      CL_WriteString (theEnv, STDERR, "* is unbound.\n");
      vPtr->value = FalseSymbol (theEnv);
      Set_EvaluationError (theEnv, true);
      return false;
    }

   /*========================================================*/
  /* The current implementation of the defmodules shouldn't */
  /* allow a construct to be defined which would cause an   */
  /* ambiguous reference, but we'll check for it anyway.    */
   /*========================================================*/

  if (count > 1)
    {
      CL_AmbiguousReferenceErrorMessage (theEnv, "defglobal",
					 ((CLIPSLexeme *) theValue)->
					 contents);
      vPtr->value = FalseSymbol (theEnv);
      Set_EvaluationError (theEnv, true);
      return false;
    }

   /*=================================*/
  /* Get the value of the defglobal. */
   /*=================================*/

  CL_CLIPSToUDFValue (&theGlobal->current, vPtr);

  return true;
}

/******************************************************************/
/* CL_QGetDefglobalUDFValue: Returns the value of a global variable. */
/******************************************************************/
bool
CL_QGetDefglobalUDFValue (Environment * theEnv,
			  Defglobal * theGlobal, UDFValue * vPtr)
{
  vPtr->value = theGlobal->current.value;

   /*===========================================================*/
  /* If the global contains a multifield value, return a copy  */
  /* of the value so that routines which use this value are    */
  /* not affected if the value of the global is later changed. */
   /*===========================================================*/

  if (theGlobal->current.header->type == MULTIFIELD_TYPE)
    {
      vPtr->begin = 0;
      vPtr->range = theGlobal->current.multifieldValue->length;
    }

  return true;
}

/*********************************************************/
/* CL_DefglobalGetValue: Returns the value of the specified */
/*   global variable in the supplied UDFValue.           */
/*********************************************************/
void
CL_DefglobalGetValue (Defglobal * theDefglobal, CLIPSValue * vPtr)
{
  vPtr->value = theDefglobal->current.value;
}

/*************************************************************/
/* CL_DefglobalSetValue: Sets the value of the specified global */
/*   variable to the value stored in the supplied UDFValue.  */
/*************************************************************/
void
CL_DefglobalSetValue (Defglobal * theDefglobal, CLIPSValue * vPtr)
{
  UDFValue temp;
  GCBlock gcb;
  Environment *theEnv = theDefglobal->header.env;

   /*=====================================*/
  /* If embedded, clear the error flags. */
   /*=====================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_ResetErrorFlags (theEnv);
    }

  CL_GCBlockStart (theEnv, &gcb);
  CL_CLIPSToUDFValue (vPtr, &temp);
  CL_QSetDefglobalValue (theEnv, theDefglobal, &temp, false);
  CL_GCBlockEnd (theEnv, &gcb);
}

/************************/
/* CL_DefglobalSetInteger: */
/************************/
void
CL_DefglobalSetInteger (Defglobal * theDefglobal, long long value)
{
  CLIPSValue cv;

  cv.integerValue = CL_CreateInteger (theDefglobal->header.env, value);

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/**********************/
/* CL_DefglobalSetFloat: */
/**********************/
void
CL_DefglobalSetFloat (Defglobal * theDefglobal, double value)
{
  CLIPSValue cv;

  cv.floatValue = CL_CreateFloat (theDefglobal->header.env, value);

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/***********************/
/* CL_DefglobalSetSymbol: */
/***********************/
void
CL_DefglobalSetSymbol (Defglobal * theDefglobal, const char *value)
{
  CLIPSValue cv;

  cv.lexemeValue = CL_CreateSymbol (theDefglobal->header.env, value);

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/***********************/
/* CL_DefglobalSetString: */
/***********************/
void
CL_DefglobalSetString (Defglobal * theDefglobal, const char *value)
{
  CLIPSValue cv;

  cv.lexemeValue = CL_CreateString (theDefglobal->header.env, value);

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/*****************************/
/* CL_DefglobalSet_InstanceName: */
/*****************************/
void
CL_DefglobalSet_InstanceName (Defglobal * theDefglobal, const char *value)
{
  CLIPSValue cv;

  cv.lexemeValue = CL_Create_InstanceName (theDefglobal->header.env, value);

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/*****************************/
/* CL_DefglobalSetCLIPSInteger: */
/*****************************/
void
CL_DefglobalSetCLIPSInteger (Defglobal * theDefglobal, CLIPSInteger * value)
{
  CLIPSValue cv;

  cv.integerValue = value;

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/***************************/
/* CL_DefglobalSetCLIPSFloat: */
/***************************/
void
CL_DefglobalSetCLIPSFloat (Defglobal * theDefglobal, CLIPSFloat * value)
{
  CLIPSValue cv;

  cv.floatValue = value;

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/****************************/
/* CL_DefglobalSetCLIPSLexeme: */
/****************************/
void
CL_DefglobalSetCLIPSLexeme (Defglobal * theDefglobal, CLIPSLexeme * value)
{
  CLIPSValue cv;

  cv.lexemeValue = value;

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/*********************/
/* CL_DefglobalSetFact: */
/*********************/
void
CL_DefglobalSetFact (Defglobal * theDefglobal, Fact * value)
{
  CLIPSValue cv;

  cv.factValue = value;

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/*************************/
/* CL_DefglobalSetInstance: */
/*************************/
void
CL_DefglobalSetInstance (Defglobal * theDefglobal, Instance * value)
{
  CLIPSValue cv;

  cv.instanceValue = value;

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/***************************/
/* CL_DefglobalSetMultifield: */
/***************************/
void
CL_DefglobalSetMultifield (Defglobal * theDefglobal, Multifield * value)
{
  CLIPSValue cv;

  cv.multifieldValue = value;

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/*************************************/
/* CL_DefglobalSetCLIPSExternalAddress: */
/*************************************/
void
CL_DefglobalSetCLIPSExternalAddress (Defglobal * theDefglobal,
				     CLIPSExternalAddress * value)
{
  CLIPSValue cv;

  cv.externalAddressValue = value;

  CL_DefglobalSetValue (theDefglobal, &cv);
}

/**********************************************************/
/* DecrementDefglobalBusyCount: Decrements the busy count */
/*   of a defglobal data structure.                       */
/**********************************************************/
static void
DecrementDefglobalBusyCount (Environment * theEnv, Defglobal * theGlobal)
{
  if (!ConstructData (theEnv)->CL_ClearInProgress)
    theGlobal->busyCount--;
}

/**********************************************************/
/* IncrementDefglobalBusyCount: Increments the busy count */
/*   of a defglobal data structure.                       */
/**********************************************************/
static void
IncrementDefglobalBusyCount (Environment * theEnv, Defglobal * theGlobal)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif

  theGlobal->busyCount++;
}

/***********************************************************************/
/* CL_UpdateDefglobalScope: Updates the scope flag of all the defglobals. */
/***********************************************************************/
void
CL_UpdateDefglobalScope (Environment * theEnv)
{
  Defglobal *theDefglobal;
  unsigned int moduleCount;
  Defmodule *theModule;
  struct defmoduleItemHeader *theItem;

   /*============================*/
  /* Loop through every module. */
   /*============================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      /*============================================================*/
      /* Loop through every defglobal in the module being examined. */
      /*============================================================*/

      theItem = (struct defmoduleItemHeader *)
	CL_GetModuleItem (theEnv, theModule,
			  DefglobalData (theEnv)->CL_DefglobalModuleIndex);

      for (theDefglobal = (Defglobal *) theItem->firstItem;
	   theDefglobal != NULL;
	   theDefglobal = CL_GetNextDefglobal (theEnv, theDefglobal))
	{
	 /*====================================================*/
	  /* If the defglobal is visible to the current module, */
	  /* then mark it as being in scope, otherwise mark it  */
	  /* as being out of scope.                             */
	 /*====================================================*/

	  if (CL_FindImportedConstruct (theEnv, "defglobal", theModule,
					theDefglobal->header.name->contents,
					&moduleCount, true, NULL) != NULL)
	    {
	      theDefglobal->inScope = true;
	    }
	  else
	    {
	      theDefglobal->inScope = false;
	    }
	}
    }
}

/*******************************************************/
/* CL_GetNextDefglobalInScope: Returns the next defglobal */
/*   that is scope of the current module. Works in a   */
/*   similar fashion to CL_GetNextDefglobal, but skips    */
/*   defglobals that are out of scope.                 */
/*******************************************************/
Defglobal *
CL_GetNextDefglobalInScope (Environment * theEnv, Defglobal * theGlobal)
{
  struct defmoduleItemHeader *theItem;

   /*=======================================*/
  /* If we're beginning the search for the */
  /* first defglobal in scope, then ...    */
   /*=======================================*/

  if (theGlobal == NULL)
    {
      /*==============================================*/
      /* If the current module has been changed since */
      /* the last time the scopes were computed, then */
      /* recompute the scopes.                        */
      /*==============================================*/

      if (DefglobalData (theEnv)->LastModuleIndex !=
	  DefmoduleData (theEnv)->ModuleChangeIndex)
	{
	  CL_UpdateDefglobalScope (theEnv);
	  DefglobalData (theEnv)->LastModuleIndex =
	    DefmoduleData (theEnv)->ModuleChangeIndex;
	}

      /*==========================================*/
      /* Get the first module and first defglobal */
      /* to start the search with.                */
      /*==========================================*/

      DefglobalData (theEnv)->TheDefmodule =
	CL_GetNextDefmodule (theEnv, NULL);
      theItem =
	(struct defmoduleItemHeader *) CL_GetModuleItem (theEnv,
							 DefglobalData
							 (theEnv)->
							 TheDefmodule,
							 DefglobalData
							 (theEnv)->
							 CL_DefglobalModuleIndex);
      theGlobal = (Defglobal *) theItem->firstItem;
    }

   /*==================================================*/
  /* Otherwise, see if the last defglobal returned by */
  /* this function has a defglobal following it.      */
   /*==================================================*/

  else
    {
      theGlobal = CL_GetNextDefglobal (theEnv, theGlobal);
    }

   /*======================================*/
  /* Continue looping through the modules */
  /* until a defglobal in scope is found. */
   /*======================================*/

  while (DefglobalData (theEnv)->TheDefmodule != NULL)
    {
      /*=====================================================*/
      /* Loop through the defglobals in the module currently */
      /* being examined to see if one is in scope.           */
      /*=====================================================*/

      for (;
	   theGlobal != NULL;
	   theGlobal = CL_GetNextDefglobal (theEnv, theGlobal))
	{
	  if (theGlobal->inScope)
	    return theGlobal;
	}

      /*================================================*/
      /* If a global in scope couldn't be found in this */
      /* module, then move on to the next module.       */
      /*================================================*/

      DefglobalData (theEnv)->TheDefmodule =
	CL_GetNextDefmodule (theEnv, DefglobalData (theEnv)->TheDefmodule);
      theItem =
	(struct defmoduleItemHeader *) CL_GetModuleItem (theEnv,
							 DefglobalData
							 (theEnv)->
							 TheDefmodule,
							 DefglobalData
							 (theEnv)->
							 CL_DefglobalModuleIndex);
      theGlobal = (Defglobal *) theItem->firstItem;
    }

   /*====================================*/
  /* All the globals in scope have been */
  /* traversed and there are none left. */
   /*====================================*/

  return NULL;
}

#if RUN_TIME

/************************************************/
/* CL_RuntimeDefglobalAction: Action to be applied */
/*   to each defglobal construct when a runtime */
/*   initialization occurs.                     */
/************************************************/
static void
CL_RuntimeDefglobalAction (Environment * theEnv,
			   ConstructHeader * theConstruct, void *buffer)
{
#if MAC_XCD
#pragma unused(buffer)
#endif
  Defglobal *theDefglobal = (Defglobal *) theConstruct;

  theDefglobal->header.env = theEnv;
  theDefglobal->current.value = VoidConstant (theEnv);
}

/*******************************/
/* Defglobal_RunTimeInitialize: */
/*******************************/
void
Defglobal_RunTimeInitialize (Environment * theEnv)
{
  CL_DoForAllConstructs (theEnv, CL_RuntimeDefglobalAction,
			 DefglobalData (theEnv)->CL_DefglobalModuleIndex,
			 true, NULL);
}

#endif

/*##################################*/
/* Additional Environment Functions */
/*##################################*/

const char *
CL_DefglobalModule (Defglobal * theDefglobal)
{
  return CL_GetConstructModuleName (&theDefglobal->header);
}

const char *
CL_DefglobalName (Defglobal * theDefglobal)
{
  return CL_GetConstructNameString (&theDefglobal->header);
}

const char *
CL_DefglobalPPFo_rm (Defglobal * theDefglobal)
{
  return CL_GetConstructPPFo_rm (&theDefglobal->header);
}

#endif /* DEFGLOBAL_CONSTRUCT */
