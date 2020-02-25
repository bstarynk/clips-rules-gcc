   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*            STRING_TYPE I/O ROUTER HEADER FILE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: I/O Router routines which allow strings to be    */
/*   used as input and output sources.                       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Used CL_genstrcpy instead of strcpy.              */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
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

#ifndef _H_strngrtr

#pragma once

#define _H_strngrtr

typedef struct stringRouter StringRouter;
typedef struct string_BuilderRouter String_BuilderRouter;

#include <stdio.h>
#include "utility.h"

#define STRING_ROUTER_DATA 48

struct stringRouter
{
  const char *name;
  const char *readString;
  char *writeString;
  size_t currentPosition;
  size_t maximumPosition;
  int read_WriteType;
  StringRouter *next;
};

struct string_BuilderRouter
{
  const char *name;
  String_Builder *SBR;
  String_BuilderRouter *next;
};

struct stringRouterData
{
  StringRouter *ListOfStringRouters;
  String_BuilderRouter *ListOfString_BuilderRouters;
};

#define StringRouterData(theEnv) ((struct stringRouterData *) GetEnvironmentData(theEnv,STRING_ROUTER_DATA))

/**************************/
/* I/O ROUTER DEFINITIONS */
/**************************/

void CL_InitializeStringRouter (Environment *);
bool CL_OpenStringSource (Environment *, const char *, const char *, size_t);
bool CL_OpenTextSource (Environment *, const char *, const char *, size_t,
			size_t);
bool CL_CloseStringSource (Environment *, const char *);
bool CL_OpenStringDestination (Environment *, const char *, char *, size_t);
bool CL_CloseStringDestination (Environment *, const char *);
bool OpenString_BuilderDestination (Environment *, const char *,
				    String_Builder *);
bool CloseString_BuilderDestination (Environment *, const char *);

#endif /* _H_strngrtr */
