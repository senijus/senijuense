#include "mailbox.h"
#include "log.h"   
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h> 

pthread_mutex_t PthreadLock;
PthreadNode_t *PthreadHead;

//创建线程邮箱结构
int MailBoxInit(void)
{
    int ret = 0;

    // 已初始化，直接返回
    if(PthreadHead != NULL)
    { 
        return 0;
    }

    PthreadHead = malloc(sizeof(PthreadNode_t));
    if(NULL == PthreadHead)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to malloc PthreadHead: %s", strerror(errno));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }

    ret = pthread_mutex_init(&PthreadLock, NULL);
    if(ret != 0)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to pthread_mutex_init pthreadlock: %s", strerror(ret));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }

    ret = pthread_mutex_init(&(PthreadHead->ListLock), NULL);
    if(ret != 0)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to pthread_mutex_init listlock: %s", strerror(ret));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }

    ret = pthread_cond_init(&(PthreadHead->ListCond), NULL);
    if(ret != 0)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to pthread_cond_init listcond: %s", strerror(ret));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        pthread_mutex_destroy(&(PthreadHead->ListLock));
        free(PthreadHead);
        return -1;
    }

    PthreadHead->pNextPthreadNode = NULL;
    PthreadHead->pListHead = NULL;
    memset(PthreadHead->pThreadName , 0, sizeof(PthreadHead->pThreadName));
    PthreadHead->Tid = 0;
    PthreadHead->pThreadFun = NULL;

    return 0;
}

int RegisterMailBoxTask(char *pThreadName, void *(*pThreadFun)(void *arg))
{
    PthreadNode_t *pnewthreadnode = NULL;
    int ret = 0;

    // 1. 申请线程节点内存
    pnewthreadnode = malloc(sizeof(PthreadNode_t));
    if(NULL == pnewthreadnode)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to malloc pnewthreadnode: %s", strerror(errno));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }

    // 2. 申请消息链表头节点内存
    pnewthreadnode->pListHead = malloc(sizeof(ListNode_t));
    if(NULL == pnewthreadnode->pListHead)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to malloc plisthead: %s", strerror(errno));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        free(pnewthreadnode);
        return -1;
    }

    // 3. 初始化节点内的互斥锁
    ret = pthread_mutex_init(&(pnewthreadnode->ListLock), NULL);
    if(ret != 0)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to pthread_mutex_init listlock: %s", strerror(ret));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        free(pnewthreadnode->pListHead);
        free(pnewthreadnode);
        return -1;
    }

    // 4. 初始化节点内的条件变量
    ret = pthread_cond_init(&(pnewthreadnode->ListCond), NULL);
    if(ret != 0)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to pthread_cond_init listcond: %s", strerror(ret));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        pthread_mutex_destroy(&(pnewthreadnode->ListLock));
        free(pnewthreadnode->pListHead);
        free(pnewthreadnode);
        return -1;
    }

    // 5. 拷贝线程名（修复strcpy缓冲区溢出风险）
    strncpy(pnewthreadnode->pThreadName, pThreadName, sizeof(pnewthreadnode->pThreadName) - 1);
    pnewthreadnode->pThreadName[sizeof(pnewthreadnode->pThreadName) - 1] = '\0'; // 确保字符串终止
    pnewthreadnode->pThreadFun = pThreadFun;
    pnewthreadnode->pListHead->pNextListNode = NULL;
    pnewthreadnode->pNextPthreadNode = NULL; // 初始化链表指针
    pnewthreadnode->Tid = 0; // 初始化TID，pthread_create后会覆盖

    // 6. 先将节点插入全局链表（加全局锁保护），再创建线程
    pthread_mutex_lock(&PthreadLock); // 加全局锁，防止链表并发修改
    pnewthreadnode->pNextPthreadNode = PthreadHead->pNextPthreadNode;
    PthreadHead->pNextPthreadNode = pnewthreadnode;
    pthread_mutex_unlock(&PthreadLock); // 解锁全局锁

    // 7. 创建线程（此时节点已在链表中，线程启动后能立即匹配到自身）
    ret = pthread_create(&(pnewthreadnode->Tid), NULL, pnewthreadnode->pThreadFun, NULL);
    if(ret != 0)
    {
        // 若线程创建失败，需从链表中移除节点并释放资源
        pthread_mutex_lock(&PthreadLock);

        // 找到前驱节点，删除当前节点
        PthreadNode_t *pPrevNode = PthreadHead;
        while(pPrevNode != NULL && pPrevNode->pNextPthreadNode != pnewthreadnode)
        {
            pPrevNode = pPrevNode->pNextPthreadNode;
        }
        if(pPrevNode != NULL)
        {
            pPrevNode->pNextPthreadNode = pnewthreadnode->pNextPthreadNode;
        }
        pthread_mutex_unlock(&PthreadLock);

        // 释放节点资源
        pthread_cond_destroy(&(pnewthreadnode->ListCond));
        pthread_mutex_destroy(&(pnewthreadnode->ListLock));
        free(pnewthreadnode->pListHead);
        free(pnewthreadnode);

        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to pthread_create: %s", strerror(ret));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }
    
    return 0;
}

