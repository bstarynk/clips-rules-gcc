   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
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
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_cstrcbin

#pragma once

#define _H_cstrcbin

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE

struct bsaveConstructHeader
{
  unsigned long name;
  unsigned long whichModule;
  unsigned long next;
};

#include "constrct.h"

#if BLOAD_AND_BSAVE
void CL_MarkConstructHeaderNeededItems (ConstructHeader *, unsigned long);
void CL_Assign_BsaveConstructHeaderVals (struct bsaveConstructHeader *,
					 ConstructHeader *);
#endif

void CL_UpdateConstructHeader (Environment *, struct bsaveConstructHeader *,
			       ConstructHeader *, ConstructType, size_t,
			       void *, size_t, void *);
void CL_UnmarkConstructHeader (Environment *, ConstructHeader *);

#endif

#endif
