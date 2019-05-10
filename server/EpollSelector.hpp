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

#ifndef EPOLL_SELECTOR
#define EPOLL_SELECTOR

#ifdef __linux__
#include <unistd.h>
#include <sys/epoll.h>
#define epoll_close(fd) close(fd)
typedef int epolld;
#define INVAL_FD -1
#elif __WIN32
#include "wepoll.h"
typedef HANDLE epolld;
#define INVAL_FD NULL
#else
#error Epoll not supported on target platform
#endif

class EpollSelector{
private:
    std::size_t maxevents;
    epolld epoll_fd;
    epoll_event* events;
public:
    EpollSelector(std::size_t Size=1024);
    ~EpollSelector();
    void add(const sf::Socket& sock, uint32_t id);
    void remove(const sf::Socket& sock);
    void mod(const sf::Socket& sock, uint32_t id);
    int wait(int timeout=-1);
    const epoll_event& getevent(uint32_t id) const;
    uint32_t at(uint32_t id) const;
};

#endif
