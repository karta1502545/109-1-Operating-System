// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    L1 = new List<Thread *>;
    L2 = new List<Thread *>;
    L3 = new List<Thread *>;
    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    delete L1;
    delete L2;
    delete L3;
} 
//----------------------------------------------------------------------
// Scheduler::Aging
//----------------------------------------------------------------------
void Scheduler::Aging()
{
    Aging_for_one_queue(L1, 1);
    Aging_for_one_queue(L2, 2);
    Aging_for_one_queue(L3, 3);
}

void Scheduler::Aging_for_one_queue(List<Thread *> *queue, int level) // Aging for every thread in this queue
{
    ListIterator<Thread *> *it = new ListIterator<Thread *>(queue);
    Thread *itThread;
    for (; !it->IsDone(); it->Next()) {
        itThread = it->Item();
        // update aging token
        itThread->aging_token += kernel->stats->totalTicks - itThread->start_aging_tick;
        itThread->start_aging_tick = kernel->stats->totalTicks;

        // exchange token for priority
        if(itThread->priority < 149){
            if(itThread->aging_token >= 1500){
                DEBUG('z', "[C] Tick ["<< kernel->stats->totalTicks <<"]: Thread [" << itThread->getID() << "] changes its priority from [" << itThread->priority << "] to [" << itThread->priority + 10 << "]");
                itThread->aging_token -= 1500;
                itThread->priority += 10;
            }
            // ensure priority below 150
            if(itThread->priority > 149){
                itThread->priority = 149;
            }
        }

        // move high priority thread if needed
        if(itThread->priority >= 50 && level == 3){
            // change this thread from L3 to L2
            DEBUG('z', "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << itThread->getID() << "] is removed from queue L[3]");
            L3->Remove(itThread);
            DEBUG('z', "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << itThread->getID() << "] is inserted into queue L[2]");
            L2->Append(itThread);
        }
        if(itThread->priority >= 100 && level == 2){
            // change this thread from L2 to L1
            DEBUG('z', "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << itThread->getID() << "] is removed from queue L[2]");
            L2->Remove(itThread);
            DEBUG('z', "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << itThread->getID() << "] is inserted into queue L[1]");
            L1->Append(itThread);
        }
    }
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    if(thread->priority >= 100){ // TODO: we should ensure that the priorities of all thread <= 149 and >=0 (revision will start from kernel.cc)
        DEBUG('z', "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[1]");
        L1->Append(thread);
    }
    else if(thread->priority >= 50){
        DEBUG('z', "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[2]");
        L2->Append(thread);
    }
    else{
        DEBUG('z', "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << thread->getID() << "] is inserted into queue L[3]");
        L3->Append(thread);
    }
    thread->start_aging_tick = kernel->stats->totalTicks;
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    Thread *itThread;
    Thread *Thread_return = NULL;
    ListIterator<Thread *> *it;
    if(!L1->IsEmpty()){
        // SJF
        Thread_return = L1->Front();
        it = new ListIterator<Thread *>(L1);
        for(; !it->IsDone(); it->Next()){
            itThread = it->Item();
            if(Thread_return->predicted_burst_time > itThread->predicted_burst_time){
                Thread_return = itThread;
            }
        }
        DEBUG('z', "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << Thread_return->getID() << "] is removed from queue L[1]");
        L1->Remove(Thread_return);
    }
    else if(!L2->IsEmpty()){
        // np-p
        Thread_return = L2->Front();
        it = new ListIterator<Thread *>(L2);
        for(; !it->IsDone(); it->Next() ){
            // cout << "Thread " << itThread->getID() << " priority: " << itThread->priority << endl;
            itThread = it->Item();
            if(Thread_return->priority < itThread->priority){
                Thread_return = itThread;
            }
        }
        DEBUG('z', "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << Thread_return->getID() << "] is removed from queue L[2]");
        L2->Remove(Thread_return);
    }
    else if(!L3->IsEmpty()){
        Thread_return = L3->Front();
        DEBUG('z', "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << Thread_return->getID() << "] is removed from queue L[3]");
        L3->Remove(Thread_return);
    }
    else{ // nothing in ready queue
        // cout << "nothing in ready queue" << endl;
        Thread_return = NULL;
    }
    return Thread_return;
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing) // we should not revise this function(12/17)
{
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    DEBUG('z', "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << "] is now selected for execution, thread [" << oldThread->getID() << "] is replaced, and it has executed [" << kernel->stats->totalTicks - oldThread->start_cpu_tick << "] ticks");
    oldThread->start_cpu_tick = kernel->stats->totalTicks;
    SWITCH(oldThread, nextThread);
    // cout << "Switch finished\n";
    // we're back, running oldThread
    
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    oldThread->start_cpu_tick = kernel->stats->totalTicks; // record new initial tick
    // cout << "Now threadID = " << oldThread->getID() << endl;
    // cout << "Now tick = " << oldThread->start_cpu_tick << endl;
    // DEBUG('z', "[F] Tick [" << oldThread->start_cpu_tick << "]: Thread [" << oldThread->getID() << "] ");
    
    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list (L1) contents:\n";    
    L1->Apply(ThreadPrint);
    cout << "Ready list (L2) contents:\n";    
    L2->Apply(ThreadPrint);
    cout << "Ready list (L3) contents:\n";    
    L3->Apply(ThreadPrint);
}
