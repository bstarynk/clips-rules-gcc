   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/19/17             */
   /*                                                     */
   /*               FILE I/O ROUTER MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: I/O Router routines which allow files to be used */
/*   as input and output sources.                            */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Added pragmas to remove compilation warnings.  */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Used gengetc and CL_genungetchar rather than      */
/*            getc and ungetc.                               */
/*                                                           */
/*            Replaced BASIC_IO and ADVANCED_IO compiler     */
/*            flags with the single IO_FUNCTIONS compiler    */
/*            flag.                                          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Added STDOUT and STDIN logical name            */
/*            definitions.                                   */
/*                                                           */
/*      6.31: Output to logical WERROR is now sent to stderr */
/*            rather than stdout.                            */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Changed return values for router functions.    */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Added flush, rewind, tell, and seek functions. */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "router.h"
#include "sysdep.h"

#include "filertr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static void ExitFileCallback (Environment *, int, void *);
static void CL_WriteFileCallback (Environment *, const char *, const char *,
				  void *);
static int ReadFileCallback (Environment *, const char *, void *);
static int UnreadFileCallback (Environment *, const char *, int, void *);
static void DeallocateFileRouterData (Environment *);

/***************************************************************/
/* CL_InitializeFileRouter: Initializes file input/output router. */
/***************************************************************/
void
CL_InitializeFileRouter (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, FILE_ROUTER_DATA,
			      sizeof (struct fileRouterData),
			      DeallocateFileRouterData);

  CL_AddRouter (theEnv, "fileio", 0, CL_FindFile,
		CL_WriteFileCallback, ReadFileCallback,
		UnreadFileCallback, ExitFileCallback, NULL);
}

/*****************************************/
/* DeallocateFileRouterData: Deallocates */
/*    environment data for file routers. */
/*****************************************/
static void
DeallocateFileRouterData (Environment * theEnv)
{
  struct fileRouter *tmpPtr, *nextPtr;

  tmpPtr = FileRouterData (theEnv)->ListOfFileRouters;
  while (tmpPtr != NULL)
    {
      nextPtr = tmpPtr->next;
      CL_GenClose (theEnv, tmpPtr->stream);
      CL_rm (theEnv, (void *) tmpPtr->logicalName,
	     strlen (tmpPtr->logicalName) + 1);
      rtn_struct (theEnv, fileRouter, tmpPtr);
      tmpPtr = nextPtr;
    }
}

/*****************************************/
/* CL_FindFptr: Returns a pointer to a file */
/*   stream for a given logical name.    */
/*****************************************/
FILE *
CL_FindFptr (Environment * theEnv, const char *logicalName)
{
  struct fileRouter *fptr;

   /*========================================================*/
  /* Check to see if standard input or output is requested. */
   /*========================================================*/

  if (strcmp (logicalName, STDOUT) == 0)
    {
      return stdout;
    }
  else if (strcmp (logicalName, STDIN) == 0)
    {
      return stdin;
    }
  else if (strcmp (logicalName, STDERR) == 0)
    {
      return stderr;
    }
  else if (strcmp (logicalName, STDWRN) == 0)
    {
      return stdout;
    }

   /*==============================================================*/
  /* Otherwise, look up the logical name on the global file list. */
   /*==============================================================*/

  fptr = FileRouterData (theEnv)->ListOfFileRouters;
  while ((fptr != NULL) ? (strcmp (logicalName, fptr->logicalName) !=
			   0) : false)
    {
      fptr = fptr->next;
    }

  if (fptr != NULL)
    return fptr->stream;

  return NULL;
}

/*****************************************************/
/* CL_FindFile: Find routine for file router logical    */
/*   names. Returns true if the specified logical    */
/*   name has an associated file stream (which means */
/*   that the logical name can be handled by the     */
/*   file router). Otherwise, false is returned.     */
/*****************************************************/
bool
CL_FindFile (Environment * theEnv, const char *logicalName, void *context)
{
  if (CL_FindFptr (theEnv, logicalName) != NULL)
    return true;

  return false;
}

