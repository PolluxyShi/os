#include <os.h>
#include <spinlock.h>

#define copy_name(dest,src) \
    (dest=pmm->alloc(strlen(src)+1), \
    strcpy(dest,src) )

volatile int ncli[MAX_CPU],intena[MAX_CPU];

task_t *tasks[40]={};
task_t *currents[MAX_CPU]={};
task_t idles[MAX_CPU];
task_t *lasts[MAX_CPU];
static spin_lock_t tasks_lk = SPIN_INIT();
int tasks_idx=0,tasks_cnt=0;

#define current currents[cpu_id]
#define last lasts[cpu_id]

#define set_flag(A,B) \
    { \
        uintptr_t p=(uintptr_t)&A->attr; \
        asm volatile("lock orl %1,(%0)"::"r"(p),"g"((B))); \
    }
    
#define neg_flag(A,B) \
    { \
        uintptr_t p=(uintptr_t)&A->attr; \
        asm volatile("lock andl %1,(%0)"::"r"(p),"g"(~(B))); \
    }

static int add_task(task_t *task){
    spin_lock(&tasks_lk);
    int ret=tasks_cnt;
    tasks[tasks_cnt++]=task;
    spin_unlock(&tasks_lk);
    return ret;
}

static Context* kmt_context_save(Event ev, Context *c){
    int cpu_id=cpu_current();
    if(last && current!=last){
        last->cpu=-1;
        spin_unlock(&last->running);
    }
    last=current;
    if(current){
        current->context=*c;
        current->ncli=ncli[cpu_id];
        current->intena=intena[cpu_id];
        // report_if(current->ncli<0);
    }
    return NULL;
}

static Context* kmt_context_switch(Event ev, Context *c){
    int cpu_id=cpu_current(),new=-1;
    // Assert(_intr_read()==0,"%d",cpu_id);
    assert(ienabled()==0);
    int cnt=10000;

    do{
        //current=rand()%tasks_cnt;
        new=rand()%tasks_cnt;
        --cnt;
        if(new>=tasks_cnt){new=0;}
        if(cnt==0){
            // Assert(_intr_read()==0,"%d",cpu_id);
            assert(ienabled()==0);
            current=NULL;
            return &idles[cpu_id].context;
        }
    }while(tasks[new]->attr ||
           spin_trylock(&tasks[new]->running));

    current=tasks[new];
    current->cpu=cpu_id;

    // for(int i=0;i<4;++i){
    //     if(current->fence1[i]!=0x13579ace||current->fence2[i]!=0xeca97531){
    //         log("Stack over/under flow!\n");
    //         while(1);
    //     }
    // }
    ncli[cpu_id]=current->ncli;
    intena[cpu_id]=current->intena;
    return &tasks[new]->context;
}

void idle(void *arg){
    while(1)yield();
}

void kmt_init(void){
    os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_context_switch);
    for(int i=0;i<cpu_count();++i){
        idles[i].attr=TASK_RUNABLE;
        idles[i].running=SPIN_INIT();
        idles[i].context= *kcontext(
            (Area){
            (void*)idles[i].stack,
            &(idles[i].stack_end)
            }, idle, NULL);
    }
}

int kmt_create(task_t *task, const char *name, void (*entry)(void*), void *arg){
    static int ignore_num=0;
    if(ignore_num>0){
        --ignore_num;
        return 0;
    }
    //task->id=tasks_cnt;
    printf("create (%d)%s\n",tasks_cnt,name);
    int task_idx=add_task(task);
    // Assert(tasks_cnt<LEN(tasks),"%d\n",tasks_cnt);
    assert(tasks_cnt<LEN(tasks));
    task->attr=TASK_RUNABLE;
    task->running=SPIN_INIT();
    task->ncli=0;
    copy_name(task->name,name);

    task->context = *kcontext(
            (Area){
            (void*)task->stack,
            &(task->stack_end)
            }, entry, arg);
    return task_idx;
}

void kmt_teardown(task_t *task){
    // Assert(0);
    assert(0);
    pmm->free(task->name);
    return ;
}

void kmt_spin_init(spinlock_t *lk, const char *name){
    lk->locked=SPIN_INIT();
    copy_name(lk->name,name);
}

// pthread_mutex_t exclu_lk=PTHREAD_MUTEX_INITIALIZER;
void kmt_spin_lock(spinlock_t *lk){
    iset(0);
    int cpu_id=cpu_current();
    while(1){
        if(lk->locked){
            if(lk->owner==current){
                ++lk->reen;
                break;
            }else{
                while(lk->locked);
            }
        }
        spin_lock(&lk->locked);
        iset(0);
        lk->reen=1;
        lk->owner=current;
        break;
    }//Use break to release lock and restore intr
    iset(1);
}

void kmt_spin_unlock(spinlock_t *lk){
    int cpu_id=cpu_current();
    if(lk->locked){
        if(lk->owner!=current){
            printf("Lock[%s] isn't held by this routine!\n",lk->name);
            // report_if(1);
        }else{
            if(--lk->reen==0){
                lk->owner=NULL;
                spin_unlock(&(lk->locked));
                iset(1);
            }
            if(lk->reen<0){
                printf("Unlock > Lock!\n");
                while(1);
            }
        }
    }else{
        // Assert(0,"Lock[%s] isn't locked!\n",lk->name);
        assert(0);
    }
}

void kmt_sem_init(sem_t *sem, const char *name, int value){
    copy_name(sem->name,name);
    sem->value=value;
    //kmt->spin_init(&(sem->lock),name);
    sem->lock=SPIN_INIT();
    sem->head=0;
    sem->tail=0;
    //log("%s: %d",sem->name,sem->value);
}

static void sem_add_task(sem_t *sem){
    int cpu_id=cpu_current();
    task_t* park=current;

    sem->pool[sem->tail++]=park;
    set_flag(park,TASK_SLEEP);
    if(sem->tail>=POOL_LEN)sem->tail-=POOL_LEN;

    spin_unlock(&(sem->lock));
    iset(1);
    yield();
    while(park->attr&TASK_SLEEP);
}

static void sem_remove_task(sem_t *sem){
    neg_flag(sem->pool[sem->head],TASK_SLEEP);
    if(++sem->head>=POOL_LEN)sem->head-=POOL_LEN;
}

void kmt_sem_wait(sem_t *sem){
    iset(0);
    spin_lock(&(sem->lock));

    if(--sem->value<0){
        return sem_add_task(sem);
    }
    spin_unlock(&(sem->lock));
    iset(1);
}

void kmt_sem_signal(sem_t *sem){
    iset(0);
    spin_lock(&(sem->lock));

    if(++sem->value<=0){
        sem_remove_task(sem);
    }
    spin_unlock(&(sem->lock));
    iset(1);
}

MODULE_DEF(kmt) = {
 // TODO
    .init = kmt_init,
    .create = kmt_create,
    .teardown = kmt_teardown,
    .spin_init = kmt_spin_init,
    .spin_lock = kmt_spin_lock,
    .spin_unlock = kmt_spin_unlock,
    .sem_init = kmt_sem_init,
    .sem_wait = kmt_sem_wait,
    .sem_signal = kmt_sem_signal,
};

