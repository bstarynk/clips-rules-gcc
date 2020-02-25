   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/19/17            */
   /*                                                     */
   /*                 FILE UTILITY MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for file commands including    */
/*   batch, dribble-on, dribble-off, save, load, bsave, and  */
/*   bload.                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Bebe Ly                                              */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.31: Fixed error in CL_AppendDribble for older         */
/*            compilers not allowing variable definition     */
/*            within for statement.                          */
/*                                                           */
/*      6.40: Split from filecom.c                           */
/*                                                           */
/*            Fix for the batch* command so that the last    */
/*            command will execute if there is not a crlf.   */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "argacces.h"
#include "commline.h"
#include "cstrcpsr.h"
#include "memalloc.h"
#include "prcdrfun.h"
#include "pprint.h"
#include "prntutil.h"
#include "router.h"
#include "scanner.h"
#include "strngrtr.h"
#include "sysdep.h"
#include "filecom.h"
#include "utility.h"

#include "fileutil.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if DEBUGGING_FUNCTIONS
static bool QueryDribbleCallback (Environment *, const char *, void *);
static int ReadDribbleCallback (Environment *, const char *, void *);
static int UnreadDribbleCallback (Environment *, const char *, int, void *);
static void ExitDribbleCallback (Environment *, int, void *);
static void CL_WriteDribbleCallback (Environment *, const char *,
				     const char *, void *);
static void PutcDribbleBuffer (Environment *, int);
#endif
static bool Query_BatchCallback (Environment *, const char *, void *);
static int Read_BatchCallback (Environment *, const char *, void *);
static int Unread_BatchCallback (Environment *, const char *, int, void *);
static void Exit_BatchCallback (Environment *, int, void *);
static void Add_Batch (Environment *, bool, FILE *, const char *, int,
		       const char *, const char *);

#if DEBUGGING_FUNCTIONS
/****************************************/
/* QueryDribbleCallback: Query callback */
/*   for the dribble router.            */
/****************************************/
static bool
QueryDribbleCallback (Environment * theEnv,
		      const char *logicalName, void *context)
{
#if MAC_XCD
#pragma unused(theEnv,context)
#endif

  if ((strcmp (logicalName, STDOUT) == 0) ||
      (strcmp (logicalName, STDIN) == 0) ||
      (strcmp (logicalName, STDERR) == 0) ||
      (strcmp (logicalName, STDWRN) == 0))
    {
      return true;
    }

  return false;
}

/******************/
/* CL_AppendDribble: */
/******************/
void
CL_AppendDribble (Environment * theEnv, const char *str)
{
  int i;

  if (!CL_DribbleActive (theEnv))
    return;

  for (i = 0; str[i] != EOS; i++)
    {
      PutcDribbleBuffer (theEnv, str[i]);
    }
}

/****************************************/
/* CL_WriteDribbleCallback: CL_Write callback */
/*    for the dribble router.           */
/****************************************/
static void
CL_WriteDribbleCallback (Environment * theEnv,
			 const char *logicalName,
			 const char *str, void *context)
{
  int i;

   /*======================================*/
  /* CL_Send the output to the dribble file. */
   /*======================================*/

  for (i = 0; str[i] != EOS; i++)
    {
      PutcDribbleBuffer (theEnv, str[i]);
    }

   /*===========================================================*/
  /* CL_Send the output to any routers interested in printing it. */
   /*===========================================================*/

  CL_DeactivateRouter (theEnv, "dribble");
  CL_WriteString (theEnv, logicalName, str);
  CL_ActivateRouter (theEnv, "dribble");
}

/**************************************/
/* ReadDribbleCallback: Read callback */
/*    for the dribble router.         */
/**************************************/
static int
ReadDribbleCallback (Environment * theEnv,
		     const char *logicalName, void *context)
{
  int rv;

   /*===========================================*/
  /* Deactivate the dribble router and get the */
  /* character from another active router.     */
   /*===========================================*/

  CL_DeactivateRouter (theEnv, "dribble");
  rv = CL_ReadRouter (theEnv, logicalName);
  CL_ActivateRouter (theEnv, "dribble");

   /*==========================================*/
  /* Put the character retrieved from another */
  /* router into the dribble buffer.          */
   /*==========================================*/

  PutcDribbleBuffer (theEnv, rv);

   /*=======================*/
  /* Return the character. */
   /*=======================*/

  return (rv);
}

