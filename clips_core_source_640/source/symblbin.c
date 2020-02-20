   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/01/16             */
   /*                                                     */
   /*                 SYMBOL_TYPE BSAVE MODULE                 */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for      */
/*    atomic data values.                                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE || BLOAD_INSTANCES || BSAVE_INSTANCES

#include "argacces.h"
#include "bload.h"
#include "bsave.h"
#include "cstrnbin.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "moduldef.h"
#include "router.h"

#include "symblbin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                        ReadNeededBitMaps(Environment *);
#if BLOAD_AND_BSAVE || BSAVE_INSTANCES
   static void                        CL_WriteNeededBitMaps(Environment *,FILE *);
#endif

#if BLOAD_AND_BSAVE || BSAVE_INSTANCES

/**********************************************/
/* CL_WriteNeededAtomicValues: CL_Save all symbols, */
/*   floats, integers, and bitmaps needed by  */
/*   this binary image to the binary file.    */
/**********************************************/
void CL_WriteNeededAtomicValues(
  Environment *theEnv,
  FILE *fp)
  {
   CL_WriteNeededSymbols(theEnv,fp);
   CL_WriteNeededFloats(theEnv,fp);
   CL_WriteNeededIntegers(theEnv,fp);
   CL_WriteNeededBitMaps(theEnv,fp);
  }

/********************************************************/
/* CL_InitAtomicValueNeededFlags: Initializes all symbols, */
/*   floats, integers, and bitmaps as being unneeded by */
/*   the binary image being saved.                      */
/********************************************************/
void CL_InitAtomicValueNeededFlags(
  Environment *theEnv)
  {
   unsigned long i;
   CLIPSLexeme *symbolPtr, **symbolArray;
   CLIPSFloat *floatPtr, **floatArray;
   CLIPSInteger *integerPtr, **integerArray;
   CLIPSBitMap *bitMapPtr, **bitMapArray;

   /*===============*/
   /* Mark symbols. */
   /*===============*/

   symbolArray = CL_GetSymbolTable(theEnv);

   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
     {
      symbolPtr = symbolArray[i];
      while (symbolPtr != NULL)
        {
         symbolPtr->neededSymbol = false;
         symbolPtr = symbolPtr->next;
        }
     }

   /*==============*/
   /* Mark floats. */
   /*==============*/

   floatArray = CL_GetFloatTable(theEnv);

   for (i = 0; i < FLOAT_HASH_SIZE; i++)
     {
      floatPtr = floatArray[i];
      while (floatPtr != NULL)
        {
         floatPtr->neededFloat = false;
         floatPtr = floatPtr->next;
        }
     }

   /*================*/
   /* Mark integers. */
   /*================*/

   integerArray = CL_GetIntegerTable(theEnv);

   for (i = 0; i < INTEGER_HASH_SIZE; i++)
     {
      integerPtr = integerArray[i];
      while (integerPtr != NULL)
        {
         integerPtr->neededInteger = false;
         integerPtr = integerPtr->next;
        }
     }

   /*===============*/
   /* Mark bitmaps. */
   /*===============*/

   bitMapArray = CL_GetBitMapTable(theEnv);

   for (i = 0; i < BITMAP_HASH_SIZE; i++)
     {
      bitMapPtr = bitMapArray[i];
      while (bitMapPtr != NULL)
        {
         bitMapPtr->neededBitMap = false;
         bitMapPtr = bitMapPtr->next;
        }
     }
  }

