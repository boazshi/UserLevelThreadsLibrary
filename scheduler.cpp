//
// Created by user on 4/11/23.
//

#include "scheduler.h"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <sys/time.h>


void sys_error(const std::string &message)
{
    std::cout << "system error: " << message << std::endl;
    exit(1);
}

void lib_error(const std::string &message)
{
    std::cout << "thread library error: " << message << std::endl;
}

Scheduler::Scheduler(int quantum_usecs) : _threads({}),
                                          _blocked_set({}),
                                          _ready_list({}),
                                          _sleep_map({}),
                                          _running(nullptr),
                                          _quantum_usecs(quantum_usecs),
                                          _total_quantum_counter(1)
{
    _threads[0] = std::make_shared<Thread>(0,
                                           nullptr);
};

int Scheduler::init()
{
    sigsetjmp(_threads[0]->env_, 1);
    sigemptyset(&(_threads[0]->env_->__saved_mask));
    _running = _threads[0];
    if (_running == nullptr)
    {
        sys_error("memory alocation failed");
    }
    return SUCCESS;
}

int Scheduler::get_total_quantum_counter() const noexcept
{
    return _total_quantum_counter;
}

int Scheduler::get_current_active() const
{
    return _running->tid_;
}

int Scheduler::get_total_thread_quantums(int tid) const
{
    return _threads.at(tid)->total_quantum_count_;
}

int Scheduler::get_lowest_tid() const noexcept
{
    for (int i = 1; i < MAX_THREAD_NUM; i++) // tid 0 reserved for main
    {
        if (_threads.find(i) == _threads.end())
        {
            return i;
        }
    }
    // we should never reach this point
    return FAILURE;
}


int Scheduler::progress_queue()
{
    if (_running != nullptr)
    {
        _ready_list.push_back(_running->tid_);
    } else if (_ready_list.empty())
    {
        return SUCCESS;
    }
    int next_tid = _ready_list.front();
    _running = _threads[next_tid];
    _ready_list.pop_front();

    return SUCCESS;
}

void Scheduler::update_sleep_queue()
{
    for (auto &it: _sleep_map)
    {
        _sleep_map[it.first] -= 1;
    }
    for (auto &it: _sleep_map)
    {
        if (it.second <= 0)
        {
            resume(it.first);
            _sleep_map.erase(it.first);
            break;
        }
    }
}

void Scheduler::make_schedule_decision()
{
    if (!(_ready_list.empty()))
    {
        progress_queue();
    }

    if (_running != nullptr)
    {
        _running->run();
    }

}


int Scheduler::get_quantum_secs() const
{
    return _quantum_usecs;
}

void Scheduler::set_quantum_secs(int quantum_usecs)
{
    _quantum_usecs = quantum_usecs;
}

int Scheduler::stop_current()
{
    _total_quantum_counter += 1;
    return _running->preempt();
}

int Scheduler::spawn(thread_entry_point entry_point)
{
    int lowest = get_lowest_tid();
    if (lowest == FAILURE)
    {
        lib_error("thread limit reached");
        return FAILURE;
    }
    if (_threads.find(lowest) != _threads.end())
    {
        lib_error("attempt to spawn 2 threads with same id");
        return FAILURE;
    }
    thread_ptr thrd = std::make_shared<Thread>(lowest,
                                               entry_point);
    if (thrd == nullptr)
    {
        sys_error("memory alocation failed");
        return FAILURE;
    }

    thrd->setup();

    _threads[thrd->tid_] = thrd;
    _ready_list.push_back(thrd->tid_);

    return thrd->tid_;
}

int Scheduler::terminate(int tid)
{
    _blocked_set.erase(tid);
    _ready_list.remove(tid);

    if (_threads.erase(tid) == 0)
    {
        lib_error("attempt to terminate main thread");
        return FAILURE;
    }

    if (_running != nullptr && _running->tid_ == tid)
    {
        stop_current();
        _running = nullptr;
        make_schedule_decision();
    }

    return SUCCESS;
}


int Scheduler::block(int tid)
{
    if (tid == 0)
    {
        lib_error("attempt to block main thread");
        return FAILURE;
    }
    if (_threads.find(tid) == _threads.end())
    {
        lib_error("attempt to block non-existent thread");
        return FAILURE;
    }
    _blocked_set.insert(tid);
    _ready_list.remove(tid);

    if (_running->tid_ == tid)
    {
        _running = nullptr;
        make_schedule_decision();
    }

    return SUCCESS;
}

int Scheduler::resume(int tid)
{
    if (_threads.find(tid) == _threads.end())
    {
        std::cout << "thread library error: resume attempt on non-existent thread" << std::endl;
        return FAILURE;
    }

    if (_blocked_set.find(tid) != _blocked_set.end())
    {
        _blocked_set.erase(tid);
        _ready_list.push_back(tid);
    }

    return SUCCESS;
}


int Scheduler::put_to_sleep(int num_quantums)
{

    if (_running->tid_ == 0)
    {
        lib_error("attempt to put to sleep non-existent thread");
        return FAILURE;
    }

    _sleep_map[_running->tid_] = num_quantums;

    if (block(_running->tid_) == FAILURE)
    {
        return FAILURE;
    }


    make_schedule_decision();

    return SUCCESS;
}
