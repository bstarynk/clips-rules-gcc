   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  10/19/17            */
   /*                                                     */
   /*             FILE I/O ROUTER HEADER FILE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: I/O Router routines which allow files to be used */
/*   as input and output sources.                            */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added environment parameter to CL_GenClose.       */
/*            Added environment parameter to CL_GenOpen.        */
/*                                                           */
/*            Added pragmas to remove compilation warnings.  */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Used gengetc and CL_genungetchar rather than      */
/*            getc and ungetc.                               */
/*                                                           */
/*            Replaced BASIC_IO and ADVANCED_IO compiler     */
/*            flags with the single IO_FUNCTIONS compiler    */
/*            flag.                                          */
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
/*            Added flush, rewind, tell, and seek functions. */
/*                                                           */
/*************************************************************/

#ifndef _H_filertr

#pragma once

#define _H_filertr

#include <stdio.h>

#define FILE_ROUTER_DATA 47

struct fileRouter
  {
   const char *logicalName;
   FILE *stream;
   struct fileRouter *next;
  };

struct fileRouterData
  {
   struct fileRouter *ListOfFileRouters;
  };

#define FileRouterData(theEnv) ((struct fileRouterData *) GetEnvironmentData(theEnv,FILE_ROUTER_DATA))

   void                           CL_InitializeFileRouter(Environment *);
   FILE                          *CL_FindFptr(Environment *,const char *);
   bool                           CL_OpenAFile(Environment *,const char *,const char *,const char *);
   bool                           CL_CloseAllFiles(Environment *);
   bool                           CL_CloseFile(Environment *,const char *);
   bool                           CL_FindFile(Environment *,const char *,void *);
   bool                           CL_FlushAllFiles(Environment *);
   bool                           CL_FlushFile(Environment *,const char *);
   bool                           CL_RewindFile(Environment *,const char *);
   long long                      CL_TellFile(Environment *,const char *);
   bool                           CL_SeekFile(Environment *,const char *,long,int);

#endif /* _H_filertr */






