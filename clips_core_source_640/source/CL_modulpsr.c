   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/01/16             */
   /*                                                     */
   /*              DEFMODULE PARSER MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses a defmodule construct.                    */
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
/*      6.30: CL_GetConstructNameAndComment API change.         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed linkage issue when DEFMODULE_CONSTRUCT   */
/*            compiler flag is set to 0.                     */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Callbacks must be environment aware.           */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFMODULE_CONSTRUCT && (! RUN_TIME) && (! BLOAD_ONLY)

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "constant.h"
#include "constrct.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "utility.h"

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#include "modulpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool ParsePortSpecifications (Environment *,
				     const char *, struct token *,
				     Defmodule *);
static bool ParseImportSpec (Environment *, const char *, struct token *,
			     Defmodule *);
static bool ParseExportSpec (Environment *, const char *, struct token *,
			     Defmodule *, Defmodule *);
static bool DeleteDefmodule (Defmodule *, Environment *);
static bool FindMultiImportConflict (Environment *, Defmodule *);
static void NotExportedErrorMessage (Environment *, const char *,
				     const char *, const char *);

/******************************************/
/* CL_SetNumberOfDefmodules: Sets the number */
/*   of defmodules currently defined.     */
/******************************************/
void
CL_SetNumberOfDefmodules (Environment * theEnv, unsigned short value)
{
  DefmoduleData (theEnv)->NumberOfDefmodules = value;
}

/****************************************************/
/* CL_AddAfterModuleChangeFunction: Adds a function to */
/*   the list of functions that are to be called    */
/*   after a module change occurs.                  */
/****************************************************/
void
CL_AddAfterModuleDefinedFunction (Environment * theEnv,
				  const char *name,
				  Void_CallFunction * func,
				  int priority, void *context)
{
  DefmoduleData (theEnv)->AfterModuleDefinedFunctions =
    CL_Add_VoidFunctionToCallList (theEnv, name, priority, func,
				   DefmoduleData
				   (theEnv)->AfterModuleDefinedFunctions,
				   context);
}

/******************************************************/
/* CL_AddPortConstructItem: Adds an item to the list of  */
/*   items that can be imported/exported by a module. */
/******************************************************/
void
CL_AddPortConstructItem (Environment * theEnv,
			 const char *theName, TokenType theType)
{
  struct portConstructItem *newItem;

  newItem = get_struct (theEnv, portConstructItem);
  newItem->constructName = theName;
  newItem->typeExpected = theType;
  newItem->next = DefmoduleData (theEnv)->ListOfPortConstructItems;
  DefmoduleData (theEnv)->ListOfPortConstructItems = newItem;
}

