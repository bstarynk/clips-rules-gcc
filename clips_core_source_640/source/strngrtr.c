   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*              STRING_TYPE I/O ROUTER MODULE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: I/O Router routines which allow strings to be    */
/*   used as input and output sources.                       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Used CL_genstrcpy instead of strcpy.              */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Changed return values for router functions.    */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setup.h"

#include "constant.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "prntutil.h"
#include "router.h"
#include "sysdep.h"

#include "strngrtr.h"

#define READ_STRING 0
#define WRITE_STRING 1

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    QueryStringCallback(Environment *,const char *,void *);
   static void                    CL_WriteStringCallback(Environment *,const char *,const char *,void *);
   static int                     ReadStringCallback(Environment *,const char *,void *);
   static int                     UnreadStringCallback(Environment *,const char *,int,void *);
   static StringRouter           *FindStringRouter(Environment *,const char *);
   static bool                    CreateReadStringSource(Environment *,const char *,const char *,size_t,size_t);
   static void                    DeallocateStringRouterData(Environment *);
   static StringCL_BuilderRouter    *FindStringCL_BuilderRouter(Environment *,const char *);
   static bool                    QueryStringCL_BuilderCallback(Environment *,const char *,void *);
   static void                    CL_WriteStringCL_BuilderCallback(Environment *,const char *,const char *,void *);

/**********************************************************/
/* CL_InitializeStringRouter: Initializes string I/O router. */
/**********************************************************/
void CL_InitializeStringRouter(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,STRING_ROUTER_DATA,sizeof(struct stringRouterData),DeallocateStringRouterData);

   CL_AddRouter(theEnv,"string",0,QueryStringCallback,CL_WriteStringCallback,ReadStringCallback,UnreadStringCallback,NULL,NULL);
   CL_AddRouter(theEnv,"stringCL_Builder",0,QueryStringCL_BuilderCallback,CL_WriteStringCL_BuilderCallback,NULL,NULL,NULL,NULL);
  }

/*******************************************/
/* DeallocateStringRouterData: Deallocates */
/*    environment data for string routers. */
/*******************************************/
static void DeallocateStringRouterData(
  Environment *theEnv)
  {
   StringRouter *tmpPtr, *nextPtr;
   StringCL_BuilderRouter *tmpSBPtr, *nextSBPtr;

   tmpPtr = StringRouterData(theEnv)->ListOfStringRouters;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      CL_rm(theEnv,(void *) tmpPtr->name,strlen(tmpPtr->name) + 1);
      rtn_struct(theEnv,stringRouter,tmpPtr);
      tmpPtr = nextPtr;
     }

   tmpSBPtr = StringRouterData(theEnv)->ListOfStringCL_BuilderRouters;
   while (tmpSBPtr != NULL)
     {
      nextSBPtr = tmpSBPtr->next;
      CL_rm(theEnv,(void *) tmpSBPtr->name,strlen(tmpSBPtr->name) + 1);
      rtn_struct(theEnv,stringCL_BuilderRouter,tmpSBPtr);
      tmpSBPtr = nextSBPtr;
     }
  }

/*********************************************************************/
/* QueryStringCallback: Find routine for string router logical names. */
/*********************************************************************/
static bool QueryStringCallback(
  Environment *theEnv,
  const char *logicalName,
  void *context)
  {
   struct stringRouter *head;

   head = StringRouterData(theEnv)->ListOfStringRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,logicalName) == 0)
        { return true; }
      head = head->next;
     }

   return false;
  }

/**********************************************************/
/* CL_WriteStringCallback: Print routine for string routers. */
/**********************************************************/
static void CL_WriteStringCallback(
  Environment *theEnv,
  const char *logicalName,
  const char *str,
  void *context)
  {
   struct stringRouter *head;

   head = FindStringRouter(theEnv,logicalName);
   if (head == NULL)
     {
      CL_SystemError(theEnv,"ROUTER",3);
      CL_ExitRouter(theEnv,EXIT_FAILURE);
      return;
     }

   if (head->readCL_WriteType != WRITE_STRING) return;

   if (head->maximumPosition == 0) return;

   if ((head->currentPosition + 1) >= head->maximumPosition) return;

   CL_genstrncpy(&head->writeString[head->currentPosition],
              str,(STD_SIZE) (head->maximumPosition - head->currentPosition) - 1);

   head->currentPosition += strlen(str);
  }