/***********************************************************/
/* PutcDribbleBuffer: Putc routine for the dribble router. */
/***********************************************************/
static void
PutcDribbleBuffer (Environment * theEnv, int rv)
{
   /*===================================================*/
  /* Receiving an end-of-file character will cause the */
  /* contents of the dribble buffer to be flushed.     */
   /*===================================================*/

  if (rv == EOF)
    {
      if (FileCommandData (theEnv)->DribbleCurrentPosition > 0)
	{
	  fprintf (FileCommandData (theEnv)->DribbleFP, "%s",
		   FileCommandData (theEnv)->DribbleBuffer);
	  FileCommandData (theEnv)->DribbleCurrentPosition = 0;
	  FileCommandData (theEnv)->DribbleBuffer[0] = EOS;
	}
    }

   /*===========================================================*/
  /* If we aren't receiving command input, then the character  */
  /* just received doesn't need to be placed in the dribble    */
  /* buffer--It can be written directly to the file. This will */
  /* occur for example when the command prompt is being        */
  /* printed (the AwaitingInput variable will be false because */
  /* command input has not been receivied yet). Before writing */
  /* the character to the file, the dribble buffer is flushed. */
   /*===========================================================*/

  else if (RouterData (theEnv)->AwaitingInput == false)
    {
      if (FileCommandData (theEnv)->DribbleCurrentPosition > 0)
	{
	  fprintf (FileCommandData (theEnv)->DribbleFP, "%s",
		   FileCommandData (theEnv)->DribbleBuffer);
	  FileCommandData (theEnv)->DribbleCurrentPosition = 0;
	  FileCommandData (theEnv)->DribbleBuffer[0] = EOS;
	}

      fputc (rv, FileCommandData (theEnv)->DribbleFP);
    }

   /*=====================================================*/
  /* Otherwise, add the character to the dribble buffer. */
   /*=====================================================*/

  else
    {
      FileCommandData (theEnv)->DribbleBuffer =
	CL_ExpandStringWithChar (theEnv, rv,
				 FileCommandData (theEnv)->DribbleBuffer,
				 &FileCommandData
				 (theEnv)->DribbleCurrentPosition,
				 &FileCommandData
				 (theEnv)->DribbleMaximumPosition,
				 FileCommandData
				 (theEnv)->DribbleMaximumPosition +
				 BUFFER_SIZE);
    }
}

/******************************************/
/* UnreadDribbleCallback: Unread callback */
/*    for the dribble router.             */
/******************************************/
static int
UnreadDribbleCallback (Environment * theEnv,
		       const char *logicalName, int ch, void *context)
{
  int rv;

   /*===============================================*/
  /* Remove the character from the dribble buffer. */
   /*===============================================*/

  if (FileCommandData (theEnv)->DribbleCurrentPosition > 0)
    FileCommandData (theEnv)->DribbleCurrentPosition--;
  FileCommandData (theEnv)->DribbleBuffer[FileCommandData (theEnv)->
					  DribbleCurrentPosition] = EOS;

   /*=============================================*/
  /* Deactivate the dribble router and pass the  */
  /* ungetc request to the other active routers. */
   /*=============================================*/

  CL_DeactivateRouter (theEnv, "dribble");
  rv = CL_UnreadRouter (theEnv, logicalName, ch);
  CL_ActivateRouter (theEnv, "dribble");

   /*==========================================*/
  /* Return the result of the ungetc request. */
   /*==========================================*/

  return (rv);
}

/**************************************/
/* ExitDribbleCallback: Exit callback */
/*    for the dribble router.         */
/**************************************/
static void
ExitDribbleCallback (Environment * theEnv, int num, void *context)
{
#if MAC_XCD
#pragma unused(num)
#endif

  if (FileCommandData (theEnv)->DribbleCurrentPosition > 0)
    {
      fprintf (FileCommandData (theEnv)->DribbleFP, "%s",
	       FileCommandData (theEnv)->DribbleBuffer);
    }

  if (FileCommandData (theEnv)->DribbleFP != NULL)
    CL_GenClose (theEnv, FileCommandData (theEnv)->DribbleFP);
}

