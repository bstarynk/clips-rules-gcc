   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/05/18             */
   /*                                                     */
   /*               SYSTEM DEPENDENT MODULE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Isolation of system dependent routines.          */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Modified CL_GenOpen to check the file length      */
/*            against the system constant FILENAME_MAX.      */
/*                                                           */
/*      6.24: Support for run-time programs directly passing */
/*            the hash tables for initialization.            */
/*                                                           */
/*            Made CL_gensystem functional for Xcode.           */
/*                                                           */
/*            Added Before_OpenFunction and After_OpenFunction */
/*            hooks.                                         */
/*                                                           */
/*            Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Updated UNIX_V CL_gentime functionality.          */
/*                                                           */
/*            Removed CL_GenOpen check against FILENAME_MAX.    */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, IBM_ICB, IBM_TBC, IBM_ZTC, and        */
/*            IBM_SC).                                       */
/*                                                           */
/*            Renamed IBM_MSC and WIN_MVC compiler flags     */
/*            and IBM_GCC to WIN_GCC.                        */
/*                                                           */
/*            Added LINUX and DARWIN compiler flags.         */
/*                                                           */
/*            Removed HELP_FUNCTIONS compilation flag and    */
/*            associated functionality.                      */
/*                                                           */
/*            Removed EMACS_EDITOR compilation flag and      */
/*            associated functionality.                      */
/*                                                           */
/*            Combined BASIC_IO and EXT_IO compilation       */
/*            flags into the single IO_FUNCTIONS flag.       */
/*                                                           */
/*            Changed the EX_MATH compilation flag to        */
/*            EXTENDED_MATH_FUNCTIONS.                       */
/*                                                           */
/*            Support for typed EXTERNAL_ADDRESS_TYPE.       */
/*                                                           */
/*            CL_GenOpen function checks for UTF-8 Byte Order   */
/*            Marker.                                        */
/*                                                           */
/*            Added CL_gengetchar, CL_genungetchar, CL_genprintfile,  */
/*            CL_genstrcpy, CL_genstrncpy, CL_genstrcat, CL_genstrncat,  */
/*            and CL_gensprintf functions.                      */
/*                                                           */
/*            Added CL_SetJmpBuffer function.                   */
/*                                                           */
/*            Added environment argument to CL_genexit.         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Added CL_genchdir function for changing the       */
/*            current directory.                             */
/*                                                           */
/*            Modified CL_gentime to return "comparable" epoch  */
/*            based values across platfo_rms.                 */
/*                                                           */
/*            Refactored code to reduce header dependencies  */
/*            in sysdep.c.                                   */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Moved CatchCtrlC to CL_main.c.                    */
/*                                                           */
/*            Removed VAX_VMS support.                       */
/*                                                           */
/*            Removed ContinueEnvFunction, PauseEnvFunction, */
/*            and RedrawScreenFunction callbacks.            */
/*                                                           */
/*            Completion code now returned by CL_gensystem.     */
/*                                                           */
/*            Added flush, rewind, tell, and seek functions. */
/*                                                           */
/*            Removed fflush calls after printing.           */
/*                                                           */
/*            CL_GenOpen, CL_genchdir, CL_genremove, and CL_genrename    */
/*            use wide character functions on Windows to     */
/*            work properly with file and directory names    */
/*            containing unicode characters.                 */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>

#if MAC_XCD
#include <sys/time.h>
#include <unistd.h>
#endif

#if WIN_MVC
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <signal.h>
#endif

#if   UNIX_7 || WIN_GCC
#include <sys/types.h>
#include <sys/timeb.h>
#include <signal.h>
#endif

#if   UNIX_V || LINUX || DARWIN
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#endif

#include "envrnmnt.h"
#include "memalloc.h"
#include "sysdep.h"

/********************/
/* ENVIRONMENT DATA */
/********************/

#define SYSTEM_DEPENDENT_DATA 58