/*****************************************************************/
/* CL_WriteNeededSymbols: Stores all of the symbols in the symbol   */
/*   table needed for this binary image in the binary save file. */
/*****************************************************************/
void CL_WriteNeededSymbols(
  Environment *theEnv,
  FILE *fp)
  {
   unsigned long i;
   size_t length;
   CLIPSLexeme **symbolArray;
   CLIPSLexeme *symbolPtr;
   unsigned long numberOfUsedSymbols = 0;
   size_t size = 0;

   /*=================================*/
   /* Get a copy of the symbol table. */
   /*=================================*/

   symbolArray = CL_GetSymbolTable(theEnv);

   /*======================================================*/
   /* Get the number of symbols and the total string size. */
   /*======================================================*/

   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
     {
      for (symbolPtr = symbolArray[i];
           symbolPtr != NULL;
           symbolPtr = symbolPtr->next)
        {
         if (symbolPtr->neededSymbol)
           {
            numberOfUsedSymbols++;
            size += strlen(symbolPtr->contents) + 1;
           }
        }
     }

   /*=============================================*/
   /* CL_Write out the symbols and the string sizes. */
   /*=============================================*/

   CL_GenCL_Write(&numberOfUsedSymbols,sizeof(unsigned long),fp);
   CL_GenCL_Write(&size,sizeof(unsigned long),fp);

   /*=============================*/
   /* CL_Write out the symbol types. */
   /*=============================*/
   
   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
     {
      for (symbolPtr = symbolArray[i];
           symbolPtr != NULL;
           symbolPtr = symbolPtr->next)
        {
         if (symbolPtr->neededSymbol)
           { CL_GenCL_Write(&symbolPtr->header.type,sizeof(unsigned short),fp); }
        }
     }
     
   /*========================*/
   /* CL_Write out the symbols. */
   /*========================*/
   
   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
     {
      for (symbolPtr = symbolArray[i];
           symbolPtr != NULL;
           symbolPtr = symbolPtr->next)
        {
         if (symbolPtr->neededSymbol)
           {
            length = strlen(symbolPtr->contents) + 1;
            CL_GenCL_Write((void *) symbolPtr->contents,length,fp);
           }
        }
     }
  }

/*****************************************************************/
/* CL_WriteNeededFloats: Stores all of the floats in the float      */
/*   table needed for this binary image in the binary save file. */
/*****************************************************************/
void CL_WriteNeededFloats(
  Environment *theEnv,
  FILE *fp)
  {
   int i;
   CLIPSFloat **floatArray;
   CLIPSFloat *floatPtr;
   unsigned long numberOfUsedFloats = 0;

   /*================================*/
   /* Get a copy of the float table. */
   /*================================*/

   floatArray = CL_GetFloatTable(theEnv);

   /*===========================*/
   /* Get the number of floats. */
   /*===========================*/

   for (i = 0; i < FLOAT_HASH_SIZE; i++)
     {
      for (floatPtr = floatArray[i];
           floatPtr != NULL;
           floatPtr = floatPtr->next)
        { if (floatPtr->neededFloat) numberOfUsedFloats++; }
     }

   /*======================================================*/
   /* CL_Write out the number of floats and the float values. */
   /*======================================================*/

   CL_GenCL_Write(&numberOfUsedFloats,sizeof(unsigned long),fp);

   for (i = 0 ; i < FLOAT_HASH_SIZE; i++)
     {
      for (floatPtr = floatArray[i];
           floatPtr != NULL;
           floatPtr = floatPtr->next)
        {
         if (floatPtr->neededFloat)
           { CL_GenCL_Write(&floatPtr->contents,
                      sizeof(floatPtr->contents),fp); }
        }
     }
  }

/******************************************************************/
/* CL_WriteNeededIntegers: Stores all of the integers in the integer */
/*   table needed for this binary image in the binary save file.  */
/******************************************************************/
void CL_WriteNeededIntegers(
  Environment *theEnv,
  FILE *fp)
  {
   int i;
   CLIPSInteger **integerArray;
   CLIPSInteger *integerPtr;
   unsigned long numberOfUsedIntegers = 0;

   /*==================================*/
   /* Get a copy of the integer table. */
   /*==================================*/

   integerArray = CL_GetIntegerTable(theEnv);

   /*=============================*/
   /* Get the number of integers. */
   /*=============================*/

   for (i = 0 ; i < INTEGER_HASH_SIZE; i++)
     {
      for (integerPtr = integerArray[i];
           integerPtr != NULL;
           integerPtr = integerPtr->next)
        {
         if (integerPtr->neededInteger) numberOfUsedIntegers++;
        }
     }

   /*==========================================================*/
   /* CL_Write out the number of integers and the integer values. */
   /*==========================================================*/

   CL_GenCL_Write(&numberOfUsedIntegers,sizeof(unsigned long),fp);

   for (i = 0 ; i < INTEGER_HASH_SIZE; i++)
     {
      for (integerPtr = integerArray[i];
           integerPtr != NULL;
           integerPtr = integerPtr->next)
        {
         if (integerPtr->neededInteger)
           {
            CL_GenCL_Write(&integerPtr->contents,
                     sizeof(integerPtr->contents),fp);
           }
        }
     }
  }

