#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>

#define SEMALIMIT 100 
#define STACKSIZE 8192
#define THREADLIMIT 1000
#define QUEUESIZE 1000

typedef struct MyThread{
  unsigned short thread_number;
  unsigned short parent_number;
  unsigned short thread_status;
  unsigned short block_counter;
  char pblock_ind;
  ucontext_t context;
 }MyThread;

MyThread threadList[THREADLIMIT]={};
unsigned short i=0;
//int jar=-1;
MyThread current={0};
ucontext_t lastContext;
unsigned short ready[QUEUESIZE]={0};
unsigned short r1=0,r2=0;

typedef struct MySemaphore{
  int value;
  unsigned short sema_number;
  int block_queue[100];
  unsigned short bh,bt;
 }MySemaphore;

MySemaphore sema[SEMALIMIT]={0};
unsigned short s=0;
