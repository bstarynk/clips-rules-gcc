   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  10/01/16             */
   /*                                                     */
   /*           FACT RETE PRINT FUNCTIONS MODULE          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Print routines for the fact rete primitives.     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Updates to support new struct members.         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT

#include "envrnmnt.h"
#include "factgen.h"
#include "prntutil.h"
#include "router.h"
#include "symbol.h"

#include "factprt.h"

/***************************************/
/* Print_FactJNCompVars1: Print routine */
/*   for the CL_FactJNCompVars1 function. */
/***************************************/
void Print_FactJNCompVars1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factCompVarsJN1Call *hack;

   hack = (struct factCompVarsJN1Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-jn-cmp-vars1 ");
   if (hack->pass) CL_WriteString(theEnv,logicalName,"= ");
   else CL_WriteString(theEnv,logicalName,"<> ");

   CL_WriteString(theEnv,logicalName,"p");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->pattern1 + 1);

   if (hack->p1lhs)
     { CL_WriteString(theEnv,logicalName," L"); }
   else if (hack->p1rhs)
     { CL_WriteString(theEnv,logicalName," R"); }

   CL_WriteString(theEnv,logicalName," s");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->slot1);

   CL_WriteString(theEnv,logicalName," p");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->pattern2 + 1);

   if (hack->p2lhs)
     { CL_WriteString(theEnv,logicalName," L"); }
   else if (hack->p2rhs)
     { CL_WriteString(theEnv,logicalName," R"); }

   CL_WriteString(theEnv,logicalName," s");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->slot2);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/***************************************/
/* Print_FactJNCompVars2: Print routine */
/*   for the CL_FactJNCompVars2 function. */
/***************************************/
void Print_FactJNCompVars2(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factCompVarsJN2Call *hack;

   hack = (struct factCompVarsJN2Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-jn-cmp-vars2 ");
   if (hack->pass) CL_WriteString(theEnv,logicalName,"= ");
   else CL_WriteString(theEnv,logicalName,"<> ");

   CL_WriteString(theEnv,logicalName,"p");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->pattern1 + 1);

   if (hack->p1lhs)
     { CL_WriteString(theEnv,logicalName," L"); }
   else if (hack->p1rhs)
     { CL_WriteString(theEnv,logicalName," R"); }

   CL_WriteString(theEnv,logicalName," s");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->slot1);

   if (hack->fromBeginning1) CL_WriteString(theEnv,logicalName, " b");
   else CL_WriteString(theEnv,logicalName," e");

   CL_WriteString(theEnv,logicalName," f");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->offset1);

   CL_WriteString(theEnv,logicalName," p");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->pattern2 + 1);

   if (hack->p2lhs)
     { CL_WriteString(theEnv,logicalName," L"); }
   else if (hack->p2rhs)
     { CL_WriteString(theEnv,logicalName," R"); }

   CL_WriteString(theEnv,logicalName," s");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->slot2);

   if (hack->fromBeginning2) CL_WriteString(theEnv,logicalName," b");
   else CL_WriteString(theEnv,logicalName," e");

   CL_WriteString(theEnv,logicalName," f");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->offset2);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/***************************************/
/* Print_FactPNCompVars1: Print routine */
/*   for the CL_FactPNCompVars1 function. */
/***************************************/
void Print_FactPNCompVars1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factCompVarsPN1Call *hack;

   hack = (struct factCompVarsPN1Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-pn-cmp-vars ");
   if (hack->pass) CL_WriteString(theEnv,logicalName,"p ");
   else CL_WriteString(theEnv,logicalName,"n ");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->field1);
   CL_WriteString(theEnv,logicalName," ");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->field2);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/**************************************/
/* Print_FactSlotLength: Print routine */
/*   for the CL_FactSlotLength function. */
/**************************************/
void Print_FactSlotLength(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factCheckLengthPNCall *hack;

   hack = (struct factCheckLengthPNCall *) ((CLIPSBitMap *) theValue)->contents;

   CL_WriteString(theEnv,logicalName,"(slot-length ");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);
   CL_WriteString(theEnv,logicalName," ");
   if (hack->exactly) CL_WriteString(theEnv,logicalName,"= ");
   else CL_WriteString(theEnv,logicalName,">= ");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->minLength);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/*************************************/
