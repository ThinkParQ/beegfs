#include <app/App.h>
#include <program/Program.h>
#include <common/app/log/LogContext.h>
#include <iterator>
#include <thread>
#include <chrono>
#include "DelayedDisposer.h"

/*
 * utility class for implementing delayed unlink of files
 */
DelayedDisposer::DelayedDisposer()
{

}

int DelayedDisposer::unlink(std::string fname, unsigned delay)
{
    App* app = Program::getApp();
    TimerQueue* timerQ = app->getTimerQueue();

    timerQ->enqueue(std::chrono::milliseconds(delay),
       [fname] {
          int ret = ::unlink(fname.c_str());
          const char* logContext = "FileInode Disposal Cleanup ";
          LogContext(logContext).log(Log_SPAM, "delayed unlink " + fname + " -> " + std::to_string(ret));
       });

    return 0;
}
