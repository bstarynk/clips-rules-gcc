   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/01/16            */
   /*                                                     */
   /*                 BLOAD HEADER FILE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
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
/*      6.40: Removed LOCALE definition.                     */
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
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_bload

#pragma once

#define _H_bload

#include "utility.h"
#include "extnfunc.h"
#include "exprnbin.h"
#include "symbol.h"
#include "sysdep.h"
#include "symblbin.h"

#define BLOAD_DATA 38

struct bloadData
  {
   const char *BinaryPrefixID;
   const char *BinaryVersionID;
   char *BinarySizes;
   struct functionDefinition **FunctionArray;
   bool CL_BloadActive;
   struct void_CallFunctionItem *Before_BloadFunctions;
   struct void_CallFunctionItem *After_BloadFunctions;
   struct bool_CallFunctionItem *CL_Clear_BloadReadyFunctions;
   struct void_CallFunctionItem *Abort_BloadFunctions;
  };

#define CL_BloadData(theEnv) ((struct bloadData *) GetEnvironmentData(theEnv,BLOAD_DATA))

#define FunctionPointer(i) ((((i) == ULONG_MAX) ? NULL : CL_BloadData(theEnv)->FunctionArray[i]))

   void                    Initialize_BloadData(Environment *);
   void                    CL_BloadCommand(Environment *,UDFContext *,UDFValue *);
   bool                    CL_Bload(Environment *,const char *);
   void                    CL_Bloadand_Refresh(Environment *,unsigned long,size_t,void (*)(Environment *,void *,unsigned long));
   bool                    CL_Bloaded(Environment *);
   void                    CL_AddBefore_BloadFunction(Environment *,const char *,Void_CallFunction *,int,void *);
   void                    CL_AddAfter_BloadFunction(Environment *,const char *,Void_CallFunction *,int,void *);
   void                    CL_Add_Clear_BloadReadyFunction(Environment *,const char *,Bool_CallFunction *,int,void *);
   void                    CL_AddAbort_BloadFunction(Environment *,const char *,Void_CallFunction *,int,void *);
   void                    Cannot_LoadWith_BloadMessage(Environment *,const char *);

#endif

