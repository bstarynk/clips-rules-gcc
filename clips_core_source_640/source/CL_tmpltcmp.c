   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*          DEFTEMPLATE CONSTRUCTS-TO-C MODULE         */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the constructs-to-c feature for the   */
/*    deftemplate construct.                                 */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Added support for templates CL_maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*            Added code for deftemplate run time            */
/*            initialization of hashed comparisons to        */
/*            constants.                                     */
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

#if DEFTEMPLATE_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME)

#define SlotPrefix() ArbitraryPrefix(DeftemplateData(theEnv)->DeftemplateCodeItem,2)

#include <stdio.h>

#include "conscomp.h"
#include "cstrncmp.h"
#include "envrnmnt.h"
#include "factcmp.h"
#include "tmpltdef.h"

#include "tmpltcmp.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool ConstructToCode (Environment *, const char *, const char *,
			     char *, unsigned int, FILE *, unsigned int,
			     unsigned int);
static void SlotToCode (Environment *, FILE *, struct templateSlot *,
			unsigned int, unsigned int, unsigned int);
static void CL_DeftemplateModuleToCode (Environment *, FILE *, Defmodule *,
					unsigned int, unsigned int,
					unsigned int);
static void DeftemplateToCode (Environment *, FILE *, Deftemplate *,
			       unsigned int, unsigned int, unsigned int,
			       unsigned int);
static void CloseDeftemplateFiles (Environment *, FILE *, FILE *, FILE *,
				   unsigned int);
static void InitDeftemplateCode (Environment *, FILE *, unsigned int,
				 unsigned int);

/*********************************************************/
/* CL_DeftemplateCompilerSetup: Initializes the deftemplate */
/*   construct for use with the constructs-to-c command. */
/*********************************************************/
void
CL_DeftemplateCompilerSetup (Environment * theEnv)
{
  DeftemplateData (theEnv)->DeftemplateCodeItem =
    CL_AddCodeGeneratorItem (theEnv, "deftemplate", 0, NULL,
			     InitDeftemplateCode, ConstructToCode, 3);
}

/*************************************************************/
/* ConstructToCode: Produces deftemplate code for a run-time */
/*   module created using the constructs-to-c function.      */
/*************************************************************/
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
  Deftemplate *theTemplate;
  struct templateSlot *slotPtr;
  unsigned int slotCount = 0, slotArrayCount = 0, slotArrayVersion = 1;
  unsigned int moduleCount = 0, moduleArrayCount = 0, moduleArrayVersion = 1;
  unsigned int templateArrayCount = 0, templateArrayVersion = 1;
  FILE *slotFile = NULL, *moduleFile = NULL, *templateFile = NULL;

   /*==================================================*/
  /* Include the appropriate deftemplate header file. */
   /*==================================================*/

  fprintf (headerFP, "#include \"tmpltdef.h\"\n");

   /*=============================================================*/
  /* Loop through all the modules, all the deftemplates, and all */
  /* the deftemplate slots writing their C code representation   */
  /* to the file as they are traversed.                          */
   /*=============================================================*/

  theModule = CL_GetNextDefmodule (theEnv, NULL);

  while (theModule != NULL)
    {
      CL_SetCurrentModule (theEnv, theModule);

      moduleFile =
	CL_OpenFileIfNeeded (theEnv, moduleFile, fileName, pathName,
			     fileNameBuffer, fileID, imageID, &fileCount,
			     moduleArrayVersion, headerFP,
			     "struct deftemplateModule",
			     ModulePrefix (DeftemplateData
					   (theEnv)->DeftemplateCodeItem),
			     false, NULL);

      if (moduleFile == NULL)
	{
	  CloseDeftemplateFiles (theEnv, moduleFile, templateFile, slotFile,
				 maxIndices);
	  return false;
	}

      CL_DeftemplateModuleToCode (theEnv, moduleFile, theModule, imageID,
				  maxIndices, moduleCount);
      moduleFile =
	CL_CloseFileIfNeeded (theEnv, moduleFile, &moduleArrayCount,
			      &moduleArrayVersion, maxIndices, NULL, NULL);

      /*=======================================================*/
      /* Loop through each of the deftemplates in this module. */
      /*=======================================================*/

      theTemplate = CL_GetNextDeftemplate (theEnv, NULL);

      while (theTemplate != NULL)
	{
	  templateFile =
	    CL_OpenFileIfNeeded (theEnv, templateFile, fileName, pathName,
				 fileNameBuffer, fileID, imageID, &fileCount,
				 templateArrayVersion, headerFP,
				 "Deftemplate",
				 ConstructPrefix (DeftemplateData
						  (theEnv)->
						  DeftemplateCodeItem), false,
				 NULL);
	  if (templateFile == NULL)
	    {
	      CloseDeftemplateFiles (theEnv, moduleFile, templateFile,
				     slotFile, maxIndices);
	      return false;
	    }

	  DeftemplateToCode (theEnv, templateFile, theTemplate, imageID,
			     maxIndices, moduleCount, slotCount);
	  templateArrayCount++;
	  templateFile =
	    CL_CloseFileIfNeeded (theEnv, templateFile, &templateArrayCount,
				  &templateArrayVersion, maxIndices, NULL,
				  NULL);

	 /*======================================================*/
	  /* Loop through each of the slots for this deftemplate. */
	 /*======================================================*/

	  slotPtr = theTemplate->slotList;
	  while (slotPtr != NULL)
	    {
	      slotFile =
		CL_OpenFileIfNeeded (theEnv, slotFile, fileName, pathName,
				     fileNameBuffer, fileID, imageID,
				     &fileCount, slotArrayVersion, headerFP,
				     "struct templateSlot", SlotPrefix (),
				     false, NULL);
	      if (slotFile == NULL)
		{
		  CloseDeftemplateFiles (theEnv, moduleFile, templateFile,
					 slotFile, maxIndices);
		  return false;
		}

	      SlotToCode (theEnv, slotFile, slotPtr, imageID, maxIndices,
			  slotCount);
	      slotCount++;
	      slotArrayCount++;
	      slotFile =
		CL_CloseFileIfNeeded (theEnv, slotFile, &slotArrayCount,
				      &slotArrayVersion, maxIndices, NULL,
				      NULL);
	      slotPtr = slotPtr->next;
	    }

	  theTemplate = CL_GetNextDeftemplate (theEnv, theTemplate);
	}

      theModule = CL_GetNextDefmodule (theEnv, theModule);
      moduleCount++;
      moduleArrayCount++;

    }

  CloseDeftemplateFiles (theEnv, moduleFile, templateFile, slotFile,
			 maxIndices);

  return true;
}

