   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*            DEFFACTS CONSTRUCTS-TO-C MODULE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the constructs-to-c feature for the   */
/*    deffacts construct.                                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
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

#include "setup.h"

#if DEFFACTS_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME)

#include <stdio.h>

#include "conscomp.h"
#include "dffctdef.h"
#include "envrnmnt.h"

#include "dffctcmp.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool ConstructToCode (Environment *, const char *, const char *,
			     char *, unsigned int, FILE *, unsigned int,
			     unsigned int);
static void DeffactsToCode (Environment *, FILE *, Deffacts *, unsigned int,
			    unsigned int, unsigned int);
static void CL_DeffactsModuleToCode (Environment *, FILE *, Defmodule *,
				     unsigned int, unsigned int,
				     unsigned int);
static void CloseDeffactsFiles (Environment *, FILE *, FILE *, unsigned int);
static void BeforeDeffactsToCode (Environment *);
static void InitDeffactsCode (Environment *, FILE *, unsigned, unsigned int);

/*************************************************************/
/* CL_DeffactsCompilerSetup: Initializes the deffacts construct */
/*    for use with the constructs-to-c command.              */
/*************************************************************/
void
CL_DeffactsCompilerSetup (Environment * theEnv)
{
  DeffactsData (theEnv)->DeffactsCodeItem =
    CL_AddCodeGeneratorItem (theEnv, "deffacts", 0, BeforeDeffactsToCode,
			     InitDeffactsCode, ConstructToCode, 2);
}

/*************************************************************/
/* BeforeDeffactsToCode: Assigns each deffacts a unique ID   */
/*   which will be used for pointer references when the data */
/*   structures are written to a file as C code              */
/*************************************************************/
static void
BeforeDeffactsToCode (Environment * theEnv)
{
  MarkConstruct_BsaveIDs (theEnv,
			  DeffactsData (theEnv)->CL_DeffactsModuleIndex);
}

/***********************************************/
/* InitDeffactsCode: CL_Writes out initialization */
/*   code for deffacts for a run-time module.  */
/***********************************************/
static void
InitDeffactsCode (Environment * theEnv,
		  FILE * initFP, unsigned imageID, unsigned maxIndices)
{
#if MAC_XCD
#pragma unused(maxIndices)
#pragma unused(imageID)
#pragma unused(theEnv)
#endif
  fprintf (initFP, "   Deffacts_RunTimeInitialize(theEnv);\n");
}

/**********************************************************/
/* ConstructToCode: Produces deffacts code for a run-time */
/*   module created using the constructs-to-c function.   */
/**********************************************************/
static bool
ConstructToCode (Environment * theEnv,
		 const char *fileName,
		 const char *pathName,
		 char *fileNameBuffer,
		 unsigned int fileID,
		 FILE * headerFP,
		 unsigned int imageID, unsigned int maxIndices)
{
  unsigned int fileCount = 1;
  Defmodule *theModule;
  Deffacts *theDeffacts;
  unsigned int moduleCount = 0, moduleArrayCount = 0, moduleArrayVersion = 1;
  unsigned int deffactsArrayCount = 0, deffactsArrayVersion = 1;
  FILE *moduleFile = NULL, *deffactsFile = NULL;

   /*===============================================*/
  /* Include the appropriate deffacts header file. */
   /*===============================================*/

  fprintf (headerFP, "#include \"dffctdef.h\"\n");

   /*=================================================================*/
  /* Loop through all the modules and all the deffacts writing their */
  /* C code representation to the file as they are traversed.        */
   /*=================================================================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      CL_SetCurrentModule (theEnv, theModule);

      moduleFile =
	CL_OpenFileIfNeeded (theEnv, moduleFile, fileName, pathName,
			     fileNameBuffer, fileID, imageID, &fileCount,
			     moduleArrayVersion, headerFP,
			     "struct deffactsModule",
			     ModulePrefix (DeffactsData (theEnv)->
					   DeffactsCodeItem), false, NULL);

      if (moduleFile == NULL)
	{
	  CloseDeffactsFiles (theEnv, moduleFile, deffactsFile, maxIndices);
	  return false;
	}

      CL_DeffactsModuleToCode (theEnv, moduleFile, theModule, imageID,
			       maxIndices, moduleCount);
      moduleFile =
	CL_CloseFileIfNeeded (theEnv, moduleFile, &moduleArrayCount,
			      &moduleArrayVersion, maxIndices, NULL, NULL);

      /*===================================================*/
      /* Loop through each of the deffacts in this module. */
      /*===================================================*/

      for (theDeffacts = CL_GetNextDeffacts (theEnv, NULL);
	   theDeffacts != NULL;
	   theDeffacts = CL_GetNextDeffacts (theEnv, theDeffacts))
	{
	  deffactsFile =
	    CL_OpenFileIfNeeded (theEnv, deffactsFile, fileName, pathName,
				 fileNameBuffer, fileID, imageID, &fileCount,
				 deffactsArrayVersion, headerFP, "Deffacts",
				 ConstructPrefix (DeffactsData (theEnv)->
						  DeffactsCodeItem), false,
				 NULL);
	  if (deffactsFile == NULL)
	    {
	      CloseDeffactsFiles (theEnv, moduleFile, deffactsFile,
				  maxIndices);
	      return false;
	    }

	  DeffactsToCode (theEnv, deffactsFile, theDeffacts, imageID,
			  maxIndices, moduleCount);
	  deffactsArrayCount++;
	  deffactsFile =
	    CL_CloseFileIfNeeded (theEnv, deffactsFile, &deffactsArrayCount,
				  &deffactsArrayVersion, maxIndices, NULL,
				  NULL);
	}

      moduleCount++;
      moduleArrayCount++;
    }

  CloseDeffactsFiles (theEnv, moduleFile, deffactsFile, maxIndices);

  return true;
}

