
   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*            DEFRULE CONSTRUCTS-TO-C MODULE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the constructs-to-c feature for the   */
/*    defrule construct.                                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed DYNAMIC_SALIENCE and                   */
/*            LOGICAL_DEPENDENCIES compilation flags.        */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Support for join network changes.              */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.31: Fixed disjunct bug in defrule iteration.       */
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

#if DEFRULE_CONSTRUCT && (! RUN_TIME) && CONSTRUCT_COMPILER

#include <stdio.h>
#include <string.h>

#include "envrnmnt.h"
#include "factbld.h"
#include "pattern.h"
#include "reteutil.h"

#include "rulecmp.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool ConstructToCode (Environment *, const char *, const char *,
			     char *, unsigned int, FILE *, unsigned int,
			     unsigned int);
static void JoinToCode (Environment *, FILE *, struct joinNode *,
			unsigned int, unsigned int);
static void LinkToCode (Environment *, FILE *, struct joinLink *,
			unsigned int, unsigned int);
static void CL_DefruleModuleToCode (Environment *, FILE *, Defmodule *,
				    unsigned int, unsigned int, unsigned int);
static void DefruleToCode (Environment *, FILE *, Defrule *, unsigned int,
			   unsigned int, unsigned int);
static void CloseDefruleFiles (Environment *, FILE *, FILE *, FILE *, FILE *,
			       unsigned int);
static void BeforeDefrulesCode (Environment *);
static void InitDefruleCode (Environment *, FILE *, unsigned int,
			     unsigned int);
static bool RuleCompilerTraverseJoins (Environment *, struct joinNode *,
				       const char *, const char *, char *,
				       unsigned int, FILE *, unsigned int,
				       unsigned int, FILE **, FILE **,
				       unsigned int *, unsigned int *,
				       unsigned int *, unsigned int *,
				       unsigned int *);
static bool TraverseJoinLinks (Environment *, struct joinLink *, const char *,
			       const char *, char *, unsigned int, FILE *,
			       unsigned int, unsigned int, FILE **,
			       unsigned int *, unsigned int *,
			       unsigned int *);

/***********************************************************/
/* CL_DefruleCompilerSetup: Initializes the defrule construct */
/*   for use with the constructs-to-c command.             */
/***********************************************************/
void
CL_DefruleCompilerSetup (Environment * theEnv)
{
  DefruleData (theEnv)->DefruleCodeItem =
    CL_AddCodeGeneratorItem (theEnv, "defrules", 0, BeforeDefrulesCode,
			     InitDefruleCode, ConstructToCode, 4);
}

/**************************************************************/
/* BeforeDefrulesCode: Assigns each defrule and join with a   */
/*   unique ID which will be used for pointer references when */
/*   the data structures are written to a file as C code      */
/**************************************************************/
static void
BeforeDefrulesCode (Environment * theEnv)
{
  unsigned long moduleCount, ruleCount, joinCount, linkCount;

  CL_TagRuleNetwork (theEnv, &moduleCount, &ruleCount, &joinCount,
		     &linkCount);
}

