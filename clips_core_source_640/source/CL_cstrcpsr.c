   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/13/17             */
   /*                                                     */
   /*              CONSTRUCT PARSER MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parsing routines and utilities for parsing       */
/*   constructs.                                             */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Made the construct redefinition message more   */
/*            prominent.                                     */
/*                                                           */
/*            Added pragmas to remove compilation warnings.  */
/*                                                           */
/*      6.30: Added code for capturing errors/warnings.      */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW, MAC_MCW, */
/*            and IBM_TBC).                                  */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            CL_GetConstructNameAndComment API change.         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*      6.40: Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_Get_HaltExecution and       */
/*            Set_HaltExecution functions.                    */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Changed return values for router functions.    */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            Added CL_GCBlockStart and CL_GCBlockEnd functions    */
/*            for garbage collection blocks.                 */
/*                                                           */
/*            File name/line count displayed for errors      */
/*            and warnings during load command.              */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if (! RUN_TIME) && (! BLOAD_ONLY)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "envrnmnt.h"
#include "router.h"
#include "watch.h"
#include "constrct.h"
#include "prcdrpsr.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "modulutl.h"
#include "modulpsr.h"
#include "pprint.h"
#include "prntutil.h"
#include "strngrtr.h"
#include "sysdep.h"
#include "utility.h"

#include "cstrcpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool CL_FindConstructBeginning (Environment *, const char *,
				       struct token *, bool, bool *);

/**********************************************************/
/* CL_Load: C access routine for the load command. Returns   */
/*   LE_OPEN_FILE_ERROR if the file couldn't be opened,   */
/*   LE_PARSING_ERROR if the file was opened but an error */
/*   occurred while loading constructs, and LE_NO_ERROR   */
/*   if the file was opened and no errors occured while   */
/*   loading.                                             */
/**********************************************************/
CL_LoadError
CL_Load (Environment * theEnv, const char *fileName)
{
  FILE *theFile = NULL;
  char *oldParsingFileName = NULL;
  int noErrorsDetected = 0;

  /*+Basile added */
  CLGCC_DBGPRINTF ("CL_Load fileName=%s", fileName);

   /*=======================================*/
  /* Open the file specified by file name. */
   /*=======================================*/

  if ((theFile = CL_GenOpen (theEnv, fileName, "r")) == NULL)
    {
      return LE_OPEN_FILE_ERROR;
    }

   /*===================================================*/
  /* Read in the constructs. Enabling fast load allows */
  /* the router system to be bypassed for quicker load */
  /* times.                                            */
   /*===================================================*/

  SetFast_Load (theEnv, theFile);

  oldParsingFileName = CL_CopyString (theEnv, CL_GetParsingFileName (theEnv));
  CL_SetParsingFileName (theEnv, fileName);

  Set_LoadInProgress (theEnv, true);
  noErrorsDetected =
    CL_LoadConstructsFromLogicalName (theEnv, (char *) theFile);
  Set_LoadInProgress (theEnv, false);

  CL_SetParsingFileName (theEnv, oldParsingFileName);
  CL_DeleteString (theEnv, oldParsingFileName);

  CL_SetWarningFileName (theEnv, NULL);
  CL_SetErrorFileName (theEnv, NULL);

  SetFast_Load (theEnv, NULL);

   /*=================*/
  /* Close the file. */
   /*=================*/

  CL_GenClose (theEnv, theFile);

   /*=================================================*/
  /* If no errors occurred during the load, return   */
  /* LE_NO_ERROR, otherwise return LE_PARSING_ERROR. */
   /*=================================================*/

  if (noErrorsDetected)
    {
      return LE_NO_ERROR;
    }

  return LE_PARSING_ERROR;
}

/*******************/
/* CL_LoadFromString: */
/*******************/
bool
CL_LoadFromString (Environment * theEnv, const char *theString, size_t theMax)
{
  bool rv;
  const char *theStrRouter = "*** load-from-string ***";

   /*==========================*/
  /* Initialize string router */
   /*==========================*/

  if ((theMax ==
       SIZE_MAX) ? (!CL_OpenStringSource (theEnv, theStrRouter, theString,
					  0)) : (!CL_OpenTextSource (theEnv,
								     theStrRouter,
								     theString,
								     0,
								     theMax)))
    return false;

   /*======================*/
  /* CL_Load the constructs. */
   /*======================*/

  rv = CL_LoadConstructsFromLogicalName (theEnv, theStrRouter);

   /*=================*/
  /* Close router.   */
   /*=================*/

  CL_CloseStringSource (theEnv, theStrRouter);

  return rv;
}

