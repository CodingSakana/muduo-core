#include "CurrentThread.h"

namespace CurrentThread
{
    thread_local int t_cachedTid = 0;

    void cacheTid() {
        if (t_cachedTid == 0) {
            // pthread_self() 返回的是 POSIX 线程库的线程标识符（pthread_t），它是用户态的标识符。
            // syscall(SYS_gettid) 返回的是内核级线程 ID（TID），这是操作系统在内核中分配的标识符。
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
} // namespace CurrentThread