/*********************************************************/
/* ConstructToCode: Produces defrule code for a run-time */
/*   module created using the constructs-to-c function.  */
/*********************************************************/
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
  Defrule *theDefrule, *theDisjunct;
  unsigned int joinArrayCount = 0, joinArrayVersion = 1;
  unsigned int linkArrayCount = 0, linkArrayVersion = 1;
  unsigned int moduleCount = 0, moduleArrayCount = 0, moduleArrayVersion = 1;
  unsigned int defruleArrayCount = 0, defruleArrayVersion = 1;
  FILE *joinFile = NULL, *moduleFile = NULL, *defruleFile = NULL, *linkFile =
    NULL;

   /*==============================================*/
  /* Include the appropriate defrule header file. */
   /*==============================================*/

  fprintf (headerFP, "#include \"ruledef.h\"\n");

   /*======================================*/
  /* CL_Save the left and right prime links. */
   /*======================================*/

  if (!TraverseJoinLinks
      (theEnv, DefruleData (theEnv)->LeftPrimeJoins, fileName, pathName,
       fileNameBuffer, fileID, headerFP, imageID, maxIndices, &linkFile,
       &fileCount, &linkArrayVersion, &linkArrayCount))
    {
      CloseDefruleFiles (theEnv, moduleFile, defruleFile, joinFile, linkFile,
			 maxIndices);
      return false;
    }

  if (!TraverseJoinLinks
      (theEnv, DefruleData (theEnv)->RightPrimeJoins, fileName, pathName,
       fileNameBuffer, fileID, headerFP, imageID, maxIndices, &linkFile,
       &fileCount, &linkArrayVersion, &linkArrayCount))
    {
      CloseDefruleFiles (theEnv, moduleFile, defruleFile, joinFile, linkFile,
			 maxIndices);
      return false;
    }

   /*=========================================================*/
  /* Loop through all the modules, all the defrules, and all */
  /* the join nodes writing their C code representation to   */
  /* the file as they are traversed.                         */
   /*=========================================================*/

  for (theModule = CL_GetNextDefmodule (theEnv, NULL);
       theModule != NULL; theModule = CL_GetNextDefmodule (theEnv, theModule))
    {
      /*=========================*/
      /* Set the current module. */
      /*=========================*/

      CL_SetCurrentModule (theEnv, theModule);

      /*==========================*/
      /* CL_Save the defrule module. */
      /*==========================*/

      moduleFile =
	CL_OpenFileIfNeeded (theEnv, moduleFile, fileName, pathName,
			     fileNameBuffer, fileID, imageID, &fileCount,
			     moduleArrayVersion, headerFP,
			     "struct defruleModule",
			     ModulePrefix (DefruleData
					   (theEnv)->DefruleCodeItem), false,
			     NULL);

      if (moduleFile == NULL)
	{
	  CloseDefruleFiles (theEnv, moduleFile, defruleFile, joinFile,
			     linkFile, maxIndices);
	  return false;
	}

      CL_DefruleModuleToCode (theEnv, moduleFile, theModule, imageID,
			      maxIndices, moduleCount);
      moduleFile =
	CL_CloseFileIfNeeded (theEnv, moduleFile, &moduleArrayCount,
			      &moduleArrayVersion, maxIndices, NULL, NULL);

      /*=========================================*/
      /* Loop through all of the defrules (and   */
      /* their disjuncts) in the current module. */
      /*=========================================*/

      for (theDefrule = CL_GetNextDefrule (theEnv, NULL);
	   theDefrule != NULL;
	   theDefrule = CL_GetNextDefrule (theEnv, theDefrule))
	{
	  for (theDisjunct = theDefrule;
	       theDisjunct != NULL; theDisjunct = theDisjunct->disjunct)
	    {
	    /*===================================*/
	      /* CL_Save the defrule data structures. */
	    /*===================================*/

	      defruleFile =
		CL_OpenFileIfNeeded (theEnv, defruleFile, fileName, pathName,
				     fileNameBuffer, fileID, imageID,
				     &fileCount, defruleArrayVersion,
				     headerFP, "Defrule",
				     ConstructPrefix (DefruleData
						      (theEnv)->
						      DefruleCodeItem), false,
				     NULL);
	      if (defruleFile == NULL)
		{
		  CloseDefruleFiles (theEnv, moduleFile, defruleFile,
				     joinFile, linkFile, maxIndices);
		  return false;
		}

	      DefruleToCode (theEnv, defruleFile, theDisjunct, imageID,
			     maxIndices, moduleCount);
	      defruleArrayCount++;
	      defruleFile =
		CL_CloseFileIfNeeded (theEnv, defruleFile, &defruleArrayCount,
				      &defruleArrayVersion, maxIndices, NULL,
				      NULL);

	    /*================================*/
	      /* CL_Save the join data structures. */
	    /*================================*/

	      if (!RuleCompilerTraverseJoins
		  (theEnv, theDisjunct->lastJoin, fileName, pathName,
		   fileNameBuffer, fileID, headerFP, imageID, maxIndices,
		   &joinFile, &linkFile, &fileCount, &joinArrayVersion,
		   &joinArrayCount, &linkArrayVersion, &linkArrayCount))
		{
		  CloseDefruleFiles (theEnv, moduleFile, defruleFile,
				     joinFile, linkFile, maxIndices);
		  return false;
		}
	    }
	}

      moduleCount++;
      moduleArrayCount++;
    }

  CloseDefruleFiles (theEnv, moduleFile, defruleFile, joinFile, linkFile,
		     maxIndices);

  return true;
}