/***************************************************/
/* ExitFileCallback: Exit routine for file router. */
/***************************************************/
static void
ExitFileCallback (Environment * theEnv, int num, void *context)
{
#if MAC_XCD
#pragma unused(num)
#endif
#if IO_FUNCTIONS
  CL_CloseAllFiles (theEnv);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
}

/******************************************************/
/* CL_WriteFileCallback: CL_Write callback for file router. */
/******************************************************/
static void
CL_WriteFileCallback (Environment * theEnv,
		      const char *logicalName, const char *str, void *context)
{
  FILE *fptr;

  fptr = CL_FindFptr (theEnv, logicalName);

  CL_genprintfile (theEnv, fptr, str);
}

/****************************************************/
/* ReadFileCallback: Read callback for file router. */
/****************************************************/
static int
ReadFileCallback (Environment * theEnv,
		  const char *logicalName, void *context)
{
  FILE *fptr;
  int theChar;

  fptr = CL_FindFptr (theEnv, logicalName);

  if (fptr == stdin)
    {
      theChar = CL_gengetchar (theEnv);
    }
  else
    {
      theChar = getc (fptr);
    }

   /*=================================================*/
  /* The following code prevents Control-D on UNIX   */
  /* machines from te_rminating all input from stdin. */
   /*=================================================*/

  if ((fptr == stdin) && (theChar == EOF))
    clearerr (stdin);

  return theChar;
}

/********************************************************/
/* UnreadFileCallback: Unread callback for file router. */
/********************************************************/
static int
UnreadFileCallback (Environment * theEnv,
		    const char *logicalName, int ch, void *context)
{
  FILE *fptr;

  fptr = CL_FindFptr (theEnv, logicalName);

  if (fptr == stdin)
    {
      return CL_genungetchar (theEnv, ch);
    }
  else
    {
      return ungetc (ch, fptr);
    }
}

/*********************************************************/
/* OpenFile: Opens a file with the specified access mode */
/*   and stores the opened stream on the list of files   */
/*   associated with logical names Returns true if the   */
/*   file was succesfully opened, otherwise false.       */
/*********************************************************/
bool
CL_OpenAFile (Environment * theEnv,
	      const char *fileName,
	      const char *accessMode, const char *logicalName)
{
  FILE *newstream;
  struct fileRouter *newRouter;
  char *theName;

   /*==================================*/
  /* Make sure the file can be opened */
  /* with the specified access mode.  */
   /*==================================*/

  if ((newstream = CL_GenOpen (theEnv, fileName, accessMode)) == NULL)
    {
      return false;
    }

   /*===========================*/
  /* Create a new file router. */
   /*===========================*/

  newRouter = get_struct (theEnv, fileRouter);
  theName = (char *) CL_gm2 (theEnv, strlen (logicalName) + 1);
  CL_genstrcpy (theName, logicalName);
  newRouter->logicalName = theName;
  newRouter->stream = newstream;

   /*==========================================*/
  /* Add the newly opened file to the list of */
  /* files associated with logical names.     */
   /*==========================================*/

  newRouter->next = FileRouterData (theEnv)->ListOfFileRouters;
  FileRouterData (theEnv)->ListOfFileRouters = newRouter;

   /*==================================*/
  /* Return true to indicate the file */
  /* was opened successfully.         */
   /*==================================*/

  return true;
}

/*************************************************************/
/* CL_CloseFile: Closes the file associated with the specified  */
/*   logical name. Returns true if the file was successfully */
/*   closed, otherwise false.                                */
/*************************************************************/
bool
CL_CloseFile (Environment * theEnv, const char *fid)
{
  struct fileRouter *fptr, *prev;

  for (fptr = FileRouterData (theEnv)->ListOfFileRouters, prev = NULL;
       fptr != NULL; fptr = fptr->next)
    {
      if (strcmp (fptr->logicalName, fid) == 0)
	{
	  CL_GenClose (theEnv, fptr->stream);
	  CL_rm (theEnv, (void *) fptr->logicalName,
		 strlen (fptr->logicalName) + 1);
	  if (prev == NULL)
	    {
	      FileRouterData (theEnv)->ListOfFileRouters = fptr->next;
	    }
	  else
	    {
	      prev->next = fptr->next;
	    }
	  CL_rm (theEnv, fptr, sizeof (struct fileRouter));

	  return true;
	}

      prev = fptr;
    }

  return false;
}

