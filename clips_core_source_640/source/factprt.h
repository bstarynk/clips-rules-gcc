   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  07/30/16            */
   /*                                                     */
   /*         FACT RETE PRINT FUNCTIONS HEADER FILE       */
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
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Updates to support new struct members.         */
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

#ifndef _H_factprt

#pragma once

#define _H_factprt

   void                           Print_FactJNCompVars1(Environment *,const char *,void *);
   void                           Print_FactJNCompVars2(Environment *,const char *,void *);
   void                           Print_FactPNCompVars1(Environment *,const char *,void *);
   void                           Print_FactJNGetVar1(Environment *,const char *,void *);
   void                           Print_FactJNGetVar2(Environment *,const char *,void *);
   void                           Print_FactJNGetVar3(Environment *,const char *,void *);
   void                           Print_FactPNGetVar1(Environment *,const char *,void *);
   void                           Print_FactPNGetVar2(Environment *,const char *,void *);
   void                           Print_FactPNGetVar3(Environment *,const char *,void *);
   void                           Print_FactSlotLength(Environment *,const char *,void *);
   void                           Print_FactPNConstant1(Environment *,const char *,void *);
   void                           Print_FactPNConstant2(Environment *,const char *,void *);

#endif /* _H_factprt */


