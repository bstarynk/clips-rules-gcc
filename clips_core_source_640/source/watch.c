   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*                    WATCH MODULE                     */
   /*******************************************************/


/*************************************************************/
/* Purpose: Support functions for the watch and unwatch      */
/*   commands.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian Dantes                                         */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added EnvCL_SetCL_WatchItem function.                */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Added Env prefix to GetCL_EvaluationError and     */
/*            SetCL_EvaluationError functions.                  */
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

#include "setup.h"

#if DEBUGGING_FUNCTIONS

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "constant.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "router.h"

#include "watch.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static CL_WatchItemRecord        *ValidCL_WatchItem(Environment *,const char *,bool *);
   static void                    DeallocateCL_WatchData(Environment *);

/**********************************************/
/* CL_InitializeCL_WatchData: Allocates environment */
/*    data for watch items.                   */
/**********************************************/
void CL_InitializeCL_WatchData(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,WATCH_DATA,sizeof(struct watchData),DeallocateCL_WatchData);
  }

/************************************************/
/* DeallocateCL_WatchData: Deallocates environment */
/*    data for watch items.                     */
/************************************************/
static void DeallocateCL_WatchData(
  Environment *theEnv)
  {
   CL_WatchItemRecord *tmpPtr, *nextPtr;

   tmpPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      rtn_struct(theEnv,watchItemRecord,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

/*************************************************************/
/* CL_AddCL_WatchItem: Adds an item to the list of watchable items */
/*   that can be set using the watch and unwatch commands.   */
/*   Returns false if the item is already in the list,       */
/*   otherwise returns true.                                 */
/*************************************************************/
bool CL_AddCL_WatchItem(
  Environment *theEnv,
  const char *name,
  int code,
  bool *flag,
  int priority,
  bool (*accessFunc)(Environment *,int,bool,struct expr *),
  bool (*printFunc)(Environment *,const char *,int,struct expr *))
  {
   CL_WatchItemRecord *newPtr, *currentPtr, *lastPtr;

   /*================================================================*/
   /* Find the insertion point in the watchable items list to place  */
   /* the new item. If the item is already in the list return false. */
   /*================================================================*/

   for (currentPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems, lastPtr = NULL;
        currentPtr != NULL;
        currentPtr = currentPtr->next)
     {
      if (strcmp(currentPtr->name,name) == 0) return false;
      if (priority < currentPtr->priority) lastPtr = currentPtr;
     }

   /*============================*/
   /* Create the new watch item. */
   /*============================*/

   newPtr = get_struct(theEnv,watchItemRecord);
   newPtr->name = name;
   newPtr->flag = flag;
   newPtr->code = code;
   newPtr->priority = priority;
   newPtr->accessFunc = accessFunc;
   newPtr->printFunc = printFunc;

   /*=================================================*/
   /* Insert the new item in the list of watch items. */
   /*=================================================*/

   if (lastPtr == NULL)
     {
      newPtr->next = CL_WatchData(theEnv)->ListOfCL_WatchItems;
      CL_WatchData(theEnv)->ListOfCL_WatchItems = newPtr;
     }
   else
     {
      newPtr->next = lastPtr->next;
      lastPtr->next = newPtr;
     }

   /*==================================================*/
   /* Return true to indicate the item has been added. */
   /*==================================================*/

   return true;
  }

/**************************************************/
/* CL_Watch: C access routine for the watch command. */
/**************************************************/
void CL_Watch(
  Environment *theEnv,
  CL_WatchItem item)
  {
   switch (item)
     {
      case ALL:
        CL_SetCL_WatchItem(theEnv,"all",true,NULL);
        break;

      case FACTS:
        CL_SetCL_WatchItem(theEnv,"facts",true,NULL);
        break;
        
      case INSTANCES:
        CL_SetCL_WatchItem(theEnv,"instances",true,NULL);
        break;
        
      case SLOTS:
        CL_SetCL_WatchItem(theEnv,"slots",true,NULL);
        break;
        
      case RULES:
        CL_SetCL_WatchItem(theEnv,"rules",true,NULL);
        break;
        
      case ACTIVATIONS:
        CL_SetCL_WatchItem(theEnv,"activations",true,NULL);
        break;
        
      case MESSAGES:
        CL_SetCL_WatchItem(theEnv,"messages",true,NULL);
        break;
        
      case MESSAGE_HANDLERS:
        CL_SetCL_WatchItem(theEnv,"message-handlers",true,NULL);
        break;
        
      case GENERIC_FUNCTIONS:
        CL_SetCL_WatchItem(theEnv,"generic-functions",true,NULL);
        break;
        
      case METHODS:
        CL_SetCL_WatchItem(theEnv,"methods",true,NULL);
        break;
        
      case DEFFUNCTIONS:
        CL_SetCL_WatchItem(theEnv,"deffunctions",true,NULL);
        break;
        
      case COMPILATIONS:
        CL_SetCL_WatchItem(theEnv,"compilations",true,NULL);
        break;
        
      case STATISTICS:
        CL_SetCL_WatchItem(theEnv,"statistics",true,NULL);
        break;
        
      case GLOBALS:
        CL_SetCL_WatchItem(theEnv,"globals",true,NULL);
        break;
        
      case FOCUS:
        CL_SetCL_WatchItem(theEnv,"focus",true,NULL);
        break;
     }
  }

/****************************************************/
/* CL_Unwatch: C access routine for the watch command. */
/****************************************************/
void CL_Unwatch(
  Environment *theEnv,
  CL_WatchItem item)
  {
   switch (item)
     {
      case ALL:
        CL_SetCL_WatchItem(theEnv,"all",false,NULL);
        break;

      case FACTS:
        CL_SetCL_WatchItem(theEnv,"facts",false,NULL);
        break;
        
      case INSTANCES:
        CL_SetCL_WatchItem(theEnv,"instances",false,NULL);
        break;
        
      case SLOTS:
        CL_SetCL_WatchItem(theEnv,"slots",false,NULL);
        break;
        
      case RULES:
        CL_SetCL_WatchItem(theEnv,"rules",false,NULL);
        break;
        
      case ACTIVATIONS:
        CL_SetCL_WatchItem(theEnv,"activations",false,NULL);
        break;
        
      case MESSAGES:
        CL_SetCL_WatchItem(theEnv,"messages",false,NULL);
        break;
        
      case MESSAGE_HANDLERS:
        CL_SetCL_WatchItem(theEnv,"message-handlers",false,NULL);
        break;
        
      case GENERIC_FUNCTIONS:
        CL_SetCL_WatchItem(theEnv,"generic-functions",false,NULL);
        break;
        
      case METHODS:
        CL_SetCL_WatchItem(theEnv,"methods",false,NULL);
        break;
        
      case DEFFUNCTIONS:
        CL_SetCL_WatchItem(theEnv,"deffunctions",false,NULL);
        break;
        
      case COMPILATIONS:
        CL_SetCL_WatchItem(theEnv,"compilations",false,NULL);
        break;
        
      case STATISTICS:
        CL_SetCL_WatchItem(theEnv,"statistics",false,NULL);
        break;
        
      case GLOBALS:
        CL_SetCL_WatchItem(theEnv,"globals",false,NULL);
        break;
        
      case FOCUS:
        CL_SetCL_WatchItem(theEnv,"focus",false,NULL);
        break;
     }
  }

/******************************************/
/* CL_GetCL_WatchState: Returns the watch state */
/*   for the specified watch item.        */
/******************************************/
bool CL_GetCL_WatchState(
  Environment *theEnv,
  CL_WatchItem item)
  {
   switch (item)
     {
      case ALL:
        return false;

      case FACTS:
        return (CL_GetCL_WatchItem(theEnv,"facts") == 1);
        
      case INSTANCES:
        return (CL_GetCL_WatchItem(theEnv,"instances") == 1);
        
      case SLOTS:
        return (CL_GetCL_WatchItem(theEnv,"slots") == 1);
        
      case RULES:
        return (CL_GetCL_WatchItem(theEnv,"rules") == 1);
        
      case ACTIVATIONS:
        return (CL_GetCL_WatchItem(theEnv,"activations") == 1);
        
      case MESSAGES:
        return (CL_GetCL_WatchItem(theEnv,"messages") == 1);
        
      case MESSAGE_HANDLERS:
        return (CL_GetCL_WatchItem(theEnv,"message-handlers") == 1);
        
      case GENERIC_FUNCTIONS:
        return (CL_GetCL_WatchItem(theEnv,"generic-functions") == 1);
        
      case METHODS:
        return (CL_GetCL_WatchItem(theEnv,"methods") == 1);
        
      case DEFFUNCTIONS:
        return (CL_GetCL_WatchItem(theEnv,"deffunctions") == 1);
        
      case COMPILATIONS:
        return (CL_GetCL_WatchItem(theEnv,"compilations") == 1);
        
      case STATISTICS:
        return (CL_GetCL_WatchItem(theEnv,"statistics") == 1);
        
      case GLOBALS:
        return (CL_GetCL_WatchItem(theEnv,"globals") == 1);
        
      case FOCUS:
        return (CL_GetCL_WatchItem(theEnv,"focus") == 1);
     }
     
   return false;
  }

/******************************************/
/* CL_SetCL_WatchState: Returns the watch state */
/*   for the specified watch item.        */
/******************************************/
void CL_SetCL_WatchState(
  Environment *theEnv,
  CL_WatchItem item,
  bool newState)
  {
   switch (item)
     {
      case ALL:
        CL_SetCL_WatchItem(theEnv,"all",newState,NULL);
        return;

      case FACTS:
        CL_SetCL_WatchItem(theEnv,"facts",newState,NULL);
        return;
        
      case INSTANCES:
        CL_SetCL_WatchItem(theEnv,"instances",newState,NULL);
        return;
        
      case SLOTS:
        CL_SetCL_WatchItem(theEnv,"slots",newState,NULL);
        return;
        
      case RULES:
        CL_SetCL_WatchItem(theEnv,"rules",newState,NULL);
        return;
        
      case ACTIVATIONS:
        CL_SetCL_WatchItem(theEnv,"activations",newState,NULL);
        return;
        
      case MESSAGES:
        CL_SetCL_WatchItem(theEnv,"messages",newState,NULL);
        return;
        
      case MESSAGE_HANDLERS:
        CL_SetCL_WatchItem(theEnv,"message-handlers",newState,NULL);
        return;
        
      case GENERIC_FUNCTIONS:
        CL_SetCL_WatchItem(theEnv,"generic-functions",newState,NULL);
        return;
        
      case METHODS:
        CL_SetCL_WatchItem(theEnv,"methods",newState,NULL);
        return;
        
      case DEFFUNCTIONS:
        CL_SetCL_WatchItem(theEnv,"deffunctions",newState,NULL);
        return;
        
      case COMPILATIONS:
        CL_SetCL_WatchItem(theEnv,"compilations",newState,NULL);
        return;
        
      case STATISTICS:
        CL_SetCL_WatchItem(theEnv,"statistics",newState,NULL);
        return;
        
      case GLOBALS:
        CL_SetCL_WatchItem(theEnv,"globals",newState,NULL);
        return;
        
      case FOCUS:
        CL_SetCL_WatchItem(theEnv,"focus",newState,NULL);
        return;
     }
  }

/********************************************************/
/* CL_WatchString: C access routine for the watch command. */
/********************************************************/
bool CL_WatchString(
  Environment *theEnv,
  const char *itemName)
  {
   return CL_SetCL_WatchItem(theEnv,itemName,true,NULL);
  }

/************************************************************/
/* CL_UnwatchString: C access routine for the unwatch command. */
/************************************************************/
bool CL_UnwatchString(
  Environment *theEnv,
  const char *itemName)
  {
   return CL_SetCL_WatchItem(theEnv,itemName,false,NULL);
  }

/********************************************************************/
/* CL_SetCL_WatchItem: Sets the state of a specified watch item to either */
/*   on or off. Returns true if the item was set, otherwise false.  */
/********************************************************************/
bool CL_SetCL_WatchItem(
  Environment *theEnv,
  const char *itemName,
  bool newState,
  struct expr *argExprs)
  {
   CL_WatchItemRecord *wPtr;

   /*===================================================*/
   /* If the name of the watch item to set is all, then */
   /* all watch items are set to the new state and true */
   /* is returned.                                      */
   /*===================================================*/

   if (strcmp(itemName,"all") == 0)
     {
      for (wPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems; wPtr != NULL; wPtr = wPtr->next)
        {
         /*==============================================*/
         /* If no specific arguments are specified, then */
         /* set the global flag for the watch item.      */
         /*==============================================*/

         if (argExprs == NULL) *(wPtr->flag) = newState;

         /*=======================================*/
         /* Set flags for individual watch items. */
         /*=======================================*/

         if ((wPtr->accessFunc == NULL) ? false :
             ((*wPtr->accessFunc)(theEnv,wPtr->code,newState,argExprs) == false))
           {
            SetCL_EvaluationError(theEnv,true);
            return false;
           }
        }
      return true;
     }

   /*=================================================*/
   /* Search for the watch item to be set in the list */
   /* of watch items. If found, set the watch item to */
   /* its new state and return true.                  */
   /*=================================================*/

   for (wPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems; wPtr != NULL; wPtr = wPtr->next)
     {
      if (strcmp(itemName,wPtr->name) == 0)
        {
         /*==============================================*/
         /* If no specific arguments are specified, then */
         /* set the global flag for the watch item.      */
         /*==============================================*/

         if (argExprs == NULL) *(wPtr->flag) = newState;

         /*=======================================*/
         /* Set flags for individual watch items. */
         /*=======================================*/

         if ((wPtr->accessFunc == NULL) ? false :
             ((*wPtr->accessFunc)(theEnv,wPtr->code,newState,argExprs) == false))
           {
            SetCL_EvaluationError(theEnv,true);
            return false;
           }

         return true;
        }
     }

   /*=================================================*/
   /* If the specified item was not found in the list */
   /* of watchable items then return false.           */
   /*=================================================*/

   return false;
  }

/****************************************************************/
/* CL_GetCL_WatchItem: Gets the current state of the specified watch  */
/*   item. Returns the state of the watch item (0 for off and 1 */
/*   for on) if the watch item is found in the list of watch    */
/*   items, otherwise -1 is returned.                           */
/****************************************************************/
int CL_GetCL_WatchItem(
  Environment *theEnv,
  const char *itemName)
  {
   CL_WatchItemRecord *wPtr;

   for (wPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems; wPtr != NULL; wPtr = wPtr->next)
     {
      if (strcmp(itemName,wPtr->name) == 0)
        {
         if (*(wPtr->flag))
           { return 1; }
         else
           { return 0; }
        }
     }

   return -1;
  }

/***************************************************************/
/* ValidCL_WatchItem: Returns true if the specified name is found */
/*   in the list of watch items, otherwise returns false.      */
/***************************************************************/
static CL_WatchItemRecord *ValidCL_WatchItem(
  Environment *theEnv,
  const char *itemName,
  bool *recognized)
  {
   CL_WatchItemRecord *wPtr;

   *recognized = true;
   if (strcmp(itemName,"all") == 0)
     return NULL;

   for (wPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems; wPtr != NULL; wPtr = wPtr->next)
     { if (strcmp(itemName,wPtr->name) == 0) return(wPtr); }

   *recognized = false;
   return NULL;
  }

/*************************************************************/
/* CL_GetNthCL_WatchName: Returns the name associated with the nth */
/*   item in the list of watchable items. If the nth item    */
/*   does not exist, then NULL is returned.                  */
/*************************************************************/
const char *CL_GetNthCL_WatchName(
  Environment *theEnv,
  int whichItem)
  {
   int i;
   CL_WatchItemRecord *wPtr;

   for (wPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems, i = 1;
        wPtr != NULL;
        wPtr = wPtr->next, i++)
     { if (i == whichItem) return(wPtr->name); }

   return NULL;
  }

/***************************************************************/
/* CL_GetNthCL_WatchValue: Returns the current state associated with */
/*   the nth item in the list of watchable items. If the nth   */
/*   item does not exist, then -1 is returned.                 */
/***************************************************************/
int CL_GetNthCL_WatchValue(
  Environment *theEnv,
  int whichItem)
  {
   int i;
   CL_WatchItemRecord *wPtr;

   for (wPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems, i = 1;
        wPtr != NULL;
        wPtr = wPtr->next, i++)
     { if (i == whichItem) return((int) *(wPtr->flag)); }

   return(-1);
  }

/**************************************/
/* CL_WatchCommand: H/L access routine   */
/*   for the watch command.           */
/**************************************/
void CL_WatchCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theValue;
   const char *argument;
   bool recognized;
   CL_WatchItemRecord *wPtr;

   /*========================================*/
   /* DeteCL_rmine which item is to be watched. */
   /*========================================*/

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theValue)) return;

   argument = theValue.lexemeValue->contents;
   wPtr = ValidCL_WatchItem(theEnv,argument,&recognized);
   if (recognized == false)
     {
      SetCL_EvaluationError(theEnv,true);
      CL_UDFInvalidArgumentMessage(context,"watchable symbol");
      return;
     }

   /*=================================================*/
   /* Check to make sure extra arguments are allowed. */
   /*=================================================*/

   if (GetNextArgument(GetFirstArgument()) != NULL)
     {
      if ((wPtr == NULL) ? true : (wPtr->accessFunc == NULL))
        {
         SetCL_EvaluationError(theEnv,true);
         CL_ExpectedCountError(theEnv,"watch",EXACTLY,1);
         return;
        }
     }

   /*=====================*/
   /* Set the watch item. */
   /*=====================*/

   CL_SetCL_WatchItem(theEnv,argument,true,GetNextArgument(GetFirstArgument()));
  }

