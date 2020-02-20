   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/15/17            */
   /*                                                     */
   /*                 UTILITY HEADER FILE                 */
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
/*            Support for typed EXTERNAL_ADDRESS_TYPE.       */
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

#ifndef _H_utility

#pragma once

#define _H_utility

#include <stdlib.h>

typedef struct callFunctionItem CL_CallFunctionItem;
typedef struct callFunctionItemWithArg CL_CallFunctionItemWithArg;
typedef struct boolCL_CallFunctionItem BoolCL_CallFunctionItem;
typedef struct voidCL_CallFunctionItem VoidCL_CallFunctionItem;

#include "evaluatn.h"
#include "moduldef.h"

typedef struct gcBlock GCBlock;
typedef struct stringCL_Builder StringCL_Builder;

struct voidCL_CallFunctionItem
  {
   const char *name;
   VoidCL_CallFunction *func;
   int priority;
   struct voidCL_CallFunctionItem *next;
   void *context;
  };

struct boolCL_CallFunctionItem
  {
   const char *name;
   BoolCL_CallFunction *func;
   int priority;
   struct boolCL_CallFunctionItem *next;
   void *context;
  };

struct callFunctionItemWithArg
  {
   const char *name;
   VoidCL_CallFunctionWithArg *func;
   int priority;
   struct callFunctionItemWithArg *next;
   void *context;
  };

struct trackedMemory
  {
   void *theMemory;
   struct trackedMemory *next;
   struct trackedMemory *prev;
   size_t memSize;
  };

struct garbageFrame
  {
   bool dirty;
   struct garbageFrame *priorFrame;
   struct ephemeron *ephemeralSymbolList;
   struct ephemeron *ephemeralFloatList;
   struct ephemeron *ephemeralIntegerList;
   struct ephemeron *ephemeralBitMapList;
   struct ephemeron *ephemeralExternalAddressList;
   Multifield *ListOfMultifields;
   Multifield *LastMultifield;
  };

struct gcBlock
  {
   struct garbageFrame newGarbageFrame;
   struct garbageFrame *oldGarbageFrame;
   UDFValue *result;
  };

struct stringCL_Builder
  {
   Environment *sbEnv;
   char *contents;
   size_t bufferCL_Reset;
   size_t length;
   size_t bufferMaximum;
  };

#define UTILITY_DATA 55

struct utilityData
  {
   struct voidCL_CallFunctionItem *ListOfCleanupFunctions;
   struct voidCL_CallFunctionItem *ListOfPeriodicFunctions;
   bool PeriodicFunctionsEnabled;
   bool YieldFunctionEnabled;
   void (*YieldCL_TimeFunction)(void);
   struct trackedMemory *trackList;
   struct garbageFrame MasterGarbageFrame;
   struct garbageFrame *CurrentGarbageFrame;
  };

#define UtilityData(theEnv) ((struct utilityData *) GetEnvironmentData(theEnv,UTILITY_DATA))

  /* Is c the start of a utf8 sequence? */
