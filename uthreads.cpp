//
// Created by יואל שמואל on 23/04/2023.
//

#include "uthreads.h"
#include <csetjmp>
#include "csetjmp"
#include "sys/time.h"
#include "signal.h"
#include "deque"
#include "queue"
#include "map"
#include "vector"
#include "iostream"
#include "string.h"
#include "algorithm"

typedef unsigned long address_t;
typedef enum State {ready,running,blocked} State;
typedef enum Signals {SIGTERMINATE  = 41, SIGBLOCK = 42, SIGSLEEP = 43 } Signals;
class Thread;
std::priority_queue<int, std::vector<int>, std::greater<int> > id_heap;
std::map<int , Thread> map_of_threads;
std::deque<int> readyQ;
std::vector<int> blockedQ;
std::vector<int> sleepingQ;
int total_of_quantums;
int runningThread;
itimerval timer;
struct sigaction sa;
sigjmp_buf env[MAX_THREAD_NUM];
#define JB_SP 4
#define JB_PC 5

address_t translate_address(address_t addr)
{
  address_t ret;
  asm volatile("xor    %%gs:0x18,%0\n"
               "rol    $0x9,%0\n"
  : "=g" (ret)
  : "0" (addr));
  return ret;
}

// Thread class start:
class Thread{
 public:
  Thread(const unsigned int id, thread_entry_point entry_point);
  Thread(const Thread& thread);
  ~Thread();
  void set_status(State new_status) {status = new_status;}
  const State get_status() const {return status;}
  int get_id() const {return id;}
  int get_sleeping_time() const {return finished_sleeping_time;}
  void set_sleeping_time(int time) {finished_sleeping_time = time;}
  char* get_sp_address() const {return sp_address;}
  thread_entry_point get_entry_point() const{return entry_point;}
  bool get_blocked_needs() const {return need_to_be_blocked;}
  void set_needs_of_block(bool need) {need_to_be_blocked = need;}
  void counter_forward(){time_counter++;}
  int get_time_counter() const {return time_counter;}

 private:
  int id;
  char* sp_address;
  int finished_sleeping_time;
  int time_counter;
  thread_entry_point entry_point;
  State status;
  bool need_to_be_blocked;
};

Thread::Thread(const unsigned int id, thread_entry_point entry_point = nullptr): id(id), entry_point(entry_point), status(ready) {
  sp_address = new char[STACK_SIZE];
  if(!sp_address)
  {
    std::cerr << "system error: allocation of stack failed\n";
    exit(1);
  }
  need_to_be_blocked = false;
  time_counter = 1;
}

Thread::~Thread() {
  delete[] sp_address;
  sp_address = nullptr;
}

Thread::Thread (const Thread &thread){
  id = thread.get_id();
  set_status(thread.get_status());
  sp_address = new char[STACK_SIZE];
  memcpy(sp_address, thread.get_sp_address(), STACK_SIZE);
  set_sleeping_time(thread.get_sleeping_time());
  time_counter = thread.get_time_counter();
  entry_point = thread.get_entry_point();
  need_to_be_blocked = thread.get_blocked_needs();
}

// Thread class end here.

// Helper functions for utility.

void reset_block_set(sigset_t block_set);

void set_timer() {
  if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
  {
    std::cerr << "system error: setitimer error\n";
    exit(1);
  }
}

sigset_t block_signals_init(){
  sigset_t block_set;
  sigaddset(&block_set, SIGVTALRM);
  sigprocmask(SIG_BLOCK, &block_set, nullptr);
  return block_set;
}

void unSleep_Thread() {
  sigset_t blocked_sigs = block_signals_init();
  for(auto thread_id: sleepingQ)
  {
    Thread* thread = &(map_of_threads.at(thread_id));
    if(thread->get_sleeping_time() <= uthread_get_total_quantums())
    {
      if(thread->get_blocked_needs())
      {
        blockedQ.push_back(thread_id);
        thread->set_status(blocked);
      }
      else{
        readyQ.push_back(thread_id);
        thread->set_status(ready);
      }
      sleepingQ.erase(std::find(sleepingQ.begin(), sleepingQ.end(),thread_id));
    }
  }
  reset_block_set(blocked_sigs);
}

void init_heap() {
  for(int i=1; i<MAX_THREAD_NUM; i++)
  {
    id_heap.push(i);
  }
}