/****************************************/
/* CL_UnwatchCommand: H/L access routine   */
/*   for the unwatch command.           */
/****************************************/
void CL_UnwatchCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theValue;
   const char *argument;
   bool recognized;
   CL_WatchItemRecord *wPtr;

   /*==========================================*/
   /* DeteCL_rmine which item is to be unwatched. */
   /*==========================================*/

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theValue)) return;

   argument = theValue.lexemeValue->contents;
   wPtr = ValidCL_WatchItem(theEnv,argument,&recognized);
   if (recognized == false)
     {
      SetCL_EvaluationError(theEnv,true);
      CL_UDFInvalidArgumentMessage(context,"watchable symbol");
      return;
     }

   /*=================================================*/
   /* Check to make sure extra arguments are allowed. */
   /*=================================================*/

   if (GetNextArgument(GetFirstArgument()) != NULL)
     {
      if ((wPtr == NULL) ? true : (wPtr->accessFunc == NULL))
        {
         SetCL_EvaluationError(theEnv,true);
         CL_ExpectedCountError(theEnv,"unwatch",EXACTLY,1);
         return;
        }
     }

   /*=====================*/
   /* Set the watch item. */
   /*=====================*/

   CL_SetCL_WatchItem(theEnv,argument,false,GetNextArgument(GetFirstArgument()));
  }

