#include <sys/epoll.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

const int Channel::kNoneEvent = 0;                  // 空事件
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI; // 读事件
const int Channel::kWriteEvent = EPOLLOUT;          // 写事件

// EventLoop: ChannelList Poller
// index_ = -1 就是 EPollPoller.cc 的 kNew
Channel::Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {
}

Channel::~Channel() {
}

// channel的tie方法什么时候调用过?  TcpConnection => channel
/**
 * TcpConnection中注册了Channel对应的回调函数，传入的回调函数均为TcpConnection
 * 对象的成员方法，因此可以说明一点就是：Channel的结束一定晚于TcpConnection对象！
 * 此处用tie去解决TcpConnection和Channel的生命周期时长问题，从而保证了Channel对象能够在
 * TcpConnection销毁前销毁。
 **/
void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}
// update 和remove => EpollPoller 更新channel在poller中的状态
/**
 * 当改变channel所表示的fd的events事件后，update负责再poller里面更改fd相应的事件epoll_ctl
 **/

void Channel::update() {
    // 通过channel所属的eventloop，调用poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}

// 在channel所属的EventLoop中把当前的channel删除掉
void Channel::remove() {
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
        // 如果提升失败了 就不做任何处理 说明Channel的TcpConnection对象已经不存在了
    } else {
        handleEventWithGuard(receiveTime);
    }
}

// 根据 poller 通知的 channel 发生的具体事件，由channel都则调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime) {
    LOG_INFO("channel handleEvent revents:%d\n", revents_);
    // 关闭
    if ((revents_ & EPOLLHUP) &&
        !(revents_ & EPOLLIN)) // 当TcpConnection对应Channel 通过shutdown 关闭写端 epoll触发EPOLLHUP
    {
        if (closeCallback_) {
            closeCallback_();
        }
    }
    // 错误
    if (revents_ & EPOLLERR) {
        if (errorCallback_) {
            errorCallback_();
        }
    }
    // 读
    if (revents_ & (EPOLLIN | EPOLLPRI)) {
        if (readCallback_) {
            readCallback_(receiveTime);
        }
    }
    // 写
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) {
            writeCallback_();
        }
    }
}