/*********************************************************/
/* CloseDeffactsFiles: Closes all of the C files created */
/*   for deffacts. Called when an error occurs or when   */
/*   the deffacts have all been written to the files.    */
/*********************************************************/
static void
CloseDeffactsFiles (Environment * theEnv,
		    FILE * moduleFile,
		    FILE * deffactsFile, unsigned maxIndices)
{
  unsigned int count = maxIndices;
  unsigned int arrayVersion = 0;

  if (deffactsFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, deffactsFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }

  if (moduleFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, moduleFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }
}

/**********************************************************/
/* CL_DeffactsModuleToCode: CL_Writes the C code representation */
/*   of a single deffacts module to the specified file.   */
/**********************************************************/
static void
CL_DeffactsModuleToCode (Environment * theEnv,
			 FILE * theFile,
			 Defmodule * theModule,
			 unsigned int imageID,
			 unsigned int maxIndices, unsigned int moduleCount)
{
#if MAC_XCD
#pragma unused(moduleCount)
#endif

  fprintf (theFile, "{");

  CL_ConstructModuleToCode (theEnv, theFile, theModule, imageID, maxIndices,
			    DeffactsData (theEnv)->CL_DeffactsModuleIndex,
			    ConstructPrefix (DeffactsData (theEnv)->
					     DeffactsCodeItem));

  fprintf (theFile, "}");
}

/*********************************************************/
/* DeffactsToCode: CL_Writes the C code representation of a */
/*   single deffacts construct to the specified file.    */
/*********************************************************/
static void
DeffactsToCode (Environment * theEnv,
		FILE * theFile,
		Deffacts * theDeffacts,
		unsigned int imageID,
		unsigned int maxIndices, unsigned int moduleCount)
{
   /*=================*/
  /* Deffacts Header */
   /*=================*/

  fprintf (theFile, "{");

  CL_ConstructHeaderToCode (theEnv, theFile, &theDeffacts->header, imageID,
			    maxIndices, moduleCount,
			    ModulePrefix (DeffactsData (theEnv)->
					  DeffactsCodeItem),
			    ConstructPrefix (DeffactsData (theEnv)->
					     DeffactsCodeItem));

  fprintf (theFile, ",");

   /*=============*/
  /* CL_Assert List */
   /*=============*/

  CL_ExpressionToCode (theEnv, theFile, theDeffacts->assertList);
  fprintf (theFile, "}");
}

/**************************************************************/
/* CL_DeffactsCModuleReference: CL_Writes the C code representation */
/*   of a reference to a deffacts module data structure.      */
/**************************************************************/
void
CL_DeffactsCModuleReference (Environment * theEnv,
			     FILE * theFile,
			     unsigned long count,
			     unsigned int imageID, unsigned int maxIndices)
{
  fprintf (theFile, "MIHS &%s%u_%lu[%lu]",
	   ModulePrefix (DeffactsData (theEnv)->DeffactsCodeItem),
	   imageID, (count / maxIndices) + 1, (count % maxIndices));
}

#endif /* DEFFACTS_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME) */