/******************************************************/
/* CL_ParseDefmodule: Coordinates all actions necessary  */
/*   for the parsing and creation of a defmodule into */
/*   the current environment.                         */
/******************************************************/
bool
CL_ParseDefmodule (Environment * theEnv, const char *readSource)
{
  CLIPSLexeme *defmoduleName;
  Defmodule *newDefmodule;
  struct token inputToken;
  unsigned int i;
  struct moduleItem *theItem;
  struct portItem *portSpecs, *nextSpec;
  struct defmoduleItemHeader *theHeader;
  struct void_CallFunctionItem *defineFunctions;
  Defmodule *redefiningMainModule = NULL;
  bool parseError;
  struct portItem *oldImportList = NULL, *oldExportList = NULL;
  bool overwrite = false;

   /*================================================*/
  /* Flush the buffer which stores the pretty print */
  /* representation for a module.  Add the already  */
  /* parsed keyword defmodule to this buffer.       */
   /*================================================*/

  CL_SetPPBufferStatus (theEnv, true);
  CL_FlushPPBuffer (theEnv);
  CL_SetIndentDepth (theEnv, 3);
  CL_SavePPBuffer (theEnv, "(defmodule ");

   /*===============================*/
  /* Modules cannot be loaded when */
  /* a binary load is in effect.   */
   /*===============================*/

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
  if ((CL_Bloaded (theEnv) == true)
      && (!ConstructData (theEnv)->CL_CheckSyntaxMode))
    {
      Cannot_LoadWith_BloadMessage (theEnv, "defmodule");
      return true;
    }
#endif

   /*=====================================================*/
  /* Parse the name and comment fields of the defmodule. */
  /* Remove the defmodule if it already exists.          */
   /*=====================================================*/

  defmoduleName =
    CL_GetConstructNameAndComment (theEnv, readSource, &inputToken,
				   "defmodule",
				   (CL_FindConstructFunction *)
				   CL_FindDefmodule,
				   (DeleteConstructFunction *)
				   DeleteDefmodule, "+", true, true, false,
				   false);
  if (defmoduleName == NULL)
    {
      return true;
    }

  if (strcmp (defmoduleName->contents, "MAIN") == 0)
    {
      redefiningMainModule = CL_FindDefmodule (theEnv, "MAIN");
    }

   /*==============================================*/
  /* Create the defmodule structure if necessary. */
   /*==============================================*/

  if (redefiningMainModule == NULL)
    {
      newDefmodule = CL_FindDefmodule (theEnv, defmoduleName->contents);
      if (newDefmodule)
	{
	  overwrite = true;
	}
      else
	{
	  newDefmodule = get_struct (theEnv, defmodule);
	  newDefmodule->header.name = defmoduleName;
	  newDefmodule->header.whichModule = NULL;
	  newDefmodule->header.usrData = NULL;
	  newDefmodule->header.constructType = DEFMODULE;
	  newDefmodule->header.env = theEnv;
	  newDefmodule->header.next = NULL;
	}
    }
  else
    {
      overwrite = true;
      newDefmodule = redefiningMainModule;
    }

  if (overwrite)
    {
      oldImportList = newDefmodule->importList;
      oldExportList = newDefmodule->exportList;
    }

  newDefmodule->importList = NULL;
  newDefmodule->exportList = NULL;

   /*===================================*/
  /* Finish parsing the defmodule (its */
  /* import/export specifications).    */
   /*===================================*/

  parseError =
    ParsePortSpecifications (theEnv, readSource, &inputToken, newDefmodule);

   /*====================================*/
  /* Check for import/export conflicts. */
   /*====================================*/

  if (!parseError)
    parseError = FindMultiImportConflict (theEnv, newDefmodule);

   /*======================================================*/
  /* If an error occured in parsing or an import conflict */
  /* was detected, abort the definition of the defmodule. */
  /* If we're only checking syntax, then we want to exit  */
  /* at this point as well.                               */
   /*======================================================*/

  if (parseError || ConstructData (theEnv)->CL_CheckSyntaxMode)
    {
      while (newDefmodule->importList != NULL)
	{
	  nextSpec = newDefmodule->importList->next;
	  rtn_struct (theEnv, portItem, newDefmodule->importList);
	  newDefmodule->importList = nextSpec;
	}

      while (newDefmodule->exportList != NULL)
	{
	  nextSpec = newDefmodule->exportList->next;
	  rtn_struct (theEnv, portItem, newDefmodule->exportList);
	  newDefmodule->exportList = nextSpec;
	}

      if ((redefiningMainModule == NULL) && (!overwrite))
	{
	  rtn_struct (theEnv, defmodule, newDefmodule);
	}

      if (overwrite)
	{
	  newDefmodule->importList = oldImportList;
	  newDefmodule->exportList = oldExportList;
	}

      if (parseError)
	return true;
      return false;
    }

   /*===============================================*/
  /* Increment the symbol table counts for symbols */
  /* used in the defmodule data structures.        */
   /*===============================================*/

  if (redefiningMainModule == NULL)
    {
      IncrementLexemeCount (newDefmodule->header.name);
    }
  else
    {
      if ((newDefmodule->importList != NULL) ||
	  (newDefmodule->exportList != NULL))
	{
	  DefmoduleData (theEnv)->MainModuleRedefinable = false;
	}
    }

  for (portSpecs = newDefmodule->importList; portSpecs != NULL;
       portSpecs = portSpecs->next)
    {
      if (portSpecs->moduleName != NULL)
	IncrementLexemeCount (portSpecs->moduleName);
      if (portSpecs->constructType != NULL)
	IncrementLexemeCount (portSpecs->constructType);
      if (portSpecs->constructName != NULL)
	IncrementLexemeCount (portSpecs->constructName);
    }

  for (portSpecs = newDefmodule->exportList; portSpecs != NULL;
       portSpecs = portSpecs->next)
    {
      if (portSpecs->moduleName != NULL)
	IncrementLexemeCount (portSpecs->moduleName);
      if (portSpecs->constructType != NULL)
	IncrementLexemeCount (portSpecs->constructType);
      if (portSpecs->constructName != NULL)
	IncrementLexemeCount (portSpecs->constructName);
    }

   /*====================================================*/
  /* Allocate storage for the module's construct lists. */
   /*====================================================*/

  if (redefiningMainModule != NULL)
    {				/* Do nothing */
    }
  else if (DefmoduleData (theEnv)->NumberOfModuleItems == 0)
    newDefmodule->itemsArray = NULL;
  else
    {
      newDefmodule->itemsArray =
	(struct defmoduleItemHeader **) CL_gm2 (theEnv,
						sizeof (void *) *
						DefmoduleData
						(theEnv)->
						NumberOfModuleItems);
      for (i = 0, theItem = DefmoduleData (theEnv)->ListOfModuleItems;
	   (i < DefmoduleData (theEnv)->NumberOfModuleItems)
	   && (theItem != NULL); i++, theItem = theItem->next)
	{
	  if (theItem->allocateFunction == NULL)
	    {
	      newDefmodule->itemsArray[i] = NULL;
	    }
	  else
	    {
	      newDefmodule->itemsArray[i] = (struct defmoduleItemHeader *)
		(*theItem->allocateFunction) (theEnv);
	      theHeader =
		(struct defmoduleItemHeader *) newDefmodule->itemsArray[i];
	      theHeader->theModule = newDefmodule;
	      theHeader->firstItem = NULL;
	      theHeader->lastItem = NULL;
	    }
	}
    }

   /*=======================================*/
  /* CL_Save the pretty print representation. */
   /*=======================================*/

  CL_SavePPBuffer (theEnv, "\n");

  if (CL_GetConserveMemory (theEnv) == true)
    {
      newDefmodule->header.ppFo_rm = NULL;
    }
  else
    {
      newDefmodule->header.ppFo_rm = CL_CopyPPBuffer (theEnv);
    }

   /*==============================================*/
  /* Add the defmodule to the list of defmodules. */
   /*==============================================*/

  if (redefiningMainModule == NULL)
    {
      if (DefmoduleData (theEnv)->LastDefmodule == NULL)
	DefmoduleData (theEnv)->ListOfDefmodules = newDefmodule;
      else
	DefmoduleData (theEnv)->LastDefmodule->header.next =
	  &newDefmodule->header;
      DefmoduleData (theEnv)->LastDefmodule = newDefmodule;
      newDefmodule->header.bsaveID =
	DefmoduleData (theEnv)->NumberOfDefmodules++;
    }

  CL_SetCurrentModule (theEnv, newDefmodule);

   /*=========================================*/
  /* Call any functions required by other    */
  /* constructs when a new module is defined */
   /*=========================================*/

  for (defineFunctions = DefmoduleData (theEnv)->AfterModuleDefinedFunctions;
       defineFunctions != NULL; defineFunctions = defineFunctions->next)
    {
      (*(void (*)(void *)) defineFunctions->func) (theEnv);
    }

   /*===============================================*/
  /* Defmodule successfully parsed with no errors. */
   /*===============================================*/

  return false;
}

