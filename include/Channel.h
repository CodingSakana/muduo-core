#pragma once

#include <functional>
#include <memory>

#include "Timestamp.h"
#include "noncopyable.h"

class EventLoop;

/**
 * 理清楚 EventLoop、Channel、Poller之间的关系  Reactor模型上对应 多路事件分发器
 * Channel理解为通道 封装了sockfd和其感兴趣的event 如EPOLLIN、EPOLLOUT事件 还绑定了poller返回的具体事件
 **/
class Channel : noncopyable { // 默认就是私有继承
public:
    using EventCallback = std::function<void()>;              // 事件的回调
    using ReadEventCallback = std::function<void(Timestamp)>; // 只读事件的回调

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // fd 得到 Poller 通知以后 用于处理事件
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb) {
        writeCallback_ = std::move(cb);
    }
    void setCloseCallback(EventCallback cb) {
        closeCallback_ = std::move(cb);
    }
    void setErrorCallback(EventCallback cb) {
        errorCallback_ = std::move(cb);
    }

    // 防止当 channel 被手动 remove 后，channel 还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const {
        return fd_;
    }
    int events() const {
        return events_;
    }
    void set_revents(int revt) {
        revents_ = revt;
    }

    // 设置 fd 相应的事件状态 相当于 epoll_ctl add delete
    void enableReading() {
        events_ |= kReadEvent;
        update();
    }
    void disableReading() {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting() {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting() {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll() {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件状态
    bool isNoneEvent() const {
        return events_ == kNoneEvent;
    }
    bool isWriting() const {
        return events_ & kWriteEvent;
    }
    bool isReading() const {
        return events_ & kReadEvent;
    }

    int index() {
        return index_;
    }
    void set_index(int idx) {
        index_ = idx;
    }

    // one loop per thread
    EventLoop* ownerLoop() {
        return loop_;
    }

    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_; // 事件循环
    const int fd_;    // fd, Poller 监听的对象
    int events_;      // 注册 fd 感兴趣的事件
    int revents_;     // Poller 返回的具体发生的事件
    int index_;       // Poller 使用

    std::weak_ptr<void> tie_;
    bool tied_;

    // 只用 channel 里可获知 fd 最终发生的具体的事件 revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};