struct systemDependentData
{
#if WIN_MVC
  int BinaryFileHandle;
  unsigned char getcBuffer[7];
  int getcLength;
  int getcPosition;
#endif
#if (! WIN_MVC)
  FILE *BinaryFP;
#endif
  int (*Before_OpenFunction) (Environment *);
  int (*After_OpenFunction) (Environment *);
  jmp_buf *jmpBuffer;
};

#define SystemDependentData(theEnv) ((struct systemDependentData *) GetEnvironmentData(theEnv,SYSTEM_DEPENDENT_DATA))

/********************************************************/
/* CL_InitializeSystemDependentData: Allocates environment */
/*    data for system dependent routines.               */
/********************************************************/
void
CL_InitializeSystemDependentData (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, SYSTEM_DEPENDENT_DATA,
			      sizeof (struct systemDependentData), NULL);
}

/*********************************************************/
/* CL_gentime: A function to return a floating point number */
/*   which indicates the present time. Used internally   */
/*   for timing rule firings and debugging.              */
/*********************************************************/
double
CL_gentime ()
{
#if MAC_XCD || UNIX_V || DARWIN || LINUX || UNIX_7
  struct timeval now;
  gettimeofday (&now, 0);
  return (now.tv_usec / 1000000.0) + now.tv_sec;
#elif WIN_MVC
  FILETIME ft;
  unsigned long long tt;

  GetSystemTimeAsFileTime (&ft);
  tt = ft.dwHighDateTime;
  tt <<= 32;
  tt |= ft.dwLowDateTime;
  tt /= 10;
  tt -= 11644473600000000ULL;
  return (double) tt / 1000000.0;
#else
  return ((double) time (NULL));
#endif
}

/*****************************************************/
/* CL_gensystem: Generic routine for passing a string   */
/*   representing a command to the operating system. */
/*****************************************************/
int
CL_gensystem (Environment * theEnv, const char *commandBuffer)
{
  return system (commandBuffer);
}

/*******************************************/
/* CL_gengetchar: Generic routine for getting */
/*    a character from stdin.              */
/*******************************************/
int
CL_gengetchar (Environment * theEnv)
{
  return (getc (stdin));
}

/***********************************************/
/* CL_genungetchar: Generic routine for ungetting */
/*    a character from stdin.                  */
/***********************************************/
int
CL_genungetchar (Environment * theEnv, int theChar)
{
  return (ungetc (theChar, stdin));
}

/****************************************************/
/* CL_genprintfile: Generic routine for print a single */
/*   character string to a file (including stdout). */
/****************************************************/
void
CL_genprintfile (Environment * theEnv, FILE * fptr, const char *str)
{
  fprintf (fptr, "%s", str);
}

/***********************************************************/
/* CL_InitializeNonportableFeatures: Initializes non-portable */
/*   features. Currently, the only non-portable feature    */
/*   requiring initialization is the interrupt handler     */
/*   which allows execution to be halted.                  */
/***********************************************************/
void
CL_InitializeNonportableFeatures (Environment * theEnv)
{
#if MAC_XCD
#pragma unused(theEnv)
#endif
}

/**************************************/
/* CL_genexit:  A generic exit function. */
/**************************************/
void
CL_genexit (Environment * theEnv, int num)
{
  if (SystemDependentData (theEnv)->jmpBuffer != NULL)
    {
      longjmp (*SystemDependentData (theEnv)->jmpBuffer, 1);
    }

  exit (num);
}

/**************************************/
/* CL_SetJmpBuffer: */
/**************************************/
void
CL_SetJmpBuffer (Environment * theEnv, jmp_buf * theJmpBuffer)
{
  SystemDependentData (theEnv)->jmpBuffer = theJmpBuffer;
}

/******************************************/
/* CL_genstrcpy: Generic CL_genstrcpy function. */
/******************************************/
char *
CL_genstrcpy (char *dest, const char *src)
{
  return strcpy (dest, src);
}

/********************************************/
/* CL_genstrncpy: Generic CL_genstrncpy function. */
/********************************************/
char *
CL_genstrncpy (char *dest, const char *src, size_t n)
{
  return strncpy (dest, src, n);
}

