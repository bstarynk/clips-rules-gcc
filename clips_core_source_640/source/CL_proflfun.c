   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*         CONSTRUCT PROFILING FUNCTIONS MODULE        */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for profiling the amount of    */
/*   time spent in constructs and user defined functions.    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Modified Output_ProfileInfo to allow a before   */
/*            and after prefix so that a string buffer does  */
/*            not need to be created to contain the entire   */
/*            prefix. This allows a buffer overflow problem  */
/*            to be corrected. DR0857.                       */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added pragmas to remove compilation warnings.  */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warnings.                             */
/*                                                           */
/*      6.30: Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_TBC).         */
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
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if PROFILING_FUNCTIONS

#include "argacces.h"
#include "classcom.h"
#include "dffnxfun.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "genrccom.h"
#include "genrcfun.h"
#include "memalloc.h"
#include "msgcom.h"
#include "router.h"
#include "sysdep.h"

#include "proflfun.h"

#include <string.h>

#define NO_PROFILE      0
#define USER_FUNCTIONS  1
#define CONSTRUCTS_CODE 2

#define OUTPUT_STRING "%-40s %7ld %15.6f  %8.2f%%  %15.6f  %8.2f%%\n"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                        Output_ProfileInfo(Environment *,const char *,struct construct_ProfileInfo *,
                                                        const char *,const char *,const char *,const char **);
   static void                        Output_UserFunctionsInfo(Environment *);
   static void                        OutputConstructsCodeInfo(Environment *);
#if (! RUN_TIME)
   static void                        CL_Profile_ClearFunction(Environment *,void *);
#endif

/******************************************************/
/* CL_ConstructProfilingFunctionDefinitions: Initializes */
/*   the construct profiling functions.               */
/******************************************************/
void CL_ConstructProfilingFunctionDefinitions(
  Environment *theEnv)
  {
   struct userDataRecord profileDataInfo = { 0, CL_Create_ProfileData, CL_Delete_ProfileData };

   CL_AllocateEnvironmentData(theEnv,PROFLFUN_DATA,sizeof(struct profileFunctionData),NULL);

   memcpy(&CL_ProfileFunctionData(theEnv)->CL_ProfileDataInfo,&profileDataInfo,sizeof(struct userDataRecord));

   CL_ProfileFunctionData(theEnv)->Last_ProfileInfo = NO_PROFILE;
   CL_ProfileFunctionData(theEnv)->PercentThreshold = 0.0;
   CL_ProfileFunctionData(theEnv)->OutputString = OUTPUT_STRING;

#if ! RUN_TIME
   CL_AddUDF(theEnv,"profile","v",1,1,"y",CL_ProfileCommand,"CL_ProfileCommand",NULL);
   CL_AddUDF(theEnv,"profile-info","v",0,0,NULL, CL_ProfileInfoCommand,"CL_ProfileInfoCommand",NULL);
   CL_AddUDF(theEnv,"profile-reset","v",0,0,NULL,CL_Profile_ResetCommand,"CL_Profile_ResetCommand",NULL);

   CL_AddUDF(theEnv,"set-profile-percent-threshold","d",1,1,"ld",Set_ProfilePercentThresholdCommand,"Set_ProfilePercentThresholdCommand",NULL);
   CL_AddUDF(theEnv,"get-profile-percent-threshold","d",0,0,NULL,CL_Get_ProfilePercentThresholdCommand,"CL_Get_ProfilePercentThresholdCommand",NULL);

   CL_ProfileFunctionData(theEnv)->CL_ProfileDataID = CL_InstallUserDataRecord(theEnv,&CL_ProfileFunctionData(theEnv)->CL_ProfileDataInfo);

   CL_Add_ClearFunction(theEnv,"profile",CL_Profile_ClearFunction,0,NULL);
#endif
  }

/**********************************/
/* CL_Create_ProfileData: Allocates a */
/*   profile user data structure. */
/**********************************/
void *CL_Create_ProfileData(
  Environment *theEnv)
  {
   struct construct_ProfileInfo *theInfo;

   theInfo = (struct construct_ProfileInfo *)
             CL_genalloc(theEnv,sizeof(struct construct_ProfileInfo));

   theInfo->numberOfEntries = 0;
   theInfo->childCall = false;
   theInfo->startTime = 0.0;
   theInfo->totalSelfTime = 0.0;
   theInfo->totalWithChildrenTime = 0.0;

   return(theInfo);
  }

/**************************************/
/* CL_Delete_ProfileData:          */
/**************************************/
void CL_Delete_ProfileData(
  Environment *theEnv,
  void *theData)
  {
   CL_genfree(theEnv,theData,sizeof(struct construct_ProfileInfo));
  }

/**************************************/
/* CL_ProfileCommand: H/L access routine */
/*   for the profile command.         */
/**************************************/
void CL_ProfileCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   const char *argument;
   UDFValue theValue;

   if (! CL_UDFFirstArgument(context,SYMBOL_BIT,&theValue)) return;
   argument = theValue.lexemeValue->contents;

   if (! CL_Profile(theEnv,argument))
     {
      CL_UDFInvalidArgumentMessage(context,"symbol with value constructs, user-functions, or off");
      return;
     }

   return;
  }

/******************************/
/* CL_Profile: C access routine  */
/*   for the profile command. */
/******************************/
bool CL_Profile(
  Environment *theEnv,
  const char *argument)
  {
   /*======================================================*/
   /* If the argument is the symbol "user-functions", then */
   /* user-defined functions should be profiled. If the    */
   /* argument is the symbol "constructs", then            */
   /* deffunctions, generic functions, message-handlers,   */
   /* and rule RHS actions are profiled.                   */
   /*======================================================*/

   if (strcmp(argument,"user-functions") == 0)
     {
      CL_ProfileFunctionData(theEnv)->CL_ProfileStartTime = CL_gentime();
      CL_ProfileFunctionData(theEnv)->CL_Profile_UserFunctions = true;
      CL_ProfileFunctionData(theEnv)->CL_ProfileConstructs = false;
      CL_ProfileFunctionData(theEnv)->Last_ProfileInfo = USER_FUNCTIONS;
     }

   else if (strcmp(argument,"constructs") == 0)
     {
      CL_ProfileFunctionData(theEnv)->CL_ProfileStartTime = CL_gentime();
      CL_ProfileFunctionData(theEnv)->CL_Profile_UserFunctions = false;
      CL_ProfileFunctionData(theEnv)->CL_ProfileConstructs = true;
      CL_ProfileFunctionData(theEnv)->Last_ProfileInfo = CONSTRUCTS_CODE;
     }

   /*======================================================*/
   /* Otherwise, if the argument is the symbol "off", then */
   /* don't profile constructs and user-defined functions. */
   /*======================================================*/

   else if (strcmp(argument,"off") == 0)
     {
      CL_ProfileFunctionData(theEnv)->CL_ProfileEndTime = CL_gentime();
      CL_ProfileFunctionData(theEnv)->CL_ProfileTotalTime += (CL_ProfileFunctionData(theEnv)->CL_ProfileEndTime - CL_ProfileFunctionData(theEnv)->CL_ProfileStartTime);
      CL_ProfileFunctionData(theEnv)->CL_Profile_UserFunctions = false;
      CL_ProfileFunctionData(theEnv)->CL_ProfileConstructs = false;
     }

   /*=====================================================*/
   /* Otherwise, generate an error since the only allowed */
   /* arguments are "on" or "off."                        */
   /*=====================================================*/

   else
     { return false; }

   return true;
  }

