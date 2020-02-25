   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/04/17             */
   /*                                                     */
   /*                     BSAVE MODULE                    */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides core routines for saving constructs to  */
/*   a binary file.                                          */
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
/*            Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Used CL_genstrncpy instead of strncpy.            */
/*                                                           */
/*            Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.31: Data sizes written to binary files for         */
/*            validation when loaded.                        */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
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

#include "setup.h"

#include "argacces.h"
#include "bload.h"
#include "cstrnbin.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "moduldef.h"
#include "prntutil.h"
#include "router.h"
#include "symblbin.h"

#include "bsave.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if BLOAD_AND_BSAVE
   static void                        FindNeededItems(Environment *);
   static void                        InitializeFunctionNeededFlags(Environment *);
   static void                        CL_WriteNeededFunctions(Environment *,FILE *);
   static size_t                      FunctionBinarySize(Environment *);
   static void                        CL_WriteBinaryHeader(Environment *,FILE *);
   static void                        CL_WriteBinaryFooter(Environment *,FILE *);
#endif
   static void                        Deallocate_BsaveData(Environment *);

/**********************************************/
/* Initialize_BsaveData: Allocates environment */
/*    data for the bsave command.             */
/**********************************************/
void Initialize_BsaveData(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,BSAVE_DATA,sizeof(struct bsaveData),Deallocate_BsaveData);
  }

/************************************************/
/* Deallocate_BsaveData: Deallocates environment */
/*    data for the bsave command.               */
/************************************************/
static void Deallocate_BsaveData(
  Environment *theEnv)
  {
   struct BinaryItem *tmpPtr, *nextPtr;

   tmpPtr = CL_BsaveData(theEnv)->ListOfBinaryItems;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      rtn_struct(theEnv,BinaryItem,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

/**************************************/
/* CL_BsaveCommand: H/L access routine   */
/*   for the bsave command.           */
/**************************************/
void CL_BsaveCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
#if (! RUN_TIME) && BLOAD_AND_BSAVE
   const char *fileName;

   fileName = CL_GetFileName(context);
   if (fileName != NULL)
     {
      if (CL_Bsave(theEnv,fileName))
        {
         returnValue->lexemeValue = TrueSymbol(theEnv);
         return;
        }
     }
#else
#if MAC_XCD
#pragma unused(theEnv,context)
#endif
#endif
   returnValue->lexemeValue = FalseSymbol(theEnv);
  }

#if BLOAD_AND_BSAVE

/****************************/
/* CL_Bsave: C access routine  */
/*   for the bsave command. */
/****************************/
bool CL_Bsave(
  Environment *theEnv,
  const char *fileName)
  {
   FILE *fp;
   struct BinaryItem *biPtr;
   char constructBuffer[CONSTRUCT_HEADER_SIZE];
   unsigned long saveExpressionCount;
   
   /*=====================================*/
   /* If embedded, clear the error flags. */
   /*=====================================*/
   
   if (CL_EvaluationData(theEnv)->CurrentExpression == NULL)
     { CL_ResetErrorFlags(theEnv); }

   /*===================================*/
   /* A bsave can't occur when a binary */
   /* image is already loaded.          */
   /*===================================*/

   if (CL_Bloaded(theEnv))
     {
      CL_PrintErrorID(theEnv,"BSAVE",1,false);
      CL_WriteString(theEnv,STDERR,
          "Cannot perfo_rm a binary save while a binary load is in effect.\n");
      return false;
     }

   /*================*/
   /* Open the file. */
   /*================*/

   if ((fp = CL_GenOpen(theEnv,fileName,"wb")) == NULL)
     {
      CL_OpenErrorMessage(theEnv,"bsave",fileName);
      return false;
     }

   /*==============================*/
   /* Remember the current module. */
   /*==============================*/

   CL_SaveCurrentModule(theEnv);

   /*==================================*/
   /* CL_Write binary header to the file. */
   /*==================================*/

   CL_WriteBinaryHeader(theEnv,fp);

   /*===========================================*/
   /* Initialize count variables, index values, */
   /* and dete_rmine some of the data structures */
   /* which need to be saved.                   */
   /*===========================================*/

   ExpressionData(theEnv)->ExpressionCount = 0;
   InitializeFunctionNeededFlags(theEnv);
   CL_InitAtomicValueNeededFlags(theEnv);
   CL_FindHashedExpressions(theEnv);
   FindNeededItems(theEnv);
   CL_SetAtomicValueIndices(theEnv,false);

   /*===============================*/
   /* CL_Save the functions and atoms. */
   /*===============================*/

   CL_WriteNeededFunctions(theEnv,fp);
   CL_WriteNeededAtomicValues(theEnv,fp);

   /*=========================================*/
   /* CL_Write out the number of expression data */
   /* structures in the binary image.         */
   /*=========================================*/

   CL_Gen_Write(&ExpressionData(theEnv)->ExpressionCount,sizeof(unsigned long),fp);

   /*===========================================*/
   /* CL_Save the numbers indicating the amount of */
   /* memory needed to bload the constructs.    */
   /*===========================================*/

   for (biPtr = CL_BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     {
      if (biPtr->bsaveStorageFunction != NULL)
        {
         CL_genstrncpy(constructBuffer,biPtr->name,CONSTRUCT_HEADER_SIZE);
         CL_Gen_Write(constructBuffer,CONSTRUCT_HEADER_SIZE,fp);
         (*biPtr->bsaveStorageFunction)(theEnv,fp);
        }
     }

   /*====================================*/
   /* CL_Write a binary footer to the file. */
   /*====================================*/

   CL_WriteBinaryFooter(theEnv,fp);

   /*===================*/
   /* CL_Save expressions. */
   /*===================*/

   ExpressionData(theEnv)->ExpressionCount = 0;
   CL_BsaveHashedExpressions(theEnv,fp);
   saveExpressionCount = ExpressionData(theEnv)->ExpressionCount;
   CL_BsaveConstructExpressions(theEnv,fp);
   ExpressionData(theEnv)->ExpressionCount = saveExpressionCount;

   /*===================*/
   /* CL_Save constraints. */
   /*===================*/

   CL_WriteNeededConstraints(theEnv,fp);

   /*==================*/
   /* CL_Save constructs. */
   /*==================*/

   for (biPtr = CL_BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     {
      if (biPtr->bsaveFunction != NULL)
        {
         CL_genstrncpy(constructBuffer,biPtr->name,CONSTRUCT_HEADER_SIZE);
         CL_Gen_Write(constructBuffer,CONSTRUCT_HEADER_SIZE,fp);
         (*biPtr->bsaveFunction)(theEnv,fp);
        }
     }

   /*===================================*/
   /* CL_Save a binary footer to the file. */
   /*===================================*/

   CL_WriteBinaryFooter(theEnv,fp);

   /*===========*/
   /* Clean up. */
   /*===========*/

   CL_RestoreAtomicValueBuckets(theEnv);

   /*=================*/
   /* Close the file. */
   /*=================*/

   CL_GenClose(theEnv,fp);

   /*=============================*/
   /* Restore the current module. */
   /*=============================*/

   CL_RestoreCurrentModule(theEnv);

   /*==================================*/
   /* Return true to indicate success. */
   /*==================================*/

   return true;
  }

/*********************************************/
/* InitializeFunctionNeededFlags: Marks each */
/*   function in the list of functions as    */
/*   being unneeded by this binary image.    */
/*********************************************/
static void InitializeFunctionNeededFlags(
  Environment *theEnv)
  {
   struct functionDefinition *functionList;

   for (functionList = CL_GetFunctionList(theEnv);
        functionList != NULL;
        functionList = functionList->next)
     { functionList->neededFunction = false; }
  }

/**********************************************************/
/* FindNeededItems: Searches through the constructs for   */
/*   the functions, constraints, or atoms that are needed */
/*   by that construct. This routine also counts the      */
/*   number of expressions in use (through a global).     */
/**********************************************************/
static void FindNeededItems(
  Environment *theEnv)
  {
   struct BinaryItem *biPtr;

   for (biPtr = CL_BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     { if (biPtr->findFunction != NULL) (*biPtr->findFunction)(theEnv); }
  }

/****************************************************/
/* CL_WriteNeededFunctions: CL_Writes the names of needed */
/*   functions to the binary save file.             */
/****************************************************/
static void CL_WriteNeededFunctions(
  Environment *theEnv,
  FILE *fp)
  {
   unsigned long count = 0;
   size_t space, length;
   struct functionDefinition *functionList;

   /*================================================*/
   /* Assign each function an index if it is needed. */
   /*================================================*/

   for (functionList = CL_GetFunctionList(theEnv);
        functionList != NULL;
        functionList = functionList->next)
     {
      if (functionList->neededFunction)
        { functionList->bsaveIndex = count++; }
      else
        { functionList->bsaveIndex = ULONG_MAX; }
     }

   /*===================================================*/
   /* CL_Write the number of function names to be written. */
   /*===================================================*/

   CL_Gen_Write(&count,sizeof(unsigned long),fp);
   if (count == 0)
     {
      CL_Gen_Write(&count,sizeof(unsigned long),fp);
      return;
     }

   /*================================*/
   /* Dete_rmine the amount of space  */
   /* needed for the function names. */
   /*================================*/

   space = FunctionBinarySize(theEnv);
   CL_Gen_Write(&space,sizeof(unsigned long),fp);

   /*===============================*/
   /* CL_Write out the function names. */
   /*===============================*/

   for (functionList = CL_GetFunctionList(theEnv);
        functionList != NULL;
        functionList = functionList->next)
     {
      if (functionList->neededFunction)
        {
         length = strlen(functionList->callFunctionName->contents) + 1;
         CL_Gen_Write((void *) functionList->callFunctionName->contents,length,fp);
        }
     }
  }

/*********************************************/
/* FunctionBinarySize: Dete_rmines the number */
/*   of bytes needed to save all of the      */
/*   function names in the binary save file. */
/*********************************************/
static size_t FunctionBinarySize(
  Environment *theEnv)
  {
   size_t size = 0;
   struct functionDefinition *functionList;

   for (functionList = CL_GetFunctionList(theEnv);
        functionList != NULL;
        functionList = functionList->next)
     {
      if (functionList->neededFunction)
        { size += strlen(functionList->callFunctionName->contents) + 1; }
     }

   return(size);
  }

/***************************************************/
/* CL_Save_BloadCount: Used to save the data structure */
/*   count values when a binary save command is    */
/*   issued when a binary image is loaded.         */
/***************************************************/
void CL_Save_BloadCount(
  Environment *theEnv,
  unsigned long cnt)
  {
   BLOADCNTSV *tmp, *prv;

   tmp = get_struct(theEnv,bloadcntsv);
   tmp->val = cnt;
   tmp->nxt = NULL;

   if (CL_BsaveData(theEnv)->CL_BloadCount_SaveTop == NULL)
     { CL_BsaveData(theEnv)->CL_BloadCount_SaveTop = tmp; }
   else
     {
      prv = CL_BsaveData(theEnv)->CL_BloadCount_SaveTop;
      while (prv->nxt != NULL)
        { prv = prv->nxt; }
      prv->nxt = tmp;
     }
  }

/**************************************************/
/* Restore_BloadCount: Restores the data structure */
/*   count values after a binary save command is  */
/*   completed when a binary image is loaded.     */
/**************************************************/
void Restore_BloadCount(
  Environment *theEnv,
  unsigned long *cnt)
  {
   BLOADCNTSV *tmp;

   *cnt = CL_BsaveData(theEnv)->CL_BloadCount_SaveTop->val;
   tmp = CL_BsaveData(theEnv)->CL_BloadCount_SaveTop;
   CL_BsaveData(theEnv)->CL_BloadCount_SaveTop = CL_BsaveData(theEnv)->CL_BloadCount_SaveTop->nxt;
   rtn_struct(theEnv,bloadcntsv,tmp);
  }

/**********************************************/
/* CL_MarkNeededItems: Examines an expression to */
/*   dete_rmine which items are needed to save */
/*   an expression as part of a binary image. */
/**********************************************/
void CL_MarkNeededItems(
  Environment *theEnv,
  struct expr *testPtr)
  {
   while (testPtr != NULL)
     {
      switch (testPtr->type)
        {
         case SYMBOL_TYPE:
         case STRING_TYPE:
         case GBL_VARIABLE:
         case INSTANCE_NAME_TYPE:
            testPtr->lexemeValue->neededSymbol = true;
            break;

         case FLOAT_TYPE:
            testPtr->floatValue->neededFloat = true;
            break;

         case INTEGER_TYPE:
            testPtr->integerValue->neededInteger = true;
            break;

         case FCALL:
            testPtr->functionValue->neededFunction = true;
            break;

         case VOID_TYPE:
           break;

         default:
           if (CL_EvaluationData(theEnv)->PrimitivesArray[testPtr->type] == NULL) break;
           if (CL_EvaluationData(theEnv)->PrimitivesArray[testPtr->type]->bitMap)
             { ((CLIPSBitMap *) testPtr->value)->neededBitMap = true; }
           break;

        }

      if (testPtr->argList != NULL)
        { CL_MarkNeededItems(theEnv,testPtr->argList); }

      testPtr = testPtr->nextArg;
     }
  }

/******************************************************/
/* CL_WriteBinaryHeader: CL_Writes a binary header used for */
/*   verification when a binary image is loaded.      */
/******************************************************/
static void CL_WriteBinaryHeader(
  Environment *theEnv,
  FILE *fp)
  {
   CL_Gen_Write((void *) CL_BloadData(theEnv)->BinaryPrefixID,strlen(CL_BloadData(theEnv)->BinaryPrefixID) + 1,fp);
   CL_Gen_Write((void *) CL_BloadData(theEnv)->BinaryVersionID,strlen(CL_BloadData(theEnv)->BinaryVersionID) + 1,fp);
   CL_Gen_Write((void *) CL_BloadData(theEnv)->BinarySizes,strlen(CL_BloadData(theEnv)->BinarySizes) + 1,fp);
  }

/******************************************************/
/* CL_WriteBinaryFooter: CL_Writes a binary footer used for */
/*   verification when a binary image is loaded.      */
/******************************************************/
static void CL_WriteBinaryFooter(
  Environment *theEnv,
  FILE *fp)
  {
   char footerBuffer[CONSTRUCT_HEADER_SIZE];

   CL_genstrncpy(footerBuffer,CL_BloadData(theEnv)->BinaryPrefixID,CONSTRUCT_HEADER_SIZE);
   CL_Gen_Write(footerBuffer,CONSTRUCT_HEADER_SIZE,fp);
  }

#endif /* BLOAD_AND_BSAVE */

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE

/**********************************************************/
/* CL_AddBinaryItem: Info_rms the bload/bsave commands of the */
/*   appropriate access functions needed to save/load the */
/*   data structures of a construct or other "item" to a  */
/*   binary file.                                         */
/**********************************************************/
bool CL_AddBinaryItem(
  Environment *theEnv,
  const char *name,
  int priority,
  void (*findFunction)(Environment *),
  void (*expressionFunction)(Environment *,FILE *),
  void (*bsaveStorageFunction)(Environment *,FILE *),
  void (*bsaveFunction)(Environment *,FILE *),
  void (*bloadStorageFunction)(Environment *),
  void (*bloadFunction)(Environment *),
  void (*clearFunction)(Environment *))
  {
   struct BinaryItem *newPtr, *currentPtr, *lastPtr = NULL;

   /*========================================*/
   /* Create the binary item data structure. */
   /*========================================*/

   newPtr = get_struct(theEnv,BinaryItem);

   newPtr->name = name;
   newPtr->findFunction = findFunction;
   newPtr->expressionFunction = expressionFunction;
   newPtr->bsaveStorageFunction = bsaveStorageFunction;
   newPtr->bsaveFunction = bsaveFunction;
   newPtr->bloadStorageFunction = bloadStorageFunction;
   newPtr->bloadFunction = bloadFunction;
   newPtr->clearFunction = clearFunction;
   newPtr->priority = priority;

   /*=================================*/
   /* If no binary items are defined, */
   /* just put the item on the list.  */
   /*=================================*/

   if (CL_BsaveData(theEnv)->ListOfBinaryItems == NULL)
     {
      newPtr->next = NULL;
      CL_BsaveData(theEnv)->ListOfBinaryItems = newPtr;
      return true;
     }

   /*=========================================*/
   /* Otherwise, place the binary item at the */
   /* appropriate place in the list of binary */
   /* items based on its priority.            */
   /*=========================================*/

   currentPtr = CL_BsaveData(theEnv)->ListOfBinaryItems;
   while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
     {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   if (lastPtr == NULL)
     {
      newPtr->next = CL_BsaveData(theEnv)->ListOfBinaryItems;
      CL_BsaveData(theEnv)->ListOfBinaryItems = newPtr;
     }
   else
     {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
     }

   /*==================================*/
   /* Return true to indicate the item */
   /* was successfully added.          */
   /*==================================*/

   return true;
  }

#endif /* BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE */




