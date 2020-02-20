   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/24/17            */
   /*                                                     */
   /*                 FILE COMMANDS MODULE                */
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
/*            Added STDOUT and STDIN logical name            */
/*            definitions.                                   */
/*                                                           */
/*      6.31: Unprocessed batch files did not deallocate     */
/*            all memory on exit.                            */
/*                                                           */
/*      6.40: Split inputSource to fileSource and            */
/*            logicalSource.                                 */
/*                                                           */
/*            Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to CL_GetCL_HaltExecution and       */
/*            SetCL_HaltExecution functions.                    */
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
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "argacces.h"
#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#include "bsave.h"
#endif
#include "commline.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "fileutil.h"
#include "memalloc.h"
#include "router.h"
#include "sysdep.h"
#include "utility.h"

#include "filecom.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    DeallocateFileCommandData(Environment *);

/***************************************/
/* CL_FileCommandDefinitions: Initializes */
/*   file commands.                    */
/***************************************/
void CL_FileCommandDefinitions(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,FILECOM_DATA,sizeof(struct fileCommandData),DeallocateFileCommandData);

#if ! RUN_TIME
#if DEBUGGING_FUNCTIONS
   CL_AddUDF(theEnv,"batch","b",1,1,"sy",CL_BatchCommand,"CL_BatchCommand",NULL);
   CL_AddUDF(theEnv,"batch*","b",1,1,"sy",CL_BatchStarCommand,"CL_BatchStarCommand",NULL);
   CL_AddUDF(theEnv,"dribble-on","b",1,1,"sy",CL_DribbleOnCommand,"CL_DribbleOnCommand",NULL);
   CL_AddUDF(theEnv,"dribble-off","b",0,0,NULL,CL_DribbleOffCommand,"CL_DribbleOffCommand",NULL);
   CL_AddUDF(theEnv,"save","b",1,1,"sy",CL_SaveCommand,"CL_SaveCommand",NULL);
#endif
   CL_AddUDF(theEnv,"load","b",1,1,"sy",CL_LoadCommand,"CL_LoadCommand",NULL);
   CL_AddUDF(theEnv,"load*","b",1,1,"sy",CL_LoadStarCommand,"CL_LoadStarCommand",NULL);
#if BLOAD_AND_BSAVE
   CL_AddUDF(theEnv,"bsave","b",1,1,"sy",CL_BsaveCommand,"CL_BsaveCommand",NULL);
#endif
#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
   InitializeCL_BsaveData(theEnv);
   InitializeCL_BloadData(theEnv);
   CL_AddUDF(theEnv,"bload","b",1,1,"sy",CL_BloadCommand,"CL_BloadCommand",NULL);
#endif
#endif
  }

/******************************************************/
/* DeallocateFileCommandData: Deallocates environment */
/*    data for file commands.                         */
/******************************************************/
static void DeallocateFileCommandData(
  Environment *theEnv)
  {
   struct batchEntry *theEntry, *nextEntry;

   theEntry = FileCommandData(theEnv)->TopOfCL_BatchList;
   while (theEntry != NULL)
     {
      nextEntry = theEntry->next;

      if (theEntry->batchType == FILE_BATCH)
        { CL_GenClose(theEnv,FileCommandData(theEnv)->TopOfCL_BatchList->fileSource); }
      else
        { CL_rm(theEnv,(void *) theEntry->theString,strlen(theEntry->theString) + 1); }

      CL_DeleteString(theEnv,(char *) theEntry->fileName);
      CL_DeleteString(theEnv,(char *) theEntry->logicalSource);
      rtn_struct(theEnv,batchEntry,theEntry);

      theEntry = nextEntry;
     }

   if (FileCommandData(theEnv)->CL_BatchBuffer != NULL)
     { CL_rm(theEnv,FileCommandData(theEnv)->CL_BatchBuffer,FileCommandData(theEnv)->CL_BatchMaximumPosition); }

   CL_DeleteString(theEnv,FileCommandData(theEnv)->batchPriorParsingFile);
   FileCommandData(theEnv)->batchPriorParsingFile = NULL;

#if DEBUGGING_FUNCTIONS
   if (FileCommandData(theEnv)->DribbleBuffer != NULL)
     { CL_rm(theEnv,FileCommandData(theEnv)->DribbleBuffer,FileCommandData(theEnv)->DribbleMaximumPosition); }

   if (FileCommandData(theEnv)->DribbleFP != NULL)
     { CL_GenClose(theEnv,FileCommandData(theEnv)->DribbleFP); }
#endif
  }

#if DEBUGGING_FUNCTIONS

/******************************************/
/* CL_DribbleOnCommand: H/L access routine   */
/*   for the dribble-on command.          */
/******************************************/
void CL_DribbleOnCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;

   if ((fileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_DribbleOn(theEnv,fileName));
  }

/*******************************************/
/* CL_DribbleOffCommand: H/L access  routine  */
/*   for the dribble-off command.          */
/*******************************************/
void CL_DribbleOffCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_DribbleOff(theEnv));
  }

#endif /* DEBUGGING_FUNCTIONS */

/**************************************/
/* CL_BatchCommand: H/L access routine   */
/*   for the batch command.           */
/**************************************/
void CL_BatchCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;

   if ((fileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,OpenCL_Batch(theEnv,fileName,false));
  }

/******************************************/
/* CL_BatchStarCommand: H/L access routine   */
/*   for the batch* command.              */
/******************************************/
void CL_BatchStarCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *fileName;

   if ((fileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   returnValue->lexemeValue = CL_CreateBoolean(theEnv,CL_BatchStar(theEnv,fileName));
  }

/***********************************************************/
/* CL_LoadCommand: H/L access routine for the load command.   */
/***********************************************************/
void CL_LoadCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)
   const char *theFileName;
   CL_LoadError rv;

   if ((theFileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if (CommandLineData(theEnv)->CL_EvaluatingCL_TopLevelCommand)
     { SetPrintWhileCL_Loading(theEnv,true); }

   if ((rv = CL_Load(theEnv,theFileName)) == LE_OPEN_FILE_ERROR)
     {
      SetPrintWhileCL_Loading(theEnv,false);
      CL_OpenErrorMessage(theEnv,"load",theFileName);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if (CommandLineData(theEnv)->CL_EvaluatingCL_TopLevelCommand)
     { SetPrintWhileCL_Loading(theEnv,false); }

   if (rv == LE_PARSING_ERROR) returnValue->lexemeValue = FalseSymbol(theEnv);
   else returnValue->lexemeValue = TrueSymbol(theEnv);
#else
   CL_WriteString(theEnv,STDOUT,"CL_Load is not available in this environment\n");
   returnValue->lexemeValue = FalseSymbol(theEnv);
#endif
  }

/****************************************************************/
/* CL_LoadStarCommand: H/L access routine for the load* command.   */
/****************************************************************/
void CL_LoadStarCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)
   const char *theFileName;
   CL_LoadError rv;

   if ((theFileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if ((rv = CL_Load(theEnv,theFileName)) == LE_OPEN_FILE_ERROR)
     {
      CL_OpenErrorMessage(theEnv,"load*",theFileName);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if (rv == LE_PARSING_ERROR) returnValue->lexemeValue = FalseSymbol(theEnv);
   else returnValue->lexemeValue = TrueSymbol(theEnv);
#else
   CL_WriteString(theEnv,STDOUT,"CL_Load* is not available in this environment\n");
   returnValue->lexemeValue = FalseSymbol(theEnv);
#endif
  }

#if DEBUGGING_FUNCTIONS
/*********************************************************/
/* CL_SaveCommand: H/L access routine for the save command. */
/*********************************************************/
void CL_SaveCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)
   const char *theFileName;

   if ((theFileName = CL_GetFileName(context)) == NULL)
     {
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   if (CL_Save(theEnv,theFileName) == false)
     {
      CL_OpenErrorMessage(theEnv,"save",theFileName);
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   returnValue->lexemeValue = TrueSymbol(theEnv);
#else
   CL_WriteString(theEnv,STDOUT,"CL_Save is not available in this environment\n");
   returnValue->lexemeValue = FalseSymbol(theEnv);
#endif
  }
#endif
