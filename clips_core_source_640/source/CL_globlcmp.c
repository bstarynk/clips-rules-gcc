   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*            DEFGLOBAL CONSTRUCTS-TO-C MODULE         */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the constructs-to-c feature for the   */
/*    defglobal construct.                                   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added support for path name argument to        */
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

#include "setup.h"

#if DEFGLOBAL_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME)

#include <stdio.h>

#include "conscomp.h"
#include "envrnmnt.h"
#include "globldef.h"

#include "globlcmp.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool ConstructToCode (Environment *, const char *, const char *,
			     char *, unsigned int, FILE *, unsigned int,
			     unsigned int);
static void DefglobalToCode (Environment *, FILE *, Defglobal *, unsigned int,
			     unsigned int, unsigned int);
static void CL_DefglobalModuleToCode (Environment *, FILE *, Defmodule *,
				      unsigned int, unsigned int,
				      unsigned int);
static void CloseDefglobalFiles (Environment *, FILE *, FILE *, unsigned int);
static void BeforeDefglobalsToCode (Environment *);
static void InitDefglobalsCode (Environment *, FILE *, unsigned, unsigned);

/***************************************************************/
/* CL_DefglobalCompilerSetup: Initializes the defglobal construct */
/*    for use with the constructs-to-c command.                */
/***************************************************************/
void
CL_DefglobalCompilerSetup (Environment * theEnv)
{
  DefglobalData (theEnv)->DefglobalCodeItem =
    CL_AddCodeGeneratorItem (theEnv, "defglobal", 0, BeforeDefglobalsToCode,
			     InitDefglobalsCode, ConstructToCode, 2);
}

/**************************************************************/
/* BeforeDefglobalsToCode: Assigns each defglobal a unique ID */
/*   which will be used for pointer references when the data  */
/*   structures are written to a file as C code               */
/**************************************************************/
static void
BeforeDefglobalsToCode (Environment * theEnv)
{
  MarkConstruct_BsaveIDs (theEnv,
			  DefglobalData (theEnv)->CL_DefglobalModuleIndex);
}

/*************************************************/
/* InitDefglobalsCode: CL_Writes out initialization */
/*   code for defglobals for a run-time module.  */
/*************************************************/
static void
InitDefglobalsCode (Environment * theEnv,
		    FILE * initFP, unsigned imageID, unsigned maxIndices)
{
#if MAC_XCD
#pragma unused(maxIndices)
#pragma unused(imageID)
#pragma unused(theEnv)
#endif
  fprintf (initFP, "   Defglobal_RunTimeInitialize(theEnv);\n");
  fprintf (initFP, "   CL_ResetDefglobals(theEnv,NULL);\n");
}

/***********************************************************/
/* ConstructToCode: Produces defglobal code for a run-time */
/*   module created using the constructs-to-c function.    */
/***********************************************************/
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
  Defglobal *theDefglobal;
  unsigned int moduleCount = 0, moduleArrayCount = 0, moduleArrayVersion = 1;
  unsigned int defglobalArrayCount = 0, defglobalArrayVersion = 1;
  FILE *moduleFile = NULL, *defglobalFile = NULL;

   /*================================================*/
  /* Include the appropriate defglobal header file. */
   /*================================================*/

  fprintf (headerFP, "#include \"globldef.h\"\n");

   /*===================================================================*/
  /* Loop through all the modules and all the defglobals writing their */
  /*  C code representation to the file as they are traversed.         */
   /*===================================================================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      CL_SetCurrentModule (theEnv, theModule);

      moduleFile =
	CL_OpenFileIfNeeded (theEnv, moduleFile, fileName, pathName,
			     fileNameBuffer, fileID, imageID, &fileCount,
			     moduleArrayVersion, headerFP,
			     "struct defglobalModule",
			     ModulePrefix (DefglobalData (theEnv)->
					   DefglobalCodeItem), false, NULL);

      if (moduleFile == NULL)
	{
	  CloseDefglobalFiles (theEnv, moduleFile, defglobalFile, maxIndices);
	  return false;
	}

      CL_DefglobalModuleToCode (theEnv, moduleFile, theModule, imageID,
				maxIndices, moduleCount);
      moduleFile =
	CL_CloseFileIfNeeded (theEnv, moduleFile, &moduleArrayCount,
			      &moduleArrayVersion, maxIndices, NULL, NULL);

      for (theDefglobal = CL_GetNextDefglobal (theEnv, NULL);
	   theDefglobal != NULL;
	   theDefglobal = CL_GetNextDefglobal (theEnv, theDefglobal))
	{
	  defglobalFile =
	    CL_OpenFileIfNeeded (theEnv, defglobalFile, fileName, pathName,
				 fileNameBuffer, fileID, imageID, &fileCount,
				 defglobalArrayVersion, headerFP, "Defglobal",
				 ConstructPrefix (DefglobalData (theEnv)->
						  DefglobalCodeItem), false,
				 NULL);
	  if (defglobalFile == NULL)
	    {
	      CloseDefglobalFiles (theEnv, moduleFile, defglobalFile,
				   maxIndices);
	      return false;
	    }

	  DefglobalToCode (theEnv, defglobalFile, theDefglobal, imageID,
			   maxIndices, moduleCount);
	  defglobalArrayCount++;
	  defglobalFile =
	    CL_CloseFileIfNeeded (theEnv, defglobalFile, &defglobalArrayCount,
				  &defglobalArrayVersion, maxIndices, NULL,
				  NULL);
	}

      moduleCount++;
      moduleArrayCount++;
    }

  CloseDefglobalFiles (theEnv, moduleFile, defglobalFile, maxIndices);

  return true;
}

/**********************************************************/
/* CloseDefglobalFiles: Closes all of the C files created */
/*   for defglobals. Called when an error occurs or when  */
/*   the defglobals have all been written to the files.   */
/**********************************************************/
static void
CloseDefglobalFiles (Environment * theEnv,
		     FILE * moduleFile,
		     FILE * defglobalFile, unsigned int maxIndices)
{
  unsigned int count = maxIndices;
  unsigned int arrayVersion = 0;

  if (defglobalFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, defglobalFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }

  if (moduleFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, moduleFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }
}