/******************************************/
/* CL_ProfileInfoCommand: H/L access routine */
/*   for the profile-info command.        */
/******************************************/
void CL_ProfileInfoCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   char buffer[512];

   /*==================================*/
   /* If code is still being profiled, */
   /* update the profile end time.     */
   /*==================================*/

   if (CL_ProfileFunctionData(theEnv)->CL_Profile_UserFunctions || CL_ProfileFunctionData(theEnv)->CL_ProfileConstructs)
     {
      CL_ProfileFunctionData(theEnv)->CL_ProfileEndTime = CL_gentime();
      CL_ProfileFunctionData(theEnv)->CL_ProfileTotalTime += (CL_ProfileFunctionData(theEnv)->CL_ProfileEndTime - CL_ProfileFunctionData(theEnv)->CL_ProfileStartTime);
     }

   /*==================================*/
   /* Print the profiling info_rmation. */
   /*==================================*/

   if (CL_ProfileFunctionData(theEnv)->Last_ProfileInfo != NO_PROFILE)
     {
      CL_gensprintf(buffer,"CL_Profile elapsed time = %g seconds\n",
                      CL_ProfileFunctionData(theEnv)->CL_ProfileTotalTime);
      CL_WriteString(theEnv,STDOUT,buffer);

      if (CL_ProfileFunctionData(theEnv)->Last_ProfileInfo == USER_FUNCTIONS)
        { CL_WriteString(theEnv,STDOUT,"Function Name                            "); }
      else if (CL_ProfileFunctionData(theEnv)->Last_ProfileInfo == CONSTRUCTS_CODE)
        { CL_WriteString(theEnv,STDOUT,"Construct Name                           "); }

      CL_WriteString(theEnv,STDOUT,"Entries         Time           %          Time+Kids     %+Kids\n");

      if (CL_ProfileFunctionData(theEnv)->Last_ProfileInfo == USER_FUNCTIONS)
        { CL_WriteString(theEnv,STDOUT,"-------------                            "); }
      else if (CL_ProfileFunctionData(theEnv)->Last_ProfileInfo == CONSTRUCTS_CODE)
        { CL_WriteString(theEnv,STDOUT,"--------------                           "); }

      CL_WriteString(theEnv,STDOUT,"-------        ------        -----        ---------     ------\n");
     }

   if (CL_ProfileFunctionData(theEnv)->Last_ProfileInfo == USER_FUNCTIONS) Output_UserFunctionsInfo(theEnv);
   if (CL_ProfileFunctionData(theEnv)->Last_ProfileInfo == CONSTRUCTS_CODE) OutputConstructsCodeInfo(theEnv);
  }

/**********************************************/
/* Start_Profile: Initiates bookkeeping needed */
/*   to profile a construct or function.      */
/**********************************************/
void Start_Profile(
  Environment *theEnv,
  struct profileFrameInfo *theFrame,
  struct userData **theList,
  bool checkFlag)
  {
   double startTime, addTime;
   struct construct_ProfileInfo *profileInfo;

   if (! checkFlag)
     {
      theFrame->profileOnExit = false;
      return;
     }

   profileInfo = (struct construct_ProfileInfo *) CL_FetchUserData(theEnv,CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theList);

   theFrame->profileOnExit = true;
   theFrame->parentCall = false;

   startTime = CL_gentime();
   theFrame->old_ProfileFrame = CL_ProfileFunctionData(theEnv)->Active_ProfileFrame;

   if (CL_ProfileFunctionData(theEnv)->Active_ProfileFrame != NULL)
     {
      addTime = startTime - CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->startTime;
      CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->totalSelfTime += addTime;
     }

   CL_ProfileFunctionData(theEnv)->Active_ProfileFrame = profileInfo;

   CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->numberOfEntries++;
   CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->startTime = startTime;

   if (! CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->childCall)
     {
      theFrame->parentCall = true;
      theFrame->parentStartTime = startTime;
      CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->childCall = true;
     }
  }

/*******************************************/
/* CL_End_Profile: Finishes bookkeeping needed */
/*   to profile a construct or function.   */
/*******************************************/
void CL_End_Profile(
  Environment *theEnv,
  struct profileFrameInfo *theFrame)
  {
   double endTime, addTime;

   if (! theFrame->profileOnExit) return;

   endTime = CL_gentime();

   if (theFrame->parentCall)
     {
      addTime = endTime - theFrame->parentStartTime;
      CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->totalWithChildrenTime += addTime;
      CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->childCall = false;
     }

   CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->totalSelfTime += (endTime - CL_ProfileFunctionData(theEnv)->Active_ProfileFrame->startTime);

   if (theFrame->old_ProfileFrame != NULL)
     { theFrame->old_ProfileFrame->startTime = endTime; }

   CL_ProfileFunctionData(theEnv)->Active_ProfileFrame = theFrame->old_ProfileFrame;
  }

