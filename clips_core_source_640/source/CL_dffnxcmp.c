   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Generic Function Construct Compiler Code         */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFFUNCTION_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME)

#include "conscomp.h"
#include "envrnmnt.h"

#include "dffnxcmp.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    ReadyDeffunctionsForCode(Environment *);
   static bool                    DeffunctionsToCode(Environment *,const char *,const char *,char *,
                                                     unsigned int,FILE *,unsigned int,unsigned int);
   static void                    CloseDeffunctionFiles(Environment *,FILE *,FILE *,unsigned int);
   static void                    CL_DeffunctionModuleToCode(Environment *,FILE *,Defmodule *,unsigned int,unsigned int);
   static void                    SingleDeffunctionToCode(Environment *,FILE *,Deffunction *,
                                                          unsigned int,unsigned int,unsigned int);
   static void                    InitDeffunctionCode(Environment *,FILE *,unsigned int,unsigned int);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : CL_SetupDeffunctionCompiler
  DESCRIPTION  : Initializes the construct compiler
                   item for deffunctions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Code generator item initialized
  NOTES        : None
 ***************************************************/
void CL_SetupDeffunctionCompiler(
  Environment *theEnv)
  {
   DeffunctionData(theEnv)->DeffunctionCodeItem = CL_AddCodeGeneratorItem(theEnv,"deffunctions",0,ReadyDeffunctionsForCode,
                                              InitDeffunctionCode,DeffunctionsToCode,2);
  }


/***************************************************
  NAME         : CL_PrintDeffunctionReference
  DESCRIPTION  : Prints a reference to the run-time
                 deffunction array for the construct
                 compiler
  INPUTS       : 1) The file output destination
                 2) A pointer to the deffunction
                 3) The id of the run-time image
                 4) The maximum number of indices
                    in any array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Reference printed
  NOTES        : None
 ***************************************************/
void CL_PrintDeffunctionReference(
  Environment *theEnv,
  FILE *fp,
  Deffunction *dfPtr,
  unsigned imageID,
  unsigned maxIndices)
  {
   if (dfPtr == NULL)
     fprintf(fp,"NULL");
   else
     fprintf(fp,"&%s%d_%lu[%lu]",ConstructPrefix(DeffunctionData(theEnv)->DeffunctionCodeItem),imageID,
                                ((dfPtr->header.bsaveID / maxIndices) + 1),
                                (dfPtr->header.bsaveID % maxIndices));
  }

/****************************************************
  NAME         : CL_DeffunctionCModuleReference
  DESCRIPTION  : Prints out a reference to a
                 deffunction module
  INPUTS       : 1) The output file
                 2) The id of the module item
                 3) The id of the image
                 4) The maximum number of elements
                    allowed in an array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction module reference printed
  NOTES        : None
 ****************************************************/