/***********************************************************/
/* CL_DefglobalModuleToCode: CL_Writes the C code representation */
/*   of a single defglobal module to the specified file.   */
/***********************************************************/
static void
CL_DefglobalModuleToCode (Environment * theEnv,
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
			    DefglobalData (theEnv)->CL_DefglobalModuleIndex,
			    ConstructPrefix (DefglobalData (theEnv)->
					     DefglobalCodeItem));

  fprintf (theFile, "}");
}

/**********************************************************/
/* DefglobalToCode: CL_Writes the C code representation of a */
/*   single defglobal construct to the specified file.    */
/**********************************************************/
static void
DefglobalToCode (Environment * theEnv,
		 FILE * theFile,
		 Defglobal * theDefglobal,
		 unsigned int imageID,
		 unsigned int maxIndices, unsigned int moduleCount)
{
   /*==================*/
  /* Defglobal Header */
   /*==================*/

  fprintf (theFile, "{");

  CL_ConstructHeaderToCode (theEnv, theFile, &theDefglobal->header, imageID,
			    maxIndices, moduleCount,
			    ModulePrefix (DefglobalData (theEnv)->
					  DefglobalCodeItem),
			    ConstructPrefix (DefglobalData (theEnv)->
					     DefglobalCodeItem));

  fprintf (theFile, ",");

   /*============================================*/
  /* CL_Watch Flag, In Scope Flag, and Busy Count. */
   /*============================================*/

  fprintf (theFile, "0,0,%ld,", theDefglobal->busyCount);

   /*================*/
  /* Current Value. */
   /*================*/

  fprintf (theFile, "{ { NULL } }");

   /*=====================*/
  /* Initial Expression. */
   /*=====================*/

  fprintf (theFile, ",");
  CL_PrintHashedExpressionReference (theEnv, theFile, theDefglobal->initial,
				     imageID, maxIndices);

  fprintf (theFile, "}");
}

/***************************************************************/
/* CL_DefglobalCModuleReference: CL_Writes the C code representation */
/*   of a reference to a defglobal module data structure.      */
/***************************************************************/
void
CL_DefglobalCModuleReference (Environment * theEnv,
			      FILE * theFile,
			      unsigned long count,
			      unsigned int imageID, unsigned int maxIndices)
{
  fprintf (theFile, "MIHS &%s%u_%lu[%lu]",
	   ModulePrefix (DefglobalData (theEnv)->DefglobalCodeItem),
	   imageID, (count / maxIndices) + 1, (count % maxIndices));
}

/******************************************************************/
/* CL_DefglobalCConstructReference: CL_Writes the C code representation */
/*   of a reference to a defglobal data structure.                */
/******************************************************************/
void
CL_DefglobalCConstructReference (Environment * theEnv,
				 FILE * theFile,
				 Defglobal * theGlobal,
				 unsigned int imageID,
				 unsigned int maxIndices)
{

  if (theGlobal == NULL)
    {
      fprintf (theFile, "NULL");
    }
  else
    {
      fprintf (theFile, "&%s%u_%lu[%lu]",
	       ConstructPrefix (DefglobalData (theEnv)->DefglobalCodeItem),
	       imageID, (theGlobal->header.bsaveID / maxIndices) + 1,
	       theGlobal->header.bsaveID % maxIndices);
    }

}

#endif /* DEFGLOBAL_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME) */
