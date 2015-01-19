// userthread.cc 
//      Entry point into the user level thread 
//
//      do_UserThreadCreat -- Kernel level function to allocate resource
//      and address space for a user level thread
//
//------------------------------------------------------------------------------

#ifdef CHANGED

#include "system.h"
#include "userthread.h"
#include "thread.h"
#include "scheduler.h"
#include "synch.h"

/*
Initialises backups of registers of a new copy of the MIPS interpreter in the same way as the primitive interpreter 
(Machine::InitRegisters\ and \ |Machine::RestoreState\ functions), and starts the interpreter (\| Machine::Run).
*/
static void StartUserThread(int data) {

  DEBUG('t', "StartUserThread with Stack Position: %d\n", currentThread->GetTid());

  if (currentThread->GetTid() < 0) {
    DEBUG('t', "Error, new Thread doesn't have a valid stack space");
    return;    
  }

  currentThread->space->InitRegisters();
  currentThread->space->RestoreState(); // TODO: Check if this need to reverse
  
  // Retrieve the function and arg
  ParamFunction *paramFunction = (ParamFunction *) data;
  // Write the argument in the register 4
  machine->WriteRegister(4, paramFunction->arg);
  // Initial program counter -- must be location of "Start"
  machine->WriteRegister(PCReg, paramFunction->function);
  machine->WriteRegister(NextPCReg, paramFunction->function + 4);
  // Set the stack pointer
  currentThread->space->MultiThreadSetStackPointer((3 * PageSize) * (currentThread->GetTid() + 1));
  
  DEBUG('l', "Add new thread: %d\n", currentThread->GetTid());
  DEBUG('l', "Thread list: \n");
  currentThread->space->activeThreads->PrintContent();
  
  machine->Run();
}

int do_UserThreadCreate(int f, int arg) {

    DEBUG('t', "Enter User Create Thread\n");

  // Use a struct to pass both the function and argument to the fork function
  ParamFunction *paramFunction = new ParamFunction();
  paramFunction->function = f;
  paramFunction->arg = arg;

  Thread *newThread = new Thread("New User Thread");
  
  //put increase counter here for synchonization problem
  currentThread->space->increaseUserProcesses();    //PROBLEM??? 

  // The thread's id is also its location on the stack
  newThread->SetTid(currentThread->space);  
                        //TODO check correctness, could be wrong 
                        // if set after fork. BUGGGYY :D
  //add to active list
  currentThread->space->activeThreads->AppendTraverse(NULL, newThread->GetTid() );
                          
  newThread->Fork(StartUserThread, (int) paramFunction);
  
  //newThread->SetTid();
  return newThread->GetTid();
}

void do_UserThreadExit() {

    DEBUG('t', "Thread \"%s\" uses User Exit\n", currentThread->getName() );
    DEBUG('t', "Status: number of current userthreads: %d\n", 
                            currentThread->space->getNumberOfUserProcesses());
    
    currentThread->space->decreaseUserProcesses();
    if (currentThread->space->getNumberOfUserProcesses() == 0){
        currentThread->space->ExitForMain->V();
    }

    // Also frees the corresponding stack location
    currentThread->FreeTid();
    
    //Remove from active list
    currentThread->space->activeThreads->RemoveTraverse(currentThread->GetTid());
    
    
    //debugging -- delete later
    DEBUG('l', "Delete thread: %d\n", currentThread->GetTid());
    DEBUG('l', "Thread list: \n");
    currentThread->space->activeThreads->PrintContent();
    
    //check queue of active lock to see anyone needs waking up
    void* thing = currentThread->space->activeLocks->RemoveTraverse(currentThread->GetTid());    
    if (thing !=NULL ) {
        // waking up the receipient
        ((JoinWaiting*) thing) -> threadWaiting->V();
        
        DEBUG('l', "Delete from waiting thread: %d\n", currentThread->GetTid());
        DEBUG('l', "Waiting thread list: \n");
        currentThread->space->activeLocks->PrintContent();
    }
        
    currentThread->Finish();
}

// return -1 if error, 0 if not running, 1 if success
int do_UserThreadJoin(int tid) {
    
    DEBUG('l', "Begin join, waiting for %d\n", tid);
    
    //Sanity check
    if (tid == currentThread->GetTid() )
        return -1;
    
    // check if the thread is in the active thread,
    if ( ! currentThread->space->activeThreads->seek(tid) )
        return 0;
    
    DEBUG('l', "Begin join2\n");
    
    //build the structure to prepare to sleep, just the semaphore
    //first, send it to the queue 
    JoinWaiting* waitingCondition = new JoinWaiting();
    waitingCondition->tid = currentThread->GetTid();
    waitingCondition->threadWaiting = currentThread->joinCondition;
    
    DEBUG('l', "Insert to waiting thread: %d\n", currentThread->GetTid());
    DEBUG('l', "Waiting thread list: \n");
    currentThread->space->activeLocks->PrintContent();
    
    // add to the queue
    currentThread->space->activeLocks->AppendTraverse( (void*) waitingCondition, tid);
    
    // the synchonization part
    if ( ! currentThread->space->activeThreads->seek(tid) ) {
        //take off the queue and returning
        currentThread->space->activeLocks->RemoveTraverse(tid);
        return 0;
    }else {
        // go to sleep
        currentThread->joinCondition->P();
    }    
    return 1;   // return after being wake up :D
}

#endif