/********************************************************/
/* ReadStringCallback: Getc routine for string routers. */
/********************************************************/
static int ReadStringCallback(
  Environment *theEnv,
  const char *logicalName,
  void *context)
  {
   struct stringRouter *head;
   int rc;

   head = FindStringRouter(theEnv,logicalName);
   if (head == NULL)
     {
      CL_SystemError(theEnv,"ROUTER",1);
      CL_ExitRouter(theEnv,EXIT_FAILURE);
     }

   if (head->readCL_WriteType != READ_STRING) return(EOF);
   if (head->currentPosition >= head->maximumPosition)
     {
      head->currentPosition++;
      return(EOF);
     }

   rc = (unsigned char) head->readString[head->currentPosition];
   head->currentPosition++;

   return(rc);
  }

/************************************************************/
/* UnreadStringCallback: Ungetc routine for string routers. */
/************************************************************/
static int UnreadStringCallback(
  Environment *theEnv,
  const char *logicalName,
  int ch,
  void *context)
  {
   struct stringRouter *head;
#if MAC_XCD
#pragma unused(ch)
#endif

   head = FindStringRouter(theEnv,logicalName);

   if (head == NULL)
     {
      CL_SystemError(theEnv,"ROUTER",2);
      CL_ExitRouter(theEnv,EXIT_FAILURE);
     }

   if (head->readCL_WriteType != READ_STRING) return 0;
   if (head->currentPosition > 0)
     { head->currentPosition--; }

   return 1;
  }

/************************************************/
/* CL_OpenStringSource: Opens a new string router. */
/************************************************/
bool CL_OpenStringSource(
  Environment *theEnv,
  const char *name,
  const char *str,
  size_t currentPosition)
  {
   size_t maximumPosition;

   if (str == NULL)
     {
      currentPosition = 0;
      maximumPosition = 0;
     }
   else
     { maximumPosition = strlen(str); }

   return(CreateReadStringSource(theEnv,name,str,currentPosition,maximumPosition));
  }

/******************************************************/
/* CL_OpenTextSource: Opens a new string router for text */
/*   (which is not NULL teCL_rminated).                  */
/******************************************************/
bool CL_OpenTextSource(
  Environment *theEnv,
  const char *name,
  const char *str,
  size_t currentPosition,
  size_t maximumPosition)
  {
   if (str == NULL)
     {
      currentPosition = 0;
      maximumPosition = 0;
     }

   return CreateReadStringSource(theEnv,name,str,currentPosition,maximumPosition);
  }

/******************************************************************/
/* CreateReadStringSource: Creates a new string router for input. */
/******************************************************************/
static bool CreateReadStringSource(
  Environment *theEnv,
  const char *name,
  const char *str,
  size_t currentPosition,
  size_t maximumPosition)
  {
   struct stringRouter *newStringRouter;
   char *theName;

   if (FindStringRouter(theEnv,name) != NULL) return false;

   newStringRouter = get_struct(theEnv,stringRouter);
   theName = (char *) CL_gm1(theEnv,strlen(name) + 1);
   CL_genstrcpy(theName,name);
   newStringRouter->name = theName;
   newStringRouter->writeString = NULL;
   newStringRouter->readString = str;
   newStringRouter->currentPosition = currentPosition;
   newStringRouter->readCL_WriteType = READ_STRING;
   newStringRouter->maximumPosition = maximumPosition;
   newStringRouter->next = StringRouterData(theEnv)->ListOfStringRouters;
   StringRouterData(theEnv)->ListOfStringRouters = newStringRouter;

   return true;
  }

/**********************************************/
/* CL_CloseStringSource: Closes a string router. */
/**********************************************/
bool CL_CloseStringSource(
  Environment *theEnv,
  const char *name)
  {
   struct stringRouter *head, *last;

   last = NULL;
   head = StringRouterData(theEnv)->ListOfStringRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,name) == 0)
        {
         if (last == NULL)
           {
            StringRouterData(theEnv)->ListOfStringRouters = head->next;
            CL_rm(theEnv,(void *) head->name,strlen(head->name) + 1);
            rtn_struct(theEnv,stringRouter,head);
            return true;
           }
         else
           {
            last->next = head->next;
            CL_rm(theEnv,(void *) head->name,strlen(head->name) + 1);
            rtn_struct(theEnv,stringRouter,head);
            return true;
           }
        }
      last = head;
      head = head->next;
     }

   return false;
  }

/******************************************************************/
/* CL_OpenStringDestination: Opens a new string router for printing. */
/******************************************************************/
bool CL_OpenStringDestination(
  Environment *theEnv,
  const char *name,
  char *str,
  size_t maximumPosition)
  {
   struct stringRouter *newStringRouter;
   char *theName;

   if (FindStringRouter(theEnv,name) != NULL) return false;

   newStringRouter = get_struct(theEnv,stringRouter);
   theName = (char *) CL_gm1(theEnv,strlen(name) + 1);
   CL_genstrcpy(theName,name);
   newStringRouter->name = theName;
   newStringRouter->readString = NULL;
   newStringRouter->writeString = str;
   newStringRouter->currentPosition = 0;
   newStringRouter->readCL_WriteType = WRITE_STRING;
   newStringRouter->maximumPosition = maximumPosition;
   newStringRouter->next = StringRouterData(theEnv)->ListOfStringRouters;
   StringRouterData(theEnv)->ListOfStringRouters = newStringRouter;

   return true;
  }