/*****************************************************************/
/* CL_WriteNeededBitMaps: Stores all of the bitmaps in the bitmap   */
/*   table needed for this binary image in the binary save file. */
/*****************************************************************/
static void CL_WriteNeededBitMaps(
  Environment *theEnv,
  FILE *fp)
  {
   int i;
   CLIPSBitMap **bitMapArray;
   CLIPSBitMap *bitMapPtr;
   unsigned long numberOfUsedBitMaps = 0, size = 0;
   unsigned short tempSize;

   /*=================================*/
   /* Get a copy of the bitmap table. */
   /*=================================*/

   bitMapArray = CL_GetBitMapTable(theEnv);

   /*======================================================*/
   /* Get the number of bitmaps and the total bitmap size. */
   /*======================================================*/

   for (i = 0; i < BITMAP_HASH_SIZE; i++)
     {
      for (bitMapPtr = bitMapArray[i];
           bitMapPtr != NULL;
           bitMapPtr = bitMapPtr->next)
        {
         if (bitMapPtr->neededBitMap)
           {
            numberOfUsedBitMaps++;
            size += (unsigned long) (bitMapPtr->size + sizeof(unsigned short));
           }
        }
     }

   /*========================================*/
   /* CL_Write out the bitmaps and their sizes. */
   /*========================================*/

   CL_GenCL_Write(&numberOfUsedBitMaps,sizeof(unsigned long),fp);
   CL_GenCL_Write(&size,sizeof(unsigned long),fp);

   for (i = 0; i < BITMAP_HASH_SIZE; i++)
     {
      for (bitMapPtr = bitMapArray[i];
           bitMapPtr != NULL;
           bitMapPtr = bitMapPtr->next)
        {
         if (bitMapPtr->neededBitMap)
           {
            tempSize = bitMapPtr->size;
            CL_GenCL_Write(&tempSize,sizeof(unsigned short),fp);
            CL_GenCL_Write((void *) bitMapPtr->contents,bitMapPtr->size,fp);
           }
        }
     }
  }

#endif /* BLOAD_AND_BSAVE || BSAVE_INSTANCES */

/*********************************************/
/* CL_ReadNeededAtomicValues: Read all symbols, */
/*   floats, integers, and bitmaps needed by */
/*   this binary image from the binary file. */
/*********************************************/
void CL_ReadNeededAtomicValues(
  Environment *theEnv)
  {
   CL_ReadNeededSymbols(theEnv);
   CL_ReadNeededFloats(theEnv);
   CL_ReadNeededIntegers(theEnv);
   ReadNeededBitMaps(theEnv);
  }

/*******************************************/
/* CL_ReadNeededSymbols: Reads in the symbols */
/*   used by the binary image.             */
/*******************************************/
void CL_ReadNeededSymbols(
  Environment *theEnv)
  {
   char *symbolNames, *namePtr;
   unsigned long space;
   unsigned short *types;
   unsigned long i;

   /*=================================================*/
   /* DeteCL_rmine the number of symbol names to be read */
   /* and space required for them.                    */
   /*=================================================*/

   CL_GenReadBinary(theEnv,&SymbolData(theEnv)->NumberOfSymbols,sizeof(long));
   CL_GenReadBinary(theEnv,&space,sizeof(unsigned long));
   if (SymbolData(theEnv)->NumberOfSymbols == 0)
     {
      SymbolData(theEnv)->SymbolArray = NULL;
      return;
     }

   /*=======================================*/
   /* Allocate area for strings to be read. */
   /*=======================================*/
   
   types = (unsigned short *) CL_gm2(theEnv,sizeof(unsigned short) * SymbolData(theEnv)->NumberOfSymbols);
   CL_GenReadBinary(theEnv,types,sizeof(unsigned short) * SymbolData(theEnv)->NumberOfSymbols);

   symbolNames = (char *) CL_gm2(theEnv,space);
   CL_GenReadBinary(theEnv,symbolNames,space);

   /*================================================*/
   /* Store the symbol pointers in the symbol array. */
   /*================================================*/

   SymbolData(theEnv)->SymbolArray = (CLIPSLexeme **)
                 CL_gm2(theEnv,sizeof(CLIPSLexeme *) * SymbolData(theEnv)->NumberOfSymbols);
   namePtr = symbolNames;
   for (i = 0; i < SymbolData(theEnv)->NumberOfSymbols; i++)
     {
      if (types[i] == SYMBOL_TYPE)
        { SymbolData(theEnv)->SymbolArray[i] = CL_CreateSymbol(theEnv,namePtr); }
      else if (types[i] == STRING_TYPE)
        { SymbolData(theEnv)->SymbolArray[i] = CL_CreateString(theEnv,namePtr); }
      else
        { SymbolData(theEnv)->SymbolArray[i] = CL_CreateCL_InstanceName(theEnv,namePtr); }

      namePtr += strlen(namePtr) + 1;
     }

   /*=======================*/
   /* Free the name buffer. */
   /*=======================*/

   CL_rm(theEnv,types,sizeof(unsigned short) * SymbolData(theEnv)->NumberOfSymbols);
   CL_rm(theEnv,symbolNames,space);
  }