/*********************************/
/* CL_DribbleOn: C access routine   */
/*   for the dribble-on command. */
/*********************************/
bool
CL_DribbleOn (Environment * theEnv, const char *fileName)
{
   /*==============================*/
  /* If a dribble file is already */
  /* open, then close it.         */
   /*==============================*/

  if (FileCommandData (theEnv)->DribbleFP != NULL)
    {
      CL_DribbleOff (theEnv);
    }

   /*========================*/
  /* Open the dribble file. */
   /*========================*/

  FileCommandData (theEnv)->DribbleFP = CL_GenOpen (theEnv, fileName, "w");
  if (FileCommandData (theEnv)->DribbleFP == NULL)
    {
      CL_OpenErrorMessage (theEnv, "dribble-on", fileName);
      return false;
    }

   /*============================*/
  /* Create the dribble router. */
   /*============================*/

  CL_AddRouter (theEnv, "dribble", 40,
		QueryDribbleCallback, CL_WriteDribbleCallback,
		ReadDribbleCallback, UnreadDribbleCallback,
		ExitDribbleCallback, NULL);

  FileCommandData (theEnv)->DribbleCurrentPosition = 0;

   /*================================================*/
  /* Call the dribble status function. This is used */
  /* by some of the machine specific interfaces to  */
  /* do things such as changing the wording of menu */
  /* items from "Turn Dribble On..." to             */
  /* "Turn Dribble Off..."                          */
   /*================================================*/

  if (FileCommandData (theEnv)->DribbleStatusFunction != NULL)
    {
      (*FileCommandData (theEnv)->DribbleStatusFunction) (theEnv, true);
    }

   /*=====================================*/
  /* Return true to indicate the dribble */
  /* file was successfully opened.       */
   /*=====================================*/

  return true;
}

/**********************************************/
/* CL_DribbleActive: Returns true if the dribble */
/*   router is active, otherwise false.       */
/**********************************************/
bool
CL_DribbleActive (Environment * theEnv)
{
  if (FileCommandData (theEnv)->DribbleFP != NULL)
    return true;

  return false;
}

/**********************************/
/* CL_DribbleOff: C access routine   */
/*   for the dribble-off command. */
/**********************************/
bool
CL_DribbleOff (Environment * theEnv)
{
  bool rv = false;

   /*================================================*/
  /* Call the dribble status function. This is used */
  /* by some of the machine specific interfaces to  */
  /* do things such as changing the wording of menu */
  /* items from "Turn Dribble On..." to             */
  /* "Turn Dribble Off..."                          */
   /*================================================*/

  if (FileCommandData (theEnv)->DribbleStatusFunction != NULL)
    {
      (*FileCommandData (theEnv)->DribbleStatusFunction) (theEnv, false);
    }

   /*=======================================*/
  /* Close the dribble file and deactivate */
  /* the dribble router.                   */
   /*=======================================*/

  if (FileCommandData (theEnv)->DribbleFP != NULL)
    {
      if (FileCommandData (theEnv)->DribbleCurrentPosition > 0)
	{
	  fprintf (FileCommandData (theEnv)->DribbleFP, "%s",
		   FileCommandData (theEnv)->DribbleBuffer);
	}
      CL_DeleteRouter (theEnv, "dribble");
      if (CL_GenClose (theEnv, FileCommandData (theEnv)->DribbleFP) == 0)
	rv = true;
    }
  else
    {
      rv = true;
    }

  FileCommandData (theEnv)->DribbleFP = NULL;

   /*============================================*/
  /* Free the space used by the dribble buffer. */
   /*============================================*/

  if (FileCommandData (theEnv)->DribbleBuffer != NULL)
    {
      CL_rm (theEnv, FileCommandData (theEnv)->DribbleBuffer,
	     FileCommandData (theEnv)->DribbleMaximumPosition);
      FileCommandData (theEnv)->DribbleBuffer = NULL;
    }

  FileCommandData (theEnv)->DribbleCurrentPosition = 0;
  FileCommandData (theEnv)->DribbleMaximumPosition = 0;

   /*============================================*/
  /* Return true if the dribble file was closed */
  /* without error, otherwise return false.     */
   /*============================================*/

  return (rv);
}

