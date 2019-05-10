#include <iostream>
#include "ModSocket.hpp"
#include "EpollSelector.hpp"

#ifdef _WIN32
#warning Epoll support is experimental on Windows trough libwepoll
#endif

EpollSelector::EpollSelector(std::size_t Size){
    maxevents = Size;
    events = new epoll_event[Size];
    epoll_fd = epoll_create(64);
    if (epoll_fd==INVAL_FD) std::cout<<"Failed to create epoll descriptor"<<std::endl;
}

EpollSelector::~EpollSelector(){
    if (epoll_close(epoll_fd)) std::cout<<"Failed to close epoll descriptor"<<std::endl;
    delete[] events;
}

void EpollSelector::add(const sf::Socket& sock, uint32_t id){
    epoll_event event = epoll_event();
    event.events=EPOLLIN|EPOLLHUP|EPOLLRDHUP;
    event.data.u32=id;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock.getHandle(), &event);
}

void EpollSelector::remove(const sf::Socket& sock){
    epoll_event event = epoll_event();
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock.getHandle(), &event);
}

void EpollSelector::mod(const sf::Socket& sock, uint32_t id){
    epoll_event event = epoll_event();
    event.events=EPOLLIN|EPOLLHUP|EPOLLRDHUP;
    event.data.u32=id;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock.getHandle(), &event);
}

int EpollSelector::wait(int timeout){
    return epoll_wait(epoll_fd, events, maxevents, timeout);
}

const epoll_event& EpollSelector::getevent(uint32_t id) const {
    return events[id];
}

uint32_t EpollSelector::at(uint32_t id) const {
    return events[id].data.u32;
}
