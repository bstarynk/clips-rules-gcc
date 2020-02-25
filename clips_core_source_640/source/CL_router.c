   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*                  I/O ROUTER MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides a centralized mechanism for handling    */
/*   input and output requests.                              */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed conversion of '\r' to '\n' from the    */
/*            EnvGetcRouter function.                        */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added support for passing context info_rmation  */
/*            to the router functions.                       */
/*                                                           */
/*      6.30: Fixed issues with passing context to routers.  */
/*                                                           */
/*            Added AwaitingInput flag.                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Added CL_InputBufferCount function.               */
/*                                                           */
/*            Added check for reuse of existing router name. */
/*                                                           */
/*            Added Env prefix to Get_EvaluationError and     */
/*            Set_EvaluationError functions.                  */
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
/*            Callbacks must be environment aware.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setup.h"

#include "argacces.h"
#include "constant.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "filertr.h"
#include "memalloc.h"
#include "prntutil.h"
#include "scanner.h"
#include "strngrtr.h"
#include "sysdep.h"

#include "router.h"

/**********************/
/* STRING DEFINITIONS */
/**********************/

const char *STDIN = "stdin";
const char *STDOUT = "stdout";
const char *STDERR = "stderr";
const char *STDWRN = "stdwrn";

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

static bool QueryRouter (Environment *, const char *, struct router *);
static void DeallocateRouterData (Environment *);

/*********************************************************/
/* CL_InitializeDefaultRouters: Initializes output streams. */
/*********************************************************/
void
CL_InitializeDefaultRouters (Environment * theEnv)
{
  CL_AllocateEnvironmentData (theEnv, ROUTER_DATA, sizeof (struct routerData),
			      DeallocateRouterData);

  RouterData (theEnv)->CommandBufferInputCount = 0;
  RouterData (theEnv)->InputUngets = 0;
  RouterData (theEnv)->AwaitingInput = true;

  CL_InitializeFileRouter (theEnv);
  CL_InitializeStringRouter (theEnv);
}

/*************************************************/
/* DeallocateRouterData: Deallocates environment */
/*    data for I/O routers.                      */
/*************************************************/
static void
DeallocateRouterData (Environment * theEnv)
{
  struct router *tmpPtr, *nextPtr;

  tmpPtr = RouterData (theEnv)->ListOfRouters;
  while (tmpPtr != NULL)
    {
      nextPtr = tmpPtr->next;
      CL_genfree (theEnv, (void *) tmpPtr->name, strlen (tmpPtr->name) + 1);
      rtn_struct (theEnv, router, tmpPtr);
      tmpPtr = nextPtr;
    }
}

/*********************/
/* CL_PrintRouterExists */
/*********************/
bool
CL_PrintRouterExists (Environment * theEnv, const char *logicalName)
{
  struct router *currentPtr;

  if (((char *) RouterData (theEnv)->Fast_SaveFilePtr) == logicalName)
    {
      return true;
    }

  currentPtr = RouterData (theEnv)->ListOfRouters;
  while (currentPtr != NULL)
    {
      if ((currentPtr->writeCallback != NULL) ? QueryRouter (theEnv,
							     logicalName,
							     currentPtr) :
	  false)
	{
	  return true;
	}
      currentPtr = currentPtr->next;
    }

  return false;
}

/**********************************/
/* CL_Write: Generic print function. */
/**********************************/
void
CL_Write (Environment * theEnv, const char *str)
{
  CL_WriteString (theEnv, STDOUT, str);
}

/************************************/
/* CL_Writeln: Generic print function. */
/************************************/
void
CL_Writeln (Environment * theEnv, const char *str)
{
  CL_WriteString (theEnv, STDOUT, str);
  CL_WriteString (theEnv, STDOUT, "\n");
}

