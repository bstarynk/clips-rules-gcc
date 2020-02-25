   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/04/17             */
   /*                                                     */
   /*                    BLOAD MODULE                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides core routines for loading constructs    */
/*   from a binary file.                                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.31: Data sizes written to binary files for         */
/*            validation when loaded.                        */
/*                                                           */
/*      6.40: Refactored code to reduce header dependencies  */
/*            in sysdep.c.                                   */
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
/*            Callbacks must be environment aware.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include "argacces.h"
#include "bsave.h"
#include "constrct.h"
#include "cstrnbin.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"
#include "utility.h"

#include "bload.h"

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static struct functionDefinition **ReadNeededFunctions (Environment *,
							unsigned long *,
							bool *);
static struct functionDefinition *Fast_FindFunction (Environment *,
						     const char *,
						     struct functionDefinition
						     *);
static bool CL_Clear_Bload (Environment *);
static void CL_Clear_BloadCallback (Environment *, void *);
static void Abort_Bload (Environment *);
static bool CL_BloadOutOfMemoryFunction (Environment *, size_t);
static void Deallocate_BloadData (Environment *);

/**********************************************/
/* Initialize_BloadData: Allocates environment */
/*    data for the bload command.             */
/**********************************************/
void
Initialize_BloadData (Environment * theEnv)
{
  char sizeBuffer[20];
  sprintf (sizeBuffer, "%2zu%2zu%2zu%2zu%2zu", sizeof (void *),
	   sizeof (double), sizeof (int), sizeof (long), sizeof (long long));

  CL_AllocateEnvironmentData (theEnv, BLOAD_DATA, sizeof (struct bloadData),
			      NULL);
  CL_AddEnvironmentCleanupFunction (theEnv, "bload", Deallocate_BloadData,
				    -1500);
  CL_Add_ClearFunction (theEnv, "bload", CL_Clear_BloadCallback, 10000, NULL);

  CL_BloadData (theEnv)->BinaryPrefixID = "\1\2\3\4CLIPS";
  CL_BloadData (theEnv)->BinaryVersionID = "V6.40";
  CL_BloadData (theEnv)->BinarySizes =
    (char *) CL_genalloc (theEnv, strlen (sizeBuffer) + 1);
  CL_genstrcpy (CL_BloadData (theEnv)->BinarySizes, sizeBuffer);
}

/************************************************/
/* Deallocate_BloadData: Deallocates environment */
/*    data for the bload command.               */
/************************************************/
static void
Deallocate_BloadData (Environment * theEnv)
{
  CL_DeallocateVoidCallList (theEnv,
			     CL_BloadData (theEnv)->Before_BloadFunctions);
  CL_DeallocateVoidCallList (theEnv,
			     CL_BloadData (theEnv)->After_BloadFunctions);
  CL_DeallocateBoolCallList (theEnv,
			     CL_BloadData (theEnv)->
			     CL_Clear_BloadReadyFunctions);
  CL_DeallocateVoidCallList (theEnv,
			     CL_BloadData (theEnv)->Abort_BloadFunctions);
  CL_genfree (theEnv, CL_BloadData (theEnv)->BinarySizes,
	      strlen (CL_BloadData (theEnv)->BinarySizes) + 1);
}

