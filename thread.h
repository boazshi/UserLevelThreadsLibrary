//
// Created by user on 4/11/23.
//

#define STACK_SIZE 4096
#include <setjmp.h>
#include <cstdint>

// TYPEDEFS
typedef intptr_t address_t;
typedef void (*thread_entry_point)(void);


class Thread
{
public:

    Thread(int tid, thread_entry_point entry_point);

    void setup();

    void run();

    int preempt();


    int tid_;
    thread_entry_point entry_point_;
    char stack_[STACK_SIZE];
    sigjmp_buf env_;
    int total_quantum_count_;

};


