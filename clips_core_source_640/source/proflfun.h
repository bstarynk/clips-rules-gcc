   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*      CONSTRUCT PROFILING FUNCTIONS HEADER FILE      */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Modified OutputCL_ProfileInfo to allow a before   */
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
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_proflfun

#pragma once

#define _H_proflfun

#include "userdata.h"

struct constructCL_ProfileInfo
  {
   struct userData usrData;
   long numberOfEntries;
   unsigned int childCall : 1;
   double startTime;
   double totalSelfTime;
   double totalWithChildrenTime;
  };

struct profileFrameInfo
  {
   unsigned int parentCall : 1;
   unsigned int profileOnExit : 1;
   double parentStartTime;
   struct constructCL_ProfileInfo *oldCL_ProfileFrame;
  };

#define PROFLFUN_DATA 15

struct profileFunctionData
  {
   double CL_ProfileStartTime;
   double CL_ProfileEndTime;
   double CL_ProfileTotalTime;
   int LastCL_ProfileInfo;
   double PercentThreshold;
   struct userDataRecord CL_ProfileDataInfo;
   unsigned char CL_ProfileDataID;
   bool CL_ProfileCL_UserFunctions;
   bool CL_ProfileConstructs;
   struct constructCL_ProfileInfo *ActiveCL_ProfileFrame;
   const char *OutputString;
  };

#define CL_ProfileFunctionData(theEnv) ((struct profileFunctionData *) GetEnvironmentData(theEnv,PROFLFUN_DATA))

   void                           CL_ConstructProfilingFunctionDefinitions(Environment *);
   void                           CL_ProfileCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_ProfileInfoCommand(Environment *,UDFContext *,UDFValue *);
   void                           StartCL_Profile(Environment *,struct profileFrameInfo *,
                                               struct userData **,bool);
   void                           CL_EndCL_Profile(Environment *,struct profileFrameInfo *);
   void                           CL_ProfileCL_ResetCommand(Environment *,UDFContext *,UDFValue *);
   void                           CL_ResetCL_ProfileInfo(struct constructCL_ProfileInfo *);

   void                           SetCL_ProfilePercentThresholdCommand(Environment *,UDFContext *,UDFValue *);
   double                         SetCL_ProfilePercentThreshold(Environment *,double);
   void                           CL_GetCL_ProfilePercentThresholdCommand(Environment *,UDFContext *,UDFValue *);
   double                         CL_GetCL_ProfilePercentThreshold(Environment *);
   bool                           CL_Profile(Environment *,const char *);
   void                           CL_DeleteCL_ProfileData(Environment *,void *);
   void                          *CL_CreateCL_ProfileData(Environment *);
   const char                    *SetCL_ProfileOutputString(Environment *,const char *);

#endif /* _H_proflfun */


