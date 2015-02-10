#include "thread.h"

     void MyThreadInit (void(*start_funct)(void *), void *args)
     {
       getcontext(&threadList[i].context);
       threadList[i].context.uc_stack.ss_sp   = malloc(STACKSIZE); 
       lastContext.uc_stack.ss_sp             = malloc(STACKSIZE);
       threadList[i].context.uc_stack.ss_size = STACKSIZE;
       threadList[i].context.uc_link          = NULL;
       threadList[i].thread_number=threadList[i].parent_number=0;
       threadList[i].thread_status=0;
       threadList[i].block_counter=0;
       makecontext(&threadList[i].context, (void(*)(void))start_funct,1,args);
       current=threadList[i];
       swapcontext(&lastContext,&(threadList[0].context));
       free(threadList[0].context.uc_stack.ss_sp);
     }

     garbageCollector()
     {
       int k;
       for(k=i-1;k>0;k--)
        if(threadList[k].thread_status==1 && threadList[k].context.uc_stack.ss_sp!=NULL)
         {free(threadList[k].context.uc_stack.ss_sp);    
          threadList[k].context.uc_stack.ss_sp=NULL;}
     }

     MyThread* MyThreadCreate (void(*start_funct)(void *), void *arg)
     {
      /* if(i%100 == 0)
       garbageCollector();*/
       int v=0,h=0;
       i++;
       if(i>=THREADLIMIT)
       {  
          printf(" i = <%d> \n",i);
          for(h=1;h<THREADLIMIT;h++)
          { if(threadList[h].thread_status ==1)
             break;} 
          if(h==THREADLIMIT){printf("Exceeded limit to createthreads<i>\n",i);exit(1);}
          v=i; i=h;
       }             
       getcontext(&threadList[i].context);
       void *p=malloc(STACKSIZE);  
       if(p==NULL){printf("Running out of memory");exit(1);}
       //else{printf("\nthread created <%d> \n",i);}
       threadList[i].context.uc_stack.ss_sp   = p;
       threadList[i].context.uc_stack.ss_size = STACKSIZE;
       threadList[i].context.uc_link          = NULL;
       threadList[i].thread_number =i;
       threadList[i].parent_number=current.thread_number;
       threadList[i].thread_status=0; 
       threadList[i].block_counter=0;
       threadList[i].pblock_ind='N';
       makecontext(&threadList[i].context, (void(*)(void))start_funct,1,arg);
       ready[r1%QUEUESIZE]=i;
       r1++;
       if(v!=0 && v>=THREADLIMIT){i=v;}
       return &(threadList[(v==0)?i:h]); 
    }

    void MyThreadYield(void)
    {
       if(r1==r2){return;}     
       ready[r1%QUEUESIZE]=current.thread_number;
       r1++;
       unsigned short p=ready[r2%QUEUESIZE];  
       r2++;current =threadList[p];
       swapcontext(&(threadList[ready[(r1-1)%QUEUESIZE]].context),&(threadList[p].context));
    }

    void MyThreadExit(void)
    {  
      if(r1==r2 && threadList[current.parent_number].block_counter==0)
      {setcontext(&(lastContext));} 

      if(threadList[current.parent_number].block_counter >0 && threadList[current.thread_number].pblock_ind=='Y')
      {
        if( --(threadList[current.parent_number].block_counter) == 0)
        { 
         ready[r1%QUEUESIZE]=current.parent_number;
         r1++; }     
      }

      unsigned short p=ready[r2%QUEUESIZE];    
      threadList[current.thread_number].thread_status=1; 
      r2++;
      current =threadList[p];
      setcontext(&(threadList[p].context));
    }

   int MyThreadJoin(MyThread *thread)
    {
      if(thread==NULL)return -1;
      if(thread->thread_status == 1)return 0;              
      if(thread->parent_number != current.thread_number) return -1;          
      if(r1==r2)return;

      threadList[current.thread_number].block_counter++;
      threadList[thread->thread_number].pblock_ind='Y';
      unsigned short p=ready[r2%QUEUESIZE];int blk=current.thread_number;  
      r2++;current =threadList[p];
      swapcontext(&(threadList[blk].context),&(threadList[p].context));
      return 0;
    } 

   void MyThreadJoinAll(void)
   {
     if(r1==r2)return;            
     unsigned short j; 
     for(j=r2;j<r1;j++) 
      if(current.thread_number==threadList[ready[j%QUEUESIZE]].parent_number
         && threadList[ready[j%QUEUESIZE]].thread_status==0)
        {threadList[current.thread_number].block_counter++;
         threadList[ready[j%QUEUESIZE]].pblock_ind='Y';}

     if(threadList[current.thread_number].block_counter!=0){ 
      unsigned short p=ready[r2%QUEUESIZE];unsigned short blk=current.thread_number;
      r2++;current =threadList[p];
      swapcontext(&(threadList[blk].context),&(threadList[p].context));
     }
   } 

MySemaphore* MySemaphoreInit(int initialValue)
{ 
    if(initialValue <0){return NULL;}	 
    if(s==SEMALIMIT){printf("Exceeded limit to create semaphores \n");exit(1);}   
    unsigned short k;
    sema[s].sema_number=s;
    sema[s].bt=0;sema[s].bh=0;
    sema[s].value=initialValue;
    for(k=0;k<10;k++) sema[s].block_queue[k]=-1;
    s++;
    return &(sema[s-1]);
}

void MySemaphoreSignal(MySemaphore *sem)
{
    if(sem==NULL)return;
    ++sema[sem->sema_number].value;
    if(sema[sem->sema_number].block_queue[sema[sem->sema_number].bt]!=-1)
    { ready[r1%QUEUESIZE]=sema[sem->sema_number].block_queue[sema[sem->sema_number].bt]; 
      r1++;  sema[sem->sema_number].bt++;}
}

void MySemaphoreWait(MySemaphore *sem)
{ 
   if(sem==NULL)return;
   --sema[sem->sema_number].value;
   if(sema[sem->sema_number].value < 0)
   {
     sema[sem->sema_number].block_queue[sema[sem->sema_number].bh]=current.thread_number;
     ++sema[sem->sema_number].bh;

     if(r1==r2){setcontext(&(lastContext));}

     int p=ready[r2%QUEUESIZE];int blk=current.thread_number;  
     r2++;current =threadList[p];
     swapcontext(&(threadList[blk].context),&(threadList[p].context));
   }
}

int MySemaphoreDestroy(MySemaphore *sem)
{
   if(sem==NULL)return;
   if(sema[sem->sema_number].value < 0){return -1; }     
   int k;for(k=0;k<10;k++) sema[sem->sema_number].block_queue[k]=-1;
   sema[sem->sema_number].bt=0; sema[sem->sema_number].bh=0;
   return 0;
}

