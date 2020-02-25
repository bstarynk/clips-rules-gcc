   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  01/29/18            */
   /*                                                     */
   /*              PRINT UTILITY HEADER FILE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Utility routines for printing various items      */
/*   and messages.                                           */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Link error occurs for the CL_SlotExistError       */
/*            function when OBJECT_SYSTEM is set to 0 in     */
/*            setup.h. DR0865                                */
/*                                                           */
/*            Added CL_DataObjectToString function.             */
/*                                                           */
/*            Added CL_SlotExistError function.                 */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Support for DATA_OBJECT_ARRAY primitive.       */
/*                                                           */
/*            Support for typed EXTERNAL_ADDRESS_TYPE.       */
/*                                                           */
/*            Used CL_gensprintf and CL_genstrcat instead of       */
/*            sprintf and strcat.                            */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added code for capturing errors/warnings.      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*      6.31: Added additional error messages for retracted  */
/*            facts, deleted instances, and invalid slots.   */
/*                                                           */
/*            Added under/overflow error message.            */
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
/*            Removed DATA_OBJECT_ARRAY primitive type.      */
/*                                                           */
/*************************************************************/

#ifndef _H_prntutil

#pragma once

#define _H_prntutil

#include <stdio.h>

#include "entities.h"

#define PRINT_UTILITY_DATA 53

struct printUtilityData
  {
   bool PreserveEscapedCharacters;
   bool AddressesToStrings;
   bool InstanceAddressesToNames;
  };

#define PrintUtilityData(theEnv) ((struct printUtilityData *) GetEnvironmentData(theEnv,PRINT_UTILITY_DATA))

   void                           CL_InitializePrintUtilityData(Environment *);
   void                           CL_WriteFloat(Environment *,const char *,double);
   void                           CL_WriteInteger(Environment *,const char *,long long);
   void                           CL_PrintUnsignedInteger(Environment *,const char *,unsigned long long);
   void                           CL_PrintAtom(Environment *,const char *,unsigned short,void *);
   void                           CL_PrintTally(Environment *,const char *,unsigned long long,const char *,const char *);
   const char                    *CL_FloatToString(Environment *,double);
   const char                    *CL_LongIntegerToString(Environment *,long long);
   const char                    *CL_DataObjectToString(Environment *,UDFValue *);
   void                           CL_SyntaxErrorMessage(Environment *,const char *);
   void                           CL_SystemError(Environment *,const char *,int);
   void                           CL_PrintErrorID(Environment *,const char *,int,bool);
   void                           CL_PrintWarningID(Environment *,const char *,int,bool);
   void                           CL_CantFindItemErrorMessage(Environment *,const char *,const char *,bool);
   void                           CL_CantDeleteItemErrorMessage(Environment *,const char *,const char *);
   void                           CL_AlreadyParsedErrorMessage(Environment *,const char *,const char *);
   void                           CL_LocalVariableErrorMessage(Environment *,const char *);
   void                           CL_DivideByZeroErrorMessage(Environment *,const char *);
   void                           SalienceInfo_rmationError(Environment *,const char *,const char *);
   void                           CL_SalienceRangeError(Environment *,int,int);
   void                           CL_SalienceNonIntegerError(Environment *);
   void                           CL_CantFindItemInFunctionErrorMessage(Environment *,const char *,const char *,const char *,bool);
   void                           CL_SlotExistError(Environment *,const char *,const char *);
   void                           CL_Fact_RetractedErrorMessage(Environment *,Fact *);
   void                           CL_FactVarSlotErrorMessage1(Environment *,Fact *,const char *);
   void                           CL_FactVarSlotErrorMessage2(Environment *,Fact *,const char *);
   void                           CL_InvalidVarSlotErrorMessage(Environment *,const char *);
   void                           CL_InstanceVarSlotErrorMessage1(Environment *,Instance *,const char *);
   void                           CL_InstanceVarSlotErrorMessage2(Environment *,Instance *,const char *);
   void                           CL_ArgumentOverUnderflowErrorMessage(Environment *,const char *,bool);

#endif /* _H_prntutil */






