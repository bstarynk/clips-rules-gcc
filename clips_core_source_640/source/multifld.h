   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  11/17/17            */
   /*                                                     */
   /*                MULTIFIELD HEADER FILE               */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for creating and manipulating           */
/*   multifield values.                                      */
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
/*            Corrected code to remove compiler warnings.    */
/*                                                           */
/*            Moved CL_ImplodeMultifield from multifun.c.       */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Used CL_DataObjectToString instead of             */
/*            ValueToString in implode$ to handle            */
/*            print representation of external addresses.    */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed issue with CL_StoreInMultifield when        */
/*            asserting void values in implied deftemplate   */
/*            facts.                                         */
/*                                                           */
/*      6.40: Refactored code to reduce header dependencies  */
/*            in sysdep.c.                                   */
/*                                                           */
/*            Removed LOCALE definition.                     */
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

#ifndef _H_multifld

#pragma once

#define _H_multifld

#include "entities.h"

typedef struct multifieldCL_Builder MultifieldCL_Builder;

struct multifieldCL_Builder
  {
   Environment *mbEnv;
   CLIPSValue *contents;
   size_t bufferCL_Reset;
   size_t length;
   size_t bufferMaximum;
  };

   Multifield                    *CL_CreateUnmanagedMultifield(Environment *,size_t);
   void                           CL_ReturnMultifield(Environment *,Multifield *);
   void                           CL_RetainMultifield(Environment *,Multifield *);
   void                           CL_ReleaseMultifield(Environment *,Multifield *);
   void                           CL_IncrementCLIPSValueMultifieldReferenceCount(Environment *,Multifield *);
   void                           CL_DecrementCLIPSValueMultifieldReferenceCount(Environment *,Multifield *);
   Multifield                    *CL_StringToMultifield(Environment *,const char *);
   Multifield                    *CL_CreateMultifield(Environment *,size_t);
   void                           CL_AddToMultifieldList(Environment *,Multifield *);
   void                           CL_FlushMultifields(Environment *);
   void                           CL_DuplicateMultifield(Environment *,UDFValue *,UDFValue *);
   void                           CL_WriteMultifield(Environment *,const char *,Multifield *);
   void                           CL_PrintMultifieldDriver(Environment *,const char *,Multifield *,size_t,size_t,bool);
   bool                           MultifieldCL_DOsEqual(UDFValue *,UDFValue *);
   void                           CL_StoreInMultifield(Environment *,UDFValue *,Expression *,bool);
   Multifield                    *CL_CopyMultifield(Environment *,Multifield *);
   bool                           CL_MultifieldsEqual(Multifield *,Multifield *);
   Multifield                    *CL_DOToMultifield(Environment *,UDFValue *);
   size_t                         CL_HashMultifield(Multifield *,size_t);
   Multifield                    *CL_GetMultifieldList(Environment *);
   CLIPSLexeme                   *CL_ImplodeMultifield(Environment *,UDFValue *);
   void                           CL_EphemerateMultifield(Environment *,Multifield *);
   Multifield                    *CL_ArrayToMultifield(Environment *,CLIPSValue *,unsigned long);
   void                           CL_NoCL_rmalizeMultifield(Environment *,UDFValue *);
   void                           CL_CLIPSToUDFValue(CLIPSValue *,UDFValue *);
   void                           CL_UDFToCLIPSValue(Environment *,UDFValue *,CLIPSValue *);
   MultifieldCL_Builder             *CL_CreateMultifieldCL_Builder(Environment *,size_t);
   void                           CL_MBCL_Reset(MultifieldCL_Builder *);
   void                           CL_MBDispose(MultifieldCL_Builder *);
   void                           CL_MBAppend(MultifieldCL_Builder *theMB,CLIPSValue *);
   Multifield                    *CL_MBCreate(MultifieldCL_Builder *);
   Multifield                    *CL_EmptyMultifield(Environment *);
   void                           CL_MBAppendCLIPSInteger(MultifieldCL_Builder *,CLIPSInteger *);
   void                           CL_MBAppendInteger(MultifieldCL_Builder *,long long);
   void                           CL_MBAppendCLIPSFloat(MultifieldCL_Builder *,CLIPSFloat *);
   void                           CL_MBAppendFloat(MultifieldCL_Builder *,double);
   void                           CL_MBAppendCLIPSLexeme(MultifieldCL_Builder *,CLIPSLexeme *);
   void                           CL_MBAppendSymbol(MultifieldCL_Builder *,const char *);
   void                           CL_MBAppendString(MultifieldCL_Builder *,const char *);
   void                           CL_MBAppendCL_InstanceName(MultifieldCL_Builder *,const char *);
   void                           CL_MBAppendCLIPSExternalAddress(MultifieldCL_Builder *,CLIPSExternalAddress *);
   void                           CL_MBAppendFact(MultifieldCL_Builder *,Fact *);
   void                           CL_MBAppendInstance(MultifieldCL_Builder *,Instance *);
   void                           CL_MBAppendMultifield(MultifieldCL_Builder *,Multifield *);
   void                           CL_MBAppendUDFValue(MultifieldCL_Builder *theMB,UDFValue *);

#endif /* _H_multifld */