/****************************************/
/* CL_WriteString: Generic print function. */
/****************************************/
void
CL_WriteString (Environment * theEnv,
		const char *logicalName, const char *str)
{
  struct router *currentPtr;

  if (str == NULL)
    return;

   /*===================================================*/
  /* If the "fast save" option is being used, then the */
  /* logical name is actually a pointer to a file and  */
  /* fprintf can be called directly to bypass querying */
  /* all of the routers.                               */
   /*===================================================*/

  if (((char *) RouterData (theEnv)->Fast_SaveFilePtr) == logicalName)
    {
      fprintf (RouterData (theEnv)->Fast_SaveFilePtr, "%s", str);
      return;
    }

   /*==============================================*/
  /* Search through the list of routers until one */
  /* is found that will handle the print request. */
   /*==============================================*/

  currentPtr = RouterData (theEnv)->ListOfRouters;
  while (currentPtr != NULL)
    {
      if ((currentPtr->writeCallback != NULL) ? QueryRouter (theEnv,
							     logicalName,
							     currentPtr) :
	  false)
	{
	  (*currentPtr->writeCallback) (theEnv, logicalName, str,
					currentPtr->context);
	  return;
	}
      currentPtr = currentPtr->next;
    }

   /*=====================================================*/
  /* The logical name was not recognized by any routers. */
   /*=====================================================*/

  if (strcmp (STDERR, logicalName) != 0)
    {
      CL_UnrecognizedRouterMessage (theEnv, logicalName);
    }
}

/***********************************************/
/* CL_ReadRouter: Generic get character function. */
/***********************************************/
int
CL_ReadRouter (Environment * theEnv, const char *logicalName)
{
  struct router *currentPtr;
  int inchar;

   /*===================================================*/
  /* If the "fast load" option is being used, then the */
  /* logical name is actually a pointer to a file and  */
  /* getc can be called directly to bypass querying    */
  /* all of the routers.                               */
   /*===================================================*/

  if (((char *) RouterData (theEnv)->Fast_LoadFilePtr) == logicalName)
    {
      inchar = getc (RouterData (theEnv)->Fast_LoadFilePtr);

      if ((inchar == '\r') || (inchar == '\n'))
	{
	  if (((char *) RouterData (theEnv)->Fast_LoadFilePtr) ==
	      RouterData (theEnv)->LineCountRouter)
	    {
	      CL_IncrementLineCount (theEnv);
	    }
	}

      /* if (inchar == '\r') return('\n'); */

      return (inchar);
    }

   /*===============================================*/
  /* If the "fast string get" option is being used */
  /* for the specified logical name, then bypass   */
  /* the router system and extract the character   */
  /* directly from the fast get string.            */
   /*===============================================*/

  if (RouterData (theEnv)->FastCharGetRouter == logicalName)
    {
      inchar =
	(unsigned char)
	RouterData (theEnv)->
	FastCharGetString[RouterData (theEnv)->FastCharGetIndex];

      RouterData (theEnv)->FastCharGetIndex++;

      if (inchar == '\0')
	return (EOF);

      if ((inchar == '\r') || (inchar == '\n'))
	{
	  if (RouterData (theEnv)->FastCharGetRouter ==
	      RouterData (theEnv)->LineCountRouter)
	    {
	      CL_IncrementLineCount (theEnv);
	    }
	}

      return (inchar);
    }

   /*==============================================*/
  /* Search through the list of routers until one */
  /* is found that will handle the getc request.  */
   /*==============================================*/

  currentPtr = RouterData (theEnv)->ListOfRouters;
  while (currentPtr != NULL)
    {
      if ((currentPtr->readCallback != NULL) ? QueryRouter (theEnv,
							    logicalName,
							    currentPtr) :
	  false)
	{
	  inchar =
	    (*currentPtr->readCallback) (theEnv, logicalName,
					 currentPtr->context);

	  if ((inchar == '\r') || (inchar == '\n'))
	    {
	      if ((RouterData (theEnv)->LineCountRouter != NULL) &&
		  (strcmp (logicalName, RouterData (theEnv)->LineCountRouter)
		   == 0))
		{
		  CL_IncrementLineCount (theEnv);
		}
	    }

	  return (inchar);
	}
      currentPtr = currentPtr->next;
    }

   /*=====================================================*/
  /* The logical name was not recognized by any routers. */
   /*=====================================================*/

  CL_UnrecognizedRouterMessage (theEnv, logicalName);
  return (-1);
}

/***************************************************/
/* CL_UnreadRouter: Generic unget character function. */
/***************************************************/
int
CL_UnreadRouter (Environment * theEnv, const char *logicalName, int ch)
{
  struct router *currentPtr;

   /*===================================================*/
  /* If the "fast load" option is being used, then the */
  /* logical name is actually a pointer to a file and  */
  /* ungetc can be called directly to bypass querying  */
  /* all of the routers.                               */
   /*===================================================*/

  if (((char *) RouterData (theEnv)->Fast_LoadFilePtr) == logicalName)
    {
      if ((ch == '\r') || (ch == '\n'))
	{
	  if (((char *) RouterData (theEnv)->Fast_LoadFilePtr) ==
	      RouterData (theEnv)->LineCountRouter)
	    {
	      CL_DecrementLineCount (theEnv);
	    }
	}

      return ungetc (ch, RouterData (theEnv)->Fast_LoadFilePtr);
    }

   /*===============================================*/
  /* If the "fast string get" option is being used */
  /* for the specified logical name, then bypass   */
  /* the router system and unget the character     */
  /* directly from the fast get string.            */
   /*===============================================*/

  if (RouterData (theEnv)->FastCharGetRouter == logicalName)
    {
      if ((ch == '\r') || (ch == '\n'))
	{
	  if (RouterData (theEnv)->FastCharGetRouter ==
	      RouterData (theEnv)->LineCountRouter)
	    {
	      CL_DecrementLineCount (theEnv);
	    }
	}

      if (RouterData (theEnv)->FastCharGetIndex > 0)
	RouterData (theEnv)->FastCharGetIndex--;
      return ch;
    }

   /*===============================================*/
  /* Search through the list of routers until one  */
  /* is found that will handle the ungetc request. */
   /*===============================================*/

  currentPtr = RouterData (theEnv)->ListOfRouters;
  while (currentPtr != NULL)
    {
      if ((currentPtr->unreadCallback != NULL) ? QueryRouter (theEnv,
							      logicalName,
							      currentPtr) :
	  false)
	{
	  if ((ch == '\r') || (ch == '\n'))
	    {
	      if ((RouterData (theEnv)->LineCountRouter != NULL) &&
		  (strcmp (logicalName, RouterData (theEnv)->LineCountRouter)
		   == 0))
		{
		  CL_DecrementLineCount (theEnv);
		}
	    }

	  return (*currentPtr->unreadCallback) (theEnv, logicalName, ch,
						currentPtr->context);
	}

      currentPtr = currentPtr->next;
    }

   /*=====================================================*/
  /* The logical name was not recognized by any routers. */
   /*=====================================================*/

  CL_UnrecognizedRouterMessage (theEnv, logicalName);
  return -1;
}

/********************************************/
/* CL_ExitRouter: Generic exit function. Calls */
/*   all of the router exit functions.      */
/********************************************/
void
CL_ExitRouter (Environment * theEnv, int num)
{
  struct router *currentPtr, *nextPtr;

  RouterData (theEnv)->Abort = false;
  currentPtr = RouterData (theEnv)->ListOfRouters;
  while (currentPtr != NULL)
    {
      nextPtr = currentPtr->next;
      if (currentPtr->active == true)
	{
	  if (currentPtr->exitCallback != NULL)
	    {
	      (*currentPtr->exitCallback) (theEnv, num, currentPtr->context);
	    }
	}
      currentPtr = nextPtr;
    }

  if (RouterData (theEnv)->Abort)
    return;
  CL_genexit (theEnv, num);
}

/********************************************/
/* CL_AbortExit: Forces CL_ExitRouter to te_rminate */
/*   after calling all closing routers.     */
/********************************************/
void
CL_AbortExit (Environment * theEnv)
{
  RouterData (theEnv)->Abort = true;
}

