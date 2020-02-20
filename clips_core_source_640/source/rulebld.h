   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  06/28/17            */
   /*                                                     */
   /*               RULE BUILD HEADER FILE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines to ntegrates a set of pattern  */
/*   and join tests associated with a rule into the pattern  */
/*   and join networks. The joins are integrated into the    */
/*   join network by routines in this module. The pattern    */
/*   is integrated by calling the external routine           */
/*   associated with the pattern parser that originally      */
/*   parsed the pattern.                                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Removed INCREMENTAL_RESET compilation flag.    */
/*                                                           */
/*            Corrected code to remove compiler warnings.    */
/*                                                           */
/*      6.30: Changes to constructing join network.          */
/*                                                           */
/*            Added support for hashed memories.             */
/*                                                           */
/*      6.31: DR#882 Logical retraction not working if       */
/*            logical CE starts with test CE.                */
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
/*************************************************************/

#ifndef _H_rulebld

#pragma once

#define _H_rulebld

#include "network.h"
#include "reorder.h"

   struct joinNode               *CL_ConstructJoins(Environment *,int,struct lhsParseNode *,int,struct joinNode *,bool,bool);
   void                           CL_AttachTestCEsToPatternCEs(Environment *,struct lhsParseNode *);

#endif /* _H_rulebld */



