#include <common.h>
#include <os.h>

static void os_init() {
  pmm->init();
  kmt->init();
}

static void os_run() {
  iset(0);
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  iset(1);
  while (1) ;
}

typedef struct irq{
    int seq,event;
    handler_t handler;
    struct irq *next;
}irq_handler;

void guard(void){
    // Assert(0,"Guard should not be called!\n");
    assert(0);
}

static irq_handler irq_guard={
    .seq=INT_MIN,
    .event=-1,
    .handler=(handler_t)guard,
    .next=&irq_guard
};

static Context * os_trap(Event ev, Context *context){
  // TODO
  iset(0);
  Context *ret = context;
  //printf("trap");

  for(struct irq *handler=irq_guard.next;
      handler!=&irq_guard;
      handler=handler->next){
      if (handler->event == EVENT_NULL || handler->event == ev.event) {
          Context *next = handler->handler(ev, context);
          if (next) ret = next;
      }
  }
  // Assert(ret!=NULL,"\nkmt_context_switch returns NULL\n");
  assert(ret!=NULL);
  //printf("%x\n",ret);
  return ret;
}

static void os_on_irq(int seq, int event, handler_t handler){
  // TODO
  irq_handler *prev=&irq_guard,*p=irq_guard.next;
  //prev->new->p
  while(p){
      if(p->seq>seq||p==&irq_guard)break;
      prev=p;
      p=p->next;
  }
  prev->next=new(irq_handler);
  prev=prev->next;

  prev->seq=seq;
  prev->event=event;
  prev->handler=handler;
  prev->next=p;
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
  .trap = os_trap,
  .on_irq = os_on_irq
};
