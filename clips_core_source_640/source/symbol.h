   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/01/16            */
   /*                                                     */
   /*                 SYMBOL HEADER FILE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Manages the atomic data value hash tables for    */
/*   storing symbols, integers, floats, and bit maps.        */
/*   Contains routines for adding entries, examining the     */
/*   hash tables, and perfoCL_rming garbage collection to       */
/*   remove entries no longer in use.                        */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: CLIPS crashing on AMD64 processor in the       */
/*            function used to generate a hash value for     */
/*            integers. DR0871                               */
/*                                                           */
/*            Support for run-time programs directly passing */
/*            the hash tables for initialization.            */
/*                                                           */
/*            Corrected code generating compilation          */
/*            warnings.                                      */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Support for hashing EXTERNAL_ADDRESS_TYPE      */
/*            data type.                                     */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Used CL_genstrcpy instead of strcpy.              */
/*                                                           */
/*            Added support for external address hash table  */
/*            and subtyping.                                 */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Added ValueToPointer and EnvValueToPointer     */
/*            macros.                                        */
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

#ifndef _H_symbol

#pragma once

#define _H_symbol

#include <stdlib.h>

#include "entities.h"

typedef struct genericHashNode GENERIC_HN;

#ifndef SYMBOL_HASH_SIZE
#define SYMBOL_HASH_SIZE       63559L
#endif

#ifndef FLOAT_HASH_SIZE
#define FLOAT_HASH_SIZE         8191
#endif

#ifndef INTEGER_HASH_SIZE
#define INTEGER_HASH_SIZE       8191
#endif

#ifndef BITMAP_HASH_SIZE
#define BITMAP_HASH_SIZE        8191
#endif

#ifndef EXTERNAL_ADDRESS_HASH_SIZE
#define EXTERNAL_ADDRESS_HASH_SIZE        8191
#endif

/******************************/
/* genericHashNode STRUCTURE: */
/******************************/
struct genericHashNode
  {
   TypeHeader header;
   struct genericHashNode *next;
   long count;
   unsigned int peCL_rmanent : 1;
   unsigned int markedEphemeral : 1;
   unsigned int needed : 1;
   unsigned int bucket : 29;
  };

/**********************************************************/
/* EPHEMERON STRUCTURE: Data structure used to keep track */
/*   of ephemeral symbols, floats, and integers.          */
/*                                                        */
/*   associatedValue: Contains a pointer to the storage   */
/*   structure for the symbol, float, or integer which is */
/*   ephemeral.                                           */
/*                                                        */
/*   next: Contains a pointer to the next ephemeral item  */
/*   in a list of ephemeral items.                        */
/**********************************************************/
struct ephemeron
  {
   GENERIC_HN *associatedValue;
   struct ephemeron *next;
  };

/***************/
/* symbolMatch */
/***************/
struct symbolMatch
  {
   CLIPSLexeme *match;
   struct symbolMatch *next;
  };

#define IncrementLexemeCount(theValue) (((CLIPSLexeme *) theValue)->count++)
#define IncrementFloatCount(theValue) (((CLIPSFloat *) theValue)->count++)
#define IncrementIntegerCount(theValue) (((CLIPSInteger *) theValue)->count++)
#define IncrementBitMapCount(theValue) (((CLIPSBitMap *) theValue)->count++)
#define IncrementExternalAddressCount(theValue) (((CLIPSExternalAddress *) theValue)->count++)

/*==================*/
/* ENVIRONMENT DATA */
/*==================*/

#define SYMBOL_DATA 49

struct symbolData
  {
   CLIPSLexeme *PositiveInfinity;
   CLIPSLexeme *NegativeInfinity;
   CLIPSInteger *Zero;
   CLIPSLexeme **SymbolTable;
   CLIPSFloat **FloatTable;
   CLIPSInteger **IntegerTable;
   CLIPSBitMap **BitMapTable;
   CLIPSExternalAddress **ExternalAddressTable;
#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE || BLOAD_INSTANCES || BSAVE_INSTANCES
   unsigned long NumberOfSymbols;
   unsigned long NumberOfFloats;
   unsigned long NumberOfIntegers;
   unsigned long NumberOfBitMaps;
   unsigned long NumberOfExternalAddresses;
   CLIPSLexeme **SymbolArray;
   CLIPSFloat **FloatArray;
   CLIPSInteger **IntegerArray;
   CLIPSBitMap **BitMapArray;
   CLIPSExternalAddress **ExternalAddressArray;
#endif
  };

