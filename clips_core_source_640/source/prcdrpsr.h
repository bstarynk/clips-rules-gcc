   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/18/16            */
   /*                                                     */
   /*       PROCEDURAL FUNCTIONS PARSER HEADER FILE       */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Local variables set with the bind function     */
/*            persist until a reset/clear command is issued. */
/*                                                           */
/*            Support for long long integers.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
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
/*            CL_Eval support for run time and bload only.      */
/*                                                           */
/*************************************************************/

#ifndef _H_prcdrpsr

#pragma once

#define _H_prcdrpsr

#include "constrnt.h"

struct BindInfo
{
  CLIPSLexeme *name;
  CONSTRAINT_RECORD *constraints;
  struct BindInfo *next;
};

void CL_ProceduralFunctionParsers (Environment *);
struct BindInfo *CL_GetParsedBindNames (Environment *);
void CL_SetParsedBindNames (Environment *, struct BindInfo *);
void CL_ClearParsedBindNames (Environment *);
bool CL_ParsedBindNamesEmpty (Environment *);
unsigned short CL_SearchParsedBindNames (Environment *, CLIPSLexeme *);
unsigned short CL_CountParsedBindNames (Environment *);
void CL_RemoveParsedBindName (Environment *, CLIPSLexeme *);
struct constraintRecord *CL_FindBindConstraints (Environment *,
						 CLIPSLexeme *);

#endif /* _H_prcdrpsr */