/*********************************************************************/
/* RuleCompilerTraverseJoins: Traverses the join network for a rule. */
/*********************************************************************/
static bool
RuleCompilerTraverseJoins (Environment * theEnv,
			   struct joinNode *joinPtr,
			   const char *fileName,
			   const char *pathName,
			   char *fileNameBuffer,
			   unsigned int fileID,
			   FILE * headerFP,
			   unsigned int imageID,
			   unsigned int maxIndices,
			   FILE ** joinFile,
			   FILE ** linkFile,
			   unsigned int *fileCount,
			   unsigned int *joinArrayVersion,
			   unsigned int *joinArrayCount,
			   unsigned int *linkArrayVersion,
			   unsigned int *linkArrayCount)
{
  for (; joinPtr != NULL; joinPtr = joinPtr->lastLevel)
    {
      if (joinPtr->marked)
	{
	  *joinFile =
	    CL_OpenFileIfNeeded (theEnv, *joinFile, fileName, pathName,
				 fileNameBuffer, fileID, imageID, fileCount,
				 *joinArrayVersion, headerFP,
				 "struct joinNode", JoinPrefix (), false,
				 NULL);
	  if (*joinFile == NULL)
	    {
	      return false;
	    }

	  JoinToCode (theEnv, *joinFile, joinPtr, imageID, maxIndices);
	  (*joinArrayCount)++;
	  *joinFile =
	    CL_CloseFileIfNeeded (theEnv, *joinFile, joinArrayCount,
				  joinArrayVersion, maxIndices, NULL, NULL);


	  if (!TraverseJoinLinks
	      (theEnv, joinPtr->nextLinks, fileName, pathName, fileNameBuffer,
	       fileID, headerFP, imageID, maxIndices, linkFile, fileCount,
	       linkArrayVersion, linkArrayCount))
	    {
	      return false;
	    }
	}

      if (joinPtr->joinFromTheRight)
	{
	  if (RuleCompilerTraverseJoins
	      (theEnv, (struct joinNode *) joinPtr->rightSideEntryStructure,
	       fileName, pathName, fileNameBuffer, fileID, headerFP, imageID,
	       maxIndices, joinFile, linkFile, fileCount, joinArrayVersion,
	       joinArrayCount, linkArrayVersion, linkArrayCount) == false)
	    {
	      return false;
	    }
	}
    }

  return true;
}

/*******************************************************/
/* TraverseJoinLinks: CL_Writes out a list of join links. */
/*******************************************************/
static bool
TraverseJoinLinks (Environment * theEnv,
		   struct joinLink *linkPtr,
		   const char *fileName,
		   const char *pathName,
		   char *fileNameBuffer,
		   unsigned int fileID,
		   FILE * headerFP,
		   unsigned int imageID,
		   unsigned int maxIndices,
		   FILE ** linkFile,
		   unsigned int *fileCount,
		   unsigned int *linkArrayVersion,
		   unsigned int *linkArrayCount)
{
  for (; linkPtr != NULL; linkPtr = linkPtr->next)
    {
      *linkFile =
	CL_OpenFileIfNeeded (theEnv, *linkFile, fileName, pathName,
			     fileNameBuffer, fileID, imageID, fileCount,
			     *linkArrayVersion, headerFP, "struct joinLink",
			     LinkPrefix (), false, NULL);

      if (*linkFile == NULL)
	{
	  return false;
	}

      LinkToCode (theEnv, *linkFile, linkPtr, imageID, maxIndices);
      (*linkArrayCount)++;
      *linkFile =
	CL_CloseFileIfNeeded (theEnv, *linkFile, linkArrayCount,
			      linkArrayVersion, maxIndices, NULL, NULL);
    }

  return true;
}