/*************************************************************/
/* DeleteDefmodule: Used by the parsing routine to dete_rmine */
/*   if a module can be redefined. Only the MAIN module can  */
/*   be redefined (and it can only be redefined once).       */
/*************************************************************/
static bool
DeleteDefmodule (Defmodule * theDefmodule, Environment * allEnv)
{
  Environment *theEnv;

  if (theDefmodule == NULL)
    {
      theEnv = allEnv;
    }
  else
    {
      theEnv = theDefmodule->header.env;
    }

  if (strcmp (CL_DefmoduleName (theDefmodule), "MAIN") == 0)
    {
      return (DefmoduleData (theEnv)->MainModuleRedefinable);
    }

  return false;
}

/*********************************************************/
/* ParsePortSpecifications: Parses the import and export */
/*   specifications found in a defmodule construct.      */
/*********************************************************/
static bool
ParsePortSpecifications (Environment * theEnv,
			 const char *readSource,
			 struct token *theToken, Defmodule * theDefmodule)
{
  bool error;

   /*=============================*/
  /* The import and export lists */
  /* are initially empty.        */
   /*=============================*/

  theDefmodule->importList = NULL;
  theDefmodule->exportList = NULL;

   /*==========================================*/
  /* Parse import/export specifications until */
  /* a right parenthesis is encountered.      */
   /*==========================================*/

