   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  01/25/18             */
   /*                                                     */
   /*            SYSTEM DEPENDENT HEADER FILE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Isolation of system dependent routines.          */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
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
/*            Removed ContinueEnvFunction, PauseEnvFunction, */
/*            and RedrawScreenFunction callbacks.            */
/*                                                           */
/*            Completion code now returned by CL_gensystem.     */
/*                                                           */
/*            Added flush, rewind, tell, and seek functions. */
/*                                                           */
/*************************************************************/

#ifndef _H_sysdep

#pragma once

#define _H_sysdep

#include <stdio.h>
#include <setjmp.h>

double CL_gentime (void);
int CL_gensystem (Environment *, const char *);
int CL_GenOpenReadBinary (Environment *, const char *, const char *);
void CL_GetSeekCurBinary (Environment *, long);
void CL_GetSeekSetBinary (Environment *, long);
void CL_GenTellBinary (Environment *, long *);
void CL_GenCloseBinary (Environment *);
void CL_GenReadBinary (Environment *, void *, size_t);
FILE *CL_GenOpen (Environment *, const char *, const char *);
int CL_GenClose (Environment *, FILE *);
int CL_GenFlush (Environment *, FILE *);
void CL_GenRewind (Environment *, FILE *);
long long CL_GenTell (Environment *, FILE *);
int CL_GenSeek (Environment *, FILE *, long, int);
void CL_genexit (Environment *, int);
int CL_genrand (void);
void CL_genseed (unsigned int);
bool CL_genremove (Environment *, const char *);
bool CL_genrename (Environment *, const char *, const char *);
char *CL_gengetcwd (char *, int);
void CL_Gen_Write (void *, size_t, FILE *);
int (*SetBefore_OpenFunction (Environment *, int (*)(Environment *)))
  (Environment *);
int (*SetAfter_OpenFunction (Environment *, int (*)(Environment *)))
  (Environment *);
int CL_gensprintf (char *, const char *, ...);
char *CL_genstrcpy (char *, const char *);
char *CL_genstrncpy (char *, const char *, size_t);
char *CL_genstrcat (char *, const char *);
char *CL_genstrncat (char *, const char *, size_t);
int CL_genchdir (Environment *, const char *);
void CL_SetJmpBuffer (Environment *, jmp_buf *);
void CL_genprintfile (Environment *, FILE *, const char *);
int CL_gengetchar (Environment *);
int CL_genungetchar (Environment *, int);
void CL_InitializeSystemDependentData (Environment *);
void CL_InitializeNonportableFeatures (Environment *);

#endif /* _H_sysdep */