/******************************************/
/* CL_genstrcat: Generic CL_genstrcat function. */
/******************************************/
char *
CL_genstrcat (char *dest, const char *src)
{
  return strcat (dest, src);
}

/********************************************/
/* CL_genstrncat: Generic CL_genstrncat function. */
/********************************************/
char *
CL_genstrncat (char *dest, const char *src, size_t n)
{
  return strncat (dest, src, n);
}

/*****************************************/
/* CL_gensprintf: Generic sprintf function. */
/*****************************************/
int
CL_gensprintf (char *buffer, const char *restrictStr, ...)
{
  va_list args;
  int rv;

  va_start (args, restrictStr);

  rv = vsprintf (buffer, restrictStr, args);

  va_end (args);

  return rv;
}

/******************************************************/
/* CL_genrand: Generic random number generator function. */
/******************************************************/
int
CL_genrand ()
{
  return (rand ());
}

/**********************************************************************/
/* CL_genseed: Generic function for seeding the random number generator. */
/**********************************************************************/
void
CL_genseed (unsigned int seed)
{
  srand (seed);
}

/*********************************************/
/* CL_gengetcwd: Generic function for returning */
/*   the current directory.                  */
/*********************************************/
char *
CL_gengetcwd (char *buffer, int buflength)
{
#if MAC_XCD
  return (getcwd (buffer, buflength));
#else
  if (buffer != NULL)
    {
      buffer[0] = 0;
    }
  return (buffer);
#endif
}

/*************************************************/
/* CL_genchdir: Generic function for setting the    */
/*   current directory. Returns 1 if successful, */
/*   0 if unsuccessful, and -1 if unavailable.   */
/*************************************************/
int
CL_genchdir (Environment * theEnv, const char *directory)
{
  int rv = -1;

   /*==========================================================*/
  /* If the directory argument is NULL, then the return value */
  /* indicates whether the chdir functionality is supported.  */
   /*==========================================================*/

  if (directory == NULL)
    {
#if MAC_XCD || DARWIN || LINUX || WIN_MVC
      return 1;
#else
      return 0;
#endif
    }

   /*========================================*/
  /* Otherwise, try changing the directory. */
   /*========================================*/

#if MAC_XCD || DARWIN || LINUX
  rv = chdir (directory) + 1;
#endif
#if WIN_MVC
  wchar_t *wdirectory;
  int wlength;

  wlength = MultiByteToWideChar (CP_UTF8, 0, directory, -1, NULL, 0);

  wdirectory = (wchar_t *) CL_genalloc (theEnv, wlength * sizeof (wchar_t));

  MultiByteToWideChar (CP_UTF8, 0, directory, -1, wdirectory, wlength);

  rv = _wchdir (wdirectory) + 1;

  CL_genfree (theEnv, wdirectory, wlength * sizeof (wchar_t));
#endif

  return rv;
}

/****************************************************/
/* CL_genremove: Generic function for removing a file. */
/****************************************************/
bool
CL_genremove (Environment * theEnv, const char *fileName)
{
#if WIN_MVC
  wchar_t *wfileName;
  int wfnlength;
#endif

#if WIN_MVC
  wfnlength = MultiByteToWideChar (CP_UTF8, 0, fileName, -1, NULL, 0);

  wfileName = (wchar_t *) CL_genalloc (theEnv, wfnlength * sizeof (wchar_t));

  MultiByteToWideChar (CP_UTF8, 0, fileName, -1, wfileName, wfnlength);

  if (_wremove (wfileName))
    {
      CL_genfree (theEnv, wfileName, wfnlength * sizeof (wchar_t));
      return false;
    }

  CL_genfree (theEnv, wfileName, wfnlength * sizeof (wchar_t));
#else
  if (remove (fileName))
    return false;
#endif

  return true;
}