  while (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      /*========================================*/
      /* Look for the opening left parenthesis. */
      /*========================================*/

      if (theToken->tknType != LEFT_PARENTHESIS_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "defmodule");
	  return true;
	}

      /*====================================*/
      /* Look for the import/export keyword */
      /* and call the appropriate functions */
      /* for parsing the specification.     */
      /*====================================*/

      CL_GetToken (theEnv, readSource, theToken);

      if (theToken->tknType != SYMBOL_TOKEN)
	{
	  CL_SyntaxErrorMessage (theEnv, "defmodule");
	  return true;
	}

      if (strcmp (theToken->lexemeValue->contents, "import") == 0)
	{
	  error =
	    ParseImportSpec (theEnv, readSource, theToken, theDefmodule);
	}
      else if (strcmp (theToken->lexemeValue->contents, "export") == 0)
	{
	  error =
	    ParseExportSpec (theEnv, readSource, theToken, theDefmodule,
			     NULL);
	}
      else
	{
	  CL_SyntaxErrorMessage (theEnv, "defmodule");
	  return true;
	}

      if (error)
	return true;

      /*============================================*/
      /* Begin parsing the next port specification. */
      /*============================================*/

      CL_PPCRAndIndent (theEnv);
      CL_GetToken (theEnv, readSource, theToken);

      if (theToken->tknType == RIGHT_PARENTHESIS_TOKEN)
	{
	  CL_PPBackup (theEnv);
	  CL_PPBackup (theEnv);
	  CL_SavePPBuffer (theEnv, ")");
	}
    }

   /*===================================*/
  /* Return false to indicate no error */
  /* occurred while parsing the        */
  /* import/export specifications.     */
   /*===================================*/

  return false;
}

/**********************************************************/
/* ParseImportSpec: Parses import specifications found in */
/*   a defmodule construct.                               */
/*                                                        */
/* <import-spec> ::= (import <module-name> <port-item>)   */
/*                                                        */
/* <port-item>   ::= ?ALL |                               */
/*                   ?NONE |                              */
/*                   <construct-name> ?ALL |              */
/*                   <construct-name> ?NONE |             */
/*                   <construct-name> <names>*            */
/**********************************************************/
static bool
ParseImportSpec (Environment * theEnv,
		 const char *readSource,
		 struct token *theToken, Defmodule * newModule)
{
  Defmodule *theModule;
  struct portItem *thePort, *oldImportSpec;
  bool found;
  unsigned int count;

   /*===========================*/
  /* Look for the module name. */
   /*===========================*/

  CL_SavePPBuffer (theEnv, " ");

  CL_GetToken (theEnv, readSource, theToken);