/******************************************/
/* Output_ProfileInfo: Prints out a single */
/*   line of profile info_rmation.         */
/******************************************/
static bool Output_ProfileInfo(
  Environment *theEnv,
  const char *itemName,
  struct construct_ProfileInfo *profileInfo,
  const char *printPrefixBefore,
  const char *printPrefix,
  const char *printPrefixAfter,
  const char **banner)
  {
   double percent = 0.0, percentWithKids = 0.0;
   char buffer[512];

   if (profileInfo == NULL) return false;

   if (profileInfo->numberOfEntries == 0) return false;

   if (CL_ProfileFunctionData(theEnv)->CL_ProfileTotalTime != 0.0)
     {
      percent = (profileInfo->totalSelfTime * 100.0) / CL_ProfileFunctionData(theEnv)->CL_ProfileTotalTime;
      if (percent < 0.005) percent = 0.0;
      percentWithKids = (profileInfo->totalWithChildrenTime * 100.0) / CL_ProfileFunctionData(theEnv)->CL_ProfileTotalTime;
      if (percentWithKids < 0.005) percentWithKids = 0.0;
     }

   if (percent < CL_ProfileFunctionData(theEnv)->PercentThreshold) return false;

   if ((banner != NULL) && (*banner != NULL))
     {
      CL_WriteString(theEnv,STDOUT,*banner);
      *banner = NULL;
     }

   if (printPrefixBefore != NULL)
     { CL_WriteString(theEnv,STDOUT,printPrefixBefore); }

   if (printPrefix != NULL)
     { CL_WriteString(theEnv,STDOUT,printPrefix); }

   if (printPrefixAfter != NULL)
     { CL_WriteString(theEnv,STDOUT,printPrefixAfter); }

   if (strlen(itemName) >= 40)
     {
      CL_WriteString(theEnv,STDOUT,itemName);
      CL_WriteString(theEnv,STDOUT,"\n");
      itemName = "";
     }

   CL_gensprintf(buffer,CL_ProfileFunctionData(theEnv)->OutputString,
                        itemName,
                        (long) profileInfo->numberOfEntries,

                        (double) profileInfo->totalSelfTime,
                        (double) percent,

                        (double) profileInfo->totalWithChildrenTime,
                        (double) percentWithKids);
   CL_WriteString(theEnv,STDOUT,buffer);

   return true;
  }

/*******************************************/
/* CL_Profile_ResetCommand: H/L access routine */
/*   for the profile-reset command.        */
/*******************************************/
void CL_Profile_ResetCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   struct functionDefinition *theFunction;
   int i;
#if DEFFUNCTION_CONSTRUCT
   Deffunction *theDeffunction;
#endif
#if DEFRULE_CONSTRUCT
   Defrule *theDefrule;
#endif
#if DEFGENERIC_CONSTRUCT
   Defgeneric *theDefgeneric;
   unsigned short methodIndex;
   Defmethod *theMethod;
#endif
#if OBJECT_SYSTEM
   Defclass *theDefclass;
   DefmessageHandler *theHandler;
   unsigned handlerIndex;
#endif

   CL_ProfileFunctionData(theEnv)->CL_ProfileStartTime = 0.0;
   CL_ProfileFunctionData(theEnv)->CL_ProfileEndTime = 0.0;
   CL_ProfileFunctionData(theEnv)->CL_ProfileTotalTime = 0.0;
   CL_ProfileFunctionData(theEnv)->Last_ProfileInfo = NO_PROFILE;

   for (theFunction = CL_GetFunctionList(theEnv);
        theFunction != NULL;
        theFunction = theFunction->next)
     {
      CL_Reset_ProfileInfo((struct construct_ProfileInfo *)
                       CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theFunction->usrData));
     }

   for (i = 0; i < MAXIMUM_PRIMITIVES; i++)
     {
      if (CL_EvaluationData(theEnv)->PrimitivesArray[i] != NULL)
        {
         CL_Reset_ProfileInfo((struct construct_ProfileInfo *)
                          CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,CL_EvaluationData(theEnv)->PrimitivesArray[i]->usrData));
        }
     }