/****************************************************/
/* CL_genrename: Generic function for renaming a file. */
/****************************************************/
bool
CL_genrename (Environment * theEnv,
	      const char *oldFileName, const char *newFileName)
{
#if WIN_MVC
  wchar_t *woldFileName, *wnewFileName;
  int wofnlength, wnfnlength;
#endif

#if WIN_MVC
  wofnlength = MultiByteToWideChar (CP_UTF8, 0, oldFileName, -1, NULL, 0);
  wnfnlength = MultiByteToWideChar (CP_UTF8, 0, newFileName, -1, NULL, 0);

  woldFileName =
    (wchar_t *) CL_genalloc (theEnv, wofnlength * sizeof (wchar_t));
  wnewFileName =
    (wchar_t *) CL_genalloc (theEnv, wnfnlength * sizeof (wchar_t));

  MultiByteToWideChar (CP_UTF8, 0, oldFileName, -1, woldFileName, wofnlength);
  MultiByteToWideChar (CP_UTF8, 0, newFileName, -1, wnewFileName, wnfnlength);

  if (_wrename (woldFileName, wnewFileName))
    {
      CL_genfree (theEnv, woldFileName, wofnlength * sizeof (wchar_t));
      CL_genfree (theEnv, wnewFileName, wnfnlength * sizeof (wchar_t));
      return false;
    }

  CL_genfree (theEnv, woldFileName, wofnlength * sizeof (wchar_t));
  CL_genfree (theEnv, wnewFileName, wnfnlength * sizeof (wchar_t));
#else
  if (rename (oldFileName, newFileName))
    return false;
#endif

  return true;
}

/***********************************/
/* SetBefore_OpenFunction: Sets the */
/*  value of Before_OpenFunction.   */
/***********************************/
int (*SetBefore_OpenFunction (Environment * theEnv,
			      int (*theFunction) (Environment
						  *))) (Environment *)
{
  int (*tempFunction) (Environment *);

  tempFunction = SystemDependentData (theEnv)->Before_OpenFunction;
  SystemDependentData (theEnv)->Before_OpenFunction = theFunction;
  return (tempFunction);
}

/**********************************/
/* SetAfter_OpenFunction: Sets the */
/*  value of After_OpenFunction.   */
/**********************************/
int (*SetAfter_OpenFunction (Environment * theEnv,
			     int (*theFunction) (Environment *))) (Environment
								   *)
{
  int (*tempFunction) (Environment *);

  tempFunction = SystemDependentData (theEnv)->After_OpenFunction;
  SystemDependentData (theEnv)->After_OpenFunction = theFunction;
  return (tempFunction);
}

/*********************************************/
/* CL_GenOpen: Trap routine for opening a file. */
/*********************************************/
FILE *
CL_GenOpen (Environment * theEnv,
	    const char *fileName, const char *accessType)
{
  FILE *theFile;
#if WIN_MVC
  wchar_t *wfileName;
  wchar_t *waccessType;
  int wfnlength, watlength;
#endif

  //+Basile adds checks
  Bogus (theEnv == NULL);
  Bogus (fileName == NULL);
  Bogus (accessType == NULL);
  Bogus (SystemDependentData (theEnv) == NULL);
  CLGCC_DBGPRINTF ("CL_GenOpen fileName=%s accessType=%s theEnv@%p", fileName,
		   accessType, theEnv);
  //-Basile

   /*==================================*/
  /* Invoke the before open function. */
   /*==================================*/

  if (SystemDependentData (theEnv)->Before_OpenFunction != NULL)
    {
      (*SystemDependentData (theEnv)->Before_OpenFunction) (theEnv);
    }

   /*================*/
  /* Open the file. */
   /*================*/

#if WIN_MVC
  wfnlength = MultiByteToWideChar (CP_UTF8, 0, fileName, -1, NULL, 0);
  watlength = MultiByteToWideChar (CP_UTF8, 0, accessType, -1, NULL, 0);

  wfileName = (wchar_t *) CL_genalloc (theEnv, wfnlength * sizeof (wchar_t));
  waccessType =
    (wchar_t *) CL_genalloc (theEnv, watlength * sizeof (wchar_t));

  MultiByteToWideChar (CP_UTF8, 0, fileName, -1, wfileName, wfnlength);
  MultiByteToWideChar (CP_UTF8, 0, accessType, -1, waccessType, watlength);

  _wfopen_s (&theFile, wfileName, waccessType);

  CL_genfree (theEnv, wfileName, wfnlength * sizeof (wchar_t));
  CL_genfree (theEnv, waccessType, watlength * sizeof (wchar_t));
#else
  theFile = fopen (fileName, accessType);
#endif

   /*=====================================*/
  /* Check for a UTF-8 Byte Order Marker */
  /* (BOM): 0xEF,0xBB,0xBF.              */
   /*=====================================*/

  if ((theFile != NULL) & (strcmp (accessType, "r") == 0))
    {
      int theChar;

      theChar = getc (theFile);
      if (theChar == 0xEF)
	{
	  theChar = getc (theFile);
	  if (theChar == 0xBB)
	    {
	      theChar = getc (theFile);
	      if (theChar != 0xBF)
		{
		  ungetc (theChar, theFile);
		}
	    }
	  else
	    {
	      ungetc (theChar, theFile);
	    }
	}
      else
	{
	  ungetc (theChar, theFile);
	}
    }

   /*=================================*/
  /* Invoke the after open function. */
   /*=================================*/

  if (SystemDependentData (theEnv)->After_OpenFunction != NULL)
    {
      (*SystemDependentData (theEnv)->After_OpenFunction) (theEnv);
    }

   /*===============================*/
  /* Return a pointer to the file. */
   /*===============================*/

  return theFile;
}