/***************************************************/
/* CL_CloseStringDestination: Closes a string router. */
/***************************************************/
bool CL_CloseStringDestination(
  Environment *theEnv,
  const char *name)
  {
   return CL_CloseStringSource(theEnv,name);
  }

/*******************************************************************/
/* FindStringRouter: Returns a pointer to the named string router. */
/*******************************************************************/
static struct stringRouter *FindStringRouter(
  Environment *theEnv,
  const char *name)
  {
   struct stringRouter *head;

   head = StringRouterData(theEnv)->ListOfStringRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,name) == 0)
        { return(head); }
      head = head->next;
     }

   return NULL;
  }

/*********************************************/
/* OpenStringCL_BuilderDestination: Opens a new */
/*   StringCL_Builder router for printing.      */
/*********************************************/
bool OpenStringCL_BuilderDestination(
  Environment *theEnv,
  const char *name,
  StringCL_Builder *theSB)
  {
   StringCL_BuilderRouter *newStringRouter;
   char *theName;

   if (FindStringCL_BuilderRouter(theEnv,name) != NULL) return false;

   newStringRouter = get_struct(theEnv,stringCL_BuilderRouter);
   theName = (char *) CL_gm1(theEnv,strlen(name) + 1);
   CL_genstrcpy(theName,name);
   newStringRouter->name = theName;
   newStringRouter->SBR = theSB;
   newStringRouter->next = StringRouterData(theEnv)->ListOfStringCL_BuilderRouters;
   StringRouterData(theEnv)->ListOfStringCL_BuilderRouters = newStringRouter;

   return true;
  }

/*****************************************/
/* CloseStringCL_BuilderDestination: Closes */
/*   a StringCL_Builder router.             */
/*****************************************/
bool CloseStringCL_BuilderDestination(
  Environment *theEnv,
  const char *name)
  {
   StringCL_BuilderRouter *head, *last;

   last = NULL;
   head = StringRouterData(theEnv)->ListOfStringCL_BuilderRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,name) == 0)
        {
         if (last == NULL)
           {
            StringRouterData(theEnv)->ListOfStringCL_BuilderRouters = head->next;
            CL_rm(theEnv,(void *) head->name,strlen(head->name) + 1);
            rtn_struct(theEnv,stringCL_BuilderRouter,head);
            return true;
           }
         else
           {
            last->next = head->next;
            CL_rm(theEnv,(void *) head->name,strlen(head->name) + 1);
            rtn_struct(theEnv,stringCL_BuilderRouter,head);
            return true;
           }
        }
      last = head;
      head = head->next;
     }

   return false;
  }

/**********************************************/
/* FindStringCL_BuilderRouter: Returns a pointer */
/*   to the named StringCL_Builder router.       */
/**********************************************/
static struct stringCL_BuilderRouter *FindStringCL_BuilderRouter(
  Environment *theEnv,
  const char *name)
  {
   StringCL_BuilderRouter *head;

   head = StringRouterData(theEnv)->ListOfStringCL_BuilderRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,name) == 0)
        { return head; }
      head = head->next;
     }

   return NULL;
  }

/*********************************************/
/* QueryStringCL_BuilderCallback: Query routine */
/*   for stringCL_Builder router logical names. */
/*********************************************/
static bool QueryStringCL_BuilderCallback(
  Environment *theEnv,
  const char *logicalName,
  void *context)
  {
   StringCL_BuilderRouter *head;

   head = StringRouterData(theEnv)->ListOfStringCL_BuilderRouters;
   while (head != NULL)
     {
      if (strcmp(head->name,logicalName) == 0)
        { return true; }
      head = head->next;
     }

   return false;
  }

/*********************************************/
/* CL_WriteStringCL_BuilderCallback: Print routine */
/*    for stringCL_Builder routers.             */
/*********************************************/
static void CL_WriteStringCL_BuilderCallback(
  Environment *theEnv,
  const char *logicalName,
  const char *str,
  void *context)
  {
   StringCL_BuilderRouter *head;

   head = FindStringCL_BuilderRouter(theEnv,logicalName);
   if (head == NULL)
     {
      CL_SystemError(theEnv,"ROUTER",3);
      CL_ExitRouter(theEnv,EXIT_FAILURE);
      return;
     }
     
   CL_SBAppend(head->SBR,str);
  }


