   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  02/03/18            */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*            CL_ResetObjectMatchTimeTags did not pass in the   */
/*            environment argument when BLOAD_ONLY was set.  */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Added support for hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*            Added support for hashed alpha memories.       */
/*                                                           */
/*      6.31: Optimization for marking relevant alpha nodes  */
/*            in the object pattern network.                 */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_objrtbin

#pragma once

#define _H_objrtbin

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM

#define OBJECTRETEBIN_DATA 34

#ifndef _H_objrtmch
#include "objrtmch.h"
#endif

struct objectReteBinaryData
  {
   unsigned long AlphaNodeCount;
   unsigned long PatternNodeCount;
   unsigned long AlphaLinkCount;
   OBJECT_ALPHA_NODE *AlphaArray;
   OBJECT_PATTERN_NODE *PatternArray;
   CLASS_ALPHA_LINK *AlphaLinkArray;
  };

#define ObjectReteBinaryData(theEnv) ((struct objectReteBinaryData *) GetEnvironmentData(theEnv,OBJECTRETEBIN_DATA))

#define ClassAlphaPointer(i)   ((i == ULONG_MAX) ? NULL : (CLASS_ALPHA_LINK *) &ObjectReteBinaryData(theEnv)->AlphaLinkArray[i])

   void                    SetupObjectPatterns_Bload(Environment *);

#endif /* DEFRULE_CONSTRUCT && OBJECT_SYSTEM */

#endif /* _H_objrtbin */