#if DEFFUNCTION_CONSTRUCT
   for (theDeffunction = CL_GetNextDeffunction(theEnv,NULL);
        theDeffunction != NULL;
        theDeffunction = CL_GetNextDeffunction(theEnv,theDeffunction))
     {
      CL_Reset_ProfileInfo((struct construct_ProfileInfo *)
                       CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theDeffunction->header.usrData));
     }
#endif

#if DEFRULE_CONSTRUCT
   for (theDefrule = CL_GetNextDefrule(theEnv,NULL);
        theDefrule != NULL;
        theDefrule = CL_GetNextDefrule(theEnv,theDefrule))
     {
      CL_Reset_ProfileInfo((struct construct_ProfileInfo *)
                       CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theDefrule->header.usrData));
     }
#endif

#if DEFGENERIC_CONSTRUCT
   for (theDefgeneric = CL_GetNextDefgeneric(theEnv,NULL);
        theDefgeneric != NULL;
        theDefgeneric = CL_GetNextDefgeneric(theEnv,theDefgeneric))
     {
      CL_Reset_ProfileInfo((struct construct_ProfileInfo *)
                       CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theDefgeneric->header.usrData));

      for (methodIndex = CL_GetNextDefmethod(theDefgeneric,0);
           methodIndex != 0;
           methodIndex = CL_GetNextDefmethod(theDefgeneric,methodIndex))
        {
         theMethod = CL_GetDefmethodPointer(theDefgeneric,methodIndex);
         CL_Reset_ProfileInfo((struct construct_ProfileInfo *)
                          CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theMethod->header.usrData));
        }
     }
#endif

#if OBJECT_SYSTEM
   for (theDefclass = CL_GetNextDefclass(theEnv,NULL);
        theDefclass != NULL;
        theDefclass = CL_GetNextDefclass(theEnv,theDefclass))
     {
      CL_Reset_ProfileInfo((struct construct_ProfileInfo *)
                       CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theDefclass->header.usrData));
      for (handlerIndex = CL_GetNextDefmessageHandler(theDefclass,0);
           handlerIndex != 0;
           handlerIndex = CL_GetNextDefmessageHandler(theDefclass,handlerIndex))
        {
         theHandler = CL_GetDefmessageHandlerPointer(theDefclass,handlerIndex);
         CL_Reset_ProfileInfo((struct construct_ProfileInfo *)
                          CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theHandler->header.usrData));
        }
     }
#endif

  }

/*************************************************/
/* CL_Reset_ProfileInfo: Sets the initial values for */
/*   a construct_ProfileInfo data structure.      */
/*************************************************/
void CL_Reset_ProfileInfo(
  struct construct_ProfileInfo *profileInfo)
  {
   if (profileInfo == NULL) return;

   profileInfo->numberOfEntries = 0;
   profileInfo->childCall = false;
   profileInfo->startTime = 0.0;
   profileInfo->totalSelfTime = 0.0;
   profileInfo->totalWithChildrenTime = 0.0;
  }

/****************************/
/* Output_UserFunctionsInfo: */
/****************************/
static void Output_UserFunctionsInfo(
  Environment *theEnv)
  {
   struct functionDefinition *theFunction;
   int i;

   for (theFunction = CL_GetFunctionList(theEnv);
        theFunction != NULL;
        theFunction = theFunction->next)
     {
      Output_ProfileInfo(theEnv,theFunction->callFunctionName->contents,
                        (struct construct_ProfileInfo *)
                           CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,
                        theFunction->usrData),
                        NULL,NULL,NULL,NULL);
     }

   for (i = 0; i < MAXIMUM_PRIMITIVES; i++)
     {
      if (CL_EvaluationData(theEnv)->PrimitivesArray[i] != NULL)
        {
         Output_ProfileInfo(theEnv,CL_EvaluationData(theEnv)->PrimitivesArray[i]->name,
                           (struct construct_ProfileInfo *)
                              CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,
                           CL_EvaluationData(theEnv)->PrimitivesArray[i]->usrData),
                           NULL,NULL,NULL,NULL);
        }
     }
  }

