#include <errno.h>
#include <string.h>
#include <unistd.h> // close

#include "Channel.h"
#include "EPollPoller.h"
#include "Logger.h"

// channel.index() 就是保存的以下这三个状态
const int kNew = -1;    // 某个channel还没添加至Poller          // channel的成员index_初始化为-1
const int kAdded = 1;   // 某个channel已经添加至Poller
const int kDeleted = 2; // 某个channel已经从Poller删除

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)) // 设置创建出的文件描述符在执行 exec() 时自动关闭（close-on-exec）
      ,
      events_(kInitEventListSize) // vector<epoll_event>(16)
{
    if (epollfd_ < 0) {
        LOG_FATAL("epoll_create error:%d \n", errno); // 这里直接就 exit(-1) 了
    }
}

EPollPoller::~EPollPoller() {
    ::close(epollfd_);
}

// 
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    // 由于频繁调用poll 实际上应该用LOG_DEBUG输出日志更为合理 当遇到并发场景 关闭DEBUG日志提升效率
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

    // events_.data() 获取 vector 的起始地址
    int numEvents = ::epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0) {
        LOG_INFO("%d events happened\n", numEvents); // LOG_DEBUG最合理
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size()) // 扩容操作
        {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    } else {
        if (saveErrno != EINTR) {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error!");
        }
    }
    return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
/**
 * 
 */
void EPollPoller::updateChannel(Channel* channel) {
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), index);

    // channel 在 Poller 中的状态
    if (index == kNew || index == kDeleted) {
        if (index == kNew) {
            int fd = channel->fd();
            channels_[fd] = channel;
        } else { // index == kDeleted
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else // channel 已经在 Poller 中注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

// 从Poller中删除channel
void EPollPoller::removeChannel(Channel* channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    // __FUNCTION__ 是一个宏，表示当前函数的名称
    LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events); // 设置channel的revents为epoll_wait返回的事件
        activeChannels->push_back(channel); // EventLoop就拿到了它的Poller给它返回的所有发生事件的 channel 列表了
    }
}

// 更新channel通道 其实就是调用epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel* channel) {
    epoll_event event;
    ::memset(&event, 0, sizeof(event));
    
    event.events = channel->events();   // 设置要监听的事件类型
    event.data.ptr = channel;

    int fd = channel->fd();

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        } else {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}