/****************************/
/* CL_Bload: C access routine  */
/*   for the bload command. */
/****************************/
bool
CL_Bload (Environment * theEnv, const char *fileName)
{
  unsigned long numberOfFunctions;
  unsigned long space;
  bool error;
  char IDbuffer[20];
  char sizesBuffer[20];
  char constructBuffer[CONSTRUCT_HEADER_SIZE];
  struct BinaryItem *biPtr;
  struct void_CallFunctionItem *bfPtr;

   /*=====================================*/
  /* If embedded, clear the error flags. */
   /*=====================================*/

  if (CL_EvaluationData (theEnv)->CurrentExpression == NULL)
    {
      CL_ResetErrorFlags (theEnv);
    }

   /*================*/
  /* Open the file. */
   /*================*/

  if (CL_GenOpenReadBinary (theEnv, "bload", fileName) == 0)
    {
      CL_OpenErrorMessage (theEnv, "bload", fileName);
      return false;
    }

   /*=====================================*/
  /* Dete_rmine if this is a binary file. */
   /*=====================================*/

  CL_GenReadBinary (theEnv, IDbuffer,
		    strlen (CL_BloadData (theEnv)->BinaryPrefixID) + 1);
  if (strcmp (IDbuffer, CL_BloadData (theEnv)->BinaryPrefixID) != 0)
    {
      CL_PrintErrorID (theEnv, "BLOAD", 2, false);
      CL_WriteString (theEnv, STDERR, "File '");
      CL_WriteString (theEnv, STDERR, fileName);
      CL_WriteString (theEnv, STDERR, "' is not a binary construct file.\n");
      CL_GenCloseBinary (theEnv);
      return false;
    }

   /*=======================================*/
  /* Dete_rmine if it's a binary file using */
  /* a fo_rmat from a different version.    */
   /*=======================================*/

  CL_GenReadBinary (theEnv, IDbuffer,
		    strlen (CL_BloadData (theEnv)->BinaryVersionID) + 1);
  if (strcmp (IDbuffer, CL_BloadData (theEnv)->BinaryVersionID) != 0)
    {
      CL_PrintErrorID (theEnv, "BLOAD", 3, false);
      CL_WriteString (theEnv, STDERR, "File '");
      CL_WriteString (theEnv, STDERR, fileName);
      CL_WriteString (theEnv, STDERR,
		      "' is an incompatible binary construct file.\n");
      CL_GenCloseBinary (theEnv);
      return false;
    }

   /*===========================================*/
  /* Dete_rmine if it's a binary file using the */
  /* correct size for pointers and numbers.    */
   /*===========================================*/

  CL_GenReadBinary (theEnv, sizesBuffer,
		    strlen (CL_BloadData (theEnv)->BinarySizes) + 1);
  if (strcmp (sizesBuffer, CL_BloadData (theEnv)->BinarySizes) != 0)
    {
      CL_PrintErrorID (theEnv, "BLOAD", 3, false);
      CL_WriteString (theEnv, STDERR, "File '");
      CL_WriteString (theEnv, STDERR, fileName);
      CL_WriteString (theEnv, STDERR,
		      "' is an incompatible binary construct file.\n");
      CL_GenCloseBinary (theEnv);
      return false;
    }

   /*====================*/
  /* CL_Clear environment. */
   /*====================*/

  if (CL_BloadData (theEnv)->CL_BloadActive)
    {
      if (CL_Clear_Bload (theEnv) == false)
	{
	  CL_GenCloseBinary (theEnv);
	  return false;
	}
    }

   /*=================================*/
  /* Dete_rmine if the KB environment */
  /* was successfully cleared.       */
   /*=================================*/

  if (CL_ClearReady (theEnv) == false)
    {
      CL_GenCloseBinary (theEnv);
      CL_PrintErrorID (theEnv, "BLOAD", 4, false);
      CL_WriteString (theEnv, STDERR, "The ");
      CL_WriteString (theEnv, STDERR, APPLICATION_NAME);
      CL_WriteString (theEnv, STDERR, " environment could not be cleared.\n");
      CL_WriteString (theEnv, STDERR, "Binary load cannot continue.\n");
      return false;
    }

   /*==================================*/
  /* Call the list of functions to be */
  /* executed before a bload occurs.  */
   /*==================================*/

  ConstructData (theEnv)->CL_ClearInProgress = true;
  for (bfPtr = CL_BloadData (theEnv)->Before_BloadFunctions;
       bfPtr != NULL; bfPtr = bfPtr->next)
    {
      (*bfPtr->func) (theEnv, bfPtr->context);
    }

  ConstructData (theEnv)->CL_ClearInProgress = false;

   /*====================================================*/
  /* Read in the functions needed by this binary image. */
   /*====================================================*/

  CL_BloadData (theEnv)->FunctionArray =
    ReadNeededFunctions (theEnv, &numberOfFunctions, &error);
  if (error)
    {
      CL_GenCloseBinary (theEnv);
      Abort_Bload (theEnv);
      return false;
    }

   /*================================================*/
  /* Read in the atoms needed by this binary image. */
   /*================================================*/

  CL_ReadNeededAtomicValues (theEnv);

   /*===========================================*/
  /* Dete_rmine the number of expressions to be */
  /* read and allocate the appropriate space   */
   /*===========================================*/

  CL_AllocateExpressions (theEnv);

   /*==========================================================*/
  /* Read in the memory requirements of the constructs stored */
  /* in this binary image and allocate the necessary space    */
   /*==========================================================*/

  for (CL_GenReadBinary (theEnv, constructBuffer, CONSTRUCT_HEADER_SIZE);
       strncmp (constructBuffer, CL_BloadData (theEnv)->BinaryPrefixID,
		CONSTRUCT_HEADER_SIZE) != 0;
       CL_GenReadBinary (theEnv, constructBuffer, CONSTRUCT_HEADER_SIZE))
    {
      bool found;

      /*================================================*/
      /* Search for the construct type in the list of   */
      /* binary items. If found, allocate the storage   */
      /* needed by the construct for this binary image. */
      /*================================================*/

      found = false;
      for (biPtr = CL_BsaveData (theEnv)->ListOfBinaryItems;
	   biPtr != NULL; biPtr = biPtr->next)
	{
	  if (strncmp (biPtr->name, constructBuffer, CONSTRUCT_HEADER_SIZE) ==
	      0)
	    {
	      if (biPtr->bloadStorageFunction != NULL)
		{
		  (*biPtr->bloadStorageFunction) (theEnv);
		  found = true;
		}
	      break;
	    }
	}

      /*==========================================*/
      /* If the construct type wasn't found, skip */
      /* the storage binary load info_rmation for  */
      /* this construct.                          */
      /*==========================================*/

      if (!found)
	{
	  CL_GenReadBinary (theEnv, &space, sizeof (unsigned long));
	  CL_GetSeekCurBinary (theEnv, (long) space);
	  if (space != 0)
	    {
	      CL_WriteString (theEnv, STDOUT, "\nSkipping ");
	      CL_WriteString (theEnv, STDOUT, constructBuffer);
	      CL_WriteString (theEnv, STDOUT,
			      " constructs because of unavailability\n");
	    }
	}
    }

   /*======================================*/
  /* CL_Refresh the pointers in expressions. */
   /*======================================*/

  CL_RefreshExpressions (theEnv);

   /*==========================*/
  /* Read in the constraints. */
   /*==========================*/

  CL_ReadNeededConstraints (theEnv);

   /*======================================================*/
  /* Read in the constructs stored in this binary image.  */
   /*======================================================*/

  for (CL_GenReadBinary (theEnv, constructBuffer, CONSTRUCT_HEADER_SIZE);
       strncmp (constructBuffer, CL_BloadData (theEnv)->BinaryPrefixID,
		CONSTRUCT_HEADER_SIZE) != 0;
       CL_GenReadBinary (theEnv, constructBuffer, CONSTRUCT_HEADER_SIZE))
    {
      bool found;

      /*==================================================*/
      /* Search for the function to load the construct    */
      /* into the previously allocated storage. If found, */
      /* call the function to load the construct.         */
      /*==================================================*/

      found = false;
      for (biPtr = CL_BsaveData (theEnv)->ListOfBinaryItems;
	   biPtr != NULL; biPtr = biPtr->next)
	{
	  if (strncmp (biPtr->name, constructBuffer, CONSTRUCT_HEADER_SIZE) ==
	      0)
	    {
	      if (biPtr->bloadFunction != NULL)
		{
		  (*biPtr->bloadFunction) (theEnv);
		  found = true;
		}
	      break;
	    }
	}

      /*==========================================*/
      /* If the construct type wasn't found, skip */
      /* the binary data for this construct.      */
      /*==========================================*/

      if (!found)
	{
	  CL_GenReadBinary (theEnv, &space, sizeof (unsigned long));
	  CL_GetSeekCurBinary (theEnv, (long) space);
	}
    }

   /*=================*/
  /* Close the file. */
   /*=================*/

  CL_GenCloseBinary (theEnv);

   /*========================================*/
  /* Free up temporary storage used for the */
  /* function and atomic value info_rmation. */
   /*========================================*/

  if (CL_BloadData (theEnv)->FunctionArray != NULL)
    {
      CL_genfree (theEnv, CL_BloadData (theEnv)->FunctionArray,
		  sizeof (struct functionDefinition *) * numberOfFunctions);
    }
  CL_FreeAtomicValueStorage (theEnv);

   /*==================================*/
  /* Call the list of functions to be */
  /* executed after a bload occurs.   */
   /*==================================*/

  for (bfPtr = CL_BloadData (theEnv)->After_BloadFunctions;
       bfPtr != NULL; bfPtr = bfPtr->next)
    {
      (*bfPtr->func) (theEnv, bfPtr->context);
    }

   /*=======================================*/
  /* Add a clear function to remove binary */
  /* load when a clear command is issued.  */
   /*=======================================*/

  CL_BloadData (theEnv)->CL_BloadActive = true;

   /*=============================*/
  /* Return true to indicate the */
  /* binary load was successful. */
   /*=============================*/

  return true;
}

