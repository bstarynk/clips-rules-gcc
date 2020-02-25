   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/02/17            */
   /*                                                     */
   /*               PRETTY PRINT HEADER FILE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for processing the pretty print         */
/*   representation of constructs.                           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Used CL_genstrcpy instead of strcpy.              */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_pprint

#pragma once

#define _H_pprint

#define PRETTY_PRINT_DATA 52

struct prettyPrintData
{
  bool PPBufferStatus;
  bool PPBufferEnabled;
  size_t IndentationDepth;
  size_t PPBufferPos;
  size_t PPBufferMax;
  size_t CL_PPBackupOnce;
  size_t CL_PPBackupTwice;
  char *PrettyPrintBuffer;
};

#define PrettyPrintData(theEnv) ((struct prettyPrintData *) GetEnvironmentData(theEnv,PRETTY_PRINT_DATA))

void CL_InitializePrettyPrintData (Environment *);
void CL_FlushPPBuffer (Environment *);
void CL_DestroyPPBuffer (Environment *);
void CL_SavePPBuffer (Environment *, const char *);
void CL_PPBackup (Environment *);
char *CL_CopyPPBuffer (Environment *);
char *CL_GetPPBuffer (Environment *);
void CL_PPCRAndIndent (Environment *);
void CL_IncrementIndentDepth (Environment *, size_t);
void CL_DecrementIndentDepth (Environment *, size_t);
void CL_SetIndentDepth (Environment *, size_t);
void CL_SetPPBufferStatus (Environment *, bool);
bool CL_GetPPBufferStatus (Environment *);
bool CL_SetPPBufferEnabled (Environment *, bool);
bool CL_GetPPBufferEnabled (Environment *);

#endif