/**********************************************/
/* CL_GenClose: Trap routine for closing a file. */
/**********************************************/
int
CL_GenClose (Environment * theEnv, FILE * theFile)
{
  int rv;

  rv = fclose (theFile);

  return rv;
}

/***********************************************/
/* CL_GenFlush: Trap routine for flushing a file. */
/***********************************************/
int
CL_GenFlush (Environment * theEnv, FILE * theFile)
{
  int rv;

  rv = fflush (theFile);

  return rv;
}

/*************************************************/
/* CL_GenRewind: Trap routine for rewinding a file. */
/*************************************************/
void
CL_GenRewind (Environment * theEnv, FILE * theFile)
{
  rewind (theFile);
}

/*************************************************/
/* CL_GenTell: Trap routine for the ftell function. */
/*************************************************/
long long
CL_GenTell (Environment * theEnv, FILE * theFile)
{
  long long rv;

  rv = ftell (theFile);

  if (rv == -1)
    {
      if (errno > 0)
	{
	  return LLONG_MIN;
	}
    }

  return rv;
}

/*************************************************/
/* CL_GenSeek: Trap routine for the fseek function. */
/*************************************************/
int
CL_GenSeek (Environment * theEnv, FILE * theFile, long offset, int whereFrom)
{
  return fseek (theFile, offset, whereFrom);
}

/************************************************************/
/* CL_GenOpenReadBinary: Generic and machine specific code for */
/*   opening a file for binary access. Only one file may be */
/*   open at a time when using this function since the file */
/*   pointer is stored in a global variable.                */
/************************************************************/
int
CL_GenOpenReadBinary (Environment * theEnv,
		      const char *funcName, const char *fileName)
{
  if (SystemDependentData (theEnv)->Before_OpenFunction != NULL)
    {
      (*SystemDependentData (theEnv)->Before_OpenFunction) (theEnv);
    }

#if WIN_MVC
  SystemDependentData (theEnv)->BinaryFileHandle =
    _open (fileName, O_RDONLY | O_BINARY);
  if (SystemDependentData (theEnv)->BinaryFileHandle == -1)
    {
      if (SystemDependentData (theEnv)->After_OpenFunction != NULL)
	{
	  (*SystemDependentData (theEnv)->After_OpenFunction) (theEnv);
	}
      return 0;
    }
#endif

#if (! WIN_MVC)
  if ((SystemDependentData (theEnv)->BinaryFP =
       fopen (fileName, "rb")) == NULL)
    {
      if (SystemDependentData (theEnv)->After_OpenFunction != NULL)
	{
	  (*SystemDependentData (theEnv)->After_OpenFunction) (theEnv);
	}
      return 0;
    }
#endif

  if (SystemDependentData (theEnv)->After_OpenFunction != NULL)
    {
      (*SystemDependentData (theEnv)->After_OpenFunction) (theEnv);
    }

  return 1;
}