/****************************************************/
/* CL_SetParsingFileName: Sets the file name currently */
/*   being parsed by the load/batch command.        */
/****************************************************/
void
CL_SetParsingFileName (Environment * theEnv, const char *fileName)
{
  char *fileNameCopy = NULL;

  if (fileName != NULL)
    {
      fileNameCopy = (char *) CL_genalloc (theEnv, strlen (fileName) + 1);
      CL_genstrcpy (fileNameCopy, fileName);
    }

  if (ConstructData (theEnv)->ParsingFileName != NULL)
    {
      CL_genfree (theEnv, ConstructData (theEnv)->ParsingFileName,
		  strlen (ConstructData (theEnv)->ParsingFileName) + 1);
    }

  ConstructData (theEnv)->ParsingFileName = fileNameCopy;
}

/*******************************************************/
/* CL_GetParsingFileName: Returns the file name currently */
/*   being parsed by the load/batch command.           */
/*******************************************************/
char *
CL_GetParsingFileName (Environment * theEnv)
{
  return ConstructData (theEnv)->ParsingFileName;
}

/**********************************************/
/* CL_SetErrorFileName: Sets the file name       */
/*   associated with the last error detected. */
/**********************************************/
void
CL_SetErrorFileName (Environment * theEnv, const char *fileName)
{
  char *fileNameCopy = NULL;

  if (fileName != NULL)
    {
      fileNameCopy = (char *) CL_genalloc (theEnv, strlen (fileName) + 1);
      CL_genstrcpy (fileNameCopy, fileName);
    }

  if (ConstructData (theEnv)->ErrorFileName != NULL)
    {
      CL_genfree (theEnv, ConstructData (theEnv)->ErrorFileName,
		  strlen (ConstructData (theEnv)->ErrorFileName) + 1);
    }

  ConstructData (theEnv)->ErrorFileName = fileNameCopy;
}

/**********************************************/
/* CL_GetErrorFileName: Returns the file name    */
/*   associated with the last error detected. */
/**********************************************/
char *
CL_GetErrorFileName (Environment * theEnv)
{
  return ConstructData (theEnv)->ErrorFileName;
}

/************************************************/
/* CL_SetWarningFileName: Sets the file name       */
/*   associated with the last warning detected. */
/************************************************/
void
CL_SetWarningFileName (Environment * theEnv, const char *fileName)
{
  char *fileNameCopy = NULL;

  if (fileName != NULL)
    {
      fileNameCopy = (char *) CL_genalloc (theEnv, strlen (fileName) + 1);
      CL_genstrcpy (fileNameCopy, fileName);
    }

  if (ConstructData (theEnv)->WarningFileName != NULL)
    {
      CL_genfree (theEnv, ConstructData (theEnv)->WarningFileName,
		  strlen (ConstructData (theEnv)->WarningFileName) + 1);
    }

  ConstructData (theEnv)->WarningFileName = fileNameCopy;
}

/************************************************/
/* CL_GetWarningFileName: Returns the file name    */
/*   associated with the last warning detected. */
/************************************************/
char *
CL_GetWarningFileName (Environment * theEnv)
{
  return ConstructData (theEnv)->WarningFileName;
}