  if (theToken->tknType != SYMBOL_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, "defmodule import specification");
      return true;
    }

   /*=====================================*/
  /* Verify the existence of the module. */
   /*=====================================*/

  if ((theModule =
       CL_FindDefmodule (theEnv, theToken->lexemeValue->contents)) == NULL)
    {
      CL_CantFindItemErrorMessage (theEnv, "defmodule",
				   theToken->lexemeValue->contents, true);
      return true;
    }

   /*========================================*/
  /* If the specified module doesn't export */
  /* any constructs, then the import        */
  /* specification is meaningless.          */
   /*========================================*/

  if (theModule->exportList == NULL)
    {
      NotExportedErrorMessage (theEnv, CL_DefmoduleName (theModule), NULL,
			       NULL);
      return true;
    }

   /*==============================================*/
  /* Parse the re_maining portion of the import    */
  /* specification and return if an error occurs. */
   /*==============================================*/

  oldImportSpec = newModule->importList;
  if (ParseExportSpec (theEnv, readSource, theToken, newModule, theModule))
    return true;

   /*========================================================*/
  /* If the ?NONE keyword was used with the import spec,    */
  /* then no constructs were actually imported and the      */
  /* import spec does not need to be checked for conflicts. */
   /*========================================================*/

  if (newModule->importList == oldImportSpec)
    return false;

   /*======================================================*/
  /* Check to see if the construct being imported can be  */
  /* by the specified module. This check exported doesn't */
  /* guarantee that a specific named construct actually   */
  /* exists. It just checks that it could be exported if  */
  /* it does exists.                                      */
   /*======================================================*/

  if (newModule->importList->constructType != NULL)
    {
      /*=============================*/
      /* Look for the construct in   */
      /* the module that exports it. */
      /*=============================*/

      found = false;
      for (thePort = theModule->exportList;
	   (thePort != NULL) && (!found); thePort = thePort->next)
	{
	  if (thePort->constructType == NULL)
	    found = true;
	  else if (thePort->constructType ==
		   newModule->importList->constructType)
	    {
	      if (newModule->importList->constructName == NULL)
		found = true;
	      else if (thePort->constructName == NULL)
		found = true;
	      else if (thePort->constructName ==
		       newModule->importList->constructName)
		{
		  found = true;
		}
	    }
	}

      /*=======================================*/
      /* If it's not exported by the specified */
      /* module, print an error message.       */
      /*=======================================*/

      if (!found)
	{
	  if (newModule->importList->constructName == NULL)
	    {
	      NotExportedErrorMessage (theEnv, CL_DefmoduleName (theModule),
				       newModule->importList->constructType->
				       contents, NULL);
	    }
	  else
	    {
	      NotExportedErrorMessage (theEnv, CL_DefmoduleName (theModule),
				       newModule->importList->constructType->
				       contents,
				       newModule->importList->constructName->
				       contents);
	    }
	  return true;
	}
    }

   /*======================================================*/
  /* Verify that specific named constructs actually exist */
  /* and can be seen from the module importing them.      */
   /*======================================================*/

  CL_SaveCurrentModule (theEnv);
  CL_SetCurrentModule (theEnv, newModule);

  for (thePort = newModule->importList;
       thePort != NULL; thePort = thePort->next)
    {
      if ((thePort->constructType == NULL)
	  || (thePort->constructName == NULL))
	{
	  continue;
	}

      theModule = CL_FindDefmodule (theEnv, thePort->moduleName->contents);
      CL_SetCurrentModule (theEnv, theModule);
      if (CL_FindImportedConstruct
	  (theEnv, thePort->constructType->contents, NULL,
	   thePort->constructName->contents, &count, true, NULL) == NULL)
	{
	  NotExportedErrorMessage (theEnv, CL_DefmoduleName (theModule),
				   thePort->constructType->contents,
				   thePort->constructName->contents);
	  CL_RestoreCurrentModule (theEnv);
	  return true;
	}
    }

  CL_RestoreCurrentModule (theEnv);

   /*===============================================*/
  /* The import list has been successfully parsed. */
   /*===============================================*/

  return false;
}