/*********************************************************/
/* CL_AddRouter: Adds an I/O router to the list of routers. */
/*********************************************************/
bool
CL_AddRouter (Environment * theEnv,
	      const char *routerName,
	      int priority,
	      RouterQueryFunction * queryFunction,
	      Router_WriteFunction * writeFunction,
	      Router_ReadFunction * readFunction,
	      RouterUnreadFunction * unreadFunction,
	      RouterExitFunction * exitFunction, void *context)
{
  struct router *newPtr, *lastPtr, *currentPtr;
  char *nameCopy;

   /*==================================================*/
  /* Reject the router if the name is already in use. */
   /*==================================================*/

  for (currentPtr = RouterData (theEnv)->ListOfRouters;
       currentPtr != NULL; currentPtr = currentPtr->next)
    {
      if (strcmp (currentPtr->name, routerName) == 0)
	{
	  return false;
	}
    }

  newPtr = get_struct (theEnv, router);

  nameCopy = (char *) CL_genalloc (theEnv, strlen (routerName) + 1);
  CL_genstrcpy (nameCopy, routerName);
  newPtr->name = nameCopy;

  newPtr->active = true;
  newPtr->context = context;
  newPtr->priority = priority;
  newPtr->queryCallback = queryFunction;
  newPtr->writeCallback = writeFunction;
  newPtr->exitCallback = exitFunction;
  newPtr->readCallback = readFunction;
  newPtr->unreadCallback = unreadFunction;
  newPtr->next = NULL;

  if (RouterData (theEnv)->ListOfRouters == NULL)
    {
      RouterData (theEnv)->ListOfRouters = newPtr;
      return true;
    }

  lastPtr = NULL;
  currentPtr = RouterData (theEnv)->ListOfRouters;
  while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
    {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
    }

  if (lastPtr == NULL)
    {
      newPtr->next = RouterData (theEnv)->ListOfRouters;
      RouterData (theEnv)->ListOfRouters = newPtr;
    }
  else
    {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
    }

  return true;
}

/*****************************************************************/
/* CL_DeleteRouter: Removes an I/O router from the list of routers. */
/*****************************************************************/
bool
CL_DeleteRouter (Environment * theEnv, const char *routerName)
{
  struct router *currentPtr, *lastPtr;

  currentPtr = RouterData (theEnv)->ListOfRouters;
  lastPtr = NULL;

  while (currentPtr != NULL)
    {
      if (strcmp (currentPtr->name, routerName) == 0)
	{
	  CL_genfree (theEnv, (void *) currentPtr->name,
		      strlen (currentPtr->name) + 1);
	  if (lastPtr == NULL)
	    {
	      RouterData (theEnv)->ListOfRouters = currentPtr->next;
	      CL_rm (theEnv, currentPtr, sizeof (struct router));
	      return true;
	    }
	  lastPtr->next = currentPtr->next;
	  CL_rm (theEnv, currentPtr, sizeof (struct router));
	  return true;
	}
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
    }

  return false;
}

/*********************************************************************/
/* CL_QueryRouters: Dete_rmines if any router recognizes a logical name. */
/*********************************************************************/
bool
CL_QueryRouters (Environment * theEnv, const char *logicalName)
{
  struct router *currentPtr;

  currentPtr = RouterData (theEnv)->ListOfRouters;
  while (currentPtr != NULL)
    {
      if (QueryRouter (theEnv, logicalName, currentPtr) == true)
	return true;
      currentPtr = currentPtr->next;
    }

  return false;
}

/************************************************/
/* QueryRouter: Dete_rmines if a specific router */
/*    recognizes a logical name.                */
/************************************************/
static bool
QueryRouter (Environment * theEnv,
	     const char *logicalName, struct router *currentPtr)
{
   /*===================================================*/
  /* If the router is inactive, then it can't respond. */
   /*===================================================*/

  if (currentPtr->active == false)
    {
      return false;
    }

   /*=============================================================*/
  /* If the router has no query function, then it can't respond. */
   /*=============================================================*/

  if (currentPtr->queryCallback == NULL)
    return false;

   /*=========================================*/
  /* Call the router's query function to see */
  /* if it recognizes the logical name.      */
   /*=========================================*/

  if ((*currentPtr->queryCallback) (theEnv, logicalName,
				    currentPtr->context) == true)
    {
      return true;
    }

  return false;
}