void scheduler(int signal) {
  sigset_t blocked_sigs = block_signals_init();
  total_of_quantums ++;
  map_of_threads.at(runningThread).counter_forward();
  unSleep_Thread();
  int finished_thread = runningThread;
  if(signal != SIGVTALRM) {set_timer();}
  if(signal == SIGTERMINATE)
  {
    if(!readyQ.empty())
    {
      runningThread = readyQ.front();
      readyQ.pop_front();
      map_of_threads.at(runningThread).set_status(running);
      map_of_threads.at(finished_thread).~Thread();
      map_of_threads.erase(finished_thread);
      id_heap.push(finished_thread);
      siglongjmp(env[runningThread], 1);
    }
    map_of_threads.erase(finished_thread);
    id_heap.push(finished_thread);
  }
  if(sigsetjmp(env[running], 1) == 0)
  {
    if(signal == SIGBLOCK)
    {
      blockedQ.push_back(finished_thread);
      map_of_threads.at(finished_thread).set_status(blocked);
    }
    if(signal == SIGSLEEP){
      sleepingQ.push_back(finished_thread);
      map_of_threads.at(finished_thread).set_needs_of_block(false);
      map_of_threads.at(finished_thread).set_status(blocked);
    }
    if(signal == SIGVTALRM)
    {
      readyQ.push_back(runningThread);
      map_of_threads.at(runningThread).set_status(ready);
    }
    if(!readyQ.empty())
    {
      runningThread = readyQ.front();
      readyQ.pop_front();
      map_of_threads.at(runningThread).set_status(running);
      siglongjmp(env[runningThread], 1);
    }
  }
  reset_block_set(blocked_sigs);
  return;
}

void reset_block_set(sigset_t block_set){
  if(sigprocmask(SIG_UNBLOCK, &block_set, nullptr)){
    std::cerr << "system error: sigprocmask error\n";
    exit(1);
  }
  if(sigpending(&block_set) == -1){
    std::cerr << "system error: sigpending error\n";
    exit(1);
  }
  int val = sigismember(&block_set, SIGVTALRM);
  if (val == 1){
    scheduler(SIGVTALRM);
  }
  if(val == -1){
    std::cerr << "system error: sigismember error\n";
    exit(1);
  }
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
  if(quantum_usecs <= 0)
  {
    std::cerr << "thread library error: length of quantum must be positive\n";
    return -1;
  }
  if (sigaction(SIGVTALRM, &sa, NULL) < 0)
  {
    std::cerr << "system error: sigaction error\n";
    exit(1);
  }
  sa.sa_handler = &scheduler;
  timer.it_value.tv_sec = quantum_usecs/1000000;
  timer.it_value.tv_usec = quantum_usecs%1000000;
  timer.it_interval.tv_sec = quantum_usecs/1000000;
  timer.it_interval.tv_usec = quantum_usecs%1000000;
  set_timer();
  init_heap();
  Thread main_thread(0);
  map_of_threads.insert(std::make_pair(0, main_thread));
  total_of_quantums = 1;
  runningThread = 0;
  sigsetjmp(env[0], 1);
  return 0;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point){
  sigset_t blocked_sigs = block_signals_init();
  if(entry_point == nullptr)
  {
    reset_block_set(blocked_sigs);
    std::cerr << "thread library error: entry_point cannot be null when spawning\n";
    return -1;
  }
  if(id_heap.empty())
  {
    reset_block_set(blocked_sigs);
    std::cerr << "thread library error: number of concurrent threads exceeding the limit\n";
    return -1;
  }
  int tid = id_heap.top();
  id_heap.pop();
  Thread new_thread(tid, entry_point);
  address_t sp = (address_t) *new_thread.get_sp_address() + STACK_SIZE - sizeof(address_t);
  address_t pc = (address_t) entry_point;
  sigsetjmp(env[tid], 1);
  (env[tid]->__jmpbuf)[JB_SP] = translate_address(sp);
  (env[tid]->__jmpbuf)[JB_PC] = translate_address(pc);
  sigemptyset(&env[tid]);
  map_of_threads.insert(std::make_pair (tid, new_thread));
  readyQ.push_back(tid);
  reset_block_set(blocked_sigs);
  return tid;
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
  sigset_t blocked_sigs = block_signals_init();
  if(map_of_threads.find(tid) == map_of_threads.end())
  {
    std::cerr << "thread library error: tid does not exists\n";
    reset_block_set(blocked_sigs);
    return -1;
  }
  if(tid == 0)
  {
    for(std::pair<int, Thread> pair: map_of_threads)
    {
      pair.second.~Thread();
    }
    sleepingQ.clear();
    readyQ.clear();
    blockedQ.clear();
    map_of_threads.clear();
    reset_block_set(blocked_sigs);
    exit(0);
  }
  Thread* thread = &map_of_threads.at(tid);
  State status = thread->get_status();
  map_of_threads.erase(tid);
  thread->~Thread();
  id_heap.push(tid);
  if(status == ready)
  {
    readyQ.erase(std::find(readyQ.begin(), readyQ.end(), tid));
  }
  if(status == blocked)
  {
    auto iterator = std::find(blockedQ.begin(), blockedQ.end(), tid);
    if(iterator == blockedQ.end())
    {
      auto iter = std::find(sleepingQ.begin(), sleepingQ.end(),tid);
      if(iter != sleepingQ.end())
      {
        sleepingQ.erase(iter);
      }
    }
    else{
      blockedQ.erase(iterator);
    }
  }
  if(status == running)
  {
    scheduler(SIGTERMINATE);
  }
  reset_block_set(blocked_sigs);
  return 0;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
  sigset_t blocked_sigs = block_signals_init();
  if(tid == 0)
  {
    std::cerr << "thread library error: cant block main thread\n";
    reset_block_set(blocked_sigs);
    return -1;
  }
  if(map_of_threads.find(tid) == map_of_threads.end())
  {
    std::cerr << "thread library error: tid doesnt exists\n";
    reset_block_set(blocked_sigs);
    return -1;
  }
  if(map_of_threads.at(tid).get_status() == blocked)
  {
    if(std::count(sleepingQ.begin(), sleepingQ.end(),tid) > 0)
    {
      map_of_threads.at(tid).set_needs_of_block(true);
    }
    reset_block_set(blocked_sigs);
    return 0; //already in blocked
  }
  if(runningThread == tid)
  {
    scheduler(SIGBLOCK);
  }
  if(std::count(readyQ.begin(), readyQ.end(),tid) > 0)
  {
    blockedQ.push_back(tid);
    readyQ.erase(std::find(readyQ.begin(), readyQ.end(), tid));
    map_of_threads.at(tid).set_status(blocked);
  }
  reset_block_set(blocked_sigs);
  return 0;
}