/*****************************************/
/* CL_ReadNeededFloats: Reads in the floats */
/*   used by the binary image.           */
/*****************************************/
void CL_ReadNeededFloats(
  Environment *theEnv)
  {
   double *floatValues;
   unsigned long i;

   /*============================================*/
   /* DeteCL_rmine the number of floats to be read. */
   /*============================================*/

   CL_GenReadBinary(theEnv,&SymbolData(theEnv)->NumberOfFloats,sizeof(long));
   if (SymbolData(theEnv)->NumberOfFloats == 0)
     {
      SymbolData(theEnv)->FloatArray = NULL;
      return;
     }

   /*===============================*/
   /* Allocate area for the floats. */
   /*===============================*/

   floatValues = (double *) CL_gm2(theEnv,sizeof(double) * SymbolData(theEnv)->NumberOfFloats);
   CL_GenReadBinary(theEnv,floatValues,(sizeof(double) * SymbolData(theEnv)->NumberOfFloats));

   /*======================================*/
   /* Store the floats in the float array. */
   /*======================================*/

   SymbolData(theEnv)->FloatArray = (CLIPSFloat **)
               CL_gm2(theEnv,sizeof(CLIPSFloat *) * SymbolData(theEnv)->NumberOfFloats);
   for (i = 0; i < SymbolData(theEnv)->NumberOfFloats; i++)
     { SymbolData(theEnv)->FloatArray[i] = CL_CreateFloat(theEnv,floatValues[i]); }

   /*========================*/
   /* Free the float buffer. */
   /*========================*/

   CL_rm(theEnv,floatValues,(sizeof(double) * SymbolData(theEnv)->NumberOfFloats));
  }

/*********************************************/
/* CL_ReadNeededIntegers: Reads in the integers */
/*   used by the binary image.               */
/*********************************************/
void CL_ReadNeededIntegers(
  Environment *theEnv)
  {
   long long *integerValues;
   unsigned long i;

   /*==============================================*/
   /* DeteCL_rmine the number of integers to be read. */
   /*==============================================*/

   CL_GenReadBinary(theEnv,&SymbolData(theEnv)->NumberOfIntegers,sizeof(unsigned long));
   if (SymbolData(theEnv)->NumberOfIntegers == 0)
     {
      SymbolData(theEnv)->IntegerArray = NULL;
      return;
     }

   /*=================================*/
   /* Allocate area for the integers. */
   /*=================================*/

   integerValues = (long long *) CL_gm2(theEnv,(sizeof(long long) * SymbolData(theEnv)->NumberOfIntegers));
   CL_GenReadBinary(theEnv,integerValues,(sizeof(long long) * SymbolData(theEnv)->NumberOfIntegers));

   /*==========================================*/
   /* Store the integers in the integer array. */
   /*==========================================*/

   SymbolData(theEnv)->IntegerArray = (CLIPSInteger **)
           CL_gm2(theEnv,(sizeof(CLIPSInteger *) * SymbolData(theEnv)->NumberOfIntegers));
   for (i = 0; i < SymbolData(theEnv)->NumberOfIntegers; i++)
     { SymbolData(theEnv)->IntegerArray[i] = CL_CreateInteger(theEnv,integerValues[i]); }

   /*==========================*/
   /* Free the integer buffer. */
   /*==========================*/

   CL_rm(theEnv,integerValues,(sizeof(long long) * SymbolData(theEnv)->NumberOfIntegers));
  }

