   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/02/18             */
   /*                                                     */
   /*         DEFMODULE BASIC COMMANDS HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the defmodule       */
/*   construct such as clear, reset, save, ppdefmodule       */
/*   list-defmodules, and get-defmodule-list.                */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "bload.h"
#include "constrct.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "modulbin.h"
#include "modulcmp.h"
#include "multifld.h"
#include "prntutil.h"
#include "router.h"

#include "modulbsc.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void CL_ClearDefmodules (Environment *, void *);
#if DEFMODULE_CONSTRUCT
static void CL_SaveDefmodules (Environment *, Defmodule *, const char *,
			       void *);
#endif

/*****************************************************************/
/* CL_DefmoduleBasicCommands: Initializes basic defmodule commands. */
/*****************************************************************/
void
CL_DefmoduleBasicCommands (Environment * theEnv)
{
  CL_Add_ClearFunction (theEnv, "defmodule", CL_ClearDefmodules, 2000, NULL);

#if DEFMODULE_CONSTRUCT
  CL_Add_SaveFunction (theEnv, "defmodule", CL_SaveDefmodules, 1100, NULL);

#if ! RUN_TIME
  CL_AddUDF (theEnv, "get-defmodule-list", "m", 0, 0, NULL,
	     CL_GetDefmoduleListFunction, "CL_GetDefmoduleListFunction",
	     NULL);

#if DEBUGGING_FUNCTIONS
  CL_AddUDF (theEnv, "list-defmodules", "v", 0, 0, NULL,
	     CL_ListDefmodulesCommand, "CL_ListDefmodulesCommand", NULL);
  CL_AddUDF (theEnv, "ppdefmodule", "v", 1, 2, ";y;ldsyn",
	     CL_PPDefmoduleCommand, "CL_PPDefmoduleCommand", NULL);
#endif
#endif
#endif

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
  CL_DefmoduleBinarySetup (theEnv);
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
  CL_DefmoduleCompilerSetup (theEnv);
#endif
}

/*********************************************************/
/* CL_ClearDefmodules: Defmodule clear routine for use with */
/*   the clear command. Creates the MAIN module.         */
/*********************************************************/
static void
CL_ClearDefmodules (Environment * theEnv, void *context)
{
#if (BLOAD || BLOAD_AND_BSAVE || BLOAD_ONLY) && (! RUN_TIME)
  if (CL_Bloaded (theEnv) == true)
    return;
#endif
#if (! RUN_TIME)
  CL_RemoveAllDefmodules (theEnv, NULL);

  CL_CreateMainModule (theEnv, NULL);
  DefmoduleData (theEnv)->MainModuleRedefinable = true;
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
}

#if DEFMODULE_CONSTRUCT

/******************************************/
/* CL_SaveDefmodules: Defmodule save routine */
/*   for use with the save command.       */
/******************************************/
static void
CL_SaveDefmodules (Environment * theEnv,
		   Defmodule * theModule,
		   const char *logicalName, void *context)
{
  const char *ppfo_rm;

  ppfo_rm = CL_DefmodulePPFo_rm (theModule);
  if (ppfo_rm != NULL)
    {
      CL_WriteString (theEnv, logicalName, ppfo_rm);
      CL_WriteString (theEnv, logicalName, "\n");
    }
}

/************************************************/
/* CL_GetDefmoduleListFunction: H/L access routine */
/*   for the get-defmodule-list function.       */
/************************************************/
void
CL_GetDefmoduleListFunction (Environment * theEnv,
			     UDFContext * context, UDFValue * returnValue)
{
  CLIPSValue result;

  CL_GetDefmoduleList (theEnv, &result);
  CL_CLIPSToUDFValue (&result, returnValue);
}

/******************************************/
/* CL_GetDefmoduleList: C access routine     */
/*   for the get-defmodule-list function. */
/******************************************/
void
CL_GetDefmoduleList (Environment * theEnv, CLIPSValue * returnValue)
{
  Defmodule *theConstruct;
  unsigned long count = 0;
  Multifield *theList;

   /*====================================*/
  /* Dete_rmine the number of constructs */
  /* of the specified type.             */
   /*====================================*/

  for (theConstruct = CL_GetNextDefmodule (theEnv, NULL);
       theConstruct != NULL;
       theConstruct = CL_GetNextDefmodule (theEnv, theConstruct))
    {
      count++;
    }

   /*===========================*/
  /* Create a multifield large */
  /* enough to store the list. */
   /*===========================*/

  theList = CL_CreateMultifield (theEnv, count);
  returnValue->value = theList;

   /*====================================*/
  /* Store the names in the multifield. */
   /*====================================*/

  for (theConstruct = CL_GetNextDefmodule (theEnv, NULL), count = 0;
       theConstruct != NULL;
       theConstruct = CL_GetNextDefmodule (theEnv, theConstruct), count++)
    {
      if (CL_EvaluationData (theEnv)->CL_HaltExecution == true)
	{
	  returnValue->multifieldValue = CL_CreateMultifield (theEnv, 0L);
	  return;
	}
      theList->contents[count].lexemeValue =
	CL_CreateSymbol (theEnv, CL_DefmoduleName (theConstruct));
    }
}

