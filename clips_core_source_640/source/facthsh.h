   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  12/30/16            */
   /*                                                     */
   /*                 FACT HASHING MODULE                 */
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
/*      6.24: Removed LOGICAL_DEPENDENCIES compilation flag. */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Fact hash table is resizable.                  */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added FactWillBe_Asserted.                      */
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
/*            UDF redesign.                                  */
/*                                                           */
/*            Modify command preserves fact id and address.  */
/*                                                           */
/*            CL_Assert returns duplicate fact. FALSE is now    */
/*            returned only if an error occurs.              */
/*                                                           */
/*************************************************************/

#ifndef _H_facthsh

#pragma once

#define _H_facthsh

#include "entities.h"

typedef struct factHashEntry FactHashEntry;

struct factHashEntry
  {
   Fact *theFact;
   FactHashEntry *next;
  };

#define SIZE_FACT_HASH 16231

   void                           CL_AddHashedFact(Environment *,Fact *,size_t);
   bool                           CL_RemoveHashedFact(Environment *,Fact *);
   size_t                         CL_HandleFactDuplication(Environment *,Fact *,Fact **,long long);
   bool                           CL_GetFactDuplication(Environment *);
   bool                           CL_SetFactDuplication(Environment *,bool);
   void                           CL_InitializeFactHashTable(Environment *);
   void                           ShowFactHashTableCommand(Environment *,UDFContext *,UDFValue *);
   size_t                         CL_HashFact(Fact *);
   bool                           FactWillBe_Asserted(Environment *,Fact *);

#endif /* _H_facthsh */