/************************************************************
  NAME         : CL_Bloadand_Refresh
  DESCRIPTION  : CL_Loads and refreshes objects - will bload
                 all objects at once, if possible, but
                 will aslo work in increments if memory is
                 restricted
  INPUTS       : 1) the number of objects to bload and update
                 2) the size of one object
                 3) An update function which takes a bloaded
                    object buffer and the index of the object
                    to refresh as arguments
  RETURNS      : Nothing useful
  SIDE EFFECTS : Objects bloaded and updated
  NOTES        : Assumes binary file pointer is positioned
                 for bloads of the objects
 ************************************************************/
void
CL_Bloadand_Refresh (Environment * theEnv,
		     unsigned long objcnt,
		     size_t objsz,
		     void (*objupdate) (Environment *, void *, unsigned long))
{
  unsigned long i, bi;
  char *buf;
  unsigned long objsmaxread, objsread;
  size_t space;
  OutOfMemoryFunction *oldOutOfMemoryFunction;

  if (objcnt == 0L)
    return;

  oldOutOfMemoryFunction =
    CL_SetOutOfMemoryFunction (theEnv, CL_BloadOutOfMemoryFunction);
  objsmaxread = objcnt;
  do
    {
      space = objsmaxread * objsz;
      buf = (char *) CL_genalloc (theEnv, space);
      if (buf == NULL)
	{
	  if ((objsmaxread / 2) == 0)
	    {
	      if ((*oldOutOfMemoryFunction) (theEnv, space) == true)
		{
		  CL_SetOutOfMemoryFunction (theEnv, oldOutOfMemoryFunction);
		  return;
		}
	    }
	  else
	    objsmaxread /= 2;
	}
    }
  while (buf == NULL);

  CL_SetOutOfMemoryFunction (theEnv, oldOutOfMemoryFunction);

  i = 0L;
  do
    {
      objsread = (objsmaxread > (objcnt - i)) ? (objcnt - i) : objsmaxread;
      CL_GenReadBinary (theEnv, buf, objsread * objsz);
      for (bi = 0L; bi < objsread; bi++, i++)
	(*objupdate) (theEnv, buf + objsz * bi, i);
    }
  while (i < objcnt);
  CL_genfree (theEnv, buf, space);
}