#endif /* DEBUGGING_FUNCTIONS */

/**************************************/
/* Query_BatchCallback: Query callback */
/*    for the batch router.           */
/**************************************/
static bool
Query_BatchCallback (Environment * theEnv,
		     const char *logicalName, void *context)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif

  if (strcmp (logicalName, STDIN) == 0)
    {
      return true;
    }

  return false;
}

/************************************/
/* Read_BatchCallback: Read callback */
/*    for the batch router.         */
/************************************/
static int
Read_BatchCallback (Environment * theEnv,
		    const char *logicalName, void *context)
{
  return (LLGetc_Batch (theEnv, logicalName, false));
}

/***************************************************/
/* LLGetc_Batch: Lower level routine for retrieving */
/*   a character when a batch file is active.      */
/***************************************************/
int
LLGetc_Batch (Environment * theEnv, const char *logicalName, bool returnOnEOF)
{
  int rv = EOF, flag = 1;

   /*=================================================*/
  /* Get a character until a valid character appears */
  /* or no more batch files are left.                */
   /*=================================================*/

  while ((rv == EOF) && (flag == 1))
    {
      if (FileCommandData (theEnv)->CL_BatchType == FILE_BATCH)
	{
	  rv = getc (FileCommandData (theEnv)->CL_BatchFileSource);
	}
      else
	{
	  rv =
	    CL_ReadRouter (theEnv,
			   FileCommandData (theEnv)->CL_BatchLogicalSource);
	}

      if (rv == EOF)
	{
	  if (FileCommandData (theEnv)->CL_BatchCurrentPosition > 0)
	    CL_WriteString (theEnv, STDOUT,
			    (char *)
			    FileCommandData (theEnv)->CL_BatchBuffer);
	  flag = Remove_Batch (theEnv);
	}
    }

   /*=========================================================*/
  /* If the character retrieved is an end-of-file character, */
  /* then there are no batch files with character input      */
  /* re_maining. Remove the batch router.                     */
   /*=========================================================*/

  if (rv == EOF)
    {
      if (FileCommandData (theEnv)->CL_BatchCurrentPosition > 0)
	CL_WriteString (theEnv, STDOUT,
			(char *) FileCommandData (theEnv)->CL_BatchBuffer);
      CL_DeleteRouter (theEnv, "batch");
      Remove_Batch (theEnv);
      if (returnOnEOF == true)
	{
	  return (EOF);
	}
      else
	{
	  return CL_ReadRouter (theEnv, logicalName);
	}
    }

   /*========================================*/
  /* Add the character to the batch buffer. */
   /*========================================*/

  if (RouterData (theEnv)->InputUngets == 0)
    {
      FileCommandData (theEnv)->CL_BatchBuffer =
	CL_ExpandStringWithChar (theEnv, (char) rv,
				 FileCommandData (theEnv)->CL_BatchBuffer,
				 &FileCommandData
				 (theEnv)->CL_BatchCurrentPosition,
				 &FileCommandData
				 (theEnv)->CL_BatchMaximumPosition,
				 FileCommandData
				 (theEnv)->CL_BatchMaximumPosition +
				 BUFFER_SIZE);
    }

   /*======================================*/
  /* If a carriage return is encountered, */
  /* then flush the batch buffer.         */
   /*======================================*/

  if ((char) rv == '\n')
    {
      CL_WriteString (theEnv, STDOUT,
		      (char *) FileCommandData (theEnv)->CL_BatchBuffer);
      FileCommandData (theEnv)->CL_BatchCurrentPosition = 0;
      if ((FileCommandData (theEnv)->CL_BatchBuffer != NULL)
	  && (FileCommandData (theEnv)->CL_BatchMaximumPosition >
	      BUFFER_SIZE))
	{
	  CL_rm (theEnv, FileCommandData (theEnv)->CL_BatchBuffer,
		 FileCommandData (theEnv)->CL_BatchMaximumPosition);
	  FileCommandData (theEnv)->CL_BatchMaximumPosition = 0;
	  FileCommandData (theEnv)->CL_BatchBuffer = NULL;
	}
    }

   /*=============================*/
  /* Increment the line counter. */
   /*=============================*/

  if (((char) rv == '\r') || ((char) rv == '\n'))
    {
      CL_IncrementLineCount (theEnv);
    }

   /*=====================================================*/
  /* Return the character retrieved from the batch file. */
   /*=====================================================*/

  return (rv);
}

