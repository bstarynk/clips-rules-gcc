   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/02/18             */
   /*                                                     */
   /*          DEFTEMPLATE BASIC COMMANDS MODULE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements core commands for the deftemplate     */
/*   construct such as clear, reset, save, undeftemplate,    */
/*   ppdeftemplate, list-deftemplates, and                   */
/*   get-deftemplate-list.                                   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove compiler warnings     */
/*            when ENVIRONMENT_API_ONLY flag is set.         */
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
/*            Removed initial-fact support.                  */
/*                                                           */
/*            Pretty print functions accept optional logical */
/*            name argument.                                 */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "constrct.h"
#include "cstrccom.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "factrhs.h"
#include "memalloc.h"
#include "multifld.h"
#include "router.h"
#include "scanner.h"
#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "tmpltbin.h"
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "tmpltcmp.h"
#endif
#include "tmpltdef.h"
#include "tmpltpsr.h"
#include "tmpltutl.h"

#include "tmpltbsc.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    CL_SaveDeftemplates(Environment *,Defmodule *,const char *,void *);

/*********************************************************************/
/* CL_DeftemplateBasicCommands: Initializes basic deftemplate commands. */
/*********************************************************************/
void CL_DeftemplateBasicCommands(
  Environment *theEnv)
  {
   CL_Add_SaveFunction(theEnv,"deftemplate",CL_SaveDeftemplates,10,NULL);

#if ! RUN_TIME
   CL_AddUDF(theEnv,"get-deftemplate-list","m",0,1,"y",CL_GetDeftemplateListFunction,"CL_GetDeftemplateListFunction",NULL);
   CL_AddUDF(theEnv,"undeftemplate","v",1,1,"y",CL_UndeftemplateCommand,"CL_UndeftemplateCommand",NULL);
   CL_AddUDF(theEnv,"deftemplate-module","y",1,1,"y",CL_DeftemplateModuleFunction,"CL_DeftemplateModuleFunction",NULL);

#if DEBUGGING_FUNCTIONS
   CL_AddUDF(theEnv,"list-deftemplates","v",0,1,"y",CL_ListDeftemplatesCommand,"CL_ListDeftemplatesCommand",NULL);
   CL_AddUDF(theEnv,"ppdeftemplate","vs",1,2,";y;ldsyn",CL_PPDeftemplateCommand,"CL_PPDeftemplateCommand",NULL);
#endif

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
   CL_DeftemplateBinarySetup(theEnv);
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
   CL_DeftemplateCompilerSetup(theEnv);
#endif

#endif
  }

/**********************************************/
/* CL_SaveDeftemplates: Deftemplate save routine */
/*   for use with the save command.           */
/**********************************************/
static void CL_SaveDeftemplates(
  Environment *theEnv,
  Defmodule *theModule,
  const char *logicalName,
  void *context)
  {
   CL_SaveConstruct(theEnv,theModule,logicalName,DeftemplateData(theEnv)->DeftemplateConstruct);
  }

/**********************************************/
/* CL_UndeftemplateCommand: H/L access routine   */
/*   for the undeftemplate command.           */
/**********************************************/
void CL_UndeftemplateCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_UndefconstructCommand(context,"undeftemplate",DeftemplateData(theEnv)->DeftemplateConstruct);
  }

/************************************/
/* CL_Undeftemplate: C access routine  */
/*   for the undeftemplate command. */
/************************************/
bool CL_Undeftemplate(
  Deftemplate *theDeftemplate,
  Environment *allEnv)
  {
   Environment *theEnv;
   
   if (theDeftemplate == NULL)
     {
      theEnv = allEnv;
      return CL_Undefconstruct(theEnv,NULL,DeftemplateData(theEnv)->DeftemplateConstruct);
     }
   else
     {
      theEnv = theDeftemplate->header.env;
      return CL_Undefconstruct(theEnv,&theDeftemplate->header,DeftemplateData(theEnv)->DeftemplateConstruct);
     }
  }

/****************************************************/
/* CL_GetDeftemplateListFunction: H/L access routine   */
/*   for the get-deftemplate-list function.         */
/****************************************************/
void CL_GetDeftemplateListFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_GetConstructListFunction(context,returnValue,DeftemplateData(theEnv)->DeftemplateConstruct);
  }

/********************************************/
/* CL_GetDeftemplateList: C access routine for */
/*   the get-deftemplate-list function.     */
/********************************************/
void CL_GetDeftemplateList(
  Environment *theEnv,
  CLIPSValue *returnValue,
  Defmodule *theModule)
  {
   UDFValue result;
   
   CL_GetConstructList(theEnv,&result,DeftemplateData(theEnv)->DeftemplateConstruct,theModule);
   CL_No_rmalizeMultifield(theEnv,&result);
   returnValue->value = result.value;
  }

/***************************************************/
/* CL_DeftemplateModuleFunction: H/L access routine   */
/*   for the deftemplate-module function.          */
/***************************************************/
void CL_DeftemplateModuleFunction(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->value = CL_GetConstructModuleCommand(context,"deftemplate-module",DeftemplateData(theEnv)->DeftemplateConstruct);
  }

#if DEBUGGING_FUNCTIONS

/**********************************************/
/* CL_PPDeftemplateCommand: H/L access routine   */
/*   for the ppdeftemplate command.           */
/**********************************************/
void CL_PPDeftemplateCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_PPConstructCommand(context,"ppdeftemplate",DeftemplateData(theEnv)->DeftemplateConstruct,returnValue);
  }

/***************************************/
/* CL_PPDeftemplate: C access routine for */
/*   the ppdeftemplate command.        */
/***************************************/
bool CL_PPDeftemplate(
  Environment *theEnv,
  const char *deftemplateName,
  const char *logicalName)
  {
   return(CL_PPConstruct(theEnv,deftemplateName,logicalName,DeftemplateData(theEnv)->DeftemplateConstruct));
  }

/*************************************************/
/* CL_ListDeftemplatesCommand: H/L access routine   */
/*   for the list-deftemplates command.          */
/*************************************************/
void CL_ListDeftemplatesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_ListConstructCommand(context,DeftemplateData(theEnv)->DeftemplateConstruct);
  }

/****************************************/
/* CL_ListDeftemplates: C access routine   */
/*   for the list-deftemplates command. */
/****************************************/
void CL_ListDeftemplates(
  Environment *theEnv,
  const char *logicalName,
  Defmodule *theModule)
  {
   CL_ListConstruct(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,logicalName,theModule);
  }

/********************************************************/
/* CL_DeftemplateGet_Watch: C access routine for retrieving */
/*   the current watch value of a deftemplate.          */
/********************************************************/
bool CL_DeftemplateGet_Watch(
  Deftemplate *theTemplate)
  {
   return theTemplate->watch;
  }

/******************************************************/
/* CL_DeftemplateSet_Watch:  C access routine for setting */
/*   the current watch value of a deftemplate.        */
/******************************************************/
void CL_DeftemplateSet_Watch(
  Deftemplate *theTemplate,
  bool newState)
  {
   theTemplate->watch = newState;
  }

/**********************************************************/
/* CL_Deftemplate_WatchAccess: Access routine for setting the */
/*   watch flag of a deftemplate via the watch command.   */
/**********************************************************/
bool CL_Deftemplate_WatchAccess(
  Environment *theEnv,
  int code,
  bool newState,
  Expression *argExprs)
  {
#if MAC_XCD
#pragma unused(code)
#endif

   return CL_ConstructSet_WatchAccess(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,newState,argExprs,
                                  (ConstructGet_WatchFunction *) CL_DeftemplateGet_Watch,
                                  (ConstructSet_WatchFunction *) CL_DeftemplateSet_Watch);
  }

/*************************************************************************/
/* CL_Deftemplate_WatchPrint: Access routine for printing which deftemplates */
/*   have their watch flag set via the list-watch-items command.         */
/*************************************************************************/
bool CL_Deftemplate_WatchPrint(
  Environment *theEnv,
  const char *logName,
  int code,
  Expression *argExprs)
  {
#if MAC_XCD
#pragma unused(code)
#endif

   return CL_ConstructPrint_WatchAccess(theEnv,DeftemplateData(theEnv)->DeftemplateConstruct,logName,argExprs,
                                    (ConstructGet_WatchFunction *) CL_DeftemplateGet_Watch,
                                    (ConstructSet_WatchFunction *) CL_DeftemplateSet_Watch);
  }

#endif /* DEBUGGING_FUNCTIONS */

#endif /* DEFTEMPLATE_CONSTRUCT */