/********************************************************/
/* CloseDefruleFiles: Closes all of the C files created */
/*   for defrule. Called when an error occurs or when   */
/*   the defrules have all been written to the files.   */
/********************************************************/
static void
CloseDefruleFiles (Environment * theEnv,
		   FILE * moduleFile,
		   FILE * defruleFile,
		   FILE * joinFile, FILE * linkFile, unsigned int maxIndices)
{
  unsigned int count = maxIndices;
  unsigned int arrayVersion = 0;

  if (linkFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, linkFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }

  if (joinFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, joinFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }

  if (defruleFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, defruleFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }

  if (moduleFile != NULL)
    {
      count = maxIndices;
      CL_CloseFileIfNeeded (theEnv, moduleFile, &count, &arrayVersion,
			    maxIndices, NULL, NULL);
    }
}

/*********************************************************/
/* CL_DefruleModuleToCode: CL_Writes the C code representation */
/*   of a single defrule module to the specified file.   */
/*********************************************************/
static void
CL_DefruleModuleToCode (Environment * theEnv,
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
			    DefruleData (theEnv)->CL_DefruleModuleIndex,
			    ConstructPrefix (DefruleData
					     (theEnv)->DefruleCodeItem));

  fprintf (theFile, ",NULL}");
}

/**********************************************************/
/* DefruleToCode: CL_Writes the C code representation of a   */
/*   single defrule data structure to the specified file. */
/**********************************************************/
static void
DefruleToCode (Environment * theEnv,
	       FILE * theFile,
	       Defrule * theDefrule,
	       unsigned int imageID,
	       unsigned int maxIndices, unsigned int moduleCount)
{
   /*==================*/
  /* Construct Header */
   /*==================*/

  fprintf (theFile, "{");

  CL_ConstructHeaderToCode (theEnv, theFile, &theDefrule->header, imageID,
			    maxIndices, moduleCount,
			    ModulePrefix (DefruleData
					  (theEnv)->DefruleCodeItem),
			    ConstructPrefix (DefruleData
					     (theEnv)->DefruleCodeItem));

   /*==========================*/
  /* Flags and Integer Values */
   /*==========================*/

  fprintf (theFile, ",%d,%d,%d,%d,%d,%d,%d,%d,",
	   theDefrule->salience, theDefrule->localVarCnt,
	   theDefrule->complexity, theDefrule->afterBreakpoint,
	   theDefrule->watchActivation, theDefrule->watchFiring,
	   theDefrule->auto_Focus, theDefrule->executing);

   /*==================*/
  /* Dynamic Salience */
   /*==================*/

  CL_ExpressionToCode (theEnv, theFile, theDefrule->dynamicSalience);
  fprintf (theFile, ",");

   /*=============*/
  /* RHS Actions */
   /*=============*/

  CL_ExpressionToCode (theEnv, theFile, theDefrule->actions);
  fprintf (theFile, ",");

   /*=========================*/
  /* Logical Dependency Join */
   /*=========================*/

  if (theDefrule->logicalJoin != NULL)
    {
      fprintf (theFile, "&%s%d_%ld[%ld],", JoinPrefix (),
	       imageID, (theDefrule->logicalJoin->bsaveID / maxIndices) + 1,
	       theDefrule->logicalJoin->bsaveID % maxIndices);
    }
  else
    {
      fprintf (theFile, "NULL,");
    }

   /*===========*/
  /* Last Join */
   /*===========*/

  if (theDefrule->lastJoin != NULL)
    {
      fprintf (theFile, "&%s%d_%ld[%ld],", JoinPrefix (),
	       imageID, (theDefrule->lastJoin->bsaveID / maxIndices) + 1,
	       theDefrule->lastJoin->bsaveID % maxIndices);
    }
  else
    {
      fprintf (theFile, "NULL,");
    }

   /*===============*/
  /* Next Disjunct */
   /*===============*/

  if (theDefrule->disjunct != NULL)
    {
      fprintf (theFile, "&%s%d_%ld[%ld]}",
	       ConstructPrefix (DefruleData (theEnv)->DefruleCodeItem),
	       imageID,
	       (theDefrule->disjunct->header.bsaveID / maxIndices) + 1,
	       theDefrule->disjunct->header.bsaveID % maxIndices);
    }
  else
    {
      fprintf (theFile, "NULL}");
    }
}

