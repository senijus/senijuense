#ifndef __MAILBOX_H__
#define __MAILBOX_H__

#include <pthread.h>

typedef struct Data
{
    double Tem;
    double Hum;
    double Volt;
    double Elec;
}Data_t;

typedef struct ListNode
{
    Data_t Tmpdata;
    struct ListNode *pNextListNode;
}ListNode_t;

typedef struct PthreadNode
{
    pthread_t Tid;
    char pThreadName[32];
    void *(* pThreadFun)(void *);
    struct PthreadNode *pNextPthreadNode;
    struct ListNode *pListHead;
    pthread_mutex_t ListLock;
    pthread_cond_t ListCond;
}PthreadNode_t;

extern int MailBoxInit(void);
extern int RegisterMailBoxTask(char *pThreadName, void *(*pThreadFun)(void *arg));
extern int MailBoxWaitAllTask(void);
extern int MailBoxSendMsg(char *pRecvThreadName, Data_t TmpData);
extern int MailBoxRecvMsg(Data_t *pTmpData);

#endif