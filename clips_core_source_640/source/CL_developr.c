   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  11/01/16             */
   /*                                                     */
   /*                   DEVELOPER MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines useful for browsing various    */
/*   data structures. The functions are provided for         */
/*   development use.                                        */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*      6.30: Added support for hashed alpha memories.       */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*            Functions enable-gc-heuristics and             */
/*            disable-gc-heuristics are no longer supported. */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers and type casts to       */
/*            remove C++ warnings.                           */
/*                                                           */
/*            Replaced deprecated WCLIPS logical name with   */
/*            WPROMPT.                                       */
/*                                                           */
/*      6.31: Crash fix for show-opn.                        */
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

#include <stdio.h>

#include "setup.h"

#include "argacces.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "factmngr.h"
#include "inscom.h"
#include "modulutl.h"
#include "prntutil.h"
#include "router.h"
#include "utility.h"

#if DEFRULE_CONSTRUCT && DEFTEMPLATE_CONSTRUCT
#include "tmpltdef.h"
#include "factbld.h"
#include "facthsh.h"
#endif

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM
#include "classcom.h"
#include "classfun.h"
#include "objrtmch.h"
#endif
#if OBJECT_SYSTEM
#include "insfun.h"
#endif

#include "developr.h"

#if DEVELOPER

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM
   static void                    PrintOPNLevel(Environment *,OBJECT_PATTERN_NODE *,char *,int);
#endif

/**************************************************/
/* DeveloperCommands: Sets up developer commands. */
/**************************************************/
void DeveloperCommands(
  Environment *theEnv)
  {
#if ! RUN_TIME
   CL_AddUDF(theEnv,"primitives-info","v",0,0,NULL,PrimitiveTablesInfoCommand,"PrimitiveTablesInfoCommand",NULL);
   CL_AddUDF(theEnv,"primitives-usage","v",0,0,NULL,PrimitiveTablesUsageCommand,"PrimitiveTablesUsageCommand",NULL);

#if DEFRULE_CONSTRUCT && DEFTEMPLATE_CONSTRUCT
   CL_AddUDF(theEnv,"validate-fact-integrity","b", 0,0,NULL,ValidateFactIntegrityCommand,"ValidateFactIntegrityCommand",NULL);

   CL_AddUDF(theEnv,"show-fpn","v",1,1,"y",ShowFactPatternNetworkCommand,"ShowFactPatternNetworkCommand",NULL);
   CL_AddUDF(theEnv,"show-fht","v",0,0,NULL,ShowFactHashTableCommand,"ShowFactHashTableCommand",NULL);
#endif

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM
   CL_AddUDF(theEnv,"show-opn","v",0,0,NULL,PrintObjectPatternNetworkCommand,"PrintObjectPatternNetworkCommand",NULL);
#endif

#if OBJECT_SYSTEM
   CL_AddUDF(theEnv,"instance-table-usage","v",0,0,NULL,InstanceTableUsageCommand,"InstanceTableUsageCommand",NULL);
#endif

#endif
  }

/******************************************************/
/* PrimitiveTablesInfo: Prints info_rmation about the  */
/*   symbol, float, integer, and bitmap tables.       */
/******************************************************/
void PrimitiveTablesInfoCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   unsigned long i;
   CLIPSLexeme **symbolArray, *symbolPtr;
   CLIPSFloat **floatArray, *floatPtr;
   CLIPSInteger **integerArray, *integerPtr;
   CLIPSBitMap **bitMapArray, *bitMapPtr;
   unsigned long long symbolCount = 0, integerCount = 0;
   unsigned long long floatCount = 0, bitMapCount = 0;

   /*====================================*/
   /* Count entries in the symbol table. */
   /*====================================*/

   symbolArray = CL_GetSymbolTable(theEnv);
   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
     {
      for (symbolPtr = symbolArray[i]; symbolPtr != NULL; symbolPtr = symbolPtr->next)
        { symbolCount++; }
     }

   /*====================================*/
   /* Count entries in the integer table. */
   /*====================================*/

   integerArray = CL_GetIntegerTable(theEnv);
   for (i = 0; i < INTEGER_HASH_SIZE; i++)
     {
      for (integerPtr = integerArray[i]; integerPtr != NULL; integerPtr = integerPtr->next)
        { integerCount++; }
     }

   /*====================================*/
   /* Count entries in the float table. */
   /*====================================*/

   floatArray = CL_GetFloatTable(theEnv);
   for (i = 0; i < FLOAT_HASH_SIZE; i++)
     {
      for (floatPtr = floatArray[i]; floatPtr != NULL; floatPtr = floatPtr->next)
        { floatCount++; }
     }

   /*====================================*/
   /* Count entries in the bitmap table. */
   /*====================================*/

   bitMapArray = CL_GetBitMapTable(theEnv);
   for (i = 0; i < BITMAP_HASH_SIZE; i++)
     {
      for (bitMapPtr = bitMapArray[i]; bitMapPtr != NULL; bitMapPtr = bitMapPtr->next)
        { bitMapCount++; }
     }

   /*========================*/
   /* Print the info_rmation. */
   /*========================*/

   CL_WriteString(theEnv,STDOUT,"Symbols: ");
   CL_PrintUnsignedInteger(theEnv,STDOUT,symbolCount);
   CL_WriteString(theEnv,STDOUT,"\n");
   CL_WriteString(theEnv,STDOUT,"Integers: ");
   CL_PrintUnsignedInteger(theEnv,STDOUT,integerCount);
   CL_WriteString(theEnv,STDOUT,"\n");
   CL_WriteString(theEnv,STDOUT,"Floats: ");
   CL_PrintUnsignedInteger(theEnv,STDOUT,floatCount);
   CL_WriteString(theEnv,STDOUT,"\n");
   CL_WriteString(theEnv,STDOUT,"BitMaps: ");
   CL_PrintUnsignedInteger(theEnv,STDOUT,bitMapCount);
   CL_WriteString(theEnv,STDOUT,"\n");
  }