/**********************************************/
/* ReadNeededFunctions: Reads in the names of */
/*   functions needed by the binary image.    */
/**********************************************/
static struct functionDefinition **
ReadNeededFunctions (Environment * theEnv,
		     unsigned long *numberOfFunctions, bool *error)
{
  char *functionNames, *namePtr;
  unsigned long space;
  size_t temp;
  unsigned long i;
  struct functionDefinition **newFunctionArray, *functionPtr;
  bool functionsNotFound = false;

   /*===================================================*/
  /* Dete_rmine the number of function names to be read */
  /* and the space required for them.                  */
   /*===================================================*/

  CL_GenReadBinary (theEnv, numberOfFunctions, sizeof (long));
  CL_GenReadBinary (theEnv, &space, sizeof (unsigned long));
  if (*numberOfFunctions == 0)
    {
      *error = false;
      return NULL;
    }

   /*=======================================*/
  /* Allocate area for strings to be read. */
   /*=======================================*/

  functionNames = (char *) CL_genalloc (theEnv, space);
  CL_GenReadBinary (theEnv, functionNames, space);

   /*====================================================*/
  /* Store the function pointers in the function array. */
   /*====================================================*/

  temp = sizeof (struct functionDefinition *) * *numberOfFunctions;
  newFunctionArray =
    (struct functionDefinition **) CL_genalloc (theEnv, temp);
  namePtr = functionNames;
  functionPtr = NULL;
  for (i = 0; i < *numberOfFunctions; i++)
    {
      if ((functionPtr =
	   Fast_FindFunction (theEnv, namePtr, functionPtr)) == NULL)
	{
	  if (!functionsNotFound)
	    {
	      CL_PrintErrorID (theEnv, "BLOAD", 6, false);
	      CL_WriteString (theEnv, STDERR,
			      "The following undefined functions are ");
	      CL_WriteString (theEnv, STDERR,
			      "referenced by this binary image:\n");
	    }

	  CL_WriteString (theEnv, STDERR, "   ");
	  CL_WriteString (theEnv, STDERR, namePtr);
	  CL_WriteString (theEnv, STDERR, "\n");
	  functionsNotFound = true;
	}

      newFunctionArray[i] = functionPtr;
      namePtr += strlen (namePtr) + 1;
    }

   /*==========================================*/
  /* Free the memory used by the name buffer. */
   /*==========================================*/

  CL_genfree (theEnv, functionNames, space);

   /*==================================================*/
  /* If any of the required functions were not found, */
  /* then free the memory used by the function array. */
   /*==================================================*/

  if (functionsNotFound)
    {
      CL_genfree (theEnv, newFunctionArray, temp);
      newFunctionArray = NULL;
    }

   /*===================================*/
  /* Set globals to appropriate values */
  /* and return the function array.    */
   /*===================================*/

  *error = functionsNotFound;
  return newFunctionArray;
}