/************************************************************/
/* CloseDeftemplateFiles: Closes all of the C files created */
/*   for deftemplates. Called when an error occurs or when  */
/*   the deftemplates have all been written to the files.   */
/************************************************************/
static void
CloseDeftemplateFiles (Environment * theEnv,
		       FILE * moduleFile,
		       FILE * templateFile,
		       FILE * slotFile, unsigned int maxIndices)
{
  unsigned int count = maxIndices;
  unsigned int arrayVersion = 0;

  if (slotFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, slotFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }

  if (templateFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, templateFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }

  if (moduleFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, moduleFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }
}

/*************************************************************/
/* CL_DeftemplateModuleToCode: CL_Writes the C code representation */
/*   of a single deftemplate module to the specified file.   */
/*************************************************************/
static void
CL_DeftemplateModuleToCode (Environment * theEnv,
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
			    DeftemplateData
			    (theEnv)->CL_DeftemplateModuleIndex,
			    ConstructPrefix (DeftemplateData
					     (theEnv)->DeftemplateCodeItem));

  fprintf (theFile, "}");
}

/************************************************************/
/* DeftemplateToCode: CL_Writes the C code representation of a */
/*   single deftemplate construct to the specified file.    */
/************************************************************/
static void
DeftemplateToCode (Environment * theEnv,
		   FILE * theFile,
		   Deftemplate * theTemplate,
		   unsigned int imageID,
		   unsigned int maxIndices,
		   unsigned int moduleCount, unsigned int slotCount)
{
   /*====================*/
  /* Deftemplate Header */
   /*====================*/

  fprintf (theFile, "{");

  CL_ConstructHeaderToCode (theEnv, theFile, &theTemplate->header, imageID,
			    maxIndices, moduleCount,
			    ModulePrefix (DeftemplateData
					  (theEnv)->DeftemplateCodeItem),
			    ConstructPrefix (DeftemplateData
					     (theEnv)->DeftemplateCodeItem));
  fprintf (theFile, ",");

   /*===========*/
  /* Slot List */
   /*===========*/

  if (theTemplate->slotList == NULL)
    {
      fprintf (theFile, "NULL,");
    }
  else
    {
      fprintf (theFile, "&%s%d_%d[%d],", SlotPrefix (),
	       imageID, (slotCount / maxIndices) + 1, slotCount % maxIndices);
    }

   /*==========================================*/
  /* Implied Flag, CL_Watch Flag, In Scope Flag, */
  /* Number of Slots, and Busy Count.         */
   /*==========================================*/

  fprintf (theFile, "%d,0,0,%d,%ld,", theTemplate->implied,
	   theTemplate->numberOfSlots, theTemplate->busyCount);

   /*=================*/
  /* Pattern Network */
   /*=================*/

  if (theTemplate->patternNetwork == NULL)
    {
      fprintf (theFile, "NULL");
    }
  else
    {
      CL_FactPatternNodeReference (theEnv, theTemplate->patternNetwork,
				   theFile, imageID, maxIndices);
    }

   /*============================================*/
  /* Print the factList and lastFact references */
  /* and close the structure.                   */
   /*============================================*/

  fprintf (theFile, ",NULL,NULL}");
}