#define COUNT_SIZE 21

/*********************************************************/
/* PrimitiveTablesUsageCommand: Prints info_rmation about */
/*   the symbol, float, integer, and bitmap tables.      */
/*********************************************************/
void PrimitiveTablesUsageCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   unsigned long i;
   unsigned long long symbolCounts[COUNT_SIZE], floatCounts[COUNT_SIZE];
   CLIPSLexeme **symbolArray, *symbolPtr;
   CLIPSFloat **floatArray, *floatPtr;
   unsigned long long symbolCount, totalSymbolCount = 0;
   unsigned long long floatCount, totalFloatCount = 0;

   for (i = 0; i < 21; i++)
     {
      symbolCounts[i] = 0;
      floatCounts[i] = 0;
     }

   /*====================================*/
   /* Count entries in the symbol table. */
   /*====================================*/

   symbolArray = CL_GetSymbolTable(theEnv);
   for (i = 0; i < SYMBOL_HASH_SIZE; i++)
     {
      symbolCount = 0;
      for (symbolPtr = symbolArray[i]; symbolPtr != NULL; symbolPtr = symbolPtr->next)
        {
         symbolCount++;
         totalSymbolCount++;
        }

      if (symbolCount < (COUNT_SIZE - 1))
        { symbolCounts[symbolCount]++; }
      else
        { symbolCounts[COUNT_SIZE - 1]++; }
     }

   /*===================================*/
   /* Count entries in the float table. */
   /*===================================*/

   floatArray = CL_GetFloatTable(theEnv);
   for (i = 0; i < FLOAT_HASH_SIZE; i++)
     {
      floatCount = 0;
      for (floatPtr = floatArray[i]; floatPtr != NULL; floatPtr = floatPtr->next)
        {
         floatCount++;
         totalFloatCount++;
        }

      if (floatCount < (COUNT_SIZE - 1))
        { floatCounts[floatCount]++; }
      else
        { floatCounts[COUNT_SIZE - 1]++; }
     }

   /*========================*/
   /* Print the info_rmation. */
   /*========================*/

   CL_WriteString(theEnv,STDOUT,"Total Symbols: ");
   CL_PrintUnsignedInteger(theEnv,STDOUT,totalSymbolCount);
   CL_WriteString(theEnv,STDOUT,"\n");
   for (i = 0; i < COUNT_SIZE; i++)
     {
      CL_PrintUnsignedInteger(theEnv,STDOUT,i);
      CL_WriteString(theEnv,STDOUT," ");
      CL_PrintUnsignedInteger(theEnv,STDOUT,symbolCounts[i]);
      CL_WriteString(theEnv,STDOUT,"\n");
     }

   CL_WriteString(theEnv,STDOUT,"\nTotal Floats: ");
   CL_PrintUnsignedInteger(theEnv,STDOUT,totalFloatCount);
   CL_WriteString(theEnv,STDOUT,"\n");
   for (i = 0; i < COUNT_SIZE; i++)
     {
      CL_PrintUnsignedInteger(theEnv,STDOUT,i);
      CL_WriteString(theEnv,STDOUT," ");
      CL_PrintUnsignedInteger(theEnv,STDOUT,floatCounts[i]);
      CL_WriteString(theEnv,STDOUT,"\n");
     }

  }