/* Print_FactJNGetVar1: Print routine */
/*   for the FactJNGetvar1 function. */
/*************************************/
void Print_FactJNGetVar1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factGetVarJN1Call *hack;

   hack = (struct factGetVarJN1Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-jn-getvar-1 ");
   if (hack->factAddress) CL_WriteString(theEnv,logicalName,"t ");
   else CL_WriteString(theEnv,logicalName,"f ");
   if (hack->allFields) CL_WriteString(theEnv,logicalName,"t ");
   else CL_WriteString(theEnv,logicalName,"f ");

   CL_WriteString(theEnv,logicalName,"p");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichPattern + 1);
   CL_WriteString(theEnv,logicalName," ");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichField);
   CL_WriteString(theEnv,logicalName," s");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);

   if (hack->lhs)
     { CL_WriteString(theEnv,logicalName," L"); }
   else if (hack->rhs)
     { CL_WriteString(theEnv,logicalName," R"); }
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/*************************************/
/* Print_FactJNGetVar2: Print routine */
/*   for the FactJNGetvar2 function. */
/*************************************/
void Print_FactJNGetVar2(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factGetVarJN2Call *hack;

   hack = (struct factGetVarJN2Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-jn-getvar-2");

   CL_WriteString(theEnv,logicalName," p");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichPattern + 1);
   CL_WriteString(theEnv,logicalName," s");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);
   if (hack->lhs)
     { CL_WriteString(theEnv,logicalName," L"); }
   else if (hack->rhs)
     { CL_WriteString(theEnv,logicalName," R"); }
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/*************************************/
/* Print_FactJNGetVar3: Print routine */
/*   for the CL_FactJNGetVar3 function. */
/*************************************/
void Print_FactJNGetVar3(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factGetVarJN3Call *hack;

   hack = (struct factGetVarJN3Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-jn-getvar-3 ");
   if (hack->fromBeginning) CL_WriteString(theEnv,logicalName,"t ");
   else CL_WriteString(theEnv,logicalName,"f ");
   if (hack->fromEnd) CL_WriteString(theEnv,logicalName,"t ");
   else CL_WriteString(theEnv,logicalName,"f ");

   CL_PrintUnsignedInteger(theEnv,logicalName,hack->beginOffset);
   CL_WriteString(theEnv,logicalName," ");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->endOffset);
   CL_WriteString(theEnv,logicalName," ");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);

   CL_WriteString(theEnv,logicalName," p");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichPattern + 1);

   if (hack->lhs)
     { CL_WriteString(theEnv,logicalName," L"); }
   else if (hack->rhs)
     { CL_WriteString(theEnv,logicalName," R"); }

   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/*************************************/
/* Print_FactPNGetVar1: Print routine */
/*   for the FactPNGetvar1 function. */
/*************************************/
void Print_FactPNGetVar1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factGetVarPN1Call *hack;

   hack = (struct factGetVarPN1Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-pn-getvar-1 ");
   if (hack->factAddress) CL_WriteString(theEnv,logicalName,"t ");
   else CL_WriteString(theEnv,logicalName,"f ");
   if (hack->allFields) CL_WriteString(theEnv,logicalName,"t F");
   else CL_WriteString(theEnv,logicalName,"f F");

   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichField);
   CL_WriteString(theEnv,logicalName," S");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/*************************************/
/* Print_FactPNGetVar2: Print routine */
/*   for the FactPNGetvar2 function. */
/*************************************/
void Print_FactPNGetVar2(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factGetVarPN2Call *hack;

   hack = (struct factGetVarPN2Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-pn-getvar-2 S");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/*************************************/
/* Print_FactPNGetVar3: Print routine */
/*   for the FactPNGetvar3 function. */
/*************************************/
void Print_FactPNGetVar3(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factGetVarPN3Call *hack;

   hack = (struct factGetVarPN3Call *) ((CLIPSBitMap *) theValue)->contents;
   CL_WriteString(theEnv,logicalName,"(fact-pn-getvar-3 ");

   if (hack->fromBeginning) CL_WriteString(theEnv,logicalName,"t ");
   else CL_WriteString(theEnv,logicalName,"f ");
   if (hack->fromEnd) CL_WriteString(theEnv,logicalName,"t B");
   else CL_WriteString(theEnv,logicalName,"f B");

   CL_PrintUnsignedInteger(theEnv,logicalName,hack->beginOffset);
   CL_WriteString(theEnv,logicalName," E");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->endOffset);
   CL_WriteString(theEnv,logicalName," S");
   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/***************************************/
/* Print_FactPNConstant1: Print routine */
/*   for the CL_FactPNConstant1 function. */
/***************************************/
void Print_FactPNConstant1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factConstantPN1Call *hack;

   hack = (struct factConstantPN1Call *) ((CLIPSBitMap *) theValue)->contents;

   CL_WriteString(theEnv,logicalName,"(fact-pn-constant1 ");

   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);

   if (hack->testForEquality) CL_WriteString(theEnv,logicalName," = ");
   else CL_WriteString(theEnv,logicalName," != ");

   CL_PrintAtom(theEnv,logicalName,GetFirstArgument()->type,GetFirstArgument()->value);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/***************************************/
/* Print_FactPNConstant2: Print routine */
/*   for the CL_FactPNConstant2 function. */
/***************************************/
void Print_FactPNConstant2(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct factConstantPN2Call *hack;

   hack = (struct factConstantPN2Call *) ((CLIPSBitMap *) theValue)->contents;

   CL_WriteString(theEnv,logicalName,"(fact-pn-constant2 ");

   CL_PrintUnsignedInteger(theEnv,logicalName,hack->whichSlot);

   CL_WriteString(theEnv,logicalName," ");

   CL_PrintUnsignedInteger(theEnv,logicalName,hack->offset);

   if (hack->testForEquality) CL_WriteString(theEnv,logicalName," = ");
   else CL_WriteString(theEnv,logicalName," != ");

   CL_PrintAtom(theEnv,logicalName,GetFirstArgument()->type,GetFirstArgument()->value);
   CL_WriteString(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

#endif /* DEFTEMPLATE_CONSTRUCT && DEFRULE_CONSTRUCT */


