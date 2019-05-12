////////////////////////////////////////////////////////////
//
// RedRelay - a Lacewing Relay protocol reimplementation
// Copyright (c) 2019 LekKit (LekKit#4400 in Discord)
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//   claim that you wrote the original software. If you use this software
//   in a product, an acknowledgment in the product documentation would be
//   appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#include <iostream>
#include "ModSocket.hpp"
#include "EpollSelector.hpp"

EpollSelector::EpollSelector(std::size_t Size){
    maxevents = Size;
    events = new epoll_event[Size];
    #ifdef KQUEUE
    epoll_fd = kqueue();
    #else
    epoll_fd = epoll_create(64);
    #endif
    if (epoll_fd==INVAL_FD) std::cout<<"Failed to create epoll descriptor"<<std::endl;
}

EpollSelector::~EpollSelector(){
    if (epoll_close(epoll_fd)) std::cout<<"Failed to close epoll descriptor"<<std::endl;
    delete[] events;
}

void EpollSelector::add(const sf::Socket& sock, uint32_t id){
	epoll_event event = epoll_event();
    #ifdef KQUEUE
    EV_SET(&event, sock.getHandle(), EVFILT_READ, EV_ADD, 0, 0, (void*)id);
    kevent(epoll_fd, &event, 1, NULL, 0, NULL);
    #else
    event.events=EPOLLIN|EPOLLHUP|EPOLLRDHUP;
    event.data.u32=id;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock.getHandle(), &event);
    #endif
}

void EpollSelector::remove(const sf::Socket& sock){
	epoll_event event = epoll_event();
    #ifdef KQUEUE
    EV_SET(&event, sock.getHandle(), 0, EV_DELETE, 0, 0, NULL);
    kevent(epoll_fd, &event, 1, NULL, 0, NULL);
    #else
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sock.getHandle(), &event);
    #endif
}

void EpollSelector::mod(const sf::Socket& sock, uint32_t id){
	epoll_event event = epoll_event();
    #ifdef KQUEUE
    EV_SET(&event, sock.getHandle(), EVFILT_READ, EV_ADD, 0, 0, (void*)id);
    kevent(epoll_fd, &event, 1, NULL, 0, NULL);
    #else
    event.events=EPOLLIN|EPOLLHUP|EPOLLRDHUP;
    event.data.u32=id;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sock.getHandle(), &event);
    #endif
}

int EpollSelector::wait(int timeout){
    #ifdef KQUEUE
    timespec ts;
    ts.tv_sec = timeout/1000;
    ts.tv_nsec = (timeout-ts.tv_sec*1000)*1000000;
    return kevent(epoll_fd, NULL, 0, events, maxevents, &ts);
    #else
    return epoll_wait(epoll_fd, events, maxevents, timeout);
    #endif
}

uint32_t EpollSelector::at(uint32_t id) const {
    #ifdef KQUEUE
    return (uint32_t)events[id].udata;
    #else
    return events[id].data.u32;
    #endif
}