#define SymbolData(theEnv) ((struct symbolData *) GetEnvironmentData(theEnv,SYMBOL_DATA))

   void                           CL_InitializeAtomTables(Environment *,CLIPSLexeme **,CLIPSFloat **,
                                                              CLIPSInteger **,CLIPSBitMap **,
                                                              CLIPSExternalAddress **);
   CLIPSLexeme                   *CL_AddSymbol(Environment *,const char *,unsigned short);
   CLIPSLexeme                   *CL_FindSymbolHN(Environment *,const char *,unsigned short);
   CLIPSFloat                    *CL_CreateFloat(Environment *,double);
   CLIPSInteger                  *CL_CreateInteger(Environment *,long long);
   void                          *CL_AddBitMap(Environment *,void *,unsigned short);
   CLIPSExternalAddress          *CL_CreateExternalAddress(Environment *,void *,unsigned short);
   CLIPSExternalAddress          *CL_CreateCExternalAddress(Environment *,void *);
   CLIPSInteger                  *CL_FindLongHN(Environment *,long long);
   size_t                         CL_HashSymbol(const char *,size_t);
   size_t                         CL_HashFloat(double,size_t);
   size_t                         CL_HashInteger(long long,size_t);
   size_t                         CL_HashBitMap(const char *,size_t,unsigned);
   size_t                         CL_HashExternalAddress(void *,size_t);
   void                           CL_RetainLexeme(Environment *,CLIPSLexeme *);
   void                           CL_RetainFloat(Environment *,CLIPSFloat *);
   void                           CL_RetainInteger(Environment *,CLIPSInteger *);
   void                           CL_IncrementBitMapReferenceCount(Environment *,CLIPSBitMap *);
   void                           CL_RetainExternalAddress(Environment *,CLIPSExternalAddress *);
   void                           CL_ReleaseLexeme(Environment *,CLIPSLexeme *);
   void                           CL_ReleaseFloat(Environment *,CLIPSFloat *);
   void                           CL_ReleaseInteger(Environment *,CLIPSInteger *);
   void                           CL_DecrementBitMapReferenceCount(Environment *,CLIPSBitMap *);
   void                           CL_ReleaseExternalAddress(Environment *,CLIPSExternalAddress *);
   void                           CL_RemoveEphemeralAtoms(Environment *);
   CLIPSLexeme                  **CL_GetSymbolTable(Environment *);
   void                           CL_SetSymbolTable(Environment *,CLIPSLexeme **);
   CLIPSFloat                   **CL_GetFloatTable(Environment *);
   void                           CL_SetFloatTable(Environment *,CLIPSFloat **);
   CLIPSInteger                 **CL_GetIntegerTable(Environment *);
   void                           CL_SetIntegerTable(Environment *,CLIPSInteger **);
   CLIPSBitMap                  **CL_GetBitMapTable(Environment *);
   void                           CL_SetBitMapTable(Environment *,CLIPSBitMap **);
   CLIPSExternalAddress         **CL_GetExternalAddressTable(Environment *);
   void                           CL_SetExternalAddressTable(Environment *,CLIPSExternalAddress **);
   void                           CL_RefreshSpecialSymbols(Environment *);
   struct symbolMatch            *CL_FindSymbolCL_Matches(Environment *,const char *,unsigned *,size_t *);
   void                           ReturnSymbolCL_Matches(Environment *,struct symbolMatch *);
   CLIPSLexeme                   *CL_GetNextSymbolMatch(Environment *,const char *,size_t,CLIPSLexeme *,bool,size_t *);
   void                           CL_ClearBitString(void *,size_t);
   void                           CL_SetAtomicValueIndices(Environment *,bool);
   void                           CL_RestoreAtomicValueBuckets(Environment *);
   void                           CL_EphemerateValue(Environment *,void *);
   CLIPSLexeme                   *CL_CreateSymbol(Environment *,const char *);
   CLIPSLexeme                   *CL_CreateString(Environment *,const char *);
   CLIPSLexeme                   *CL_CreateCL_InstanceName(Environment *,const char *);
   CLIPSLexeme                   *CL_CreateBoolean(Environment *,bool);
   bool                           CL_BitStringHasBitsSet(void *,unsigned);

#endif /* _H_symbol */