/*****************************/
/* OutputConstructsCodeInfo: */
/*****************************/
static void OutputConstructsCodeInfo(
  Environment *theEnv)
  {
#if (! DEFFUNCTION_CONSTRUCT) && (! DEFGENERIC_CONSTRUCT) && (! OBJECT_SYSTEM) && (! DEFRULE_CONSTRUCT)
#pragma unused(theEnv)
#endif
#if DEFFUNCTION_CONSTRUCT
   Deffunction *theDeffunction;
#endif
#if DEFRULE_CONSTRUCT
   Defrule *theDefrule;
#endif
#if DEFGENERIC_CONSTRUCT
   Defgeneric *theDefgeneric;
   Defmethod *theMethod;
   unsigned short methodIndex;
   String_Builder *theSB;
#endif
#if OBJECT_SYSTEM
   Defclass *theDefclass;
   DefmessageHandler *theHandler;
   unsigned handlerIndex;
#endif
#if DEFGENERIC_CONSTRUCT || OBJECT_SYSTEM
   const char *prefix, *prefixBefore, *prefixAfter;
#endif
   const char *banner;

   banner = "\n*** Deffunctions ***\n\n";

#if DEFFUNCTION_CONSTRUCT
   for (theDeffunction = CL_GetNextDeffunction(theEnv,NULL);
        theDeffunction != NULL;
        theDeffunction = CL_GetNextDeffunction(theEnv,theDeffunction))
     {
      Output_ProfileInfo(theEnv,CL_DeffunctionName(theDeffunction),
                        (struct construct_ProfileInfo *)
                          CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theDeffunction->header.usrData),
                        NULL,NULL,NULL,&banner);
     }
#endif

   banner = "\n*** Defgenerics ***\n";
#if DEFGENERIC_CONSTRUCT
   theSB = CL_CreateString_Builder(theEnv,512);
   for (theDefgeneric = CL_GetNextDefgeneric(theEnv,NULL);
        theDefgeneric != NULL;
        theDefgeneric = CL_GetNextDefgeneric(theEnv,theDefgeneric))
     {
      prefixBefore = "\n";
      prefix = CL_DefgenericName(theDefgeneric);
      prefixAfter = "\n";

      for (methodIndex = CL_GetNextDefmethod(theDefgeneric,0);
           methodIndex != 0;
           methodIndex = CL_GetNextDefmethod(theDefgeneric,methodIndex))
        {
         theMethod = CL_GetDefmethodPointer(theDefgeneric,methodIndex);

         CL_DefmethodDescription(theDefgeneric,methodIndex,theSB);
         if (Output_ProfileInfo(theEnv,theSB->contents,
                               (struct construct_ProfileInfo *)
                                  CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theMethod->header.usrData),
                               prefixBefore,prefix,prefixAfter,&banner))
           {
            prefixBefore = NULL;
            prefix = NULL;
            prefixAfter = NULL;
           }
        }
     }
   CL_SBDispose(theSB);
#endif

   banner = "\n*** Defclasses ***\n";
#if OBJECT_SYSTEM
   for (theDefclass = CL_GetNextDefclass(theEnv,NULL);
        theDefclass != NULL;
        theDefclass = CL_GetNextDefclass(theEnv,theDefclass))
     {
      prefixAfter = "\n";
      prefix = CL_DefclassName(theDefclass);
      prefixBefore = "\n";

      for (handlerIndex = CL_GetNextDefmessageHandler(theDefclass,0);
           handlerIndex != 0;
           handlerIndex = CL_GetNextDefmessageHandler(theDefclass,handlerIndex))
        {
         theHandler = CL_GetDefmessageHandlerPointer(theDefclass,handlerIndex);
         if (Output_ProfileInfo(theEnv,CL_DefmessageHandlerName(theDefclass,handlerIndex),
                               (struct construct_ProfileInfo *)
                                  CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,
                               theHandler->header.usrData),
                               prefixBefore,prefix,prefixAfter,&banner))
           {
            prefixBefore = NULL;
            prefix = NULL;
            prefixAfter = NULL;
           }
        }

     }
#endif

   banner = "\n*** Defrules ***\n\n";