/***************************************************/
/* JoinToCode: CL_Writes the C code representation of */
/*   a single join node to the specified file.     */
/***************************************************/
static void
JoinToCode (Environment * theEnv,
	    FILE * joinFile,
	    struct joinNode *theJoin,
	    unsigned int imageID, unsigned int maxIndices)
{
  struct patternParser *theParser;

   /*===========================*/
  /* Mark the join as visited. */
   /*===========================*/

  theJoin->marked = 0;

   /*===========================*/
  /* Flags and Integer Values. */
   /*===========================*/

  fprintf (joinFile, "{%d,%d,%d,%d,%d,0,0,%d,%d,0,0,0,0,0,0,",
	   theJoin->firstJoin, theJoin->logicalJoin,
	   theJoin->joinFromTheRight, theJoin->patternIsNegated,
	   theJoin->patternIsExists,
	   // initialize,
	   // marked
	   theJoin->rhsType, theJoin->depth);
  // bsaveID
  // memoryLeftAdds
  // memoryRightAdds
  // memoryLeftDeletes
  // memoryRightDeletes
  // memoryCompares

   /*==========================*/
  /* Left and right Memories. */
   /*==========================*/

  fprintf (joinFile, "NULL,NULL,");

   /*====================*/
  /* Network Expression */
   /*====================*/

  CL_PrintHashedExpressionReference (theEnv, joinFile, theJoin->networkTest,
				     imageID, maxIndices);
  fprintf (joinFile, ",");

  CL_PrintHashedExpressionReference (theEnv, joinFile,
				     theJoin->secondaryNetworkTest, imageID,
				     maxIndices);
  fprintf (joinFile, ",");

  CL_PrintHashedExpressionReference (theEnv, joinFile, theJoin->leftHash,
				     imageID, maxIndices);
  fprintf (joinFile, ",");

  CL_PrintHashedExpressionReference (theEnv, joinFile, theJoin->rightHash,
				     imageID, maxIndices);
  fprintf (joinFile, ",");

   /*============================*/
  /* Right Side Entry Structure */
   /*============================*/

  if (theJoin->rightSideEntryStructure == NULL)
    {
      fprintf (joinFile, "NULL,");
    }
  else if (theJoin->joinFromTheRight == false)
    {
      theParser = CL_GetPatternParser (theEnv, theJoin->rhsType);
      if (theParser->codeReferenceFunction == NULL)
	fprintf (joinFile, "NULL,");
      else
	{
	  fprintf (joinFile, "VS ");
	  (*theParser->codeReferenceFunction) (theEnv,
					       theJoin->
					       rightSideEntryStructure,
					       joinFile, imageID, maxIndices);
	  fprintf (joinFile, ",");
	}
    }
  else
    {
      fprintf (joinFile, "&%s%u_%lu[%lu],", JoinPrefix (),
	       imageID,
	       (((struct joinNode *) theJoin->rightSideEntryStructure)->
		bsaveID / maxIndices) + 1,
	       ((struct joinNode *) theJoin->rightSideEntryStructure)->
	       bsaveID % maxIndices);
    }

   /*=================*/
  /* Next Join Level */
   /*=================*/

  if (theJoin->nextLinks == NULL)
    {
      fprintf (joinFile, "NULL,");
    }
  else
    {
      fprintf (joinFile, "&%s%d_%ld[%ld],", LinkPrefix (),
	       imageID, (theJoin->nextLinks->bsaveID / maxIndices) + 1,
	       theJoin->nextLinks->bsaveID % maxIndices);
    }

   /*=================*/
  /* Last Join Level */
   /*=================*/

  if (theJoin->lastLevel == NULL)
    {
      fprintf (joinFile, "NULL,");
    }
  else
    {
      fprintf (joinFile, "&%s%d_%ld[%ld],", JoinPrefix (),
	       imageID, (theJoin->lastLevel->bsaveID / maxIndices) + 1,
	       theJoin->lastLevel->bsaveID % maxIndices);
    }

   /*==================*/
  /* Right Match Node */
   /*==================*/

  if (theJoin->rightMatchNode == NULL)
    {
      fprintf (joinFile, "NULL,");
    }
  else
    {
      fprintf (joinFile, "&%s%d_%ld[%ld],", JoinPrefix (),
	       imageID, (theJoin->rightMatchNode->bsaveID / maxIndices) + 1,
	       theJoin->rightMatchNode->bsaveID % maxIndices);
    }

   /*==================*/
  /* Rule to Activate */
   /*==================*/

  if (theJoin->ruleToActivate == NULL)
    {
      fprintf (joinFile, "NULL}");
    }
  else
    {
      fprintf (joinFile, "&%s%d_%ld[%ld]}",
	       ConstructPrefix (DefruleData (theEnv)->DefruleCodeItem),
	       imageID,
	       (theJoin->ruleToActivate->header.bsaveID / maxIndices) + 1,
	       theJoin->ruleToActivate->header.bsaveID % maxIndices);
    }
}

