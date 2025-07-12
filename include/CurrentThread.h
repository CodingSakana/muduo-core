#pragma once

#include <sys/syscall.h>
#include <unistd.h>

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace CurrentThread
{
    extern thread_local int t_cachedTid; // 保存tid缓存 因为系统调用非常耗时 拿到tid后将其保存

    void cacheTid();

    inline int tid() // 内联函数只在当前文件中起作用
    {
        // __builtin_expect 是一种底层优化 此语句意思是如果还未获取tid 进入if 通过cacheTid()系统调用获取tid
        if (UNLIKELY(t_cachedTid == 0)) {
            cacheTid();
        }
        return t_cachedTid;
    }
} // namespace CurrentThread