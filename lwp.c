#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "lwp.h"

#define INITIALSTACK 2048

//initialize/define globals
lwp_context context;                //context
lwp_context lwp_ptable[LWP_PROC_LIMIT];   //process table
int lwp_procs = 0;                  //process count
int lwp_running = -1;               //currently running process
ptr_int_t *glob_sp;

int RoundRobin(){
   // wrap around if this is the last process in table
   if (lwp_running==lwp_procs-1){
      return 0;
   } 
   // otherwise return the next waiting process
   return (lwp_running+1);
}

// set scheduler
schedfun scheduler = RoundRobin;

// create new thread
int new_lwp(lwpfun function, void * argument, size_t stacksize){
   int i;
   // check if we've reached process limit
   if (lwp_procs==LWP_PROC_LIMIT){
      return -1;
   } 
   // create conditional to check if # processes > ptable size (max)
   ptr_int_t *sp, *bp, *stack;   // declare stack pointer, stack
   stack = malloc(stacksize*4);  // allocate heap space for fake stack (stacksize words)
   sp = stack + stacksize*4;     // bring sp to beginning of stack (high addr)
   *sp = (ptr_int_t) argument;   // push function's args
   sp--;                         // move sp
   *sp = (ptr_int_t) lwp_exit;   // push lwp_exit return address (just for convention)
   sp--;                         // move sp
   *sp = (ptr_int_t) function;   // push function's address (that we want to run)
   sp--;                         // move sp
   *sp = 0;                      // push bogus base pointer value
   bp = sp;                      // bp now holds sp
   // push 6 regs
   for (i=0; i<6; i++){
      sp--;
      *sp = 44;
   }
   //7th reg must be &bbp
   sp--;
   *sp = (ptr_int_t)bp;   //final stack value is now bbp
   //set globals, return pid
   context = (lwp_context){.pid=lwp_procs, .stack=stack, .stacksize=stacksize, .sp=sp};
   lwp_ptable[lwp_procs] = context;
   return lwp_procs++;
}

void lwp_start(){
   // returns if no processes exist
   if (lwp_procs==0){
      return;
   }
   // save og state and sp
   SAVE_STATE();
   GetSP(glob_sp);
   // schedules and runs an lwp
   lwp_running = scheduler();
   SetSP(lwp_ptable[lwp_running].sp);
   RESTORE_STATE();
   return;
}

//like lwp_start but ur replacing thread this time.
void lwp_yield(){
   lwp_context c;
   // save this lwp's state and sp
   SAVE_STATE();  
   GetSP(lwp_ptable[lwp_running].sp);     
   // schedule and run next thread
   lwp_running = scheduler();    // get new lwp
   c = lwp_ptable[lwp_running];  // get new lwp context
   SetSP(c.sp);      // set sp to new contexts' stack pointer
   RESTORE_STATE();  // restore new thread
   return;   
}

// what does it mean to terminate the current lwp?
// shift all pid's down as you also shift contexts in the ptable
void lwp_exit(){
   lwp_context c;
   int i = lwp_running;
   // shift threads down 
   while (i<lwp_procs-1){
      lwp_ptable[i] = lwp_ptable[i+1];
      i++;
   }
   lwp_procs--;   //decrement # running processes
   // stop if no threads exist
   if (lwp_procs==0){
      lwp_stop();
   }
   else{
      // setup next thread to run
      lwp_running--;    // account for threads shifting down
      lwp_running = scheduler();    // get next process
      c = lwp_ptable[lwp_running];  // grab next thread's context
      SetSP(c.sp);      // set new contexts' stack pointer
      RESTORE_STATE();  // restore new thread
   }
   return;
}

// stop all threads in their current state (can resume with lwp_start)
void lwp_stop(){
   SAVE_STATE();     // save current state
   GetSP(lwp_ptable[lwp_running].sp);     // store current sp
   SetSP(glob_sp);   // restore og sp
   RESTORE_STATE();  // restore og state
}

// set the scheduler. default is round robin
void lwp_set_scheduler(schedfun sched){
   if(!sched){
      scheduler = sched;
   }
}

// current pid is running pid
int lwp_getpid(){
   return lwp_running;
}

////////////////////////////////Notes/////////////////////////////////////////////////////////////
// yield saves the current thread's state, uses the scheduler to choose who runs next, and restores that thread's state and moves the sp so that that thread can resume
//
// scheduler simply decides who runs next if a thread yields or exits. An easy implementation is incrementing lwp_running, unless there's an issue like it doesn't exist or smth (in which case u can return to 0 or smth and look from there)
// schedfun points to the function type (round robin)
// set_scheduler returns the pid of the next process
// use lwp_procs+1 to assign pid's
// use lwp_running + 1 to assign the next process to run, if there's an issue, wrap around back to 0 and keep going around. If none, program ends.


/*int AlwaysZero() {
  // A scheduler that always run the first one 
  return 0;
}*/


/*int main(void){
   int i;
   // set zero scheduler
   lwp_set_scheduler(AlwaysZero);
   // create threads
   for(i=1;i<=5;i++) {
      new_lwp(indentnum,(void*)i,INITIALSTACK);
   }
   // start
   lwp_start(); 
   return 0;
}*/
