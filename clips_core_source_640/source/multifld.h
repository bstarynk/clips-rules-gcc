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

typedef struct multifield_Builder Multifield_Builder;

struct multifield_Builder
{
  Environment *mbEnv;
  CLIPSValue *contents;
  size_t buffer_Reset;
  size_t length;
  size_t bufferMaximum;
};

Multifield *CL_CreateUnmanagedMultifield (Environment *, size_t);
void CL_ReturnMultifield (Environment *, Multifield *);
void CL_RetainMultifield (Environment *, Multifield *);
void CL_ReleaseMultifield (Environment *, Multifield *);
void CL_IncrementCLIPSValueMultifieldReferenceCount (Environment *,
						     Multifield *);
void CL_DecrementCLIPSValueMultifieldReferenceCount (Environment *,
						     Multifield *);
Multifield *CL_StringToMultifield (Environment *, const char *);
Multifield *CL_CreateMultifield (Environment *, size_t);
void CL_AddToMultifieldList (Environment *, Multifield *);
void CL_FlushMultifields (Environment *);
void CL_DuplicateMultifield (Environment *, UDFValue *, UDFValue *);
void CL_WriteMultifield (Environment *, const char *, Multifield *);
void CL_PrintMultifieldDriver (Environment *, const char *, Multifield *,
			       size_t, size_t, bool);
bool Multifield_DOsEqual (UDFValue *, UDFValue *);
void CL_StoreInMultifield (Environment *, UDFValue *, Expression *, bool);
Multifield *CL_CopyMultifield (Environment *, Multifield *);
bool CL_MultifieldsEqual (Multifield *, Multifield *);
Multifield *CL_DOToMultifield (Environment *, UDFValue *);
size_t CL_HashMultifield (Multifield *, size_t);
Multifield *CL_GetMultifieldList (Environment *);
CLIPSLexeme *CL_ImplodeMultifield (Environment *, UDFValue *);
void CL_EphemerateMultifield (Environment *, Multifield *);
Multifield *CL_ArrayToMultifield (Environment *, CLIPSValue *, unsigned long);
void CL_No_rmalizeMultifield (Environment *, UDFValue *);
void CL_CLIPSToUDFValue (CLIPSValue *, UDFValue *);
void CL_UDFToCLIPSValue (Environment *, UDFValue *, CLIPSValue *);
Multifield_Builder *CL_CreateMultifield_Builder (Environment *, size_t);
void CL_MB_Reset (Multifield_Builder *);
void CL_MBDispose (Multifield_Builder *);
void CL_MBAppend (Multifield_Builder * theMB, CLIPSValue *);
Multifield *CL_MBCreate (Multifield_Builder *);
Multifield *CL_EmptyMultifield (Environment *);
void CL_MBAppendCLIPSInteger (Multifield_Builder *, CLIPSInteger *);
void CL_MBAppendInteger (Multifield_Builder *, long long);
void CL_MBAppendCLIPSFloat (Multifield_Builder *, CLIPSFloat *);
void CL_MBAppendFloat (Multifield_Builder *, double);
void CL_MBAppendCLIPSLexeme (Multifield_Builder *, CLIPSLexeme *);
void CL_MBAppendSymbol (Multifield_Builder *, const char *);
void CL_MBAppendString (Multifield_Builder *, const char *);
void CL_MBAppend_InstanceName (Multifield_Builder *, const char *);
void CL_MBAppendCLIPSExternalAddress (Multifield_Builder *,
				      CLIPSExternalAddress *);
void CL_MBAppendFact (Multifield_Builder *, Fact *);
void CL_MBAppendInstance (Multifield_Builder *, Instance *);
void CL_MBAppendMultifield (Multifield_Builder *, Multifield *);
void CL_MBAppendUDFValue (Multifield_Builder * theMB, UDFValue *);

#endif /* _H_multifld */