/***********************************************/
/* CL_GenReadBinary: Generic and machine specific */
/*   code for reading from a file.             */
/***********************************************/
void
CL_GenReadBinary (Environment * theEnv, void *dataPtr, size_t size)
{
#if WIN_MVC
  char *tempPtr;

  tempPtr = (char *) dataPtr;
  while (size > INT_MAX)
    {
      _read (SystemDependentData (theEnv)->BinaryFileHandle, tempPtr,
	     INT_MAX);
      size -= INT_MAX;
      tempPtr = tempPtr + INT_MAX;
    }

  if (size > 0)
    {
      _read (SystemDependentData (theEnv)->BinaryFileHandle, tempPtr,
	     (unsigned int) size);
    }
#endif

#if (! WIN_MVC)
  fread (dataPtr, size, 1, SystemDependentData (theEnv)->BinaryFP);
#endif
}

/***************************************************/
/* CL_GetSeekCurBinary:  Generic and machine specific */
/*   code for seeking a position in a file.        */
/***************************************************/
void
CL_GetSeekCurBinary (Environment * theEnv, long offset)
{
#if WIN_MVC
  _lseek (SystemDependentData (theEnv)->BinaryFileHandle, offset, SEEK_CUR);
#endif

#if (! WIN_MVC)
  fseek (SystemDependentData (theEnv)->BinaryFP, offset, SEEK_CUR);
#endif
}

/***************************************************/
/* CL_GetSeekSetBinary:  Generic and machine specific */
/*   code for seeking a position in a file.        */
/***************************************************/
void
CL_GetSeekSetBinary (Environment * theEnv, long offset)
{
#if WIN_MVC
  _lseek (SystemDependentData (theEnv)->BinaryFileHandle, offset, SEEK_SET);
#endif

#if (! WIN_MVC)
  fseek (SystemDependentData (theEnv)->BinaryFP, offset, SEEK_SET);
#endif
}

/************************************************/
/* CL_GenTellBinary:  Generic and machine specific */
/*   code for telling a position in a file.     */
/************************************************/
void
CL_GenTellBinary (Environment * theEnv, long *offset)
{
#if WIN_MVC
  *offset =
    _lseek (SystemDependentData (theEnv)->BinaryFileHandle, 0, SEEK_CUR);
#endif

#if (! WIN_MVC)
  *offset = ftell (SystemDependentData (theEnv)->BinaryFP);
#endif
}

/****************************************/
/* CL_GenCloseBinary:  Generic and machine */
/*   specific code for closing a file.  */
/****************************************/
void
CL_GenCloseBinary (Environment * theEnv)
{
  if (SystemDependentData (theEnv)->Before_OpenFunction != NULL)
    {
      (*SystemDependentData (theEnv)->Before_OpenFunction) (theEnv);
    }

#if WIN_MVC
  _close (SystemDependentData (theEnv)->BinaryFileHandle);
#endif

#if (! WIN_MVC)
  fclose (SystemDependentData (theEnv)->BinaryFP);
#endif

  if (SystemDependentData (theEnv)->After_OpenFunction != NULL)
    {
      (*SystemDependentData (theEnv)->After_OpenFunction) (theEnv);
    }
}

/***********************************************/
/* CL_Gen_Write: Generic routine for writing to a  */
/*   file. No machine specific code as of yet. */
/***********************************************/
void
CL_Gen_Write (void *dataPtr, size_t size, FILE * fp)
{
  if (size == 0)
    return;
#if UNIX_7
  fwrite (dataPtr, size, 1, fp);
#else
  fwrite (dataPtr, size, 1, fp);
#endif
}