/****************************************/
/* Unread_BatchCallback: Unread callback */
/*    for the batch router.             */
/****************************************/
static int
Unread_BatchCallback (Environment * theEnv,
		      const char *logicalName, int ch, void *context)
{
#if MAC_XCD
#pragma unused(logicalName)
#endif

  if (FileCommandData (theEnv)->CL_BatchCurrentPosition > 0)
    FileCommandData (theEnv)->CL_BatchCurrentPosition--;
  if (FileCommandData (theEnv)->CL_BatchBuffer != NULL)
    FileCommandData (theEnv)->CL_BatchBuffer[FileCommandData (theEnv)->
					     CL_BatchCurrentPosition] = EOS;
  if (FileCommandData (theEnv)->CL_BatchType == FILE_BATCH)
    {
      return ungetc (ch, FileCommandData (theEnv)->CL_BatchFileSource);
    }

  return CL_UnreadRouter (theEnv,
			  FileCommandData (theEnv)->CL_BatchLogicalSource,
			  ch);
}

/************************************/
/* Exit_BatchCallback: Exit callback */
/*    for the batch router.         */
/************************************/
static void
Exit_BatchCallback (Environment * theEnv, int num, void *context)
{
#if MAC_XCD
#pragma unused(num,context)
#endif
  CloseAll_BatchSources (theEnv);
}

/**************************************************/
/* CL_Batch: C access routine for the batch command. */
/**************************************************/
bool
CL_Batch (Environment * theEnv, const char *fileName)
{
  return (Open_Batch (theEnv, fileName, false));
}

/***********************************************/
/* Open_Batch: Adds a file to the list of files */
/*   opened with the batch command.            */
/***********************************************/
bool
Open_Batch (Environment * theEnv, const char *fileName, bool placeAtEnd)
{
  FILE *theFile;

   /*======================*/
  /* Open the batch file. */
   /*======================*/

  theFile = CL_GenOpen (theEnv, fileName, "r");

  if (theFile == NULL)
    {
      CL_OpenErrorMessage (theEnv, "batch", fileName);
      return false;
    }

   /*============================*/
  /* Create the batch router if */
  /* it doesn't already exist.  */
   /*============================*/

  if (FileCommandData (theEnv)->TopOf_BatchList == NULL)
    {
      CL_AddRouter (theEnv, "batch", 20, Query_BatchCallback, NULL,
		    Read_BatchCallback, Unread_BatchCallback,
		    Exit_BatchCallback, NULL);
    }

   /*===============================================================*/
  /* If a batch file is already open, save its current line count. */
   /*===============================================================*/

  if (FileCommandData (theEnv)->TopOf_BatchList != NULL)
    {
      FileCommandData (theEnv)->TopOf_BatchList->lineNumber =
	CL_GetLineCount (theEnv);
    }

#if (! RUN_TIME) && (! BLOAD_ONLY)

   /*========================================================================*/
  /* If this is the first batch file, remember the prior parsing file name. */
   /*========================================================================*/

  if (FileCommandData (theEnv)->TopOf_BatchList == NULL)
    {
      FileCommandData (theEnv)->batchPriorParsingFile =
	CL_CopyString (theEnv, CL_GetParsingFileName (theEnv));
    }

   /*=======================================================*/
  /* Create the error capture router if it does not exist. */
   /*=======================================================*/

  CL_SetParsingFileName (theEnv, fileName);
  CL_SetLineCount (theEnv, 0);

  CL_CreateErrorCaptureRouter (theEnv);
#endif

   /*====================================*/
  /* Add the newly opened batch file to */
  /* the list of batch files opened.    */
   /*====================================*/

  Add_Batch (theEnv, placeAtEnd, theFile, NULL, FILE_BATCH, NULL, fileName);

   /*===================================*/
  /* Return true to indicate the batch */
  /* file was successfully opened.     */
   /*===================================*/

  return true;
}

