   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/18/16            */
   /*                                                     */
   /*             ENVRNMNT BUILD HEADER FILE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for supporting multiple environments.   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.40: Added to separate environment creation and     */
/*            deletion code.                                 */
/*                                                           */
/*************************************************************/

#ifndef _H_envrnbld

#pragma once

#define _H_envrnbld

#include <stdbool.h>

#include "envrnmnt.h"
#include "extnfunc.h"

   Environment                   *CL_CreateEnvironment(void);
   Environment                   *CL_CreateCL_RuntimeEnvironment(CLIPSLexeme **,CLIPSFloat **,
                                                           CLIPSInteger **,CLIPSBitMap **,
                                                           struct functionDefinition *);
   bool                           CL_DestroyEnvironment(Environment *);

#endif /* _H_envrnbld */