#if DEFRULE_CONSTRUCT
   for (theDefrule = CL_GetNextDefrule(theEnv,NULL);
        theDefrule != NULL;
        theDefrule = CL_GetNextDefrule(theEnv,theDefrule))
     {
      Output_ProfileInfo(theEnv,CL_DefruleName(theDefrule),
                        (struct construct_ProfileInfo *)
                          CL_TestUserData(CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theDefrule->header.usrData),
                        NULL,NULL,NULL,&banner);
     }
#endif

  }

/*********************************************************/
/* Set_ProfilePercentThresholdCommand: H/L access routine */
/*   for the set-profile-percent-threshold command.      */
/*********************************************************/
void Set_ProfilePercentThresholdCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   UDFValue theValue;
   double newThreshold;

   if (! CL_UDFFirstArgument(context,NUMBER_BITS,&theValue))
     { return; }

   newThreshold = CVCoerceToFloat(&theValue);

   if ((newThreshold < 0.0) || (newThreshold > 100.0))
     {
      CL_UDFInvalidArgumentMessage(context,"number in the range 0 to 100");
      returnValue->floatValue = CL_CreateFloat(theEnv,-1.0);
     }
   else
     { returnValue->floatValue = CL_CreateFloat(theEnv,Set_ProfilePercentThreshold(theEnv,newThreshold)); }
  }

/****************************************************/
/* Set_ProfilePercentThreshold: C access routine for */
/*   the set-profile-percent-threshold command.     */
/****************************************************/
double Set_ProfilePercentThreshold(
  Environment *theEnv,
  double value)
  {
   double oldPercentThreshhold;

   if ((value < 0.0) || (value > 100.0))
     { return(-1.0); }

   oldPercentThreshhold = CL_ProfileFunctionData(theEnv)->PercentThreshold;

   CL_ProfileFunctionData(theEnv)->PercentThreshold = value;

   return(oldPercentThreshhold);
  }

/*********************************************************/
/* CL_Get_ProfilePercentThresholdCommand: H/L access routine */
/*   for the get-profile-percent-threshold command.      */
/*********************************************************/
void CL_Get_ProfilePercentThresholdCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   returnValue->floatValue = CL_CreateFloat(theEnv,CL_ProfileFunctionData(theEnv)->PercentThreshold);
  }

/****************************************************/
/* CL_Get_ProfilePercentThreshold: C access routine for */
/*   the get-profile-percent-threshold command.     */
/****************************************************/
double CL_Get_ProfilePercentThreshold(
  Environment *theEnv)
  {
   return(CL_ProfileFunctionData(theEnv)->PercentThreshold);
  }

/**********************************************************/
/* Set_ProfileOutputString: Sets the output string global. */
/**********************************************************/
const char *Set_ProfileOutputString(
  Environment *theEnv,
  const char *value)
  {
   const char *oldOutputString;

   if (value == NULL)
     { return(CL_ProfileFunctionData(theEnv)->OutputString); }

   oldOutputString = CL_ProfileFunctionData(theEnv)->OutputString;

   CL_ProfileFunctionData(theEnv)->OutputString = value;

   return(oldOutputString);
  }

#if (! RUN_TIME)
/******************************************************************/
/* CL_Profile_ClearFunction: Profiling clear routine for use with the */
/*   clear command. Removes user data attached to user functions. */
/******************************************************************/
static void CL_Profile_ClearFunction(
  Environment *theEnv,
  void *context)
  {
   struct functionDefinition *theFunction;
   int i;

   for (theFunction = CL_GetFunctionList(theEnv);
        theFunction != NULL;
        theFunction = theFunction->next)
     {
      theFunction->usrData =
        CL_DeleteUserData(theEnv,CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,theFunction->usrData);
     }

   for (i = 0; i < MAXIMUM_PRIMITIVES; i++)
     {
      if (CL_EvaluationData(theEnv)->PrimitivesArray[i] != NULL)
        {
         CL_EvaluationData(theEnv)->PrimitivesArray[i]->usrData =
           CL_DeleteUserData(theEnv,CL_ProfileFunctionData(theEnv)->CL_ProfileDataID,CL_EvaluationData(theEnv)->PrimitivesArray[i]->usrData);
        }
     }
  }
#endif

#endif /* PROFILING_FUNCTIONS */