/*****************************************************************/
/* CL_LoadConstructsFromLogicalName: CL_Loads a set of constructs into */
/*   the current environment from a specified logical name.      */
/*****************************************************************/
bool
CL_LoadConstructsFromLogicalName (Environment * theEnv,
				  const char *readSource)
{
  CL_BuildError constructFlag;
  struct token theToken;
  bool noErrors = true;
  bool foundConstruct;
  GCBlock gcb;
  long oldLineCountValue;
  const char *oldLineCountRouter;

   /*===================================================*/
  /* Create a router to capture the error info_rmation. */
   /*===================================================*/

  CL_CreateErrorCaptureRouter (theEnv);

   /*==============================*/
  /* Initialize the line counter. */
   /*==============================*/

  oldLineCountValue = CL_SetLineCount (theEnv, 1);
  oldLineCountRouter = RouterData (theEnv)->LineCountRouter;
  RouterData (theEnv)->LineCountRouter = readSource;

   /*=====================================*/
  /* If embedded, clear the error flags. */
   /*=====================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_ResetErrorFlags (theEnv);
    }

   /*==========================================*/
  /* Set up the frame for garbage collection. */
   /*==========================================*/

  CL_GCBlockStart (theEnv, &gcb);

   /*========================================================*/
  /* Find the beginning of the first construct in the file. */
   /*========================================================*/

  CL_GetToken (theEnv, readSource, &theToken);
  foundConstruct =
    CL_FindConstructBeginning (theEnv, readSource, &theToken, false,
			       &noErrors);

   /*==================================================*/
  /* Parse the file until the end of file is reached. */
   /*==================================================*/

  while ((foundConstruct == true) && (CL_Get_HaltExecution (theEnv) == false))
    {
      /*===========================================================*/
      /* CL_Clear the pretty print buffer in preparation for parsing. */
      /*===========================================================*/

      CL_FlushPPBuffer (theEnv);

      /*======================*/
      /* Parse the construct. */
      /*======================*/

      constructFlag =
	CL_ParseConstruct (theEnv, theToken.lexemeValue->contents,
			   readSource);

      /*==============================================================*/
      /* If an error occurred while parsing, then find the beginning  */
      /* of the next construct (but don't generate any more error     */
      /* messages--in effect, skip everything until another construct */
      /* is found).                                                   */
      /*==============================================================*/

      if (constructFlag == BE_PARSING_ERROR)
	{
	  CL_WriteString (theEnv, STDERR, "\nERROR:\n");
	  CL_WriteString (theEnv, STDERR, CL_GetPPBuffer (theEnv));
	  CL_WriteString (theEnv, STDERR, "\n");

	  CL_FlushParsingMessages (theEnv);

	  noErrors = false;
	  CL_GetToken (theEnv, readSource, &theToken);
	  foundConstruct =
	    CL_FindConstructBeginning (theEnv, readSource, &theToken, true,
				       &noErrors);
	}

      /*======================================================*/
      /* Otherwise, find the beginning of the next construct. */
      /*======================================================*/

      else
	{
	  CL_FlushParsingMessages (theEnv);
	  CL_GetToken (theEnv, readSource, &theToken);
	  foundConstruct =
	    CL_FindConstructBeginning (theEnv, readSource, &theToken, false,
				       &noErrors);
	}

      /*=====================================================*/
      /* Yield time if necessary to foreground applications. */
      /*=====================================================*/

      if (foundConstruct)
	{
	  IncrementLexemeCount (theToken.value);
	}

      CL_CleanCurrentGarbageFrame (theEnv, NULL);
      CL_CallPeriodicTasks (theEnv);

      CL_YieldTime (theEnv);

      if (foundConstruct)
	{
	  CL_ReleaseLexeme (theEnv, theToken.lexemeValue);
	}
    }

   /*========================================================*/
  /* Print a carriage return if a single character is being */
  /* printed to indicate constructs are being processed.    */
   /*========================================================*/

#if DEBUGGING_FUNCTIONS
  if ((CL_Get_WatchItem (theEnv, "compilations") != 1)
      && CL_GetPrintWhile_Loading (theEnv))
#else
  if (CL_GetPrintWhile_Loading (theEnv))
#endif
    {
      CL_WriteString (theEnv, STDOUT, "\n");
    }

   /*=============================================================*/
  /* Once the load is complete, destroy the pretty print buffer. */
  /* This frees up any memory that was used to create the pretty */
  /* print fo_rms for constructs during parsing. Thus calls to    */
  /* the mem-used function will accurately reflect the amount of */
  /* memory being used after a load command.                     */
   /*=============================================================*/

  CL_DestroyPPBuffer (theEnv);

   /*======================================*/
  /* Remove the garbage collection frame. */
   /*======================================*/

  CL_GCBlockEnd (theEnv, &gcb);
  CL_CallPeriodicTasks (theEnv);

   /*==============================*/
  /* Deactivate the line counter. */
   /*==============================*/

  CL_SetLineCount (theEnv, oldLineCountValue);
  RouterData (theEnv)->LineCountRouter = oldLineCountRouter;

   /*===========================================*/
  /* Invoke the parser error callback function */
  /* and delete the error capture router.      */
   /*===========================================*/

  CL_FlushParsingMessages (theEnv);
  CL_DeleteErrorCaptureRouter (theEnv);

   /*==========================================================*/
  /* Return a boolean flag which indicates whether any errors */
  /* were encountered while loading the constructs.           */
   /*==========================================================*/

  return noErrors;
}