/*****************************************************************/
/* OpenString_Batch: Opens a string source for batch processing.  */
/*   The memory allocated for the argument stringName must be    */
/*   deallocated by the user. The memory allocated for theString */
/*   will be deallocated by the batch routines when batch        */
/*   processing for the  string is completed.                    */
/*****************************************************************/
bool
OpenString_Batch (Environment * theEnv,
		  const char *stringName,
		  const char *theString, bool placeAtEnd)
{
  if (CL_OpenStringSource (theEnv, stringName, theString, 0) == false)
    {
      return false;
    }

  if (FileCommandData (theEnv)->TopOf_BatchList == NULL)
    {
      CL_AddRouter (theEnv, "batch", 20,
		    Query_BatchCallback, NULL,
		    Read_BatchCallback, Unread_BatchCallback,
		    Exit_BatchCallback, NULL);
    }

  Add_Batch (theEnv, placeAtEnd, NULL, stringName, STRING_BATCH, theString,
	     NULL);

  return true;
}

/*******************************************************/
/* Add_Batch: Creates the batch file data structure and */
/*   adds it to the list of opened batch files.        */
/*******************************************************/
static void
Add_Batch (Environment * theEnv,
	   bool placeAtEnd,
	   FILE * theFileSource,
	   const char *theLogicalSource,
	   int type, const char *theString, const char *theFileName)
{
  struct batchEntry *bptr;

   /*=========================*/
  /* Create the batch entry. */
   /*=========================*/

  bptr = get_struct (theEnv, batchEntry);
  bptr->batchType = type;
  bptr->fileSource = theFileSource;
  bptr->logicalSource = CL_CopyString (theEnv, theLogicalSource);
  bptr->theString = theString;
  bptr->fileName = CL_CopyString (theEnv, theFileName);
  bptr->lineNumber = 0;
  bptr->next = NULL;

   /*============================*/
  /* Add the entry to the list. */
   /*============================*/

  if (FileCommandData (theEnv)->TopOf_BatchList == NULL)
    {
      FileCommandData (theEnv)->TopOf_BatchList = bptr;
      FileCommandData (theEnv)->BottomOf_BatchList = bptr;
      FileCommandData (theEnv)->CL_BatchType = type;
      FileCommandData (theEnv)->CL_BatchFileSource = theFileSource;
      FileCommandData (theEnv)->CL_BatchLogicalSource = bptr->logicalSource;
      FileCommandData (theEnv)->CL_BatchCurrentPosition = 0;
    }
  else if (placeAtEnd == false)
    {
      bptr->next = FileCommandData (theEnv)->TopOf_BatchList;
      FileCommandData (theEnv)->TopOf_BatchList = bptr;
      FileCommandData (theEnv)->CL_BatchType = type;
      FileCommandData (theEnv)->CL_BatchFileSource = theFileSource;
      FileCommandData (theEnv)->CL_BatchLogicalSource = bptr->logicalSource;
      FileCommandData (theEnv)->CL_BatchCurrentPosition = 0;
    }
  else
    {
      FileCommandData (theEnv)->BottomOf_BatchList->next = bptr;
      FileCommandData (theEnv)->BottomOf_BatchList = bptr;
    }
}

