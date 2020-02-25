   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*                  USER DATA MODULE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for attaching user data to constructs,  */
/*   facts, instances, user functions, etc.                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include <stdlib.h>

#include "setup.h"

#include "envrnmnt.h"

#include "userdata.h"

/*************************************************/
/* CL_InitializeUserDataData: Allocates environment */
/*    data for user data routines.               */
/*************************************************/
void CL_InitializeUserDataData(
  Environment *theEnv)
  {
   CL_AllocateEnvironmentData(theEnv,USER_DATA_DATA,sizeof(struct userDataData),NULL);
  }

/******************************************************/
/* CL_InstallUserDataRecord: Installs a user data record */
/*   in the user data record array and returns the    */
/*   integer data ID associated with the record.      */
/******************************************************/
unsigned char CL_InstallUserDataRecord(
  Environment *theEnv,
  struct userDataRecord *theRecord)
  {
   theRecord->dataID = UserDataData(theEnv)->UserDataRecordCount;
   UserDataData(theEnv)->UserDataRecordArray[UserDataData(theEnv)->UserDataRecordCount] = theRecord;
   return(UserDataData(theEnv)->UserDataRecordCount++);
  }

/*****************************************************/
/* CL_FetchUserData: Searches for user data info_rmation */
/*   from a list of user data structures. A new user */
/*   data structure is created if one is not found.  */
/*****************************************************/
struct userData *CL_FetchUserData(
  Environment *theEnv,
  unsigned char userDataID,
  struct userData **theList)
  {
   struct userData *theData;

   for (theData = *theList;
        theData != NULL;
        theData = theData->next)
     {
      if (theData->dataID == userDataID)
        { return(theData); }
     }

   theData = (struct userData *) (*UserDataData(theEnv)->UserDataRecordArray[userDataID]->createUserData)(theEnv);
   theData->dataID = userDataID;
   theData->next = *theList;
   *theList = theData;

   return(theData);
  }

/*****************************************************/
/* CL_TestUserData: Searches for user data info_rmation  */
/*   from a list of user data structures. NULL is    */
/*   returned if the appropriate user data structure */
/*   is not found.                                   */
/*****************************************************/
struct userData *CL_TestUserData(
  unsigned char userDataID,
  struct userData *theList)
  {
   struct userData *theData;

   for (theData = theList;
        theData != NULL;
        theData = theData->next)
     {
      if (theData->dataID == userDataID)
        { return(theData); }
     }

   return NULL;
  }

/***************************************************************/
/* CL_ClearUserDataList: Deallocates a linked list of user data.  */
/***************************************************************/
void CL_ClearUserDataList(
  Environment *theEnv,
  struct userData *theList)
  {
   struct userData *nextData;

   while (theList != NULL)
     {
      nextData = theList->next;
      (*UserDataData(theEnv)->UserDataRecordArray[theList->dataID]->deleteUserData)(theEnv,theList);
      theList = nextData;
     }
  }

/*************************************************/
/* CL_DeleteUserData: Removes user data info_rmation */
/*   from a list of user data structures.        */
/*************************************************/
struct userData *CL_DeleteUserData(
  Environment *theEnv,
  unsigned char userDataID,
  struct userData *theList)
  {
   struct userData *theData, *lastData = NULL;

   for (theData = theList;
        theData != NULL;
        theData = theData->next)
     {
      if (theData->dataID == userDataID)
        {
         if (lastData == NULL)
           { theList = theData->next; }
         else
           { lastData->next = theData->next; }

         (*UserDataData(theEnv)->UserDataRecordArray[userDataID]->deleteUserData)(theEnv,theData);
         return(theList);
        }

      lastData = theData;
     }

   return(theList);
  }