/**********************************************************/
/* ParseExportSpec: Parses export specifications found in */
/*   a defmodule construct. This includes parsing the     */
/*   re_maining specification found in an import           */
/*   specification after the module name.                 */
/**********************************************************/
static bool
ParseExportSpec (Environment * theEnv,
		 const char *readSource,
		 struct token *theToken,
		 Defmodule * newModule, Defmodule * importModule)
{
  struct portItem *newPort;
  CLIPSLexeme *theConstruct, *moduleName;
  struct portConstructItem *thePortConstruct;
  const char *errorMessage;

   /*===========================================*/
  /* Set up some variables for error messages. */
   /*===========================================*/

  if (importModule != NULL)
    {
      errorMessage = "defmodule import specification";
      moduleName = importModule->header.name;
    }
  else
    {
      errorMessage = "defmodule export specification";
      moduleName = NULL;
    }

   /*=============================================*/
  /* Handle the special variables ?ALL and ?NONE */
  /* in the import/export specification.         */
   /*=============================================*/

  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, theToken);

  if (theToken->tknType == SF_VARIABLE_TOKEN)
    {
      /*==============================*/
      /* Check to see if the variable */
      /* is either ?ALL or ?NONE.     */
      /*==============================*/

      if (strcmp (theToken->lexemeValue->contents, "ALL") == 0)
	{
	  newPort = (struct portItem *) get_struct (theEnv, portItem);
	  newPort->moduleName = moduleName;
	  newPort->constructType = NULL;
	  newPort->constructName = NULL;
	  newPort->next = NULL;
	}
      else if (strcmp (theToken->lexemeValue->contents, "NONE") == 0)
	{
	  newPort = NULL;
	}
      else
	{
	  CL_SyntaxErrorMessage (theEnv, errorMessage);
	  return true;
	}

      /*=======================================================*/
      /* The export/import specification must end with a right */
      /* parenthesis after ?ALL or ?NONE at this point.        */
      /*=======================================================*/

      CL_GetToken (theEnv, readSource, theToken);

      if (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
	{
	  if (newPort != NULL)
	    rtn_struct (theEnv, portItem, newPort);
	  CL_PPBackup (theEnv);
	  CL_SavePPBuffer (theEnv, " ");
	  CL_SavePPBuffer (theEnv, theToken->printFo_rm);
	  CL_SyntaxErrorMessage (theEnv, errorMessage);
	  return true;
	}

      /*=====================================*/
      /* Add the new specification to either */
      /* the import or export list.          */
      /*=====================================*/

      if (newPort != NULL)
	{
	  if (importModule != NULL)
	    {
	      newPort->next = newModule->importList;
	      newModule->importList = newPort;
	    }
	  else
	    {
	      newPort->next = newModule->exportList;
	      newModule->exportList = newPort;
	    }
	}

      /*============================================*/
      /* Return false to indicate the import/export */
      /* specification was successfully parsed.     */
      /*============================================*/

      return false;
    }

   /*========================================================*/
  /* If the ?ALL and ?NONE keywords were not used, then the */
  /* token must be the name of an importable construct.     */
   /*========================================================*/

  if (theToken->tknType != SYMBOL_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, errorMessage);
      return true;
    }

  theConstruct = theToken->lexemeValue;

  if ((thePortConstruct =
       CL_ValidPortConstructItem (theEnv, theConstruct->contents)) == NULL)
    {
      CL_SyntaxErrorMessage (theEnv, errorMessage);
      return true;
    }

   /*=============================================================*/
  /* If the next token is the special variable ?ALL, then all    */
  /* constructs of the specified type are imported/exported. If  */
  /* the next token is the special variable ?NONE, then no       */
  /* constructs of the specified type will be imported/exported. */
   /*=============================================================*/

  CL_SavePPBuffer (theEnv, " ");
  CL_GetToken (theEnv, readSource, theToken);

  if (theToken->tknType == SF_VARIABLE_TOKEN)
    {
      /*==============================*/
      /* Check to see if the variable */
      /* is either ?ALL or ?NONE.     */
      /*==============================*/

      if (strcmp (theToken->lexemeValue->contents, "ALL") == 0)
	{
	  newPort = (struct portItem *) get_struct (theEnv, portItem);
	  newPort->moduleName = moduleName;
	  newPort->constructType = theConstruct;
	  newPort->constructName = NULL;
	  newPort->next = NULL;
	}
      else if (strcmp (theToken->lexemeValue->contents, "NONE") == 0)
	{
	  newPort = NULL;
	}
      else
	{
	  CL_SyntaxErrorMessage (theEnv, errorMessage);
	  return true;
	}

      /*=======================================================*/
      /* The export/import specification must end with a right */
      /* parenthesis after ?ALL or ?NONE at this point.        */
      /*=======================================================*/

      CL_GetToken (theEnv, readSource, theToken);

      if (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
	{
	  if (newPort != NULL)
	    rtn_struct (theEnv, portItem, newPort);
	  CL_PPBackup (theEnv);
	  CL_SavePPBuffer (theEnv, " ");
	  CL_SavePPBuffer (theEnv, theToken->printFo_rm);
	  CL_SyntaxErrorMessage (theEnv, errorMessage);
	  return true;
	}

      /*=====================================*/
      /* Add the new specification to either */
      /* the import or export list.          */
      /*=====================================*/

      if (newPort != NULL)
	{
	  if (importModule != NULL)
	    {
	      newPort->next = newModule->importList;
	      newModule->importList = newPort;
	    }
	  else
	    {
	      newPort->next = newModule->exportList;
	      newModule->exportList = newPort;
	    }
	}

      /*============================================*/
      /* Return false to indicate the import/export */
      /* specification was successfully parsed.     */
      /*============================================*/

      return false;
    }

   /*============================================*/
  /* There must be at least one named construct */
  /* in the import/export list at this point.   */
   /*============================================*/

  if (theToken->tknType == RIGHT_PARENTHESIS_TOKEN)
    {
      CL_SyntaxErrorMessage (theEnv, errorMessage);
      return true;
    }

   /*=====================================*/
  /* Read in the list of imported items. */
   /*=====================================*/

  while (theToken->tknType != RIGHT_PARENTHESIS_TOKEN)
    {
      if (theToken->tknType != thePortConstruct->typeExpected)
	{
	  CL_SyntaxErrorMessage (theEnv, errorMessage);
	  return true;
	}

      /*========================================*/
      /* Create the data structure to represent */
      /* the import/export specification for    */
      /* the named construct.                   */
      /*========================================*/

      newPort = (struct portItem *) get_struct (theEnv, portItem);
      newPort->moduleName = moduleName;
      newPort->constructType = theConstruct;
      newPort->constructName = theToken->lexemeValue;

      /*=====================================*/
      /* Add the new specification to either */
      /* the import or export list.          */
      /*=====================================*/

      if (importModule != NULL)
	{
	  newPort->next = newModule->importList;
	  newModule->importList = newPort;
	}
      else
	{
	  newPort->next = newModule->exportList;
	  newModule->exportList = newPort;
	}

      /*===================================*/
      /* Move on to the next import/export */
      /* specification.                    */
      /*===================================*/

      CL_SavePPBuffer (theEnv, " ");
      CL_GetToken (theEnv, readSource, theToken);
    }

   /*=============================*/
  /* Fix up pretty print buffer. */
   /*=============================*/

  CL_PPBackup (theEnv);
  CL_PPBackup (theEnv);
  CL_SavePPBuffer (theEnv, ")");

   /*============================================*/
  /* Return false to indicate the import/export */
  /* specification was successfully parsed.     */
   /*============================================*/

  return false;
}