/*******************************************/
/* ReadNeededBitMaps: Reads in the bitmaps */
/*   used by the binary image.             */
/*******************************************/
static void ReadNeededBitMaps(
  Environment *theEnv)
  {
   char *bitMapStorage, *bitMapPtr;
   unsigned long space;
   unsigned long i;
   unsigned short *tempSize;

   /*=======================================*/
   /* DeteCL_rmine the number of bitmaps to be */
   /* read and space required for them.     */
   /*=======================================*/

   CL_GenReadBinary(theEnv,&SymbolData(theEnv)->NumberOfBitMaps,sizeof(long));
   CL_GenReadBinary(theEnv,&space,sizeof(unsigned long));
   if (SymbolData(theEnv)->NumberOfBitMaps == 0)
     {
      SymbolData(theEnv)->BitMapArray = NULL;
      return;
     }

   /*=======================================*/
   /* Allocate area for bitmaps to be read. */
   /*=======================================*/

   bitMapStorage = (char *) CL_gm2(theEnv,space);
   CL_GenReadBinary(theEnv,bitMapStorage,space);

   /*================================================*/
   /* Store the bitMap pointers in the bitmap array. */
   /*================================================*/

   SymbolData(theEnv)->BitMapArray = (CLIPSBitMap **)
                 CL_gm2(theEnv,sizeof(CLIPSBitMap *) * SymbolData(theEnv)->NumberOfBitMaps);
   bitMapPtr = bitMapStorage;
   for (i = 0; i < SymbolData(theEnv)->NumberOfBitMaps; i++)
     {
      tempSize = (unsigned short *) bitMapPtr;
      SymbolData(theEnv)->BitMapArray[i] = (CLIPSBitMap *) CL_AddBitMap(theEnv,bitMapPtr+sizeof(unsigned short),*tempSize);
      bitMapPtr += *tempSize + sizeof(unsigned short);
     }

   /*=========================*/
   /* Free the bitmap buffer. */
   /*=========================*/

   CL_rm(theEnv,bitMapStorage,space);
  }

/**********************************************************/
/* CL_FreeAtomicValueStorage: Returns the memory allocated   */
/*   for storing the pointers to atomic data values used  */
/*   in refreshing expressions and other data structures. */
/**********************************************************/
void CL_FreeAtomicValueStorage(
  Environment *theEnv)
  {
   if (SymbolData(theEnv)->SymbolArray != NULL)
     CL_rm(theEnv,SymbolData(theEnv)->SymbolArray,sizeof(CLIPSLexeme *) * SymbolData(theEnv)->NumberOfSymbols);
   if (SymbolData(theEnv)->FloatArray != NULL)
     CL_rm(theEnv,SymbolData(theEnv)->FloatArray,sizeof(CLIPSFloat *) * SymbolData(theEnv)->NumberOfFloats);
   if (SymbolData(theEnv)->IntegerArray != NULL)
     CL_rm(theEnv,SymbolData(theEnv)->IntegerArray,sizeof(CLIPSInteger *) * SymbolData(theEnv)->NumberOfIntegers);
   if (SymbolData(theEnv)->BitMapArray != NULL)
     CL_rm(theEnv,SymbolData(theEnv)->BitMapArray,sizeof(CLIPSBitMap *) * SymbolData(theEnv)->NumberOfBitMaps);

   SymbolData(theEnv)->SymbolArray = NULL;
   SymbolData(theEnv)->FloatArray = NULL;
   SymbolData(theEnv)->IntegerArray = NULL;
   SymbolData(theEnv)->BitMapArray = NULL;
   SymbolData(theEnv)->NumberOfSymbols = 0;
   SymbolData(theEnv)->NumberOfFloats = 0;
   SymbolData(theEnv)->NumberOfIntegers = 0;
   SymbolData(theEnv)->NumberOfBitMaps = 0;
  }

#endif /* BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE || BLOAD_INSTANCES || BSAVE_INSTANCES */