/******************************************************************/
/* Remove_Batch: Removes the top entry on the list of batch files. */
/******************************************************************/
bool
Remove_Batch (Environment * theEnv)
{
  struct batchEntry *bptr;
  bool rv, file_Batch = false;

  if (FileCommandData (theEnv)->TopOf_BatchList == NULL)
    return false;

   /*==================================================*/
  /* Close the source from which batch input is read. */
   /*==================================================*/

  if (FileCommandData (theEnv)->TopOf_BatchList->batchType == FILE_BATCH)
    {
      file_Batch = true;
      CL_GenClose (theEnv,
		   FileCommandData (theEnv)->TopOf_BatchList->fileSource);
#if (! RUN_TIME) && (! BLOAD_ONLY)
      CL_FlushParsingMessages (theEnv);
      CL_DeleteErrorCaptureRouter (theEnv);
#endif
    }
  else
    {
      CL_CloseStringSource (theEnv,
			    FileCommandData (theEnv)->TopOf_BatchList->
			    logicalSource);
      CL_rm (theEnv,
	     (void *) FileCommandData (theEnv)->TopOf_BatchList->theString,
	     strlen (FileCommandData (theEnv)->TopOf_BatchList->theString) +
	     1);
    }

   /*=================================*/
  /* Remove the entry from the list. */
   /*=================================*/

  CL_DeleteString (theEnv,
		   (char *) FileCommandData (theEnv)->TopOf_BatchList->
		   fileName);
  bptr = FileCommandData (theEnv)->TopOf_BatchList;
  FileCommandData (theEnv)->TopOf_BatchList =
    FileCommandData (theEnv)->TopOf_BatchList->next;

  CL_DeleteString (theEnv, (char *) bptr->logicalSource);
  rtn_struct (theEnv, batchEntry, bptr);

   /*========================================================*/
  /* If there are no batch files re_maining to be processed, */
  /* then free the space used by the batch buffer.          */
   /*========================================================*/

  if (FileCommandData (theEnv)->TopOf_BatchList == NULL)
    {
      FileCommandData (theEnv)->BottomOf_BatchList = NULL;
      FileCommandData (theEnv)->CL_BatchFileSource = NULL;
      FileCommandData (theEnv)->CL_BatchLogicalSource = NULL;
      if (FileCommandData (theEnv)->CL_BatchBuffer != NULL)
	{
	  CL_rm (theEnv, FileCommandData (theEnv)->CL_BatchBuffer,
		 FileCommandData (theEnv)->CL_BatchMaximumPosition);
	  FileCommandData (theEnv)->CL_BatchBuffer = NULL;
	}
      FileCommandData (theEnv)->CL_BatchCurrentPosition = 0;
      FileCommandData (theEnv)->CL_BatchMaximumPosition = 0;
      rv = false;

#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (file_Batch)
	{
	  CL_SetParsingFileName (theEnv,
				 FileCommandData
				 (theEnv)->batchPriorParsingFile);
	  CL_DeleteString (theEnv,
			   FileCommandData (theEnv)->batchPriorParsingFile);
	  FileCommandData (theEnv)->batchPriorParsingFile = NULL;
	}
#endif
    }

   /*===========================================*/
  /* Otherwise move on to the next batch file. */
   /*===========================================*/

  else
    {
      FileCommandData (theEnv)->CL_BatchType =
	FileCommandData (theEnv)->TopOf_BatchList->batchType;
      FileCommandData (theEnv)->CL_BatchFileSource =
	FileCommandData (theEnv)->TopOf_BatchList->fileSource;
      FileCommandData (theEnv)->CL_BatchLogicalSource =
	FileCommandData (theEnv)->TopOf_BatchList->logicalSource;
      FileCommandData (theEnv)->CL_BatchCurrentPosition = 0;
      rv = true;
#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (FileCommandData (theEnv)->TopOf_BatchList->batchType == FILE_BATCH)
	{
	  CL_SetParsingFileName (theEnv,
				 FileCommandData (theEnv)->TopOf_BatchList->
				 fileName);
	}

      CL_SetLineCount (theEnv,
		       FileCommandData (theEnv)->TopOf_BatchList->lineNumber);
#endif
    }

   /*====================================================*/
  /* Return true if a batch file if there are re_maining */
  /* batch files to be processed, otherwise false.      */
   /*====================================================*/

  return (rv);
}

/****************************************/
/* CL_BatchActive: Returns true if a batch */
/*   file is open, otherwise false.     */
/****************************************/
bool
CL_BatchActive (Environment * theEnv)
{
  if (FileCommandData (theEnv)->TopOf_BatchList != NULL)
    return true;

  return false;
}

/******************************************************/
/* CloseAll_BatchSources: Closes all open batch files. */
/******************************************************/
void
CloseAll_BatchSources (Environment * theEnv)
{
   /*================================================*/
  /* Free the batch buffer if it contains anything. */
   /*================================================*/

  if (FileCommandData (theEnv)->CL_BatchBuffer != NULL)
    {
      if (FileCommandData (theEnv)->CL_BatchCurrentPosition > 0)
	CL_WriteString (theEnv, STDOUT,
			(char *) FileCommandData (theEnv)->CL_BatchBuffer);
      CL_rm (theEnv, FileCommandData (theEnv)->CL_BatchBuffer,
	     FileCommandData (theEnv)->CL_BatchMaximumPosition);
      FileCommandData (theEnv)->CL_BatchBuffer = NULL;
      FileCommandData (theEnv)->CL_BatchCurrentPosition = 0;
      FileCommandData (theEnv)->CL_BatchMaximumPosition = 0;
    }

   /*==========================*/
  /* Delete the batch router. */
   /*==========================*/

  CL_DeleteRouter (theEnv, "batch");

   /*=====================================*/
  /* Close each of the open batch files. */
   /*=====================================*/

  while (Remove_Batch (theEnv))
    {				/* Do Nothing */
    }
}