/*******************************************************/
/* CL_DeactivateRouter: Deactivates a specific router. */
/*******************************************************/
bool
CL_DeactivateRouter (Environment * theEnv, const char *routerName)
{
  struct router *currentPtr;

  currentPtr = RouterData (theEnv)->ListOfRouters;

  while (currentPtr != NULL)
    {
      if (strcmp (currentPtr->name, routerName) == 0)
	{
	  currentPtr->active = false;
	  return true;
	}
      currentPtr = currentPtr->next;
    }

  return false;
}

/************************************************/
/* CL_ActivateRouter: Activates a specific router. */
/************************************************/
bool
CL_ActivateRouter (Environment * theEnv, const char *routerName)
{
  struct router *currentPtr;

  currentPtr = RouterData (theEnv)->ListOfRouters;

  while (currentPtr != NULL)
    {
      if (strcmp (currentPtr->name, routerName) == 0)
	{
	  currentPtr->active = true;
	  return true;
	}
      currentPtr = currentPtr->next;
    }

  return false;
}

/*****************************************/
/* CL_FindRouter: Locates the named router. */
/*****************************************/
Router *
CL_FindRouter (Environment * theEnv, const char *routerName)
{
  Router *currentPtr;

  for (currentPtr = RouterData (theEnv)->ListOfRouters;
       currentPtr != NULL; currentPtr = currentPtr->next)
    {
      if (strcmp (currentPtr->name, routerName) == 0)
	{
	  return currentPtr;
	}
    }

  return NULL;
}

/********************************************************/
/* SetFast_Load: Used to bypass router system for loads. */
/********************************************************/
void
SetFast_Load (Environment * theEnv, FILE * filePtr)
{
  RouterData (theEnv)->Fast_LoadFilePtr = filePtr;
}

/********************************************************/
/* SetFast_Save: Used to bypass router system for saves. */
/********************************************************/
void
SetFast_Save (Environment * theEnv, FILE * filePtr)
{
  RouterData (theEnv)->Fast_SaveFilePtr = filePtr;
}

/******************************************************/
/* CL_GetFast_Load: Returns the "fast load" file pointer. */
/******************************************************/
FILE *
CL_GetFast_Load (Environment * theEnv)
{
  return (RouterData (theEnv)->Fast_LoadFilePtr);
}

/******************************************************/
/* CL_GetFast_Save: Returns the "fast save" file pointer. */
/******************************************************/
FILE *
CL_GetFast_Save (Environment * theEnv)
{
  return (RouterData (theEnv)->Fast_SaveFilePtr);
}

/*****************************************************/
/* CL_UnrecognizedRouterMessage: Standard error message */
/*   for an unrecognized router name.                */
/*****************************************************/
void
CL_UnrecognizedRouterMessage (Environment * theEnv, const char *logicalName)
{
  CL_PrintErrorID (theEnv, "ROUTER", 1, false);
  CL_WriteString (theEnv, STDERR, "Logical name '");
  CL_WriteString (theEnv, STDERR, logicalName);
  CL_WriteString (theEnv, STDERR, "' was not recognized by any routers.\n");
}

/*****************************************/
/* CL_PrintNRouter: Generic print function. */
/*****************************************/
void
CL_PrintNRouter (Environment * theEnv,
		 const char *logicalName,
		 const char *str, unsigned long length)
{
  char *tempStr;

  tempStr = (char *) CL_genalloc (theEnv, length + 1);
  CL_genstrncpy (tempStr, str, length);
  tempStr[length] = 0;
  CL_WriteString (theEnv, logicalName, tempStr);
  CL_genfree (theEnv, tempStr, length + 1);
}

/*********************/
/* CL_InputBufferCount: */
/*********************/
size_t
CL_InputBufferCount (Environment * theEnv)
{
  return RouterData (theEnv)->CommandBufferInputCount;
}
