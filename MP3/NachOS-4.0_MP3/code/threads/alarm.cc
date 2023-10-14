// alarm.cc
//	Routines to use a hardware timer device to provide a
//	software alarm clock.  For now, we just provide time-slicing.
//
//	Not completely implemented.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "alarm.h"
#include "main.h"

//----------------------------------------------------------------------
// Alarm::Alarm
//      Initialize a software alarm clock.  Start up a timer device
//
//      "doRandom" -- if true, arrange for the hardware interrupts to 
//		occur at random, instead of fixed, intervals.
//----------------------------------------------------------------------

Alarm::Alarm(bool doRandom)
{
    timer = new Timer(doRandom, this);
}

//----------------------------------------------------------------------
// Alarm::CallBack
//	Software interrupt handler for the timer device. The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as 
//	if the interrupted thread called Yield at the point it is 
//	was interrupted.
//
//	For now, just provide time-slicing.  Only need to time slice 
//      if we're currently running something (in other words, not idle).
//----------------------------------------------------------------------

void 
Alarm::CallBack()
{
    Interrupt *interrupt = kernel->interrupt;
    MachineStatus status = interrupt->getStatus();
    
    // perform aging algorithm per 100 ticks
    kernel->scheduler->Aging();

    // determine when to yield (from running state to ready state)
    if (status != IdleMode) {
        int level = 0;
        ASSERT(kernel->currentThread->priority>=0 && kernel->currentThread->priority <=149);
        int thread_priority = kernel->currentThread->priority;
        if(thread_priority>=100 && thread_priority<=149){
            level = 1;
        }
        else if(thread_priority>=50 && thread_priority<=99){
            level = 2;
        }
        else if(thread_priority>=0 && thread_priority<=49){
            level = 3;
        }
        // cout << "entering Alarm::CallBack()" << endl;
        // DEBUG('z', "Tick:[" << kernel->stats->totalTicks <<"], calling YieldOnReturn(), Thread[" << kernel->currentThread->getID() << "]running to ready");
        bool context_switch = false;
        if(level == 1 && !(kernel->scheduler->L1->IsEmpty())){ // L1 in cpu VS L1 in queue.
            context_switch = true;
        }
        if(level == 3){ // L3 in cpu VS L3, L2, L1 in queue.
            context_switch = true;
        }
        if(level == 2 && !(kernel->scheduler->L1->IsEmpty())){ // L2 in cpu VS L1 in queue.
            context_switch = true;
        }

        if(context_switch){
            // DEBUG('z', "Tick:[" << kernel->stats->totalTicks <<"], calling YieldOnReturn(), Thread[" << kernel->currentThread->getID() << "]running to ready");
            interrupt->YieldOnReturn();
        }
    }
}