/************************************************/
/* CL_ListCL_WatchItemsCommand: H/L access routines   */
/*   for the list-watch-items command.          */
/************************************************/
void CL_ListCL_WatchItemsCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_WatchItemRecord *wPtr;
   UDFValue theValue;
   bool recognized;

   /*=======================*/
   /* List the watch items. */
   /*=======================*/

   if (GetFirstArgument() == NULL)
     {
      for (wPtr = CL_WatchData(theEnv)->ListOfCL_WatchItems; wPtr != NULL; wPtr = wPtr->next)
        {
         CL_WriteString(theEnv,STDOUT,wPtr->name);
         if (*(wPtr->flag)) CL_WriteString(theEnv,STDOUT," = on\n");
         else CL_WriteString(theEnv,STDOUT," = off\n");
        }
      return;
     }

   /*=======================================*/
   /* DeteCL_rmine which item is to be listed. */
   /*=======================================*/

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theValue)) return;
   wPtr = ValidCL_WatchItem(theEnv,theValue.lexemeValue->contents,&recognized);
   if ((recognized == false) || (wPtr == NULL))
     {
      SetCL_EvaluationError(theEnv,true);
      CL_ExpectedTypeError1(theEnv,"list-watch-items",1,"'watchable symbol'");
      return;
     }

   /*=================================================*/
   /* Check to make sure extra arguments are allowed. */
   /*=================================================*/

   if ((wPtr->printFunc == NULL) &&
       (GetNextArgument(GetFirstArgument()) != NULL))
     {
      SetCL_EvaluationError(theEnv,true);
      CL_ExpectedCountError(theEnv,"list-watch-items",EXACTLY,1);
      return;
     }

   /*====================================*/
   /* List the status of the watch item. */
   /*====================================*/

   CL_WriteString(theEnv,STDOUT,wPtr->name);
   if (*(wPtr->flag)) CL_WriteString(theEnv,STDOUT," = on\n");
   else CL_WriteString(theEnv,STDOUT," = off\n");

   /*============================================*/
   /* List the status of individual watch items. */
   /*============================================*/

   if (wPtr->printFunc != NULL)
     {
      if ((*wPtr->printFunc)(theEnv,STDOUT,wPtr->code,
                             GetNextArgument(GetFirstArgument())) == false)
        { SetCL_EvaluationError(theEnv,true); }
     }
  }

