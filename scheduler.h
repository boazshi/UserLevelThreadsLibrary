//
// Created by user on 4/11/23.
//

#ifndef EX2_SCHEDULER_H
#define EX2_SCHEDULER_H

#include "thread.h"
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <list>
#include <memory>
#define MAX_THREAD_NUM 100 /* maximal number of threads */
#define FAILURE -1
#define SUCCESS 0

//typedefs:
typedef std::shared_ptr<Thread> thread_ptr;
typedef std::unordered_set<int> BlockedSet; //gets id, returns block stat
typedef std::list<int> ReadyList;
typedef std::unordered_map<int, int> SleepMap;
typedef std::unordered_map<int, thread_ptr> ThreadMap;

void sys_error(const std::string &message);
void lib_error(const std::string &message);

/**
 * This class is tightly coupled with uthreads.h and actually functions
 * as a helper class for this library.
 */
class Scheduler
{
public:

    /**
     * initiates a Scheduler instance, meant to use to create and manage
     * user level threads.
     * @param quantum_usecs number of ticks (without interruptions) each
     * thread will be allowed to run before rotating
     */
    Scheduler(int quantum_usecs);

    /**
     * inits main thread as the running thread. See uthreads.init
     * @return 0 if successfull, -1 otherwise
     */
    int init();

    /**
     * Spawns a thread that runs the entry_point function.
     * @param entry_point function to run
     * @return 0 if successfull, -1 otherwise
     */
    int spawn(thread_entry_point entry_point);

    /**
     * terminates thread with thread id tid.
     * @param tid thread id
     * @return 0 if successfull, -1 otherwise
     */
    int terminate(int tid);

    /**
     * blocks thread with thread id tid. Blocked threads wont continue running
     * until a resume method is called upon them.
     * @param tid thread id
     * @return 0 if successfull, -1 otherwise
     */
    int block(int tid);

    /**
     * resumes thread with thread id tid. See block for more info.
     * @param tid thread id
     * @return 0 if successfull, -1 otherwise
     */
    int resume(int tid);

    /**
     * blocks a thread for a fixed amount of qunatums.
     * @param num_quantums number of qunatums that the thread will be blocked
     * @return 0 if successfull, -1 otherwise
     */
    int put_to_sleep(int num_quantums);

    /**
     * stops the current thread, saving its state in the process.
     * @return 0 if successfull, -1 otherwise
     */
    int stop_current();

    /**
     * the main loop of the class. Progeresses sleep timers, and makes
     * a decision which thread should be running next, using the Round Robin
     * algo.
     */
    void make_schedule_decision();

    /**
     * getter for the qunatum_secs property, which is the virtual time of each
     * quantum
     * @return
     */
    int get_quantum_secs() const;

    /**
     * setter method for quantum_secs property
     */
    void set_quantum_secs(int quantum_usecs);

    /**
     * progresses the ready queue.
     * @return 0 if successfull, -1 otherwise
     */
    int progress_queue();

    /**
     * getter method for total_quantum_counter property, which is the total
     * virtual time the scheudler is running.
     * @return 0 if successfull, -1 otherwise
     */
    int get_total_quantum_counter() const noexcept;

    /**
     * getter method for total_quantum_counter property of a specific thread.
     * @return the property
     */
    int get_total_thread_quantums(int tid) const;

    /**
     * gets the current active thread id.
     * @return
     */
    int get_current_active() const;

    /**
     * updates the sleep counters for sleeping threads
     */
    void update_sleep_queue();

private:
    ThreadMap _threads;
    BlockedSet _blocked_set;
    ReadyList _ready_list;
    SleepMap _sleep_map;
    thread_ptr _running;
    int _quantum_usecs;
    int _total_quantum_counter;

    /**
     * calculates the lowest possible id.
     */
    int get_lowest_tid() const noexcept;

};


#endif //EX2_SCHEDULER_H
