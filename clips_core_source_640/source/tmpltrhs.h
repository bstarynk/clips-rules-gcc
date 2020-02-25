   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*          DEFTEMPLATE RHS PARSING HEADER FILE        */
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
/*      6.24: Added additional argument required for         */
/*            CL_DeriveDefaultFromConstraints.                  */
/*                                                           */
/*            Added additional argument required for         */
/*            CL_InvalidDeftemplateSlotMessage.                 */
/*                                                           */
/*      6.30: Added const qualifiers to remove C++           */
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
/*************************************************************/

#ifndef _H_tmpltrhs

#pragma once

#define _H_tmpltrhs

#include "expressn.h"
#include "scanner.h"
#include "tmpltdef.h"

struct expr *Parse_AssertTemplate (Environment *, const char *,
				   struct token *, bool *, TokenType, bool,
				   Deftemplate *);

#endif /* _H_tmpltrhs */
