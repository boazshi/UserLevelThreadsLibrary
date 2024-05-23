//
// Created by user on 4/11/23.
//

#include "thread.h"
#include "threads_utils.cpp"
#include <csignal>

Thread::Thread(int tid, thread_entry_point entry_point): tid_(tid), entry_point_(entry_point), total_quantum_count_(0){}

void Thread::setup()
{
    // initializes env[tid] to use the right stack, and to run from the function 'entry_point', when we'll use
    // siglongjmp to jump into the thread.
    address_t sp = (address_t) (stack_) + STACK_SIZE - sizeof(address_t);
    auto pc = (address_t) entry_point_;
    sigsetjmp(env_, 1);
    (env_->__jmpbuf)[JB_SP] = translate_address(sp);
    (env_->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&(env_->__saved_mask));
}


void Thread::run()
{
    siglongjmp(this->env_, 1);
}

int Thread::preempt()
{
    total_quantum_count_+=1;
    return sigsetjmp(this->env_, 1);
}