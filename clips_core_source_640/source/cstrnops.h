   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/18/16            */
   /*                                                     */
   /*           CONSTRAINT OPERATIONS HEADER FILE         */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides functions for perfo_rming operations on  */
/*   constraint records including computing the intersection */
/*   and union of constraint records.                        */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#ifndef _H_cstrnops

#pragma once

#define _H_cstrnops

#include "evaluatn.h"
#include "constrnt.h"

struct constraintRecord *CL_IntersectConstraints (Environment *,
						  struct constraintRecord *,
						  struct constraintRecord *);
struct constraintRecord *CL_UnionConstraints (Environment *,
					      struct constraintRecord *,
					      struct constraintRecord *);
#if (! BLOAD_ONLY)
void CL_RemoveConstantFromConstraint (Environment *, int, void *,
				      CONSTRAINT_RECORD *);
#endif

#endif /* _H_cstrnops */