#if DEBUGGING_FUNCTIONS

/********************************************/
/* CL_PPDefmoduleCommand: H/L access routine   */
/*   for the ppdefmodule command.           */
/********************************************/
void
CL_PPDefmoduleCommand (Environment * theEnv,
		       UDFContext * context, UDFValue * returnValue)
{
  const char *defmoduleName;
  const char *logicalName;
  const char *ppFo_rm;

  defmoduleName =
    CL_GetConstructName (context, "ppdefmodule", "defmodule name");
  if (defmoduleName == NULL)
    return;

  if (UDFHasNextArgument (context))
    {
      logicalName = CL_GetLogicalName (context, STDOUT);
      if (logicalName == NULL)
	{
	  CL_IllegalLogicalNameMessage (theEnv, "ppdefmodule");
	  Set_HaltExecution (theEnv, true);
	  Set_EvaluationError (theEnv, true);
	  return;
	}
    }
  else
    {
      logicalName = STDOUT;
    }

  if (strcmp (logicalName, "nil") == 0)
    {
      ppFo_rm = CL_PPDefmoduleNil (theEnv, defmoduleName);

      if (ppFo_rm == NULL)
	{
	  CL_CantFindItemErrorMessage (theEnv, "defmodule", defmoduleName,
				       true);
	}

      returnValue->lexemeValue = CL_CreateString (theEnv, ppFo_rm);

      return;
    }

  CL_PPDefmodule (theEnv, defmoduleName, logicalName);

  return;
}

/****************************************/
/* CL_PPDefmoduleNil: C access routine for */
/*   the ppdefmodule command.           */
/****************************************/
const char *
CL_PPDefmoduleNil (Environment * theEnv, const char *defmoduleName)
{
  Defmodule *defmodulePtr;

  defmodulePtr = CL_FindDefmodule (theEnv, defmoduleName);
  if (defmodulePtr == NULL)
    {
      CL_CantFindItemErrorMessage (theEnv, "defmodule", defmoduleName, true);
      return NULL;
    }

  if (CL_DefmodulePPFo_rm (defmodulePtr) == NULL)
    return "";

  return CL_DefmodulePPFo_rm (defmodulePtr);
}

/*************************************/
/* CL_PPDefmodule: C access routine for */
/*   the ppdefmodule command.        */
/*************************************/
bool
CL_PPDefmodule (Environment * theEnv,
		const char *defmoduleName, const char *logicalName)
{
  Defmodule *defmodulePtr;

  defmodulePtr = CL_FindDefmodule (theEnv, defmoduleName);
  if (defmodulePtr == NULL)
    {
      CL_CantFindItemErrorMessage (theEnv, "defmodule", defmoduleName, true);
      return false;
    }

  if (CL_DefmodulePPFo_rm (defmodulePtr) == NULL)
    return true;
  CL_WriteString (theEnv, logicalName, CL_DefmodulePPFo_rm (defmodulePtr));

  return true;
}

/***********************************************/
/* CL_ListDefmodulesCommand: H/L access routine   */
/*   for the list-defmodules command.          */
/***********************************************/
void
CL_ListDefmodulesCommand (Environment * theEnv,
			  UDFContext * context, UDFValue * returnValue)
{
  CL_ListDefmodules (theEnv, STDOUT);
}

/**************************************/
/* CL_ListDefmodules: C access routine   */
/*   for the list-defmodules command. */
/**************************************/
void
CL_ListDefmodules (Environment * theEnv, const char *logicalName)
{
  Defmodule *theModule;
  unsigned int count = 0;

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      CL_WriteString (theEnv, logicalName, CL_DefmoduleName (theModule));
      CL_WriteString (theEnv, logicalName, "\n");
      count++;
    }

  CL_PrintTally (theEnv, logicalName, count, "defmodule", "defmodules");
}

#endif /* DEBUGGING_FUNCTIONS */

#endif /* DEFMODULE_CONSTRUCT */