/*****************************************/
/* Fast_FindFunction: Search the function */
/*   list for a specific function.       */
/*****************************************/
static struct functionDefinition *
Fast_FindFunction (Environment * theEnv,
		   const char *functionName,
		   struct functionDefinition *lastFunction)
{
  struct functionDefinition *theList, *theFunction;

   /*========================*/
  /* Get the function list. */
   /*========================*/

  theList = CL_GetFunctionList (theEnv);
  if (theList == NULL)
    {
      return NULL;
    }

   /*=======================================*/
  /* If we completed a previous function   */
  /* search, start where we last left off. */
   /*=======================================*/

  if (lastFunction != NULL)
    {
      theFunction = lastFunction->next;
    }
  else
    {
      theFunction = theList;
    }

   /*======================================================*/
  /* Traverse the rest of the function list searching for */
  /* the named function wrapping around if necessary.     */
   /*======================================================*/

  while (strcmp (functionName, theFunction->callFunctionName->contents) != 0)
    {
      theFunction = theFunction->next;
      if (theFunction == lastFunction)
	return NULL;
      if (theFunction == NULL)
	theFunction = theList;
    }

   /*=======================*/
  /* Return the pointer to */
  /* the found function.   */
   /*=======================*/

  return (theFunction);
}

/******************************************/
/* CL_Bloaded: Returns true if the current   */
/*   environment is the result of a bload */
/*   command, otherwise returns false.    */
/******************************************/
bool
CL_Bloaded (Environment * theEnv)
{
  return CL_BloadData (theEnv)->CL_BloadActive;
}

/***************************************/
/* CL_Clear_BloadCallback: CL_Clears a binary */
/*   image from the KB environment.    */
/***************************************/
static void
CL_Clear_BloadCallback (Environment * theEnv, void *context)
{
  CL_Clear_Bload (theEnv);
}

/*************************************/
/* CL_Clear_Bload: CL_Clears a binary image */
/*   from the KB environment.        */
/*************************************/
static bool
CL_Clear_Bload (Environment * theEnv)
{
  struct BinaryItem *biPtr;
  struct bool_CallFunctionItem *bfPtr;
  bool ready, error;

   /*======================================*/
  /* If bload is not active, then there's */
  /* no need to clear bload data.         */
   /*======================================*/

  if (!CL_BloadData (theEnv)->CL_BloadActive)
    {
      return true;
    }

   /*=================================================*/
  /* Make sure it's safe to clear the bloaded image. */
   /*=================================================*/

  error = false;
  for (bfPtr = CL_BloadData (theEnv)->CL_Clear_BloadReadyFunctions;
       bfPtr != NULL; bfPtr = bfPtr->next)
    {
      ready = (bfPtr->func) (theEnv, bfPtr->context);

      if (ready == false)
	{
	  if (!error)
	    {
	      CL_PrintErrorID (theEnv, "BLOAD", 5, false);
	      CL_WriteString (theEnv, STDERR,
			      "Some constructs are still in use by the current binary image:\n");
	    }
	  CL_WriteString (theEnv, STDERR, "   ");
	  CL_WriteString (theEnv, STDERR, bfPtr->name);
	  CL_WriteString (theEnv, STDERR, "\n");
	  error = true;
	}
    }

   /*==================================================*/
  /* If some constructs are still in use and can't be */
  /* cleared, indicate the binary load can't continue */
  /* and return false to indicate this condition.     */
   /*==================================================*/

  if (error == true)
    {
      CL_WriteString (theEnv, STDERR, "Binary clear cannot continue.\n");
      return false;
    }

   /*=============================*/
  /* Call bload clear functions. */
   /*=============================*/

  for (biPtr = CL_BsaveData (theEnv)->ListOfBinaryItems;
       biPtr != NULL; biPtr = biPtr->next)
    {
      if (biPtr->clearFunction != NULL)
	(*biPtr->clearFunction) (theEnv);
    }

   /*===========================*/
  /* Free bloaded expressions. */
   /*===========================*/

  CL_Clear_BloadedExpressions (theEnv);

   /*===========================*/
  /* Free bloaded constraints. */
   /*===========================*/

  CL_Clear_BloadedConstraints (theEnv);

   /*==================================*/
  /* Remove the bload clear function. */
   /*==================================*/

  CL_BloadData (theEnv)->CL_BloadActive = false;

   /*====================================*/
  /* Return true to indicate the binary */
  /* image was successfully cleared.    */
   /*====================================*/

  return true;
}

/*************************************************/
/* Abort_Bload: Cleans up effects of before-bload */
/*   functions in event of failure.              */
/*************************************************/
static void
Abort_Bload (Environment * theEnv)
{
  struct void_CallFunctionItem *bfPtr;

  for (bfPtr = CL_BloadData (theEnv)->Abort_BloadFunctions;
       bfPtr != NULL; bfPtr = bfPtr->next)
    {
      (*bfPtr->func) (theEnv, bfPtr->context);
    }
}

/********************************************/
/* CL_AddBefore_BloadFunction: Adds a function  */
/*   to the list of functions called before */
/*   a binary load occurs.                  */
/********************************************/
void
CL_AddBefore_BloadFunction (Environment * theEnv,
			    const char *name,
			    Void_CallFunction * func,
			    int priority, void *context)
{
  CL_BloadData (theEnv)->Before_BloadFunctions =
    CL_Add_VoidFunctionToCallList (theEnv, name, priority, func,
				   CL_BloadData (theEnv)->
				   Before_BloadFunctions, context);
}

/*******************************************/
/* CL_AddAfter_BloadFunction: Adds a function  */
/*   to the list of functions called after */
/*   a binary load occurs.                 */
/*******************************************/
void
CL_AddAfter_BloadFunction (Environment * theEnv,
			   const char *name,
			   Void_CallFunction * func,
			   int priority, void *context)
{
  CL_BloadData (theEnv)->After_BloadFunctions =
    CL_Add_VoidFunctionToCallList (theEnv, name, priority, func,
				   CL_BloadData (theEnv)->
				   After_BloadFunctions, context);
}

/**************************************************/
/* CL_Add_Clear_BloadReadyFunction: Adds a function to */
/*   the list of functions called to dete_rmine if */
/*   a binary image can be cleared.               */
/**************************************************/
void
CL_Add_Clear_BloadReadyFunction (Environment * theEnv,
				 const char *name,
				 Bool_CallFunction * func,
				 int priority, void *context)
{
  CL_BloadData (theEnv)->CL_Clear_BloadReadyFunctions =
    CL_AddBoolFunctionToCallList (theEnv, name, priority, func,
				  CL_BloadData (theEnv)->
				  CL_Clear_BloadReadyFunctions, context);
}

/*********************************************/
/* CL_AddAbort_BloadFunction: Adds a function to */
/*   the list of functions called if a bload */
/*   has to be aborted.                      */
/*********************************************/
void
CL_AddAbort_BloadFunction (Environment * theEnv,
			   const char *name,
			   Void_CallFunction * func,
			   int priority, void *context)
{
  CL_BloadData (theEnv)->Abort_BloadFunctions =
    CL_Add_VoidFunctionToCallList (theEnv, name, priority, func,
				   CL_BloadData (theEnv)->
				   Abort_BloadFunctions, context);
}

/*******************************************************
  NAME         : CL_BloadOutOfMemoryFunction
  DESCRIPTION  : Memory function used by bload to
                   prevent exiting when out of
                   memory - used by CL_Bloadand_Refresh
  INPUTS       : The memory request size (unused)
  RETURNS      : True (indicates a failure and for
                 the memory functions to simply
                 return a NULL pointer)
  SIDE EFFECTS : None
  NOTES        : None
 *******************************************************/
static bool
CL_BloadOutOfMemoryFunction (Environment * theEnv, size_t size)
{
#if MAC_XCD
#pragma unused(size,theEnv)
#endif
  return true;
}

/*****************************************************/
/* Cannot_LoadWith_BloadMessage: Generic error message */
/*   for indicating that a construct can't be loaded */
/*   when a binary image is active.                  */
/*****************************************************/
void
Cannot_LoadWith_BloadMessage (Environment * theEnv, const char *constructName)
{
  CL_PrintErrorID (theEnv, "BLOAD", 1, true);
  CL_WriteString (theEnv, STDERR, "Cannot load ");
  CL_WriteString (theEnv, STDERR, constructName);
  CL_WriteString (theEnv, STDERR, " construct with binary load in effect.\n");
}

#endif /* (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) */

/**************************************/
/* CL_BloadCommand: H/L access routine   */
/*   for the bload command.           */
/**************************************/
void
CL_BloadCommand (Environment * theEnv,
		 UDFContext * context, UDFValue * returnValue)
{
#if (! RUN_TIME) && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
  const char *fileName;

  fileName = CL_GetFileName (context);
  if (fileName != NULL)
    {
      returnValue->lexemeValue =
	CL_CreateBoolean (theEnv, CL_Bload (theEnv, fileName));
      return;
    }
#else
#if MAC_XCD
#pragma unused(theEnv,context)
#endif
#endif
  returnValue->lexemeValue = FalseSymbol (theEnv);
}