#if DEFRULE_CONSTRUCT && DEFTEMPLATE_CONSTRUCT

/******************************************************/
/* ValidateFactIntegrityCommand: Command for checking */
/*   the facts for atom value integrity.              */
/******************************************************/
void ValidateFactIntegrityCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   Fact *theFact;
   Multifield *theSegment;
   size_t i;
   CLIPSLexeme *theSymbol;
   CLIPSFloat *theFloat;
   CLIPSInteger *theInteger;

   if (theEnv->initialized == false)
     {
      returnValue->lexemeValue = TrueSymbol(theEnv);
      return;
     }

   for (theFact = CL_GetNextFact(theEnv,NULL);
        theFact != NULL;
        theFact = CL_GetNextFact(theEnv,theFact))
     {
      if (theFact->patternHeader.busyCount <= 0)
        {
         returnValue->lexemeValue = FalseSymbol(theEnv);
         return;
        }

      theSegment = &theFact->theProposition;

      for (i = 0 ; i < theSegment->length ; i++)
        {
         if ((theSegment->contents[i].header->type == SYMBOL_TYPE) ||
             (theSegment->contents[i].header->type == STRING_TYPE) ||
             (theSegment->contents[i].header->type == INSTANCE_NAME_TYPE))
           {
            theSymbol = theSegment->contents[i].lexemeValue;
            if (theSymbol->count <= 0)
              {
               returnValue->lexemeValue = FalseSymbol(theEnv);
               return;
              }
           }

         if (theSegment->contents[i].header->type == INTEGER_TYPE)
           {
            theInteger = theSegment->contents[i].integerValue;
            if (theInteger->count <= 0)
              {
               returnValue->lexemeValue = FalseSymbol(theEnv);
               return;
              }
           }

         if (theSegment->contents[i].header->type == FLOAT_TYPE)
           {
            theFloat = theSegment->contents[i].floatValue;
            if (theFloat->count <= 0)
              {
               returnValue->lexemeValue = FalseSymbol(theEnv);
               return;
              }
           }
        }
     }

   returnValue->lexemeValue = TrueSymbol(theEnv);
  }

/*************************************************************/
/* ShowFactPatternNetworkCommand: Command for displaying the */
/*   fact pattern network for a specified deftemplate.       */
/*************************************************************/
void ShowFactPatternNetworkCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   struct factPatternNode *patternPtr;
   Deftemplate *theDeftemplate;
   const char *theName;
   int depth = 0, i;

   theName = CL_GetConstructName(context,"show-fpn","template name");
   if (theName == NULL) return;

   theDeftemplate = CL_FindDeftemplate(theEnv,theName);
   if (theDeftemplate == NULL) return;

   patternPtr = theDeftemplate->patternNetwork;
   while (patternPtr != NULL)
     {
      for (i = 0; i < depth; i++) CL_WriteString(theEnv,STDOUT," ");
      if (patternPtr->header.singlefieldNode) CL_WriteString(theEnv,STDOUT,"SF   ");
      else if (patternPtr->header.multifieldNode)
        {
         CL_WriteString(theEnv,STDOUT,"MF");
         if (patternPtr->header.endSlot) CL_WriteString(theEnv,STDOUT,")");
         else CL_WriteString(theEnv,STDOUT,"*");
         CL_PrintUnsignedInteger(theEnv,STDOUT,patternPtr->leaveFields);
         CL_WriteString(theEnv,STDOUT," ");
        }

      CL_WriteString(theEnv,STDOUT,"Slot: ");

      CL_PrintUnsignedInteger(theEnv,STDOUT,patternPtr->whichSlot);
      CL_WriteString(theEnv,STDOUT," Field: ");
      CL_PrintUnsignedInteger(theEnv,STDOUT,patternPtr->whichField);
      CL_WriteString(theEnv,STDOUT," Expression: ");
      if (patternPtr->networkTest == NULL) CL_WriteString(theEnv,STDOUT,"None");
      else CL_PrintExpression(theEnv,STDOUT,patternPtr->networkTest);
      CL_WriteString(theEnv,STDOUT," RightHash: ");
      if (patternPtr->header.rightHash == NULL) CL_WriteString(theEnv,STDOUT,"None");
      else CL_PrintExpression(theEnv,STDOUT,patternPtr->header.rightHash);
      CL_WriteString(theEnv,STDOUT,"\n");

      if (patternPtr->nextLevel == NULL)
        {
         while (patternPtr->rightNode == NULL)
           {
            patternPtr = patternPtr->lastLevel;
            depth--;
            if (patternPtr == NULL) return;
           }
         patternPtr = patternPtr->rightNode;
        }
      else
        {
         patternPtr = patternPtr->nextLevel;
         depth++;
        }
     }
  }

