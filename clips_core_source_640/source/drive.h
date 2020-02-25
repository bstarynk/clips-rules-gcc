   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*                  DRIVE HEADER FILE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Handles join network activity associated with    */
/*   with the addition of a data entity such as a fact or    */
/*   instance.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added support for hashed memories.             */
/*                                                           */
/*            Added additional developer statistics to help  */
/*            analyze join network perfo_rmance.              */
/*                                                           */
/*            Removed pseudo-facts used in not CE.           */
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

#ifndef _H_drive

#pragma once

#define _H_drive

#include "expressn.h"
#include "match.h"
#include "network.h"

void Network_Assert (Environment *, struct partialMatch *, struct joinNode *);
bool CL_EvaluateJoinExpression (Environment *, struct expr *,
				struct joinNode *);
void Network_AssertLeft (Environment *, struct partialMatch *,
			 struct joinNode *, int);
void Network_AssertRight (Environment *, struct partialMatch *,
			  struct joinNode *, int);
void CL_PPDrive (Environment *, struct partialMatch *, struct partialMatch *,
		 struct joinNode *, int);
unsigned long CL_BetaMemoryHashValue (Environment *, struct expr *,
				      struct partialMatch *,
				      struct partialMatch *,
				      struct joinNode *);
bool CL_EvaluateSecondaryNetworkTest (Environment *, struct partialMatch *,
				      struct joinNode *);
void CL_EPMDrive (Environment *, struct partialMatch *, struct joinNode *,
		  int);

#endif /* _H_drive */
