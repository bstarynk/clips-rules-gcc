   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/01/16            */
   /*                                                     */
   /*              FILE COMMANDS HEADER FILE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for file commands including    */
/*   batch, dribble-on, dribble-off, save, load, bsave, and  */
/*   bload.                                                  */
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
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added code for capturing errors/warnings.      */
/*                                                           */
/*            Added AwaitingInput flag.                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
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
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_filecom

#pragma once

#define _H_filecom

#include <stdio.h>
#include <stdlib.h>

#include "entities.h"

typedef struct batchEntry CL_BatchEntry;

/***************/
/* STRUCTURES  */
/***************/

struct batchEntry
{
  int batchType;
  FILE *fileSource;
  const char *logicalSource;
  const char *theString;
  const char *fileName;
  long lineNumber;
  CL_BatchEntry *next;
};

/***************/
/* DEFINITIONS */
/***************/

#define FILE_BATCH      0
#define STRING_BATCH    1

#define BUFFER_SIZE   120

#define FILECOM_DATA 14

struct fileCommandData
{
#if DEBUGGING_FUNCTIONS
  FILE *DribbleFP;
  char *DribbleBuffer;
  size_t DribbleCurrentPosition;
  size_t DribbleMaximumPosition;
  int (*DribbleStatusFunction) (Environment *, bool);
#endif
  int CL_BatchType;
  FILE *CL_BatchFileSource;
  const char *CL_BatchLogicalSource;
  char *CL_BatchBuffer;
  size_t CL_BatchCurrentPosition;
  size_t CL_BatchMaximumPosition;
  CL_BatchEntry *TopOf_BatchList;
  CL_BatchEntry *BottomOf_BatchList;
  char *batchPriorParsingFile;
};

#define FileCommandData(theEnv) ((struct fileCommandData *) GetEnvironmentData(theEnv,FILECOM_DATA))

void CL_FileCommandDefinitions (Environment *);
void CL_BatchCommand (Environment *, UDFContext *, UDFValue *);
void CL_BatchStarCommand (Environment *, UDFContext *, UDFValue *);
void CL_LoadCommand (Environment *, UDFContext *, UDFValue *);
void CL_LoadStarCommand (Environment *, UDFContext *, UDFValue *);
void CL_SaveCommand (Environment *, UDFContext *, UDFValue *);
void CL_DribbleOnCommand (Environment *, UDFContext *, UDFValue *);
void CL_DribbleOffCommand (Environment *, UDFContext *, UDFValue *);

#endif /* _H_filecom */