#endif

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM

/***************************************************
  NAME         : PrintObjectPatternNetworkCommand
  DESCRIPTION  : Displays an indented printout of
                 the object pattern network
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Object pattern network displayed
  NOTES        : None
 ***************************************************/
void PrintObjectPatternNetworkCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   char indentbuf[80];

   indentbuf[0] = '\0';
   PrintOPNLevel(theEnv,CL_ObjectNetworkPointer(theEnv),indentbuf,0);
  }

/**********************************************************
  NAME         : PrintOPNLevel
  DESCRIPTION  : Recursivley prints object pattern network
  INPUTS       : 1) The current object pattern network node
                 2) A buffer holding preceding indentation
                    text showing the level in the tree
                 3) The length of the indentation text
  RETURNS      : Nothing useful
  SIDE EFFECTS : Pattern nodes recursively printed
  NOTES        : None
 **********************************************************/
static void PrintOPNLevel(
  Environment *theEnv,
  OBJECT_PATTERN_NODE *pptr,
  char *indentbuf,
  int ilen)
  {
   CLASS_BITMAP *cbmp;
   SLOT_BITMAP *sbmp;
   unsigned i;
   OBJECT_PATTERN_NODE *uptr;
   OBJECT_ALPHA_NODE *alphaPtr;

   while (pptr != NULL)
     {
      CL_WriteString(theEnv,STDOUT,indentbuf);
      if (pptr->alphaNode != NULL)
        CL_WriteString(theEnv,STDOUT,"+");
      CL_WriteString(theEnv,STDOUT,CL_FindIDSlotName(theEnv,pptr->slotNameID)->contents);
      CL_WriteString(theEnv,STDOUT," (");
      CL_PrintUnsignedInteger(theEnv,STDOUT,pptr->slotNameID);
      CL_WriteString(theEnv,STDOUT,") ");
      CL_WriteString(theEnv,STDOUT,pptr->endSlot ? "EPF#" : "PF#");
      CL_PrintUnsignedInteger(theEnv,STDOUT,pptr->whichField);
      CL_WriteString(theEnv,STDOUT," ");
      CL_WriteString(theEnv,STDOUT,pptr->multifieldNode ? "$? " : "? ");
      if (pptr->networkTest != NULL)
        CL_PrintExpression(theEnv,STDOUT,pptr->networkTest);
      CL_WriteString(theEnv,STDOUT,"\n");
      alphaPtr = pptr->alphaNode;
      while (alphaPtr != NULL)
        {
         CL_WriteString(theEnv,STDOUT,indentbuf);
         CL_WriteString(theEnv,STDOUT,"     Classes:");
         cbmp = (CLASS_BITMAP *) alphaPtr->classbmp->contents;
         for (i = 0 ; i <= cbmp->maxid ; i++)
           if (TestBitMap(cbmp->map,i))
             {
              CL_WriteString(theEnv,STDOUT," ");
              CL_WriteString(theEnv,STDOUT,CL_DefclassName(DefclassData(theEnv)->ClassIDMap[i]));
             }
         if (alphaPtr->slotbmp != NULL)
           {
            sbmp = (SLOT_BITMAP *) alphaPtr->slotbmp->contents;
            CL_WriteString(theEnv,STDOUT," *** Slots:");
            for (i = NAME_ID ; i <= sbmp->maxid ; i++)
              if (TestBitMap(sbmp->map,i))
                {
                 for (uptr = pptr ; uptr != NULL ; uptr  = uptr->lastLevel)
                   if (uptr->slotNameID == i)
                     break;
                 if (uptr == NULL)
                   {
                    CL_WriteString(theEnv,STDOUT," ");
                    CL_WriteString(theEnv,STDOUT,CL_FindIDSlotName(theEnv,i)->contents);
                   }
                }
           }
         if (alphaPtr->header.rightHash != NULL)
           {
            CL_WriteString(theEnv,STDOUT," RH: ");
            CL_PrintExpression(theEnv,STDOUT,alphaPtr->header.rightHash);
           }

         CL_WriteString(theEnv,STDOUT,"\n");
         alphaPtr = alphaPtr->nxtInGroup;
        }
      indentbuf[ilen++] = (char) ((pptr->rightNode != NULL) ? '|' : ' ');
      indentbuf[ilen++] = ' ';
      indentbuf[ilen++] = ' ';
      indentbuf[ilen] = '\0';
      PrintOPNLevel(theEnv,pptr->nextLevel,indentbuf,ilen);
      ilen -= 3;
      indentbuf[ilen] = '\0';
      pptr = pptr->rightNode;
     }
  }