/*****************************************************/
/* SlotToCode: CL_Writes the C code representation of a */
/*   single deftemplate slot to the specified file.  */
/*****************************************************/
static void
SlotToCode (Environment * theEnv,
	    FILE * theFile,
	    struct templateSlot *theSlot,
	    unsigned int imageID,
	    unsigned int maxIndices, unsigned int slotCount)
{
   /*===========*/
  /* Slot Name */
   /*===========*/

  fprintf (theFile, "{");
  CL_PrintSymbolReference (theEnv, theFile, theSlot->slotName);

   /*=============================*/
  /* Multislot and Default Flags */
   /*=============================*/

  fprintf (theFile, ",%d,%d,%d,%d,", theSlot->multislot, theSlot->noDefault,
	   theSlot->defaultPresent, theSlot->defaultDynamic);

   /*=============*/
  /* Constraints */
   /*=============*/

  CL_PrintConstraintReference (theEnv, theFile, theSlot->constraints, imageID,
			       maxIndices);

   /*===============*/
  /* Default Value */
   /*===============*/

  fprintf (theFile, ",");
  CL_PrintHashedExpressionReference (theEnv, theFile, theSlot->defaultList,
				     imageID, maxIndices);

   /*============*/
  /* Facet List */
   /*============*/

  fprintf (theFile, ",");
  CL_PrintHashedExpressionReference (theEnv, theFile, theSlot->facetList,
				     imageID, maxIndices);
  fprintf (theFile, ",");

   /*===========*/
  /* Next Slot */
   /*===========*/

  if (theSlot->next == NULL)
    {
      fprintf (theFile, "NULL}");
    }
  else
    {
      fprintf (theFile, "&%s%d_%d[%d]}", SlotPrefix (), imageID,
	       ((slotCount + 1) / maxIndices) + 1,
	       (slotCount + 1) % maxIndices);
    }
}

/*****************************************************************/
/* CL_DeftemplateCModuleReference: CL_Writes the C code representation */
/*   of a reference to a deftemplate module data structure.      */
/*****************************************************************/
void
CL_DeftemplateCModuleReference (Environment * theEnv,
				FILE * theFile,
				unsigned long count,
				unsigned int imageID, unsigned int maxIndices)
{
  fprintf (theFile, "MIHS &%s%u_%lu[%lu]",
	   ModulePrefix (DeftemplateData (theEnv)->DeftemplateCodeItem),
	   imageID, (count / maxIndices) + 1, (count % maxIndices));
}

/********************************************************************/
/* CL_DeftemplateCConstructReference: CL_Writes the C code representation */
/*   of a reference to a deftemplate data structure.                */
/********************************************************************/
void
CL_DeftemplateCConstructReference (Environment * theEnv,
				   FILE * theFile,
				   Deftemplate * theDeftemplate,
				   unsigned int imageID,
				   unsigned int maxIndices)
{
  if (theDeftemplate == NULL)
    {
      fprintf (theFile, "NULL");
    }
  else
    {
      fprintf (theFile, "&%s%u_%lu[%lu]",
	       ConstructPrefix (DeftemplateData
				(theEnv)->DeftemplateCodeItem), imageID,
	       (theDeftemplate->header.bsaveID / maxIndices) + 1,
	       theDeftemplate->header.bsaveID % maxIndices);
    }

}

/*******************************************/
/* InitDeftemplateCode: CL_Writes out runtime */
/*   initialization code for deftemplates. */
/*******************************************/
static void
InitDeftemplateCode (Environment * theEnv,
		     FILE * initFP,
		     unsigned int imageID, unsigned int maxIndices)
{
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(imageID)
#pragma unused(maxIndices)
#endif

  fprintf (initFP, "   Deftemplate_RunTimeInitialize(theEnv);\n");
}

#endif /* DEFTEMPLATE_CONSTRUCT && CONSTRUCT_COMPILER && (! RUN_TIME) */
