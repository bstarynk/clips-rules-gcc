   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*          DEFTEMPLATE UTILITIES HEADER FILE          */
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
/*      6.23: Added support for templates CL_maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added additional arguments to                  */
/*            CL_InvalidDeftemplateSlotMessage function.        */
/*                                                           */
/*            Added additional arguments to                  */
/*            CL_PrintTemplateFact function.                    */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Used CL_gensprintf instead of sprintf.            */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*            CL_Watch facts for modify command only prints     */
/*            changed slots.                                 */
/*                                                           */
/*************************************************************/

#ifndef _H_tmpltutl

#pragma once

#define _H_tmpltutl

#include "constrnt.h"
#include "evaluatn.h"
#include "expressn.h"
#include "factmngr.h"
#include "symbol.h"

   void                           CL_InvalidDeftemplateSlotMessage(Environment *,const char *,const char *,bool);
   void                           CL_SingleFieldCL_SlotCardinalityError(Environment *,const char *);
   void                           CL_MultiIntoSingleFieldSlotError(Environment *,struct templateSlot *,Deftemplate *);
   void                           CL_CheckTemplateFact(Environment *,Fact *);
   bool                           CL_CheckRHSCL_SlotTypes(Environment *,struct expr *,struct templateSlot *,const char *);
   struct templateSlot           *CL_GetNthSlot(Deftemplate *,long long);
   int                            CL_FindSlotPosition(Deftemplate *,CLIPSLexeme *);
   void                           CL_PrintTemplateFact(Environment *,const char *,Fact *,bool,bool,const char *);
   void                           CL_UpdateDeftemplateScope(Environment *);
   struct templateSlot           *CL_FindSlot(Deftemplate *,CLIPSLexeme *,unsigned short *);
   Deftemplate                   *CL_CreateImpliedDeftemplate(Environment *,CLIPSLexeme *,bool);

#endif /* _H_tmpltutl */