#endif

#if OBJECT_SYSTEM

/*******************************************************/
/* InstanceTableUsageCommand: Prints info_rmation about */
/*   the instances in the instance hash table.         */
/*******************************************************/
void InstanceTableUsageCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   unsigned long i;
   int instanceCounts[COUNT_SIZE];
   Instance *ins;
   unsigned long instanceCount, totalInstanceCount = 0;

   for (i = 0; i < COUNT_SIZE; i++)
     { instanceCounts[i] = 0; }

   /*======================================*/
   /* Count entries in the instance table. */
   /*======================================*/

   for (i = 0; i < INSTANCE_TABLE_HASH_SIZE; i++)
     {
      instanceCount = 0;
      for (ins = InstanceData(theEnv)->InstanceTable[i]; ins != NULL; ins = ins->nxtHash)
        {
         instanceCount++;
         totalInstanceCount++;
        }

      if (instanceCount < (COUNT_SIZE - 1))
        { instanceCounts[instanceCount]++; }
      else
        { instanceCounts[COUNT_SIZE - 1]++; }
     }

   /*========================*/
   /* Print the info_rmation. */
   /*========================*/

   CL_WriteString(theEnv,STDOUT,"Total CL_Instances: ");
   CL_PrintUnsignedInteger(theEnv,STDOUT,totalInstanceCount);
   CL_WriteString(theEnv,STDOUT,"\n");
   for (i = 0; i < COUNT_SIZE; i++)
     {
      CL_PrintUnsignedInteger(theEnv,STDOUT,i);
      CL_WriteString(theEnv,STDOUT," ");
      CL_PrintUnsignedInteger(theEnv,STDOUT,instanceCounts[i]);
      CL_WriteString(theEnv,STDOUT,"\n");
     }
  }

#endif

#if DEFRULE_CONSTRUCT

/******************/
/* ExamineMemory: */
/******************/
static void ExamineMemory(
  Environment *theEnv,
  struct joinNode *theJoin,
  struct betaMemory *theMemory)
  {
#if MAC_XCD
#pragma unused(theJoin)
#endif
   if (theMemory->size > 10000)
     {
      /* Set a break point here */
     }
  }

/*************************/
/* TraverseBetaMemories: */
/*************************/
static void TraverseBetaMemories(
  Environment *theEnv,
  struct joinNode *theJoin)
  {
   if (theJoin == NULL)
     { return; }

   if (theJoin->lastLevel != NULL)
     { TraverseBetaMemories(theEnv,theJoin->lastLevel); }

   if (theJoin->depth > 2)
     { ExamineMemory(theEnv,theJoin,theJoin->leftMemory); }

   if (theJoin->joinFromTheRight)
     { TraverseBetaMemories(theEnv,(struct joinNode *) theJoin->rightSideEntryStructure); }

   if ((theJoin->joinFromTheRight) &&
       (((struct joinNode *) (theJoin->rightSideEntryStructure))->depth > 1))
     { ExamineMemory(theEnv,theJoin,theJoin->rightMemory); }
  }

/***********************************/
/* ValidateRuleBetaMemoriesAction: */
/***********************************/
static void ValidateRuleBetaMemoriesAction(
  Environment *theEnv,
  ConstructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
   Defrule *rulePtr;

   for (rulePtr = (Defrule *) theConstruct;
        rulePtr != NULL;
        rulePtr = rulePtr->disjunct)
     {
      TraverseBetaMemories(theEnv,rulePtr->lastJoin);
     }
  }

/*******************************/
/* ValidateBetaMemoriesCommand */
/*******************************/
void ValidateBetaMemoriesCommand(
  Environment *theEnv,
  UDFContext *context,
  UDFValue *returnValue)
  {
   CL_WriteString(theEnv,STDOUT,"ValidateBetaMemories");
   CL_DoForAllConstructs(theEnv,ValidateRuleBetaMemoriesAction,DefruleData(theEnv)->CL_DefruleModuleIndex,false,NULL);
  }

#endif

#endif