/********************************************************************/
/* CL_FindConstructBeginning: Searches for a left parenthesis followed */
/*   by the name of a valid construct. Used by the load command to  */
/*   find the next construct to be parsed. Returns true is the      */
/*   beginning of a construct was found, otherwise false.           */
/********************************************************************/
static bool
CL_FindConstructBeginning (Environment * theEnv,
			   const char *readSource,
			   struct token *theToken,
			   bool errorCorrection, bool *noErrors)
{
  bool leftParenthesisFound = false;
  bool firstAttempt = true;

   /*===================================================*/
  /* Process tokens until the beginning of a construct */
  /* is found or there are no more tokens.             */
   /*===================================================*/

  while (theToken->tknType != STOP_TOKEN)
    {
      /*=====================================================*/
      /* Constructs begin with a left parenthesis. Make note */
      /* that the opening parenthesis has been found.        */
      /*=====================================================*/

      if (theToken->tknType == LEFT_PARENTHESIS_TOKEN)
	{
	  leftParenthesisFound = true;
	}

      /*=================================================================*/
      /* The name of the construct follows the opening left parenthesis. */
      /* If it is the name of a valid construct, then return true.       */
      /* Otherwise, reset the flags to look for the beginning of a       */
      /* construct. If error correction is being perfo_rmed (i.e. the     */
      /* last construct parsed had an error in it), then don't bother to */
      /* print an error message, otherwise, print an error message.      */
      /*=================================================================*/

      else if ((theToken->tknType == SYMBOL_TOKEN)
	       && (leftParenthesisFound == true))
	{
	 /*===========================================================*/
	  /* Is this a valid construct name (e.g., defrule, deffacts). */
	 /*===========================================================*/

	  if (CL_FindConstruct (theEnv, theToken->lexemeValue->contents) !=
	      NULL)
	    return true;

	 /*===============================================*/
	  /* The construct name is invalid. Print an error */
	  /* message if one hasn't already been printed.   */
	 /*===============================================*/

	  if (firstAttempt && (!errorCorrection))
	    {
	      errorCorrection = true;
	      *noErrors = false;
	      CL_PrintErrorID (theEnv, "CSTRCPSR", 1, true);
	      CL_WriteString (theEnv, STDERR,
			      "Expected the beginning of a construct.\n");
	      CLGCC_DBGPRINTF("CSTRCPSR error");
	    }

	 /*======================================================*/
	  /* Indicate that an error has been found and that we're */
	  /* looking for a left parenthesis again.                */
	 /*======================================================*/

	  firstAttempt = false;
	  leftParenthesisFound = false;
	}

      /*====================================================================*/
      /* Any token encountered other than a left parenthesis or a construct */
      /* name following a left parenthesis is illegal. Again, if error      */
      /* correction is in progress, no error message is printed, otherwise, */
      /*  an error message is printed.                                      */
      /*====================================================================*/

      else
	{
	  if (firstAttempt && (!errorCorrection))
	    {
	      errorCorrection = true;
	      *noErrors = false;
	      CL_PrintErrorID (theEnv, "CSTRCPSR", 1, true);
	      CL_WriteString (theEnv, STDERR,
			      "Expected the beginning of a construct.\n");
	      CLGCC_DBGPRINTF("CSTRCPSR error");
	    }

	  firstAttempt = false;
	  leftParenthesisFound = false;
	}

      /*============================================*/
      /* Move on to the next token to be processed. */
      /*============================================*/

      CL_GetToken (theEnv, readSource, theToken);
    }

   /*===================================================================*/
  /* Couldn't find the beginning of a construct, so false is returned. */
   /*===================================================================*/

  return false;
}

/***********************************************************/
/* QueryErrorCallback: Query routine for the error router. */
/***********************************************************/
static bool
QueryErrorCallback (Environment * theEnv,
		    const char *logicalName, void *context)
{
#if MAC_XCD
#pragma unused(theEnv,context)
#endif

  if ((strcmp (logicalName, STDERR) == 0) ||
      (strcmp (logicalName, STDWRN) == 0))
    {
      return true;
    }

  return false;
}