#if ! RUN_TIME

/*******************************************************/
/* CL_BatchStar: C access routine for the batch* command. */
/*******************************************************/
bool
CL_BatchStar (Environment * theEnv, const char *fileName)
{
  int inchar;
  bool done = false;
  FILE *theFile;
  char *theString = NULL;
  size_t position = 0;
  size_t maxChars = 0;
#if (! RUN_TIME) && (! BLOAD_ONLY)
  char *oldParsingFileName;
  long oldLineCountValue;
#endif
   /*======================*/
  /* Open the batch file. */
   /*======================*/

  theFile = CL_GenOpen (theEnv, fileName, "r");

  if (theFile == NULL)
    {
      CL_OpenErrorMessage (theEnv, "batch", fileName);
      return false;
    }

   /*======================================*/
  /* Setup for capturing errors/warnings. */
   /*======================================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
  oldParsingFileName = CL_CopyString (theEnv, CL_GetParsingFileName (theEnv));
  CL_SetParsingFileName (theEnv, fileName);

  CL_CreateErrorCaptureRouter (theEnv);

  oldLineCountValue = CL_SetLineCount (theEnv, 1);
#endif

   /*=====================================*/
  /* If embedded, clear the error flags. */
   /*=====================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_ResetErrorFlags (theEnv);
    }

   /*=============================================*/
  /* CL_Evaluate commands from the file one by one. */
   /*=============================================*/

  while (!done)
    {
      inchar = getc (theFile);
      if (inchar == EOF)
	{
	  inchar = '\n';
	  done = true;
	}

      theString =
	CL_ExpandStringWithChar (theEnv, inchar, theString, &position,
				 &maxChars, maxChars + 80);

      if (CL_CompleteCommand (theString) != 0)
	{
	  CL_FlushPPBuffer (theEnv);
	  CL_SetPPBufferStatus (theEnv, false);
	  CL_RouteCommand (theEnv, theString, false);
	  CL_FlushPPBuffer (theEnv);
	  Set_HaltExecution (theEnv, false);
	  Set_EvaluationError (theEnv, false);
	  CL_FlushBindList (theEnv, NULL);
	  CL_genfree (theEnv, theString, maxChars);
	  theString = NULL;
	  maxChars = 0;
	  position = 0;
#if (! RUN_TIME) && (! BLOAD_ONLY)
	  CL_FlushParsingMessages (theEnv);
#endif
	}

      if ((inchar == '\r') || (inchar == '\n'))
	{
	  CL_IncrementLineCount (theEnv);
	}
    }

  if (theString != NULL)
    {
      CL_genfree (theEnv, theString, maxChars);
    }

   /*=======================*/
  /* Close the batch file. */
   /*=======================*/

  CL_GenClose (theEnv, theFile);

   /*========================================*/
  /* Cleanup for capturing errors/warnings. */
   /*========================================*/

#if (! RUN_TIME) && (! BLOAD_ONLY)
  CL_FlushParsingMessages (theEnv);
  CL_DeleteErrorCaptureRouter (theEnv);

  CL_SetLineCount (theEnv, oldLineCountValue);

  CL_SetParsingFileName (theEnv, oldParsingFileName);
  CL_DeleteString (theEnv, oldParsingFileName);
#endif

  return true;
}

#else

/***********************************************/
/* CL_BatchStar: This is the non-functional stub  */
/*   provided for use with a run-time version. */
/***********************************************/
bool
CL_BatchStar (Environment * theEnv, const char *fileName)
{
  CL_PrintErrorID (theEnv, "FILECOM", 1, false);
  CL_WriteString (theEnv, STDERR,
		  "Function batch* does not work in run time modules.\n");
  return false;
}

#endif