/***************************************************/
/* LinkToCode: CL_Writes the C code representation of */
/*   a single join node to the specified file.     */
/***************************************************/
static void
LinkToCode (Environment * theEnv,
	    FILE * theFile,
	    struct joinLink *theLink,
	    unsigned int imageID, unsigned int maxIndices)
{
   /*==================*/
  /* Enter Direction. */
   /*==================*/

  fprintf (theFile, "{%d,", theLink->enterDirection);

   /*======*/
  /* Join */
   /*======*/

  if (theLink->join == NULL)
    {
      fprintf (theFile, "NULL,");
    }
  else
    {
      fprintf (theFile, "&%s%d_%ld[%ld],", JoinPrefix (),
	       imageID, (theLink->join->bsaveID / maxIndices) + 1,
	       theLink->join->bsaveID % maxIndices);
    }

   /*======*/
  /* Next */
   /*======*/

  if (theLink->next == NULL)
    {
      fprintf (theFile, "NULL,");
    }
  else
    {
      fprintf (theFile, "&%s%d_%ld[%ld],", LinkPrefix (),
	       imageID, (theLink->next->bsaveID / maxIndices) + 1,
	       theLink->next->bsaveID % maxIndices);
    }

   /*===========*/
  /* CL_Bsave ID. */
   /*===========*/

  fprintf (theFile, "0}");
}

/*************************************************************/
/* CL_DefruleCModuleReference: CL_Writes the C code representation */
/*   of a reference to a defrule module data structure.      */
/*************************************************************/
void
CL_DefruleCModuleReference (Environment * theEnv,
			    FILE * theFile,
			    unsigned long count,
			    unsigned int imageID, unsigned int maxIndices)
{
  fprintf (theFile, "MIHS &%s%u_%lu[%lu]",
	   ModulePrefix (DefruleData (theEnv)->DefruleCodeItem), imageID,
	   (count / maxIndices) + 1, (count % maxIndices));
}


/*****************************************************************/
/* InitDefruleCode: CL_Writes out initialization code for defrules. */
/*****************************************************************/
static void
InitDefruleCode (Environment * theEnv,
		 FILE * initFP, unsigned int imageID, unsigned int maxIndices)
{
#if MAC_XCD
#pragma unused(maxIndices)
#pragma unused(theEnv)
#pragma unused(imageID)
#endif

  fprintf (initFP, "   Defrule_RunTimeInitialize(theEnv,");

  if (DefruleData (theEnv)->RightPrimeJoins == NULL)
    {
      fprintf (initFP, "NULL,");
    }
  else
    {
      fprintf (initFP, "&%s%u_%lu[%lu],", LinkPrefix (),
	       imageID,
	       (DefruleData (theEnv)->RightPrimeJoins->bsaveID / maxIndices) +
	       1,
	       DefruleData (theEnv)->RightPrimeJoins->bsaveID % maxIndices);
    }

  if (DefruleData (theEnv)->LeftPrimeJoins == NULL)
    {
      fprintf (initFP, "NULL);\n");
    }
  else
    {
      fprintf (initFP, "&%s%u_%lu[%lu]);\n", LinkPrefix (),
	       imageID,
	       (DefruleData (theEnv)->LeftPrimeJoins->bsaveID / maxIndices) +
	       1, DefruleData (theEnv)->LeftPrimeJoins->bsaveID % maxIndices);
    }
}

#endif /* DEFRULE_CONSTRUCT && (! RUN_TIME) && CONSTRUCT_COMPILER */