/***********************************************************/
/* CL_WriteErrorCallback: CL_Write routine for the error router. */
/***********************************************************/
static void
CL_WriteErrorCallback (Environment * theEnv,
		       const char *logicalName,
		       const char *str, void *context)
{
  if (strcmp (logicalName, STDERR) == 0)
    {
      ConstructData (theEnv)->ErrorString =
	CL_AppendToString (theEnv, str, ConstructData (theEnv)->ErrorString,
			   &ConstructData (theEnv)->CurErrPos,
			   &ConstructData (theEnv)->MaxErrChars);
    }
  else if (strcmp (logicalName, STDWRN) == 0)
    {
      ConstructData (theEnv)->WarningString =
	CL_AppendToString (theEnv, str, ConstructData (theEnv)->WarningString,
			   &ConstructData (theEnv)->CurWrnPos,
			   &ConstructData (theEnv)->MaxWrnChars);
    }

  CL_DeactivateRouter (theEnv, "error-capture");
  CL_WriteString (theEnv, logicalName, str);
  CL_ActivateRouter (theEnv, "error-capture");
}

/***********************************************/
/* CL_CreateErrorCaptureRouter: Creates the error */
/*   capture router if it doesn't exists.      */
/***********************************************/
void
CL_CreateErrorCaptureRouter (Environment * theEnv)
{
   /*===========================================================*/
  /* Don't bother creating the error capture router if there's */
  /* no parser callback. The implication of this is that the   */
  /* parser callback should be created before any routines     */
  /* which could generate errors are called.                   */
   /*===========================================================*/

  if (ConstructData (theEnv)->ParserErrorCallback == NULL)
    return;

   /*=======================================================*/
  /* If the router hasn't already been created, create it. */
   /*=======================================================*/

  if (ConstructData (theEnv)->errorCaptureRouterCount == 0)
    {
      CL_AddRouter (theEnv, "error-capture", 40,
		    QueryErrorCallback, CL_WriteErrorCallback,
		    NULL, NULL, NULL, NULL);
    }

   /*==================================================*/
  /* Increment the count for the number of references */
  /* that want the error capture router functioning.  */
   /*==================================================*/

  ConstructData (theEnv)->errorCaptureRouterCount++;
}

/***********************************************/
/* CL_DeleteErrorCaptureRouter: Deletes the error */
/*   capture router if it exists.              */
/***********************************************/
void
CL_DeleteErrorCaptureRouter (Environment * theEnv)
{
   /*===========================================================*/
  /* Don't bother deleting the error capture router if there's */
  /* no parser callback. The implication of this is that the   */
  /* parser callback should be created before any routines     */
  /* which could generate errors are called.                   */
   /*===========================================================*/

  if (ConstructData (theEnv)->ParserErrorCallback == NULL)
    return;

  ConstructData (theEnv)->errorCaptureRouterCount--;

  if (ConstructData (theEnv)->errorCaptureRouterCount == 0)
    {
      CL_DeleteRouter (theEnv, "error-capture");
    }
}

/*******************************************************/
/* CL_FlushParsingMessages: Invokes the callback routines */
/*   for any existing warning/error messages.          */
/*******************************************************/
void
CL_FlushParsingMessages (Environment * theEnv)
{
   /*===========================================================*/
  /* Don't bother flushing the error capture router if there's */
  /* no parser callback. The implication of this is that the   */
  /* parser callback should be created before any routines     */
  /* which could generate errors are called.                   */
   /*===========================================================*/

  if (ConstructData (theEnv)->ParserErrorCallback == NULL)
    return;

   /*=================================*/
  /* If an error occurred invoke the */
  /* parser error callback function. */
   /*=================================*/

  if (ConstructData (theEnv)->ErrorString != NULL)
    {
      (*ConstructData (theEnv)->ParserErrorCallback) (theEnv,
						      CL_GetErrorFileName
						      (theEnv), NULL,
						      ConstructData
						      (theEnv)->ErrorString,
						      ConstructData
						      (theEnv)->ErrLineNumber,
						      ConstructData
						      (theEnv)->ParserErrorContext);
    }

  if (ConstructData (theEnv)->WarningString != NULL)
    {
      (*ConstructData (theEnv)->ParserErrorCallback) (theEnv,
						      CL_GetWarningFileName
						      (theEnv),
						      ConstructData
						      (theEnv)->WarningString,
						      NULL,
						      ConstructData
						      (theEnv)->WrnLineNumber,
						      ConstructData
						      (theEnv)->ParserErrorContext);
    }

   /*===================================*/
  /* Delete the error capture strings. */
   /*===================================*/

  CL_SetErrorFileName (theEnv, NULL);
  if (ConstructData (theEnv)->ErrorString != NULL)
    {
      CL_genfree (theEnv, ConstructData (theEnv)->ErrorString,
		  strlen (ConstructData (theEnv)->ErrorString) + 1);
    }
  ConstructData (theEnv)->ErrorString = NULL;
  ConstructData (theEnv)->CurErrPos = 0;
  ConstructData (theEnv)->MaxErrChars = 0;

  CL_SetWarningFileName (theEnv, NULL);
  if (ConstructData (theEnv)->WarningString != NULL)
    {
      CL_genfree (theEnv, ConstructData (theEnv)->WarningString,
		  strlen (ConstructData (theEnv)->WarningString) + 1);
    }
  ConstructData (theEnv)->WarningString = NULL;
  ConstructData (theEnv)->CurWrnPos = 0;
  ConstructData (theEnv)->MaxWrnChars = 0;
}

