   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*              CLASS COMMANDS HEADER FILE             */
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
/*      6.23: Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
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
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#ifndef _H_classcom

#pragma once

#define _H_classcom

typedef enum
  {
   CONVENIENCE_MODE,
   CONSERVATION_MODE
  } ClassDefaultsMode;

#include "cstrccom.h"
#include "moduldef.h"
#include "object.h"
#include "symbol.h"

   const char             *CL_DefclassName(Defclass *);
   const char             *CL_DefclassPPFo_rm(Defclass *);
   struct defmoduleItemHeader
                          *Get_DefclassModule(Environment *,Defclass *);
   const char             *CL_DefclassModule(Defclass *);
   CLIPSLexeme            *Get_DefclassNamePointer(Defclass *);
   void                    CL_SetNextDefclass(Defclass *,Defclass *);
   void                    SetCL_DefclassPPFo_rm(Environment *,Defclass *,char *);

   Defclass               *CL_FindDefclass(Environment *,const char *);
   Defclass               *CL_FindDefclassInModule(Environment *,const char *);
   Defclass               *CL_LookupDefclassByMdlOrScope(Environment *,const char *);
   Defclass               *Lookup_DefclassInScope(Environment *,const char *);
   Defclass               *CL_LookupDefclassAnywhere(Environment *,Defmodule *,const char *);
   bool                    CL_DefclassInScope(Environment *,Defclass *,Defmodule *);
   Defclass               *CL_GetNextDefclass(Environment *,Defclass *);
   bool                    CL_DefclassIsDeletable(Defclass *);

   void                    CL_UndefclassCommand(Environment *,UDFContext *,UDFValue *);
   ClassDefaultsMode       CL_SetClassDefaultsMode(Environment *,ClassDefaultsMode);
   ClassDefaultsMode       CL_GetClassDefaultsMode(Environment *);
   void                    CL_GetClassDefaultsModeCommand(Environment *,UDFContext *,UDFValue *);
   void                    CL_SetClassDefaultsModeCommand(Environment *,UDFContext *,UDFValue *);

#if DEBUGGING_FUNCTIONS
   void                    CL_PPDefclassCommand(Environment *,UDFContext *,UDFValue *);
   void                    CL_ListDefclassesCommand(Environment *,UDFContext *,UDFValue *);
   void                    CL_ListDefclasses(Environment *,const char *,Defmodule *);
   bool                    CL_DefclassGet_Watch_Instances(Defclass *);
   void                    CL_DefclassSet_Watch_Instances(Defclass *,bool);
   bool                    CL_DefclassGet_WatchSlots(Defclass *);
   void                    CL_DefclassSet_WatchSlots(Defclass *,bool);
   bool                    CL_Defclass_WatchAccess(Environment *,int,bool,Expression *);
   bool                    CL_Defclass_WatchPrint(Environment *,const char *,int,Expression *);
#endif

   void                    CL_GetDefclassListFunction(Environment *,UDFContext *,UDFValue *);
   void                    CL_GetDefclassList(Environment *,CLIPSValue *,Defmodule *);
   bool                    CL_Undefclass(Defclass *,Environment *);
   bool                    CL_HasSuperclass(Defclass *,Defclass *);

   CLIPSLexeme            *CL_CheckClassAndSlot(UDFContext *,const char *,Defclass **);

#if (! BLOAD_ONLY) && (! RUN_TIME)
   void                    CL_SaveDefclasses(Environment *,Defmodule *,const char *,void *);
#endif

#endif /* _H_classcom */