/*******************************************/
/* CL_GetCL_WatchItemCommand: H/L access routine */
/*   for the get-watch-item command.       */
/*******************************************/
void CL_GetCL_WatchItemCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theValue;
   const char *argument;
   bool recognized;

   /*========================================*/
   /* DeteCL_rmine which item is to be watched. */
   /*========================================*/

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theValue))
     { return; }

   argument = theValue.lexemeValue->contents;
   ValidCL_WatchItem(theEnv,argument,&recognized);
   if (recognized == false)
     {
      SetCL_EvaluationError(theEnv,true);
      CL_ExpectedTypeError1(theEnv,"get-watch-item",1,"'watchable symbol'");
      returnValue->lexemeValue = FalseSymbol(theEnv);
      return;
     }

   /*===========================*/
   /* Get the watch item value. */
   /*===========================*/

   if (CL_GetCL_WatchItem(theEnv,argument) == 1)
     { returnValue->lexemeValue = TrueSymbol(theEnv); }
   else
     { returnValue->lexemeValue = FalseSymbol(theEnv); }
  }

/*************************************************************/
/* CL_WatchFunctionDefinitions: Initializes the watch commands. */
/*************************************************************/
void CL_WatchFunctionDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   CL_AddUDF(theEnv,"watch","v",1,UNBOUNDED,"*;y",CL_WatchCommand,"CL_WatchCommand",NULL);
   CL_AddUDF(theEnv,"unwatch","v",1,UNBOUNDED,"*;y",CL_UnwatchCommand,"CL_UnwatchCommand",NULL);
   CL_AddUDF(theEnv,"get-watch-item","b",1,1,"y",CL_GetCL_WatchItemCommand,"CL_GetCL_WatchItemCommand",NULL);
   CL_AddUDF(theEnv,"list-watch-items","v",0,UNBOUNDED,"*;y",CL_ListCL_WatchItemsCommand,"CL_ListCL_WatchItemsCommand",NULL);
#endif
  }

#endif /* DEBUGGING_FUNCTIONS */