#define IsUTF8Start(ch) (((ch) & 0xC0) != 0x80)
#define IsUTF8MultiByteStart(ch) ((((unsigned char) ch) >= 0xC0) && (((unsigned char) ch) <= 0xF7))
#define IsUTF8MultiByteContinuation(ch) ((((unsigned char) ch) >= 0x80) && (((unsigned char) ch) <= 0xBF))

   void                           CL_InitializeUtilityData(Environment *);
   bool                           CL_AddCleanupFunction(Environment *,const char *,VoidCL_CallFunction *,int,void *);
   bool                           CL_AddPeriodicFunction(Environment *,const char *,VoidCL_CallFunction *,int,void *);
   bool                           CL_RemoveCleanupFunction(Environment *,const char *);
   bool                           CL_RemovePeriodicFunction(Environment *,const char *);
   char                          *CL_CopyString(Environment *,const char *);
   void                           CL_DeleteString(Environment *,char *);
   const char                    *CL_AppendStrings(Environment *,const char *,const char *);
   const char                    *StringPrintFoCL_rm(Environment *,const char *);
   char                          *CL_AppendToString(Environment *,const char *,char *,size_t *,size_t *);
   char                          *CL_InsertInString(Environment *,const char *,size_t,char *,size_t *,size_t *);
   char                          *CL_AppendNToString(Environment *,const char *,char *,size_t,size_t *,size_t *);
   char                          *CL_EnlargeString(Environment *,size_t,char *,size_t *,size_t *);
   char                          *CL_ExpandStringWithChar(Environment *,int,char *,size_t *,size_t *,size_t);
   VoidCL_CallFunctionItem          *CL_AddCL_VoidFunctionToCallList(Environment *,const char *,int,VoidCL_CallFunction *,
                                                            VoidCL_CallFunctionItem *,void *);
   BoolCL_CallFunctionItem          *CL_AddBoolFunctionToCallList(Environment *,const char *,int,BoolCL_CallFunction *,
                                                            BoolCL_CallFunctionItem *,void *);
   VoidCL_CallFunctionItem          *CL_RemoveCL_VoidFunctionFromCallList(Environment *,const char *,
                                                                 VoidCL_CallFunctionItem *,bool *);
   BoolCL_CallFunctionItem          *CL_RemoveBoolFunctionFromCallList(Environment *,const char *,
                                                                 BoolCL_CallFunctionItem *,bool *);
   void                           CL_DeallocateVoidCallList(Environment *,VoidCL_CallFunctionItem *);
   void                           CL_DeallocateBoolCallList(Environment *,BoolCL_CallFunctionItem *);
   CL_CallFunctionItemWithArg       *CL_AddFunctionToCallListWithArg(Environment *,const char *,int,
                                                               VoidCL_CallFunctionWithArg *,
                                                               CL_CallFunctionItemWithArg *,void *);
   CL_CallFunctionItemWithArg       *CL_RemoveFunctionFromCallListWithArg(Environment *,const char *,
                                                                            struct callFunctionItemWithArg *,
                                                                            bool *);
   void                           CL_DeallocateCallListWithArg(Environment *,struct callFunctionItemWithArg *);
   VoidCL_CallFunctionItem          *CL_GetCL_VoidFunctionFromCallList(Environment *,const char *,VoidCL_CallFunctionItem *);
   BoolCL_CallFunctionItem          *CL_GetBoolFunctionFromCallList(Environment *,const char *,BoolCL_CallFunctionItem *);
   size_t                         CL_ItemHashValue(Environment *,unsigned short,void *,size_t);
   void                           CL_YieldTime(Environment *);
   bool                           CL_EnablePeriodicFunctions(Environment *,bool);
   bool                           CL_EnableYieldFunction(Environment *,bool);
   struct trackedMemory          *CL_AddTrackedMemory(Environment *,void *,size_t);
   void                           CL_RemoveTrackedMemory(Environment *,struct trackedMemory *);
   void                           CL_UTF8Increment(const char *,size_t *);
   size_t                         CL_UTF8Offset(const char *,size_t);
   size_t                         CL_UTF8Length(const char *);
   size_t                         CL_UTF8CharNum(const char *,size_t);
   void                           CL_RestorePriorGarbageFrame(Environment *,struct garbageFrame *,struct garbageFrame *,UDFValue *);
   void                           CL_CallCleanupFunctions(Environment *);
   void                           CL_CallPeriodicTasks(Environment *);
   void                           CL_CleanCurrentGarbageFrame(Environment *,UDFValue *);
   void                           CL_GCBlockStart(Environment *,GCBlock *);
   void                           CL_GCBlockEnd(Environment *,GCBlock *);
   void                           CL_GCBlockEndUDF(Environment *,GCBlock *,UDFValue *);
   StringCL_Builder                 *CL_CreateStringCL_Builder(Environment *,size_t);
   void                           CL_SBDispose(StringCL_Builder *);
   void                           CL_SBAppend(StringCL_Builder *,const char *);
   void                           CL_SBAppendInteger(StringCL_Builder *,long long);
   void                           CL_SBAppendFloat(StringCL_Builder *,double);
   void                           CL_SBAddChar(StringCL_Builder *,int);
   void                           SBCL_Reset(StringCL_Builder *);
   char                          *CL_SBCopy(StringCL_Builder *);
   void                          *CL_GetPeriodicFunctionContext(Environment *,const char *);

#endif /* _H_utility */