/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
  sigset_t blocked_sigs = block_signals_init();
  if(map_of_threads.find(tid) == map_of_threads.end())
  {
    std::cerr << "thread library error: tid doesnt exists\n";
    reset_block_set(blocked_sigs);
    return -1;
  }
  if(std::count(readyQ.begin(), readyQ.end(),tid) > 0 || runningThread == tid)
  {
    reset_block_set(blocked_sigs);
    return 0;
  }
  if(std::count(sleepingQ.begin(), sleepingQ.end(),tid) > 0)
  {
    map_of_threads.at(tid).set_needs_of_block(false);
  }
  else{
    blockedQ.erase(std::find(blockedQ.begin(), blockedQ.end(),tid));
    readyQ.push_back(tid);
    map_of_threads.at(tid).set_status(ready);
  }
  reset_block_set(blocked_sigs);
  return 0;
}


/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums){
  sigset_t blocked_sigs = block_signals_init();
  if(num_quantums <= 0)
  {
    std::cerr << "thread library error: num of quantum must be positive number\n";
    reset_block_set(blocked_sigs);
    return -1;
  }
  int tid = uthread_get_tid();
  if(tid == 0)
  {
    std::cerr << "thread library error: maid thread can't call sleep\n";
    reset_block_set(blocked_sigs);
    return -1;
  }
  map_of_threads.at(tid).set_sleeping_time(num_quantums +
  uthread_get_total_quantums() + 1);
  scheduler(SIGSLEEP);
  reset_block_set(blocked_sigs);
  return 0;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){return runningThread;}



/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums(){return total_of_quantums;}



/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
  sigset_t blocked_sigs = block_signals_init();
  if(map_of_threads.find(tid) == map_of_threads.end())
  {
    std::cerr << "thread library error: tid doesnt exists\n";
    reset_block_set(blocked_sigs);
    return -1;
  }
  reset_block_set(blocked_sigs);
  return map_of_threads.at(tid).get_time_counter();
}