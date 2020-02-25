   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*          CONSTRAINT CONSTRUCTS-TO-C HEADER          */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the constructs-to-c feature for       */
/*    constraint records.                                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added allowed-classes slot facet.              */
/*                                                           */
/*            Added environment parameter to CL_GenClose.       */
/*                                                           */
/*      6.30: Added support for path name argument to        */
/*            constructs-to-c.                               */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#ifndef _H_cstrncmp

#pragma once

#define _H_cstrncmp

#include "evaluatn.h"
#include "constrnt.h"

#include <stdio.h>

void CL_PrintConstraintReference (Environment *, FILE *, CONSTRAINT_RECORD *,
				  unsigned int, unsigned int);
void ConstraintRecordToCode (FILE *, CONSTRAINT_RECORD *);
void CL_ConstraintsToCode (Environment *, const char *, const char *, char *,
			   unsigned int, FILE *, unsigned int, unsigned int);

#endif /* _H_cstrncmp */