/*************************************************************/
/* CL_ValidPortConstructItem: Returns true if a given construct */
/*   name is in the list of constructs which can be exported */
/*   and imported, otherwise false is returned.              */
/*************************************************************/
struct portConstructItem *
CL_ValidPortConstructItem (Environment * theEnv, const char *theName)
{
  struct portConstructItem *theItem;

  for (theItem = DefmoduleData (theEnv)->ListOfPortConstructItems;
       theItem != NULL; theItem = theItem->next)
    {
      if (strcmp (theName, theItem->constructName) == 0)
	return (theItem);
    }

  return NULL;
}

/***********************************************************/
/* FindMultiImportConflict: Dete_rmines if a module imports */
/*   the same named construct from more than one module    */
/*   (i.e. an ambiguous reference which is not allowed).   */
/***********************************************************/
static bool
FindMultiImportConflict (Environment * theEnv, Defmodule * theModule)
{
  Defmodule *testModule;
  unsigned int count;
  struct portConstructItem *thePCItem;
  Construct *theConstruct;
  ConstructHeader *theCItem;

   /*==========================*/
  /* CL_Save the current module. */
   /*==========================*/

  CL_SaveCurrentModule (theEnv);

   /*============================*/
  /* Loop through every module. */
   /*============================*/

  for (testModule = CL_GetNextDefmodule (theEnv, NULL);
       testModule != NULL;
       testModule = CL_GetNextDefmodule (theEnv, testModule))
    {
      /*========================================*/
      /* Loop through every construct type that */
      /* can be imported/exported by a module.  */
      /*========================================*/

      for (thePCItem = DefmoduleData (theEnv)->ListOfPortConstructItems;
	   thePCItem != NULL; thePCItem = thePCItem->next)
	{
	  CL_SetCurrentModule (theEnv, testModule);

	 /*=====================================================*/
	  /* Loop through every construct of the specified type. */
	 /*=====================================================*/

	  theConstruct = CL_FindConstruct (theEnv, thePCItem->constructName);

	  for (theCItem = (*theConstruct->getNextItemFunction) (theEnv, NULL);
	       theCItem != NULL;
	       theCItem =
	       (*theConstruct->getNextItemFunction) (theEnv, theCItem))
	    {
	     /*===============================================*/
	      /* Check to see if the specific construct in the */
	      /* module can be imported with more than one     */
	      /* reference into the module we're examining for */
	      /* ambiguous import  specifications.             */
	     /*===============================================*/

	      CL_SetCurrentModule (theEnv, theModule);
	      CL_FindImportedConstruct (theEnv, thePCItem->constructName,
					NULL,
					(*theConstruct->
					 getConstructNameFunction)
					(theCItem)->contents, &count, false,
					NULL);
	      if (count > 1)
		{
		  CL_ImportExportConflictMessage (theEnv, "defmodule",
						  CL_DefmoduleName
						  (theModule),
						  thePCItem->constructName,
						  (*theConstruct->
						   getConstructNameFunction)
						  (theCItem)->contents);
		  CL_RestoreCurrentModule (theEnv);
		  return true;
		}

	      CL_SetCurrentModule (theEnv, testModule);
	    }
	}
    }

   /*=============================*/
  /* Restore the current module. */
   /*=============================*/

  CL_RestoreCurrentModule (theEnv);

   /*=======================================*/
  /* Return false to indicate no ambiguous */
  /* references were found.                */
   /*=======================================*/

  return false;
}