/**********************************************/
/* CL_CloseAllFiles: Closes all files associated */
/*   with a file I/O router. Returns true if  */
/*   any file was closed, otherwise false.    */
/**********************************************/
bool
CL_CloseAllFiles (Environment * theEnv)
{
  struct fileRouter *fptr, *prev;

  if (FileRouterData (theEnv)->ListOfFileRouters == NULL)
    return false;

  fptr = FileRouterData (theEnv)->ListOfFileRouters;

  while (fptr != NULL)
    {
      CL_GenClose (theEnv, fptr->stream);
      prev = fptr;
      CL_rm (theEnv, (void *) fptr->logicalName,
	     strlen (fptr->logicalName) + 1);
      fptr = fptr->next;
      CL_rm (theEnv, prev, sizeof (struct fileRouter));
    }

  FileRouterData (theEnv)->ListOfFileRouters = NULL;

  return true;
}

/*************************************************************/
/* CL_FlushFile: Flushes the file associated with the specified */
/*   logical name. Returns true if the file was successfully */
/*   flushed, otherwise false.                               */
/*************************************************************/
bool
CL_FlushFile (Environment * theEnv, const char *fid)
{
  struct fileRouter *fptr;

  for (fptr = FileRouterData (theEnv)->ListOfFileRouters;
       fptr != NULL; fptr = fptr->next)
    {
      if (strcmp (fptr->logicalName, fid) == 0)
	{
	  CL_GenFlush (theEnv, fptr->stream);
	  return true;
	}
    }

  return false;
}

/***********************************************/
/* CL_FlushAllFiles: Flushes all files associated */
/*   with a file I/O router. Returns true if   */
/*   any file was flushed, otherwise false.    */
/***********************************************/
bool
CL_FlushAllFiles (Environment * theEnv)
{
  struct fileRouter *fptr;

  if (FileRouterData (theEnv)->ListOfFileRouters == NULL)
    return false;

  for (fptr = FileRouterData (theEnv)->ListOfFileRouters;
       fptr != NULL; fptr = fptr->next)
    {
      CL_GenFlush (theEnv, fptr->stream);
    }

  return true;
}

/*****************************************************/
/* CL_RewindFile: Rewinds the file associated with the  */
/*   specified logical name. Returns true if the     */
/*   file was successfully rewound, otherwise false. */
/*****************************************************/
bool
CL_RewindFile (Environment * theEnv, const char *fid)
{
  struct fileRouter *fptr;

  for (fptr = FileRouterData (theEnv)->ListOfFileRouters;
       fptr != NULL; fptr = fptr->next)
    {
      if (strcmp (fptr->logicalName, fid) == 0)
	{
	  CL_GenRewind (theEnv, fptr->stream);
	  return true;
	}
    }

  return false;
}

/**************************************************/
/* CL_TellFile: Returns the file position associated */
/*   with the specified logical name.             */
/**************************************************/
long long
CL_TellFile (Environment * theEnv, const char *fid)
{
  struct fileRouter *fptr;

  for (fptr = FileRouterData (theEnv)->ListOfFileRouters;
       fptr != NULL; fptr = fptr->next)
    {
      if (strcmp (fptr->logicalName, fid) == 0)
	{
	  return CL_GenTell (theEnv, fptr->stream);
	}
    }

  return LLONG_MIN;
}

/***********************************************/
/* CL_SeekFile: Sets the file position associated */
/*   with the specified logical name.          */
/***********************************************/
bool
CL_SeekFile (Environment * theEnv,
	     const char *fid, long offset, int whereFrom)
{
  struct fileRouter *fptr;

  for (fptr = FileRouterData (theEnv)->ListOfFileRouters;
       fptr != NULL; fptr = fptr->next)
    {
      if (strcmp (fptr->logicalName, fid) == 0)
	{
	  if (CL_GenSeek (theEnv, fptr->stream, offset, whereFrom))
	    {
	      return false;
	    }
	  else
	    {
	      return true;
	    }
	}
    }

  return false;
}
