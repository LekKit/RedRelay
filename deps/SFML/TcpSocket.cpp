////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2018 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/IpAddress.hpp>
#include <SFML/Network/SocketImpl.hpp>
#include <SFML/System/Err.hpp>
#include <algorithm>
#include <cstring>

#ifdef _MSC_VER
    #pragma warning(disable: 4127) // "conditional expression is constant" generated by the FD_SET macro
#endif


namespace
{
    // Define the low-level send/receive flags, which depend on the OS
    #ifdef SFML_SYSTEM_LINUX
        const int flags = MSG_NOSIGNAL;
    #else
        const int flags = 0;
    #endif
}

namespace sf
{
////////////////////////////////////////////////////////////
TcpSocket::TcpSocket() :
Socket(Tcp)
{

}


////////////////////////////////////////////////////////////
unsigned short TcpSocket::getLocalPort() const
{
    if (getHandle() != priv::SocketImpl::invalidSocket())
    {
        // Retrieve informations about the local end of the socket
        sockaddr_in address;
        priv::SocketImpl::AddrLength size = sizeof(address);
        if (getsockname(getHandle(), reinterpret_cast<sockaddr*>(&address), &size) != -1)
        {
            return ntohs(address.sin_port);
        }
    }

    // We failed to retrieve the port
    return 0;
}


////////////////////////////////////////////////////////////
IpAddress TcpSocket::getRemoteAddress() const
{
    if (getHandle() != priv::SocketImpl::invalidSocket())
    {
        // Retrieve informations about the remote end of the socket
        sockaddr_in address;
        priv::SocketImpl::AddrLength size = sizeof(address);
        if (getpeername(getHandle(), reinterpret_cast<sockaddr*>(&address), &size) != -1)
        {
            return IpAddress(ntohl(address.sin_addr.s_addr));
        }
    }

    // We failed to retrieve the address
    return IpAddress::None;
}


////////////////////////////////////////////////////////////
unsigned short TcpSocket::getRemotePort() const
{
    if (getHandle() != priv::SocketImpl::invalidSocket())
    {
        // Retrieve informations about the remote end of the socket
        sockaddr_in address;
        priv::SocketImpl::AddrLength size = sizeof(address);
        if (getpeername(getHandle(), reinterpret_cast<sockaddr*>(&address), &size) != -1)
        {
            return ntohs(address.sin_port);
        }
    }

    // We failed to retrieve the port
    return 0;
}


////////////////////////////////////////////////////////////
Socket::Status TcpSocket::connect(const IpAddress& remoteAddress, unsigned short remotePort, Time timeout)
{
    // Disconnect the socket if it is already connected
    disconnect();

    // Create the internal socket if it doesn't exist
    create();

    // Create the remote address
    sockaddr_in address = priv::SocketImpl::createAddress(remoteAddress.toInteger(), remotePort);

    if (timeout <= Time::Zero)
    {
        // ----- We're not using a timeout: just try to connect -----

        // Connect the socket
        if (::connect(getHandle(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1)
            return priv::SocketImpl::getErrorStatus();

        // Connection succeeded
        return Done;
    }
    else
    {
        // ----- We're using a timeout: we'll need a few tricks to make it work -----

        // Save the previous blocking state
        bool blocking = isBlocking();

        // Switch to non-blocking to enable our connection timeout
        if (blocking)
            setBlocking(false);

        // Try to connect to the remote address
        if (::connect(getHandle(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) >= 0)
        {
            // We got instantly connected! (it may no happen a lot...)
            setBlocking(blocking);
            return Done;
        }

        // Get the error status
        Status status = priv::SocketImpl::getErrorStatus();

        // If we were in non-blocking mode, return immediately
        if (!blocking)
            return status;

        // Otherwise, wait until something happens to our socket (success, timeout or error)
        if (status == Socket::NotReady)
        {
            // Setup the selector
            fd_set selector;
            FD_ZERO(&selector);
            FD_SET(getHandle(), &selector);

            // Setup the timeout
            timeval time;
            time.tv_sec  = static_cast<long>(timeout.asMicroseconds() / 1000000);
            time.tv_usec = static_cast<long>(timeout.asMicroseconds() % 1000000);

            // Wait for something to write on our socket (which means that the connection request has returned)
            if (select(static_cast<int>(getHandle() + 1), NULL, &selector, NULL, &time) > 0)
            {
                // At this point the connection may have been either accepted or refused.
                // To know whether it's a success or a failure, we must check the address of the connected peer
                if (getRemoteAddress() != IpAddress::None)
                {
                    // Connection accepted
                    status = Done;
                }
                else
                {
                    // Connection refused
                    status = priv::SocketImpl::getErrorStatus();
                }
            }
            else
            {
                // Failed to connect before timeout is over
                status = priv::SocketImpl::getErrorStatus();
            }
        }

        // Switch back to blocking mode
        setBlocking(true);

        return status;
    }
}


////////////////////////////////////////////////////////////
void TcpSocket::disconnect()
{
    // Close the socket
    close();

    // Reset the pending packet data
    m_pendingPacket = PendingPacket();
}


////////////////////////////////////////////////////////////
Socket::Status TcpSocket::send(const void* data, std::size_t size)
{
    if (!isBlocking())
        err() << "Warning: Partial sends might not be handled properly." << std::endl;

    std::size_t sent;

    return send(data, size, sent);
}


////////////////////////////////////////////////////////////
Socket::Status TcpSocket::send(const void* data, std::size_t size, std::size_t& sent)
{
    // Check the parameters
    if (!data || (size == 0))
    {
        err() << "Cannot send data over the network (no data to send)" << std::endl;
        return Error;
    }

    // Loop until every byte has been sent
    int result = 0;
    for (sent = 0; sent < size; sent += result)
    {
        // Send a chunk of data
        result = ::send(getHandle(), static_cast<const char*>(data) + sent, static_cast<int>(size - sent), flags);

        // Check for errors
        if (result < 0)
        {
            Status status = priv::SocketImpl::getErrorStatus();

            if ((status == NotReady) && sent)
                return Partial;

            return status;
        }
    }

    return Done;
}


////////////////////////////////////////////////////////////
Socket::Status TcpSocket::receive(void* data, std::size_t size, std::size_t& received)
{
    // First clear the variables to fill
    received = 0;

    // Check the destination buffer
    if (!data)
    {
        err() << "Cannot receive data from the network (the destination buffer is invalid)" << std::endl;
        return Error;
    }

    // Receive a chunk of bytes
    int sizeReceived = recv(getHandle(), static_cast<char*>(data), static_cast<int>(size), flags);

    // Check the number of bytes received
    if (sizeReceived > 0)
    {
        received = static_cast<std::size_t>(sizeReceived);
        return Done;
    }
    else if (sizeReceived == 0)
    {
        return Socket::Disconnected;
    }
    else
    {
        return priv::SocketImpl::getErrorStatus();
    }
}


////////////////////////////////////////////////////////////
TcpSocket::PendingPacket::PendingPacket() :
Size        (0),
SizeReceived(0),
Data        ()
{

}

} // namespace sf