/******************************************************/
/* NotExportedErrorMessage: Generalized error message */
/*  for indicating that a construct type or specific  */
/*  named construct is not exported.                  */
/******************************************************/
static void
NotExportedErrorMessage (Environment * theEnv,
			 const char *theModule,
			 const char *theConstruct, const char *theName)
{
  CL_PrintErrorID (theEnv, "MODULPSR", 1, true);
  CL_WriteString (theEnv, STDERR, "Module '");
  CL_WriteString (theEnv, STDERR, theModule);
  CL_WriteString (theEnv, STDERR, "' does not export ");

  if (theConstruct == NULL)
    CL_WriteString (theEnv, STDERR, "any constructs");
  else if (theName == NULL)
    {
      CL_WriteString (theEnv, STDERR, "any ");
      CL_WriteString (theEnv, STDERR, theConstruct);
      CL_WriteString (theEnv, STDERR, " constructs");
    }
  else
    {
      CL_WriteString (theEnv, STDERR, "the ");
      CL_WriteString (theEnv, STDERR, theConstruct);
      CL_WriteString (theEnv, STDERR, " '");
      CL_WriteString (theEnv, STDERR, theName);
      CL_WriteString (theEnv, STDERR, "'");
    }

  CL_WriteString (theEnv, STDERR, ".\n");
}

/*************************************************************/
/* CL_FindImportExportConflict: Dete_rmines if the definition of */
/*   a construct would cause an import/export conflict. The  */
/*   construct is not yet defined when this function is      */
/*   called. True is returned if an import/export conflicts  */
/*   is found, otherwise false is returned.                  */
/*************************************************************/
bool
CL_FindImportExportConflict (Environment * theEnv,
			     const char *constructName,
			     Defmodule * matchModule, const char *findName)
{
  Defmodule *theModule;
  struct moduleItem *theModuleItem;
  unsigned int count;

   /*===========================================================*/
  /* If the construct type can't be imported or exported, then */
  /* it's not possible to have an import/export conflict.      */
   /*===========================================================*/

  if (CL_ValidPortConstructItem (theEnv, constructName) == NULL)
    return false;

   /*============================================*/
  /* There module name should already have been */
  /* separated fromthe construct's name.        */
   /*============================================*/

  if (CL_FindModuleSeparator (findName))
    return false;

   /*===============================================================*/
  /* The construct must be capable of being stored within a module */
  /* (this test should never fail). The construct must also have   */
  /* a find function associated with it so we can actually look    */
  /* for import/export conflicts.                                  */
   /*===============================================================*/

  if ((theModuleItem = CL_FindModuleItem (theEnv, constructName)) == NULL)
    return false;

  if (theModuleItem->findFunction == NULL)
    return false;

   /*==========================*/
  /* CL_Save the current module. */
   /*==========================*/

  CL_SaveCurrentModule (theEnv);

   /*================================================================*/
  /* Look at each module and count each definition of the specified */
  /* construct which is visible to the module. If more than one     */
  /* definition is visible, then an import/export conflict exists   */
  /* and true is returned.                                          */
   /*================================================================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      CL_SetCurrentModule (theEnv, theModule);

      CL_FindImportedConstruct (theEnv, constructName, NULL, findName, &count,
				true, matchModule);
      if (count > 1)
	{
	  CL_RestoreCurrentModule (theEnv);
	  return true;
	}
    }

   /*==========================================*/
  /* Restore the current module. No conflicts */
  /* were detected so false is returned.      */
   /*==========================================*/

  CL_RestoreCurrentModule (theEnv);
  return false;
}

#endif /* DEFMODULE_CONSTRUCT && (! RUN_TIME) && (! BLOAD_ONLY) */
