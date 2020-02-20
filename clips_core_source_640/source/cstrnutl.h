   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*            CONSTRAINT UTILITY HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Utility routines for manipulating, initializing, */
/*   creating, copying, and comparing constraint records.    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian Dantes                                         */
/*                                                           */
/* Revision History:                                         */
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
/*************************************************************/

#ifndef _H_cstrnutl

#pragma once

#define _H_cstrnutl

#include "constrnt.h"

   struct constraintRecord       *CL_GetConstraintRecord(Environment *);
   int                            CL_CompareNumbers(Environment *,int,void *,int,void *);
   struct constraintRecord       *CL_CopyConstraintRecord(Environment *,CONSTRAINT_RECORD *);
   bool                           CL_SetConstraintType(int,CONSTRAINT_RECORD *);
   void                           CL_SetAnyAllowedFlags(CONSTRAINT_RECORD *,bool);
   void                           CL_SetAnyRestrictionFlags(CONSTRAINT_RECORD *,bool);
   CONSTRAINT_RECORD             *CL_FunctionCallToConstraintRecord(Environment *,void *);
   CONSTRAINT_RECORD             *CL_ExpressionToConstraintRecord(Environment *,struct expr *);
   CONSTRAINT_RECORD             *CL_ArgumentTypeToConstraintRecord(Environment *,unsigned);

#endif /* _H_cstrnutl */