int MailBoxWaitAllTask(void)
{
    //遍历邮箱链表
    //依次pthread_join回收任务
    //回收申请的邮箱链表的空间
    int i = 0;
    PthreadNode_t *ptmpPthreadNode = NULL;
    PthreadNode_t *pfreePthreadNode = NULL;
    ListNode_t *ptmpListNode = NULL;
    ListNode_t *pfreeListNode = NULL;

    ptmpPthreadNode = PthreadHead->pNextPthreadNode;
    pfreePthreadNode = PthreadHead->pNextPthreadNode;

    while(ptmpPthreadNode != NULL)
    {
        pthread_join(ptmpPthreadNode->Tid, NULL);

        pthread_cond_destroy(&(ptmpPthreadNode->ListCond));
        pthread_mutex_destroy(&(ptmpPthreadNode->ListLock));

        ptmpListNode = ptmpPthreadNode->pListHead;
        pfreeListNode = ptmpPthreadNode->pListHead;

        while(ptmpListNode != NULL)
        {
            ptmpListNode = ptmpListNode->pNextListNode;
            free(pfreeListNode);
            pfreeListNode = ptmpListNode;
        }

        ptmpPthreadNode = ptmpPthreadNode->pNextPthreadNode;
        free(pfreePthreadNode);
        pfreePthreadNode = ptmpPthreadNode;
    }

    pthread_mutex_destroy(&PthreadLock);
    pthread_mutex_destroy(&(PthreadHead->ListLock));
    pthread_cond_destroy(&(PthreadHead->ListCond));
    free(PthreadHead);

    PthreadHead = NULL;

    return 0;
}

int MailBoxSendMsg(char *pRecvThreadName, Data_t TmpData)
{
    //在邮箱链表中查找对应的接收线程
    //申请队列节点
    //将数据放进去
    //将数据插入队尾
    PthreadNode_t *ptmpPthreadNode = NULL;
    ListNode_t *newListNode = NULL;
    ListNode_t *tmpListNode = NULL;

    ptmpPthreadNode = PthreadHead->pNextPthreadNode;

    pthread_mutex_lock(&PthreadLock); // 加全局锁
    while(ptmpPthreadNode != NULL)
    {
        if(0 == strcmp(ptmpPthreadNode->pThreadName, pRecvThreadName))
        {
            break;
        }
        ptmpPthreadNode = ptmpPthreadNode->pNextPthreadNode;
    }
    pthread_mutex_unlock(&PthreadLock); // 解锁全局锁

    if(NULL == ptmpPthreadNode)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to find RecvThreadName: %s", pRecvThreadName);
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }

    newListNode = malloc(sizeof(ListNode_t));
    if(NULL == newListNode)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to malloc newlistnode: %s", strerror(errno));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }
    newListNode->pNextListNode = NULL;
    newListNode->Tmpdata = TmpData;

    pthread_mutex_lock(&(ptmpPthreadNode->ListLock));
    tmpListNode = ptmpPthreadNode->pListHead;
    while(tmpListNode->pNextListNode != NULL)
    {
        tmpListNode = tmpListNode->pNextListNode;
    }
    tmpListNode->pNextListNode = newListNode;

    // 唤醒等待该消息的线程
    pthread_cond_signal(&(ptmpPthreadNode->ListCond));
    
    // 解锁
    pthread_mutex_unlock(&(ptmpPthreadNode->ListLock));

    // 新增：发送成功日志
    char log_buf[256] = {0};
    snprintf(log_buf, sizeof(log_buf), "MailBoxSendMsg: send msg success, threadName=%s", pRecvThreadName);
    LogWrite(LOG_LEVEL_INFO, log_buf);

    return 0;
}

int MailBoxRecvMsg(Data_t *pTmpData)
{
    //根据pthread_self()在邮箱链表中查找自己的线程任务节点
    //从队列中拿去队头的信息
    //如果没有队头信息则循环直到拿到信息继续执行
    pthread_t tid;
    PthreadNode_t *ptmpPthreadNode = NULL;
    ListNode_t *freeListNode = NULL;

    tid = pthread_self();
    ptmpPthreadNode = PthreadHead->pNextPthreadNode;

    while(ptmpPthreadNode != NULL)
    {
        if(ptmpPthreadNode->Tid == tid)
        {
            break;
        }
        ptmpPthreadNode = ptmpPthreadNode->pNextPthreadNode;
    }

    if(NULL == ptmpPthreadNode)
    {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "fail to find self thread node, tid=%lu", (unsigned long)tid);
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }

    // 加锁保护条件变量和链表操作
    pthread_mutex_lock(&(ptmpPthreadNode->ListLock));

    // 无消息时阻塞等待（条件变量），替代轮询
    while(ptmpPthreadNode->pListHead->pNextListNode == NULL)
    {
        // 阻塞时会自动释放锁，被唤醒后重新获取锁
        pthread_cond_wait(&(ptmpPthreadNode->ListCond), &(ptmpPthreadNode->ListLock));
    }

    // 有消息时取出队头
    freeListNode = ptmpPthreadNode->pListHead->pNextListNode;
    *pTmpData = freeListNode->Tmpdata;
    ptmpPthreadNode->pListHead->pNextListNode = freeListNode->pNextListNode;
    free(freeListNode);
    freeListNode = NULL;

    // 解锁
    pthread_mutex_unlock(&(ptmpPthreadNode->ListLock));

    // 新增：接收成功日志
    char log_buf[256] = {0};
    snprintf(log_buf, sizeof(log_buf), "MailBoxRecvMsg: recv msg success, threadName=%s", ptmpPthreadNode->pThreadName);
    LogWrite(LOG_LEVEL_INFO, log_buf);

    return 0;
}