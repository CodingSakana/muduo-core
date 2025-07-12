#include <stdlib.h>

#include "EPollPoller.h"
#include "Poller.h"

/** 为什么不在 Poller.cc 直接实现？
 * 因为 Poller 是基类，但是返回的是派生类
 * 如果在 Poller.cc 里面写的话需要包含派生类头文件
 * 这是不合理的
 */
Poller* Poller::newDefaultPoller(EventLoop* loop) {
    if (::getenv("MUDUO_USE_POLL")) {
        return nullptr; // 生成poll的实例（目前这个实现暂时没有）
    } else {
        return new EPollPoller(loop); // 生成epoll的实例
    }
}