/***************************************/
/* CL_ParseConstruct: Parses a construct. */
/***************************************/
CL_BuildError
CL_ParseConstruct (Environment * theEnv,
		   const char *name, const char *logicalName)
{
  Construct *currentPtr;
  CL_BuildError rv;
  bool ov;
  GCBlock gcb;

   /*=================================*/
  /* Look for a valid construct name */
  /* (e.g. defrule, deffacts).       */
   /*=================================*/

  currentPtr = CL_FindConstruct (theEnv, name);
  if (currentPtr == NULL)
    return BE_CONSTRUCT_NOT_FOUND_ERROR;

   /*==========================================*/
  /* Set up the frame for garbage collection. */
   /*==========================================*/

  CL_GCBlockStart (theEnv, &gcb);

   /*==================================*/
  /* Prepare the parsing environment. */
   /*==================================*/

  ov = CL_Get_HaltExecution (theEnv);
  Set_EvaluationError (theEnv, false);
  Set_HaltExecution (theEnv, false);
  CL_ClearParsedBindNames (theEnv);
  CL_PushRtnBrkContexts (theEnv);
  ExpressionData (theEnv)->ReturnContext = false;
  ExpressionData (theEnv)->BreakContext = false;

   /*=======================================*/
  /* Call the construct's parsing routine. */
   /*=======================================*/

  ConstructData (theEnv)->ParsingConstruct = true;

  if ((*currentPtr->parseFunction) (theEnv, logicalName))
    {
      rv = BE_PARSING_ERROR;
    }
  else
    {
      rv = BE_NO_ERROR;
    }

  ConstructData (theEnv)->ParsingConstruct = false;

   /*===============================*/
  /* Restore environment settings. */
   /*===============================*/

  CL_PopRtnBrkContexts (theEnv);

  CL_ClearParsedBindNames (theEnv);
  CL_SetPPBufferStatus (theEnv, false);
  Set_HaltExecution (theEnv, ov);

   /*======================================*/
  /* Remove the garbage collection frame. */
   /*======================================*/

  CL_GCBlockEnd (theEnv, &gcb);
  CL_CallPeriodicTasks (theEnv);

   /*==============================*/
  /* Return the status of parsing */
  /* the construct.               */
   /*==============================*/

  return rv;
}

/******************************************************/
/* CL_ImportExportConflictMessage: Generic error message */
/*   for an import/export module conflict detected    */
/*   when a construct is being defined.               */
/******************************************************/
void
CL_ImportExportConflictMessage (Environment * theEnv,
				const char *constructName,
				const char *itemName,
				const char *causedByConstruct,
				const char *causedByName)
{
  CL_PrintErrorID (theEnv, "CSTRCPSR", 3, true);
  CL_WriteString (theEnv, STDERR, "Cannot define ");
  CL_WriteString (theEnv, STDERR, constructName);
  CL_WriteString (theEnv, STDERR, " '");
  CL_WriteString (theEnv, STDERR, itemName);
  CL_WriteString (theEnv, STDERR, "' because of an import/export conflict");

  if (causedByConstruct == NULL)
    CL_WriteString (theEnv, STDERR, ".\n");
  else
    {
      CL_WriteString (theEnv, STDERR, " caused by the ");
      CL_WriteString (theEnv, STDERR, causedByConstruct);
      CL_WriteString (theEnv, STDERR, " '");
      CL_WriteString (theEnv, STDERR, causedByName);
      CL_WriteString (theEnv, STDERR, "'.\n");
    }
}

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */
