   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/15/17             */
   /*                                                     */
   /*                   UTILITY MODULE                    */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides a set of utility functions useful to    */
/*   other modules. Primarily these are the functions for    */
/*   handling periodic garbage collection and appending      */
/*   string data.                                            */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian Dantes                                         */
/*      Jeff Bezanson                                        */
/*         www.cprogramming.com/tutorial/unicode.html        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added CL_CopyString, CL_DeleteString,                */
/*            CL_InsertInString,and CL_EnlargeString functions.    */
/*                                                           */
/*            Used CL_genstrncpy function instead of strncpy    */
/*            function.                                      */
/*                                                           */
/*            Support for typed EXTERNAL_ADDRESS.            */
/*                                                           */
/*            Support for tracked memory (allows memory to   */
/*            be freed if CLIPS is exited while executing).  */
/*                                                           */
/*            Added UTF-8 routines.                          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Fix for memory used discrepancy.               */
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
/*            Added CL_GCBlockStart and CL_GCBlockEnd functions    */
/*            for garbage collection blocks.                 */
/*                                                           */
/*            Added StringCL_Builder functions.                 */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "commline.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "factmngr.h"
#include "facthsh.h"
#include "memalloc.h"
#include "multifld.h"
#include "prntutil.h"
#include "symbol.h"
#include "sysdep.h"

#include "utility.h"

#define MAX_EPHEMERAL_COUNT 1000L
#define MAX_EPHEMERAL_SIZE 10240L
#define COUNT_INCREMENT 1000L
#define SIZE_INCREMENT 10240L

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    DeallocateUtilityData(Environment *);

/************************************************/
/* CL_InitializeUtilityData: Allocates environment */
/*    data for utility routines.                */
/************************************************/
void CL_InitializeUtilityData(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,UTILITY_DATA,sizeof(struct utilityData),DeallocateUtilityData);

   UtilityData(theEnv)->CurrentGarbageFrame = &UtilityData(theEnv)->MasterGarbageFrame;

   UtilityData(theEnv)->PeriodicFunctionsEnabled = true;
   UtilityData(theEnv)->YieldFunctionEnabled = true;
  }

/**************************************************/
/* DeallocateUtilityData: Deallocates environment */
/*    data for utility routines.                  */
/**************************************************/
static void DeallocateUtilityData(
  Environment *theEnv)
  {
   struct trackedMemory *tmpTM, *nextTM;
   struct garbageFrame *theGarbageFrame;
   struct ephemeron *edPtr, *nextEDPtr;
   Multifield *tmpMFPtr, *nextMFPtr;

   /*======================*/
   /* Free tracked memory. */
   /*======================*/

   tmpTM = UtilityData(theEnv)->trackList;
   while (tmpTM != NULL)
     {
      nextTM = tmpTM->next;
      CL_genfree(theEnv,tmpTM->theMemory,tmpTM->memSize);
      rtn_struct(theEnv,trackedMemory,tmpTM);
      tmpTM = nextTM;
     }

   /*==========================*/
   /* Free callback functions. */
   /*==========================*/

   CL_DeallocateVoidCallList(theEnv,UtilityData(theEnv)->ListOfPeriodicFunctions);
   CL_DeallocateVoidCallList(theEnv,UtilityData(theEnv)->ListOfCleanupFunctions);

   /*=========================================*/
   /* Free the ephemerons tracking data which */
   /* needs to be garbage collected.          */
   /*=========================================*/

   while (UtilityData(theEnv)->CurrentGarbageFrame != NULL)
     {
      theGarbageFrame = UtilityData(theEnv)->CurrentGarbageFrame;

      edPtr = theGarbageFrame->ephemeralSymbolList;

      while (edPtr != NULL)
        {
         nextEDPtr = edPtr->next;
         rtn_struct(theEnv,ephemeron,edPtr);
         edPtr = nextEDPtr;
        }

      edPtr = theGarbageFrame->ephemeralFloatList;

      while (edPtr != NULL)
        {
         nextEDPtr = edPtr->next;
         rtn_struct(theEnv,ephemeron,edPtr);
         edPtr = nextEDPtr;
        }

      edPtr = theGarbageFrame->ephemeralIntegerList;

      while (edPtr != NULL)
        {
         nextEDPtr = edPtr->next;
         rtn_struct(theEnv,ephemeron,edPtr);
         edPtr = nextEDPtr;
        }

      edPtr = theGarbageFrame->ephemeralBitMapList;

      while (edPtr != NULL)
        {
         nextEDPtr = edPtr->next;
         rtn_struct(theEnv,ephemeron,edPtr);
         edPtr = nextEDPtr;
        }

      edPtr = theGarbageFrame->ephemeralExternalAddressList;

      while (edPtr != NULL)
        {
         nextEDPtr = edPtr->next;
         rtn_struct(theEnv,ephemeron,edPtr);
         edPtr = nextEDPtr;
        }

      /*==========================*/
      /* Free up multifield data. */
      /*==========================*/

      tmpMFPtr = theGarbageFrame->ListOfMultifields;
      while (tmpMFPtr != NULL)
        {
         nextMFPtr = tmpMFPtr->next;
         CL_ReturnMultifield(theEnv,tmpMFPtr);
         tmpMFPtr = nextMFPtr;
        }

      UtilityData(theEnv)->CurrentGarbageFrame = UtilityData(theEnv)->CurrentGarbageFrame->priorFrame;
     }
  }

/*****************************/
/* CL_CleanCurrentGarbageFrame: */
/*****************************/
void CL_CleanCurrentGarbageFrame(
  Environment *theEnv,
  UDFValue *returnValue)
  {
   struct garbageFrame *currentGarbageFrame;

   currentGarbageFrame = UtilityData(theEnv)->CurrentGarbageFrame;

   if (! currentGarbageFrame->dirty) return;

   if (returnValue != NULL)
     { CL_RetainUDFV(theEnv,returnValue); }

   CL_CallCleanupFunctions(theEnv);
   CL_RemoveEphemeralAtoms(theEnv);
   CL_FlushMultifields(theEnv);

   if (returnValue != NULL)
     { CL_ReleaseUDFV(theEnv,returnValue); }

   if ((currentGarbageFrame->ephemeralFloatList == NULL) &&
       (currentGarbageFrame->ephemeralIntegerList == NULL) &&
       (currentGarbageFrame->ephemeralSymbolList == NULL) &&
       (currentGarbageFrame->ephemeralBitMapList == NULL) &&
       (currentGarbageFrame->ephemeralExternalAddressList == NULL) &&
       (currentGarbageFrame->LastMultifield == NULL))
     { currentGarbageFrame->dirty = false; }
  }

/*****************************/
/* CL_RestorePriorGarbageFrame: */
/*****************************/
void CL_RestorePriorGarbageFrame(
  Environment *theEnv,
  struct garbageFrame *newGarbageFrame,
  struct garbageFrame *oldGarbageFrame,
  UDFValue *returnValue)
  {
   if (newGarbageFrame->dirty)
     {
      if (returnValue != NULL) CL_RetainUDFV(theEnv,returnValue);
      CL_CallCleanupFunctions(theEnv);
      CL_RemoveEphemeralAtoms(theEnv);
      CL_FlushMultifields(theEnv);
     }

   UtilityData(theEnv)->CurrentGarbageFrame = oldGarbageFrame;

   if (newGarbageFrame->dirty)
     {
      if (newGarbageFrame->ListOfMultifields != NULL)
        {
         if (oldGarbageFrame->ListOfMultifields == NULL)
           { oldGarbageFrame->ListOfMultifields = newGarbageFrame->ListOfMultifields; }
         else
           { oldGarbageFrame->LastMultifield->next = newGarbageFrame->ListOfMultifields; }

         oldGarbageFrame->LastMultifield = newGarbageFrame->LastMultifield;
         oldGarbageFrame->dirty = true;
        }

      if (returnValue != NULL) CL_ReleaseUDFV(theEnv,returnValue);
     }

   if (returnValue != NULL)
     { CL_EphemerateValue(theEnv,returnValue->value); }
  }

/*****************/
/* CL_GCBlockStart: */
/*****************/
void CL_GCBlockStart(
  Environment *theEnv,
  GCBlock *theBlock)
  {
   theBlock->oldGarbageFrame = UtilityData(theEnv)->CurrentGarbageFrame;
   memset(&theBlock->newGarbageFrame,0,sizeof(struct garbageFrame));
   theBlock->newGarbageFrame.priorFrame = theBlock->oldGarbageFrame;
   UtilityData(theEnv)->CurrentGarbageFrame = &theBlock->newGarbageFrame;
  }

/***************/
/* CL_GCBlockEnd: */
/***************/
void CL_GCBlockEnd(
  Environment *theEnv,
  GCBlock *theBlock)
  {
   CL_RestorePriorGarbageFrame(theEnv,&theBlock->newGarbageFrame,theBlock->oldGarbageFrame,NULL);
  }

/******************/
/* CL_GCBlockEndUDF: */
/******************/
void CL_GCBlockEndUDF(
  Environment *theEnv,
  GCBlock *theBlock,
  UDFValue *rv)
  {
   CL_RestorePriorGarbageFrame(theEnv,&theBlock->newGarbageFrame,theBlock->oldGarbageFrame,rv);
  }

/*************************/
/* CL_CallCleanupFunctions: */
/*************************/
void CL_CallCleanupFunctions(
  Environment *theEnv)
  {
   struct voidCL_CallFunctionItem *cleanupPtr;

   for (cleanupPtr = UtilityData(theEnv)->ListOfCleanupFunctions;
        cleanupPtr != NULL;
        cleanupPtr = cleanupPtr->next)
     { (*cleanupPtr->func)(theEnv,cleanupPtr->context); }
  }

/**************************************************/
/* CL_CallPeriodicTasks: Calls the list of functions */
/*   for handling periodic tasks.                 */
/**************************************************/
void CL_CallPeriodicTasks(
  Environment *theEnv)
  {
   struct voidCL_CallFunctionItem *periodPtr;

   if (UtilityData(theEnv)->PeriodicFunctionsEnabled)
     {
      for (periodPtr = UtilityData(theEnv)->ListOfPeriodicFunctions;
           periodPtr != NULL;
           periodPtr = periodPtr->next)
        { (*periodPtr->func)(theEnv,periodPtr->context); }
     }
  }

/***************************************************/
/* CL_AddCleanupFunction: Adds a function to the list */
/*   of functions called to perfoCL_rm cleanup such   */
/*   as returning free memory to the memory pool.  */
/***************************************************/
bool CL_AddCleanupFunction(
  Environment *theEnv,
  const char *name,
  VoidCL_CallFunction *theFunction,
  int priority,
  void *context)
  {
   UtilityData(theEnv)->ListOfCleanupFunctions =
     CL_AddCL_VoidFunctionToCallList(theEnv,name,priority,theFunction,
                           UtilityData(theEnv)->ListOfCleanupFunctions,context);
   return true;
  }

/****************************************************/
/* CL_AddPeriodicFunction: Adds a function to the list */
/*   of functions called to handle periodic tasks.  */
/****************************************************/
bool CL_AddPeriodicFunction(
  Environment *theEnv,
  const char *name,
  VoidCL_CallFunction *theFunction,
  int priority,
  void *context)
  {
   UtilityData(theEnv)->ListOfPeriodicFunctions =
     CL_AddCL_VoidFunctionToCallList(theEnv,name,priority,theFunction,
                           UtilityData(theEnv)->ListOfPeriodicFunctions,context);
   return true;
  }

/***************************************************/
/* CL_GetPeriodicFunctionContext: Returns the context */
/*   associated with a periodic function.          */
/***************************************************/
void *CL_GetPeriodicFunctionContext(
  Environment *theEnv,
  const char *name)
  {
   struct voidCL_CallFunctionItem *theItem;

   theItem = CL_GetCL_VoidFunctionFromCallList(theEnv,name,
                                         UtilityData(theEnv)->ListOfPeriodicFunctions);

   if (theItem == NULL) return NULL;

   return theItem->context;
  }

/*******************************************************/
/* CL_RemoveCleanupFunction: Removes a function from the  */
/*   list of functions called to perfoCL_rm cleanup such  */
/*   as returning free memory to the memory pool.      */
/*******************************************************/
bool CL_RemoveCleanupFunction(
  Environment *theEnv,
  const char *name)
  {
   bool found;

   UtilityData(theEnv)->ListOfCleanupFunctions =
      CL_RemoveCL_VoidFunctionFromCallList(theEnv,name,UtilityData(theEnv)->ListOfCleanupFunctions,&found);

   return found;
  }

/********************************************************/
/* CL_RemovePeriodicFunction: Removes a function from the  */
/*   list of functions called to handle periodic tasks. */
/********************************************************/
bool CL_RemovePeriodicFunction(
  Environment *theEnv,
  const char *name)
  {
   bool found;

   UtilityData(theEnv)->ListOfPeriodicFunctions =
      CL_RemoveCL_VoidFunctionFromCallList(theEnv,name,UtilityData(theEnv)->ListOfPeriodicFunctions,&found);

   return found;
  }

/*****************************************************/
/* StringPrintFoCL_rm: Generates printed representation */
/*   of a string. Replaces / with // and " with /".  */
/*****************************************************/
const char *StringPrintFoCL_rm(
  Environment *theEnv,
  const char *str)
  {
   int i = 0;
   size_t pos = 0;
   size_t max = 0;
   char *theString = NULL;
   CLIPSLexeme *thePtr;

   theString = CL_ExpandStringWithChar(theEnv,'"',theString,&pos,&max,max+80);
   while (str[i] != EOS)
     {
      if ((str[i] == '"') || (str[i] == '\\'))
        {
         theString = CL_ExpandStringWithChar(theEnv,'\\',theString,&pos,&max,max+80);
         theString = CL_ExpandStringWithChar(theEnv,str[i],theString,&pos,&max,max+80);
        }
      else
        { theString = CL_ExpandStringWithChar(theEnv,str[i],theString,&pos,&max,max+80); }
      i++;
     }

   theString = CL_ExpandStringWithChar(theEnv,'"',theString,&pos,&max,max+80);

   thePtr = CL_CreateString(theEnv,theString);
   CL_rm(theEnv,theString,max);
   
   return thePtr->contents;
  }

/**************************************************************/
/* CL_CopyString: Copies a string using CLIPS memory management. */
/**************************************************************/
char *CL_CopyString(
  Environment *theEnv,
  const char *theString)
  {
   char *stringCopy = NULL;

   if (theString != NULL)
     {
      stringCopy = (char *) CL_genalloc(theEnv,strlen(theString) + 1);
      CL_genstrcpy(stringCopy,theString);
     }

   return stringCopy;
  }

/*****************************************************************/
/* CL_DeleteString: Deletes a string using CLIPS memory management. */
/*****************************************************************/
void CL_DeleteString(
  Environment *theEnv,
  char *theString)
  {
   if (theString != NULL)
     { CL_genfree(theEnv,theString,strlen(theString) + 1); }
  }

/***********************************************************/
/* CL_AppendStrings: Appends two strings together. The string */
/*   created is added to the SymbolTable, so it is not     */
/*   necessary to deallocate the string returned.          */
/***********************************************************/
const char *CL_AppendStrings(
  Environment *theEnv,
  const char *str1,
  const char *str2)
  {
   size_t pos = 0;
   size_t max = 0;
   char *theString = NULL;
   CLIPSLexeme *thePtr;

   theString = CL_AppendToString(theEnv,str1,theString,&pos,&max);
   theString = CL_AppendToString(theEnv,str2,theString,&pos,&max);

   thePtr = CL_CreateString(theEnv,theString);
   CL_rm(theEnv,theString,max);
   return thePtr->contents;
  }

/******************************************************/
/* CL_AppendToString: Appends a string to another string */
/*   (expanding the other string if necessary).       */
/******************************************************/
char *CL_AppendToString(
  Environment *theEnv,
  const char *appendStr,
  char *oldStr,
  size_t *oldPos,
  size_t *oldMax)
  {
   size_t length;

   /*=========================================*/
   /* Expand the old string so it can contain */
   /* the new string (if necessary).          */
   /*=========================================*/

   length = strlen(appendStr);

   /*==============================================================*/
   /* Return NULL if the old string was not successfully expanded. */
   /*==============================================================*/

   if ((oldStr = CL_EnlargeString(theEnv,length,oldStr,oldPos,oldMax)) == NULL) { return NULL; }

   /*===============================================*/
   /* Append the new string to the expanded string. */
   /*===============================================*/

   CL_genstrcpy(&oldStr[*oldPos],appendStr);
   *oldPos += length;

   /*============================================================*/
   /* Return the expanded string containing the appended string. */
   /*============================================================*/

   return oldStr;
  }

/**********************************************************/
/* CL_InsertInString: Inserts a string within another string */
/*   (expanding the other string if necessary).           */
/**********************************************************/
char *CL_InsertInString(
  Environment *theEnv,
  const char *insertStr,
  size_t position,
  char *oldStr,
  size_t *oldPos,
  size_t *oldMax)
  {
   size_t length;

   /*=========================================*/
   /* Expand the old string so it can contain */
   /* the new string (if necessary).          */
   /*=========================================*/

   length = strlen(insertStr);

   /*==============================================================*/
   /* Return NULL if the old string was not successfully expanded. */
   /*==============================================================*/

   if ((oldStr = CL_EnlargeString(theEnv,length,oldStr,oldPos,oldMax)) == NULL) { return NULL; }

   /*================================================================*/
   /* Shift the contents to the right of insertion point so that the */
   /* new text does not overwrite what is currently in the string.   */
   /*================================================================*/

   memmove(&oldStr[position],&oldStr[position+length],*oldPos - position);

   /*===============================================*/
   /* Insert the new string in the expanded string. */
   /*===============================================*/

   CL_genstrncpy(&oldStr[*oldPos],insertStr,length);
   *oldPos += length;

   /*============================================================*/
   /* Return the expanded string containing the appended string. */
   /*============================================================*/

   return(oldStr);
  }

/*******************************************************************/
/* CL_EnlargeString: Enlarges a string by the specified amount.       */
/*******************************************************************/
char *CL_EnlargeString(
  Environment *theEnv,
  size_t length,
  char *oldStr,
  size_t *oldPos,
  size_t *oldMax)
  {
   size_t newMax;
   
   /*=========================================*/
   /* Expand the old string so it can contain */
   /* the new string (if necessary).          */
   /*=========================================*/

   if (length + *oldPos + 1 > *oldMax)
     {
      newMax = length + *oldPos + 1;
      if (newMax < sizeof(char *))
        { newMax = sizeof(char *); }

      oldStr = (char *) CL_genrealloc(theEnv,oldStr,*oldMax,newMax);
      
      *oldMax = newMax;
     }

   /*==============================================================*/
   /* Return NULL if the old string was not successfully expanded. */
   /*==============================================================*/

   if (oldStr == NULL) { return NULL; }

   return(oldStr);
  }

/*******************************************************/
/* CL_AppendNToString: Appends a string to another string */
/*   (expanding the other string if necessary). Only a */
/*   specified number of characters are appended from  */
/*   the string.                                       */
/*******************************************************/
char *CL_AppendNToString(
  Environment *theEnv,
  const char *appendStr,
  char *oldStr,
  size_t length,
  size_t *oldPos,
  size_t *oldMax)
  {
   size_t lengthWithEOS;
   size_t newSize;

   /*====================================*/
   /* DeteCL_rmine the number of characters */
   /* to be appended from the string.    */
   /*====================================*/

   if (appendStr[length-1] != '\0') lengthWithEOS = length + 1;
   else lengthWithEOS = length;

   /*=========================================*/
   /* Expand the old string so it can contain */
   /* the new string (if necessary).          */
   /*=========================================*/

   if (lengthWithEOS + *oldPos > *oldMax)
     {
      newSize = *oldPos + lengthWithEOS;
      if (newSize < sizeof(char *))
        { newSize = sizeof(char *); }

      oldStr = (char *) CL_genrealloc(theEnv,oldStr,*oldMax,newSize);
      *oldMax = newSize;
     }

   /*==============================================================*/
   /* Return NULL if the old string was not successfully expanded. */
   /*==============================================================*/

   if (oldStr == NULL) { return NULL; }

   /*==================================*/
   /* Append N characters from the new */
   /* string to the expanded string.   */
   /*==================================*/

   CL_genstrncpy(&oldStr[*oldPos],appendStr,length);
   *oldPos += (lengthWithEOS - 1);
   oldStr[*oldPos] = '\0';

   /*============================================================*/
   /* Return the expanded string containing the appended string. */
   /*============================================================*/

   return(oldStr);
  }

/*******************************************************/
/* CL_ExpandStringWithChar: Adds a character to a string, */
/*   reallocating space for the string if it needs to  */
/*   be enlarged. The backspace character causes the   */
/*   size of the string to reduced if it is "added" to */
/*   the string.                                       */
/*******************************************************/
char *CL_ExpandStringWithChar(
  Environment *theEnv,
  int inchar,
  char *str,
  size_t *pos,
  size_t *max,
  size_t newSize)
  {
   if ((*pos + 1) >= *max)
     {
      if (newSize < sizeof(char *))
        { newSize = sizeof(char *); }
      str = (char *) CL_genrealloc(theEnv,str,*max,newSize);
      *max = newSize;
     }

  if (inchar != '\b')
    {
     str[*pos] = (char) inchar;
     (*pos)++;
     str[*pos] = '\0';
    }
  else
    {
     /*===========================================================*/
     /* First delete any UTF-8 multibyte continuation characters. */
     /*===========================================================*/

     while ((*pos > 1) && IsUTF8MultiByteContinuation(str[*pos - 1]))
       { (*pos)--; }

     /*===================================================*/
     /* Now delete the first byte of the UTF-8 character. */
     /*===================================================*/

     if (*pos > 0) (*pos)--;
     str[*pos] = '\0';
    }

   return(str);
  }

/**********************************************************/
/* CL_AddCL_VoidFunctionToCallList: Adds a function to a list   */
/*   of functions which are called to perfoCL_rm certain     */
/*   operations (e.g. clear, reset, and bload functions). */
/**********************************************************/
struct voidCL_CallFunctionItem *CL_AddCL_VoidFunctionToCallList(
  Environment *theEnv,
  const char *name,
  int priority,
  VoidCL_CallFunction *func,
  struct voidCL_CallFunctionItem *head,
  void *context)
  {
   struct voidCL_CallFunctionItem *newPtr, *currentPtr, *lastPtr = NULL;
   char  *nameCopy;

   newPtr = get_struct(theEnv,voidCL_CallFunctionItem);

   nameCopy = (char *) CL_genalloc(theEnv,strlen(name) + 1);
   CL_genstrcpy(nameCopy,name);
   newPtr->name = nameCopy;

   newPtr->func = func;
   newPtr->priority = priority;
   newPtr->context = context;

   if (head == NULL)
     {
      newPtr->next = NULL;
      return(newPtr);
     }

   currentPtr = head;
   while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
     {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   if (lastPtr == NULL)
     {
      newPtr->next = head;
      head = newPtr;
     }
   else
     {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
     }

   return(head);
  }

/**********************************************************/
/* CL_AddBoolFunctionToCallList: Adds a function to a list   */
/*   of functions which are called to perfoCL_rm certain     */
/*   operations (e.g. clear, reset, and bload functions). */
/**********************************************************/
BoolCL_CallFunctionItem *CL_AddBoolFunctionToCallList(
  Environment *theEnv,
  const char *name,
  int priority,
  BoolCL_CallFunction *func,
  BoolCL_CallFunctionItem *head,
  void *context)
  {
   struct boolCL_CallFunctionItem *newPtr, *currentPtr, *lastPtr = NULL;
   char  *nameCopy;

   newPtr = get_struct(theEnv,boolCL_CallFunctionItem);

   nameCopy = (char *) CL_genalloc(theEnv,strlen(name) + 1);
   CL_genstrcpy(nameCopy,name);
   newPtr->name = nameCopy;

   newPtr->func = func;
   newPtr->priority = priority;
   newPtr->context = context;

   if (head == NULL)
     {
      newPtr->next = NULL;
      return(newPtr);
     }

   currentPtr = head;
   while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
     {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   if (lastPtr == NULL)
     {
      newPtr->next = head;
      head = newPtr;
     }
   else
     {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
     }

   return(head);
  }

/****************************************************************/
/* CL_GetCL_VoidFunctionFromCallList: Retrieves a function from a list of */
/*   functions which are called to perfoCL_rm certain operations   */
/*   (e.g. clear, reset, and bload functions).                  */
/****************************************************************/
struct voidCL_CallFunctionItem *CL_GetCL_VoidFunctionFromCallList(
  Environment *theEnv,
  const char *name,
  struct voidCL_CallFunctionItem *head)
  {
   struct voidCL_CallFunctionItem *currentPtr;

   for (currentPtr = head; currentPtr != NULL; currentPtr = currentPtr->next)
     {
      if (strcmp(name,currentPtr->name) == 0)
        { return currentPtr; }
     }

   return NULL;
  }

/****************************************************************/
/* CL_GetBoolFunctionFromCallList: Retrieves a function from a list of */
/*   functions which are called to perfoCL_rm certain operations   */
/*   (e.g. clear, reset, and bload functions).                  */
/****************************************************************/
struct boolCL_CallFunctionItem *CL_GetBoolFunctionFromCallList(
  Environment *theEnv,
  const char *name,
  struct boolCL_CallFunctionItem *head)
  {
   struct boolCL_CallFunctionItem *currentPtr;

   for (currentPtr = head; currentPtr != NULL; currentPtr = currentPtr->next)
     {
      if (strcmp(name,currentPtr->name) == 0)
        { return currentPtr; }
     }

   return NULL;
  }

/******************************************************************/
/* CL_RemoveCL_VoidFunctionFromCallList: Removes a function from a list */
/*   of functions which are called to perfoCL_rm certain operations  */
/*   (e.g. clear, reset, and bload functions).                    */
/******************************************************************/
struct voidCL_CallFunctionItem *CL_RemoveCL_VoidFunctionFromCallList(
  Environment *theEnv,
  const char *name,
  struct voidCL_CallFunctionItem *head,
  bool *found)
  {
   struct voidCL_CallFunctionItem *currentPtr, *lastPtr;

   *found = false;
   lastPtr = NULL;
   currentPtr = head;

   while (currentPtr != NULL)
     {
      if (strcmp(name,currentPtr->name) == 0)
        {
         *found = true;
         if (lastPtr == NULL)
           { head = currentPtr->next; }
         else
           { lastPtr->next = currentPtr->next; }

         CL_genfree(theEnv,(void *) currentPtr->name,strlen(currentPtr->name) + 1);
         rtn_struct(theEnv,voidCL_CallFunctionItem,currentPtr);
         return head;
        }

      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   return head;
  }

/******************************************************************/
/* CL_RemoveBoolFunctionFromCallList: Removes a function from a list */
/*   of functions which are called to perfoCL_rm certain operations  */
/*   (e.g. clear, reset, and bload functions).                    */
/******************************************************************/
struct boolCL_CallFunctionItem *CL_RemoveBoolFunctionFromCallList(
  Environment *theEnv,
  const char *name,
  struct boolCL_CallFunctionItem *head,
  bool *found)
  {
   struct boolCL_CallFunctionItem *currentPtr, *lastPtr;

   *found = false;
   lastPtr = NULL;
   currentPtr = head;

   while (currentPtr != NULL)
     {
      if (strcmp(name,currentPtr->name) == 0)
        {
         *found = true;
         if (lastPtr == NULL)
           { head = currentPtr->next; }
         else
           { lastPtr->next = currentPtr->next; }

         CL_genfree(theEnv,(void *) currentPtr->name,strlen(currentPtr->name) + 1);
         rtn_struct(theEnv,boolCL_CallFunctionItem,currentPtr);
         return head;
        }

      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   return head;
  }

/*************************************************************/
/* CL_DeallocateVoidCallList: Removes all functions from a list */
/*   of functions which are called to perfoCL_rm certain        */
/*   operations (e.g. clear, reset, and bload functions).    */
/*************************************************************/
void CL_DeallocateVoidCallList(
  Environment *theEnv,
  struct voidCL_CallFunctionItem *theList)
  {
   struct voidCL_CallFunctionItem *tmpPtr, *nextPtr;

   tmpPtr = theList;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      CL_genfree(theEnv,(void *) tmpPtr->name,strlen(tmpPtr->name) + 1);
      rtn_struct(theEnv,voidCL_CallFunctionItem,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

/*************************************************************/
/* CL_DeallocateBoolCallList: Removes all functions from a list */
/*   of functions which are called to perfoCL_rm certain        */
/*   operations (e.g. clear, reset, and bload functions).    */
/*************************************************************/
void CL_DeallocateBoolCallList(
  Environment *theEnv,
  struct boolCL_CallFunctionItem *theList)
  {
   struct boolCL_CallFunctionItem *tmpPtr, *nextPtr;

   tmpPtr = theList;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      CL_genfree(theEnv,(void *) tmpPtr->name,strlen(tmpPtr->name) + 1);
      rtn_struct(theEnv,boolCL_CallFunctionItem,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

/***********************************************************/
/* CL_AddFunctionToCallListWithArg: Adds a function to a list */
/*   of functions which are called to perfoCL_rm certain      */
/*   operations (e.g. clear, reset, and bload functions).  */
/***********************************************************/
struct callFunctionItemWithArg *CL_AddFunctionToCallListWithArg(
  Environment *theEnv,
  const char *name,
  int priority,
  VoidCL_CallFunctionWithArg *func,
  struct callFunctionItemWithArg *head,
  void *context)
  {
   struct callFunctionItemWithArg *newPtr, *currentPtr, *lastPtr = NULL;

   newPtr = get_struct(theEnv,callFunctionItemWithArg);

   newPtr->name = name;
   newPtr->func = func;
   newPtr->priority = priority;
   newPtr->context = context;

   if (head == NULL)
     {
      newPtr->next = NULL;
      return(newPtr);
     }

   currentPtr = head;
   while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
     {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   if (lastPtr == NULL)
     {
      newPtr->next = head;
      head = newPtr;
     }
   else
     {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
     }

   return(head);
  }

/**************************************************************/
/* CL_RemoveFunctionFromCallListWithArg: Removes a function from */
/*   a list of functions which are called to perfoCL_rm certain  */
/*   operations (e.g. clear, reset, and bload functions).     */
/**************************************************************/
struct callFunctionItemWithArg *CL_RemoveFunctionFromCallListWithArg(
  Environment *theEnv,
  const char *name,
  struct callFunctionItemWithArg *head,
  bool *found)
  {
   struct callFunctionItemWithArg *currentPtr, *lastPtr;

   *found = false;
   lastPtr = NULL;
   currentPtr = head;

   while (currentPtr != NULL)
     {
      if (strcmp(name,currentPtr->name) == 0)
        {
         *found = true;
         if (lastPtr == NULL)
           { head = currentPtr->next; }
         else
           { lastPtr->next = currentPtr->next; }

         rtn_struct(theEnv,callFunctionItemWithArg,currentPtr);
         return(head);
        }

      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   return(head);
  }

/****************************************************************/
/* CL_DeallocateCallListWithArg: Removes all functions from a list */
/*   of functions which are called to perfoCL_rm certain           */
/*   operations (e.g. clear, reset, and bload functions).       */
/****************************************************************/
void CL_DeallocateCallListWithArg(
  Environment *theEnv,
  struct callFunctionItemWithArg *theList)
  {
   struct callFunctionItemWithArg *tmpPtr, *nextPtr;

   tmpPtr = theList;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      rtn_struct(theEnv,callFunctionItemWithArg,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

/*****************************************/
/* CL_ItemHashValue: Returns the hash value */
/*   for the specified value.            */
/*****************************************/
size_t CL_ItemHashValue(
  Environment *theEnv,
  unsigned short theType,
  void *theValue,
  size_t theRange)
  {
#if OBJECT_SYSTEM
   union
     {
      void *vv;
      unsigned uv;
     } fis;
#endif

   switch(theType)
     {
      case FLOAT_TYPE:
        return CL_HashFloat(((CLIPSFloat *) theValue)->contents,theRange);

      case INTEGER_TYPE:
        return CL_HashInteger(((CLIPSInteger *) theValue)->contents,theRange);

      case SYMBOL_TYPE:
      case STRING_TYPE:
#if OBJECT_SYSTEM
      case INSTANCE_NAME_TYPE:
#endif
        return CL_HashSymbol(((CLIPSLexeme *) theValue)->contents,theRange);

      case MULTIFIELD_TYPE:
        return CL_HashMultifield((Multifield *) theValue,theRange);

#if DEFTEMPLATE_CONSTRUCT
      case FACT_ADDRESS_TYPE:
        return (((Fact *) theValue)->hashValue % theRange);
#endif

      case EXTERNAL_ADDRESS_TYPE:
        return CL_HashExternalAddress(((CLIPSExternalAddress *) theValue)->contents,theRange);

#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS_TYPE:
        fis.uv = 0;
        fis.vv = theValue;
        return (fis.uv % theRange);
#endif
     }

   CL_SystemError(theEnv,"UTILITY",1);
   return 0;
  }

/********************************************/
/* CL_YieldTime: Yields time to a user-defined */
/*   function. Intended to allow foreground */
/*   application responsiveness when CLIPS  */
/*   is running in the background.          */
/********************************************/
void CL_YieldTime(
  Environment *theEnv)
  {
   if ((UtilityData(theEnv)->YieldCL_TimeFunction != NULL) && UtilityData(theEnv)->YieldFunctionEnabled)
     { (*UtilityData(theEnv)->YieldCL_TimeFunction)(); }
  }

/****************************/
/* CL_EnablePeriodicFunctions: */
/****************************/
bool CL_EnablePeriodicFunctions(
  Environment *theEnv,
  bool value)
  {
   bool oldValue;

   oldValue = UtilityData(theEnv)->PeriodicFunctionsEnabled;

   UtilityData(theEnv)->PeriodicFunctionsEnabled = value;

   return oldValue;
  }

/************************/
/* CL_EnableYieldFunction: */
/************************/
bool CL_EnableYieldFunction(
  Environment *theEnv,
  bool value)
  {
   bool oldValue;

   oldValue = UtilityData(theEnv)->YieldFunctionEnabled;

   UtilityData(theEnv)->YieldFunctionEnabled = value;

   return oldValue;
  }

/*************************************************************************/
/* CL_AddTrackedMemory: Tracked memory is memory allocated by CLIPS that's  */
/*   referenced by a variable on the stack, but not by any environment   */
/*   data structure. An example would be the storage for local variables */
/*   allocated when a deffunction is executed. Tracking this memory      */
/*   allows it to be removed later when using longjmp as the code that   */
/*   would noCL_rmally deallocate the memory would be bypassed.             */
/*************************************************************************/
struct trackedMemory *CL_AddTrackedMemory(
  Environment *theEnv,
  void *theMemory,
  size_t theSize)
  {
   struct trackedMemory *newPtr;

   newPtr = get_struct(theEnv,trackedMemory);

   newPtr->prev = NULL;
   newPtr->theMemory = theMemory;
   newPtr->memSize = theSize;
   newPtr->next = UtilityData(theEnv)->trackList;
   UtilityData(theEnv)->trackList = newPtr;

   return newPtr;
  }

/************************/
/* CL_RemoveTrackedMemory: */
/************************/
void CL_RemoveTrackedMemory(
  Environment *theEnv,
  struct trackedMemory *theTracker)
  {
   if (theTracker->prev == NULL)
     { UtilityData(theEnv)->trackList = theTracker->next; }
   else
     { theTracker->prev->next = theTracker->next; }

   if (theTracker->next != NULL)
     { theTracker->next->prev = theTracker->prev; }

   rtn_struct(theEnv,trackedMemory,theTracker);
  }

/******************************************/
/* CL_UTF8Length: Returns the logical number */
/*   of characters in a UTF8 string.      */
/******************************************/
size_t CL_UTF8Length(
  const char *s)
  {
   size_t i = 0, length = 0;

   while (s[i] != '\0')
     {
      CL_UTF8Increment(s,&i);
      length++;
     }

   return(length);
  }

/*********************************************/
/* CL_UTF8Increment: Finds the beginning of the */
/*   next character in a UTF8 string.        */
/*********************************************/
void CL_UTF8Increment(
  const char *s,
  size_t *i)
  {
   (void) (IsUTF8Start(s[++(*i)]) ||
           IsUTF8Start(s[++(*i)]) ||
           IsUTF8Start(s[++(*i)]) ||
           ++(*i));
  }

/****************************************************/
/* CL_UTF8Offset: Converts the logical character index */
/*   in a UTF8 string to the actual byte offset.    */
/****************************************************/
size_t CL_UTF8Offset(
  const char *str,
  size_t charnum)
  {
   size_t offs = 0;

   while ((charnum > 0) && (str[offs]))
     {
      (void) (IsUTF8Start(str[++offs]) ||
              IsUTF8Start(str[++offs]) ||
              IsUTF8Start(str[++offs]) ||
              ++offs);

      charnum--;
     }

   return offs;
  }

/*************************************************/
/* CL_UTF8CharNum: Converts the UTF8 character byte */
/*   offset to the logical character index.      */
/*************************************************/
size_t CL_UTF8CharNum(
  const char *s,
  size_t offset)
  {
   size_t charnum = 0, offs=0;

   while ((offs < offset) && (s[offs]))
     {
      (void) (IsUTF8Start(s[++offs]) ||
              IsUTF8Start(s[++offs]) ||
              IsUTF8Start(s[++offs]) ||
              ++offs);

      charnum++;
     }

   return charnum;
  }

/************************/
/* CL_CreateStringCL_Builder: */
/************************/
StringCL_Builder *CL_CreateStringCL_Builder(
  Environment *theEnv,
  size_t theSize)
  {
   StringCL_Builder *theSB;
   
   theSB = get_struct(theEnv,stringCL_Builder);
   if (theSB == NULL) return NULL;
   
   theSize++;
   theSB->sbEnv = theEnv;
   theSB->bufferCL_Reset = theSize;
   theSB->bufferMaximum = theSize;
   theSB->length = 0;
   theSB->contents = (char *) CL_gm2(theEnv,theSize);
   theSB->contents[0] = EOS;
   
   return theSB;
  }

/*************/
/* CL_SBAppend: */
/*************/
void CL_SBAppend(
  StringCL_Builder *theSB,
  const char *appendString)
  {
   theSB->contents = CL_AppendToString(theSB->sbEnv,appendString,
                                    theSB->contents,&theSB->length,&theSB->bufferMaximum);
  }

/********************/
/* CL_SBAppendInteger: */
/********************/
void CL_SBAppendInteger(
  StringCL_Builder *theSB,
  long long value)
  {
   const char *appendString;

   appendString = CL_LongIntegerToString(theSB->sbEnv,value);

   theSB->contents = CL_AppendToString(theSB->sbEnv,appendString,
                                    theSB->contents,&theSB->length,&theSB->bufferMaximum);
  }

/******************/
/* CL_SBAppendFloat: */
/******************/
void CL_SBAppendFloat(
  StringCL_Builder *theSB,
  double value)
  {
   const char *appendString;

   appendString = CL_FloatToString(theSB->sbEnv,value);

   theSB->contents = CL_AppendToString(theSB->sbEnv,appendString,
                                    theSB->contents,&theSB->length,&theSB->bufferMaximum);
  }

/**************/
/* CL_SBAddChar: */
/**************/
void CL_SBAddChar(
  StringCL_Builder *theSB,
  int theChar)
  {
   theSB->contents = CL_ExpandStringWithChar(theSB->sbEnv,theChar,theSB->contents,
                                          &theSB->length,&theSB->bufferMaximum,
                                          theSB->bufferMaximum+80);
  }

/***********/
/* SBCL_Reset */
/***********/
void SBCL_Reset(
  StringCL_Builder *theSB)
  {
   if (theSB->bufferCL_Reset != theSB->bufferMaximum)
     {
      CL_rm(theSB->sbEnv,theSB->contents,theSB->bufferMaximum);
      theSB->contents = (char *) CL_gm2(theSB->sbEnv,theSB->bufferCL_Reset);
      theSB->bufferMaximum = theSB->bufferCL_Reset;
     }
     
   theSB->length = 0;
   theSB->contents[0] = EOS;
  }

/**********/
/* CL_SBCopy */
/**********/
char *CL_SBCopy(
  StringCL_Builder *theSB)
  {
   char *stringCopy;

   stringCopy = (char *) malloc(strlen(theSB->contents) + 1);
   strcpy(stringCopy,theSB->contents);

   return stringCopy;
  }

/**************/
/* CL_SBDispose: */
/**************/
void CL_SBDispose(
  StringCL_Builder *theSB)
  {
   Environment *theEnv = theSB->sbEnv;
   CL_rm(theEnv,theSB->contents,theSB->bufferMaximum);
   rtn_struct(theEnv,stringCL_Builder,theSB);
  }