void CL_DeffunctionCModuleReference(
  Environment *theEnv,
  FILE *theFile,
  unsigned long count,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   fprintf(theFile,"MIHS &%s%u_%lu[%lu]",
                      ModulePrefix(DeffunctionData(theEnv)->DeffunctionCodeItem),
                      imageID,
                      (count / maxIndices) + 1,
                      (count % maxIndices));
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : ReadyDeffunctionsForCode
  DESCRIPTION  : Sets index of deffunctions
                   for use in compiled expressions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : CL_BsaveIndices set
  NOTES        : None
 ***************************************************/
static void ReadyDeffunctionsForCode(
  Environment *theEnv)
  {
   MarkConstruct_BsaveIDs(theEnv,DeffunctionData(theEnv)->CL_DeffunctionModuleIndex);
  }

/**************************************************/
/* InitDeffunctionCode: CL_Writes out initialization */
/*   code for deffunction for a run-time module.  */
/**************************************************/
static void InitDeffunctionCode(
  Environment *theEnv,
  FILE *initFP,
  unsigned int imageID,
  unsigned int maxIndices)
  {
#if MAC_XCD
#pragma unused(maxIndices)
#pragma unused(imageID)
#pragma unused(theEnv)
#endif
   fprintf(initFP,"   Deffunction_RunTimeInitialize(theEnv);\n");
  }

/*******************************************************
  NAME         : DeffunctionsToCode
  DESCRIPTION  : CL_Writes out static array code for
                   deffunctions
  INPUTS       : 1) The base name of the construct set
                 2) The base id for this construct
                 3) The file pointer for the header file
                 4) The base id for the construct set
                 5) The max number of indices allowed
                    in an array
  RETURNS      : False on errors,
                 True if deffunctions written
  SIDE EFFECTS : Code written to files
  NOTES        : None
 *******************************************************/
static bool DeffunctionsToCode(
  Environment *theEnv,
  const char *fileName,
  const char *pathName,
  char *fileNameBuffer,
  unsigned int fileID,
  FILE *headerFP,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   unsigned int fileCount = 1;
   Defmodule *theModule;
   Deffunction *theDeffunction;
   unsigned int moduleCount = 0, moduleArrayCount = 0, moduleArrayVersion = 1;
   unsigned int deffunctionArrayCount = 0, deffunctionArrayVersion = 1;
   FILE *moduleFile = NULL, *deffunctionFile = NULL;

   /* ===============================================
      Include the appropriate deffunction header file
      =============================================== */
   fprintf(headerFP,"#include \"dffnxfun.h\"\n");

   /* =============================================================
      Loop through all the modules and all the deffunctions writing
      their C code representation to the file as they are traversed
      ============================================================= */
   theModule = CL_GetNextDefmodule(theEnv,NULL);

   while (theModule != NULL)
     {
      CL_SetCurrentModule(theEnv,theModule);

      moduleFile = CL_OpenFileIfNeeded(theEnv,moduleFile,fileName,pathName,fileNameBuffer,fileID,imageID,&fileCount,
                                    moduleArrayVersion,headerFP,
                                    "CL_DeffunctionModuleData",ModulePrefix(DeffunctionData(theEnv)->DeffunctionCodeItem),
                                    false,NULL);

      if (moduleFile == NULL)
        {
         CloseDeffunctionFiles(theEnv,moduleFile,deffunctionFile,maxIndices);
         return false;
        }

      CL_DeffunctionModuleToCode(theEnv,moduleFile,theModule,imageID,maxIndices);
      moduleFile = CL_CloseFileIfNeeded(theEnv,moduleFile,&moduleArrayCount,&moduleArrayVersion,
                                     maxIndices,NULL,NULL);

      theDeffunction = CL_GetNextDeffunction(theEnv,NULL);

      while (theDeffunction != NULL)
        {
         deffunctionFile = CL_OpenFileIfNeeded(theEnv,deffunctionFile,fileName,pathName,fileNameBuffer,fileID,imageID,&fileCount,
                                            deffunctionArrayVersion,headerFP,
                                            "Deffunction",ConstructPrefix(DeffunctionData(theEnv)->DeffunctionCodeItem),
                                            false,NULL);
         if (deffunctionFile == NULL)
           {
            CloseDeffunctionFiles(theEnv,moduleFile,deffunctionFile,maxIndices);
            return false;
           }

         SingleDeffunctionToCode(theEnv,deffunctionFile,theDeffunction,imageID,
                                 maxIndices,moduleCount);
         deffunctionArrayCount++;
         deffunctionFile = CL_CloseFileIfNeeded(theEnv,deffunctionFile,&deffunctionArrayCount,
                                             &deffunctionArrayVersion,maxIndices,NULL,NULL);

         theDeffunction = CL_GetNextDeffunction(theEnv,theDeffunction);
        }

      theModule = CL_GetNextDefmodule(theEnv,theModule);
      moduleCount++;
      moduleArrayCount++;
     }

   CloseDeffunctionFiles(theEnv,moduleFile,deffunctionFile,maxIndices);

   return true;
  }

/***************************************************
  NAME         : CloseDeffunctionFiles
  DESCRIPTION  : Closes construct compiler files
                  for deffunction structures
  INPUTS       : 1) The deffunction module file
                 2) The deffunction structure file
                 3) The maximum number of indices
                    allowed in an array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Files closed
  NOTES        : None
 ***************************************************/
static void CloseDeffunctionFiles(
  Environment *theEnv,
  FILE *moduleFile,
  FILE *deffunctionFile,
  unsigned int maxIndices)
  {
   unsigned int count = maxIndices;
   unsigned int arrayVersion = 0;

   if (deffunctionFile != NULL)
     {
      count = maxIndices;
      CL_CloseFileIfNeeded(theEnv,deffunctionFile,&count,&arrayVersion,maxIndices,NULL,NULL);
     }

   if (moduleFile != NULL)
     {
      count = maxIndices;
      CL_CloseFileIfNeeded(theEnv,moduleFile,&count,&arrayVersion,maxIndices,NULL,NULL);
     }
  }

/***************************************************
  NAME         : CL_DeffunctionModuleToCode
  DESCRIPTION  : CL_Writes out the C values for a
                 deffunction module item
  INPUTS       : 1) The output file
                 2) The module for the deffunctions
                 3) The compile image id
                 4) The maximum number of elements
                    in an array
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction module item written
  NOTES        : None
 ***************************************************/
static void CL_DeffunctionModuleToCode(
  Environment *theEnv,
  FILE *theFile,
  Defmodule *theModule,
  unsigned int imageID,
  unsigned int maxIndices)
  {
   fprintf(theFile,"{");
   CL_ConstructModuleToCode(theEnv,theFile,theModule,imageID,maxIndices,
                         DeffunctionData(theEnv)->CL_DeffunctionModuleIndex,ConstructPrefix(DeffunctionData(theEnv)->DeffunctionCodeItem));
   fprintf(theFile,"}");
  }

/***************************************************
  NAME         : SingleDeffunctionToCode
  DESCRIPTION  : CL_Writes out a single deffunction's
                 data to the file
  INPUTS       : 1) The output file
                 2) The deffunction
                 3) The compile image id
                 4) The maximum number of
                    elements in an array
                 5) The module index
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deffunction data written
  NOTES        : None
 ***************************************************/
static void SingleDeffunctionToCode(
  Environment *theEnv,
  FILE *theFile,
  Deffunction *theDeffunction,
  unsigned int imageID,
  unsigned int maxIndices,
  unsigned int moduleCount)
  {
   /* ==================
      Deffunction Header
      ================== */

   fprintf(theFile,"{");
   CL_ConstructHeaderToCode(theEnv,theFile,&theDeffunction->header,imageID,maxIndices,moduleCount,
                         ModulePrefix(DeffunctionData(theEnv)->DeffunctionCodeItem),
                         ConstructPrefix(DeffunctionData(theEnv)->DeffunctionCodeItem));

   /* =========================
      Deffunction specific data
      ========================= */
   fprintf(theFile,",0,0,0,");
   CL_ExpressionToCode(theEnv,theFile,theDeffunction->code);
   fprintf(theFile,",%d,%d,%d",
           theDeffunction->minNumberOfParameters,
           theDeffunction->maxNumberOfParameters,
           theDeffunction->numberOfLocalVars);

   fprintf(theFile,"}");
  }

#endif
