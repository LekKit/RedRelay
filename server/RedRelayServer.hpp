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

#ifndef REDRELAY_SERVER
#define REDRELAY_SERVER

#define REDRELAY_SERVER_BUILD 9

#include <ctime>
#include <unordered_map>
#include <string>
#include <vector>
#include "IDPool.hpp"
#include <SFML/Network.hpp>

#ifdef REDRELAY_EPOLL
#include "EpollSelector.hpp"
#endif

#ifdef REDRELAY_MULTITHREAD
#include <thread>
#endif

namespace rs{

class RelayPacket{
private:
    std::size_t capacity;
    std::size_t size;
    uint8_t type;
    char* buffer;
public:
    RelayPacket(std::size_t Size=65536);
    ~RelayPacket();
    void Reallocate(std::size_t newcapacity);
    void SetType(const uint8_t Type);
    void AddByte(const uint8_t Byte);
    void AddShort(const uint16_t Short);
    void AddInt(const uint32_t Int);
    void AddString(const std::string& String);
    const char* GetPacket();
    std::size_t GetPacketSize() const;
    std::size_t GetSize() const;
    void Clear();
};

class Peer{
friend class RedRelayServer;
private:
    static sf::TcpSocket defsocket;

    char buffer[65536];
    uint32_t packetsize=0;
    uint32_t buffbegin=0;
    sf::TcpSocket* Socket=&defsocket;
    uint16_t UdpPort=0;
    float LastPing=0; //Time of last ping reply from peer
    std::string Name;
    std::vector<uint16_t> Channels; //Channels used by peer, represented as ID

    uint32_t MessageSize() const;
    uint8_t SizeOffset() const;
    bool MessageReady() const;

public:
    std::string GetName() const;
    const std::vector<uint16_t>& GetJoinedChannels() const;
    sf::IpAddress GetIP() const;
};

class Channel{
friend class RedRelayServer;
private:
    std::string Name;
    std::vector<uint16_t> Peers; //Peers in channel, represented as ID
    bool HideFromList=false, CloseOnLeave=false; //Channel flags
    uint16_t Master; //Channel master ID
public:
    std::string GetName() const;
    bool IsHidden() const;
    bool IsAutoClosed() const;
    const std::vector<uint16_t>& GetPeerList() const;
    uint16_t GetPeersCount() const;
    uint16_t GetMasterID() const;
};

class Connection{ //Used for clients before handshake
friend class RedRelayServer;
private:
    sf::TcpSocket* Socket=NULL;
    std::size_t received=0;
    char buffer[14];
};

class RedRelayServer{
private:
    struct callstruct{
        void (*Error)(const std::string& ErrorMessage);
        void (*ServerStart)(uint16_t Port);
        bool (*PeerConnect)(uint16_t PeerID, const sf::IpAddress& Address, std::string& DenyReason);
        void (*PeerDisconnect)(uint16_t PeerID);
        bool (*NameSet)(uint16_t PeerID, std::string& Name, std::string& DenyReason);
        bool (*ChannelJoin)(uint16_t PeerID, uint16_t ChannelID, std::string& DenyReason);
        bool (*ChannelLeave)(uint16_t PeerID, uint16_t ChannelID, std::string& DenyReason);
        void (*ChannelClosed)(uint16_t ChannelID);
        bool (*ChannelsListRequest)(uint16_t PeerID, std::string& DenyReason);
        void (*ServerMessageSent)(uint16_t PeerID, uint8_t Subchannel, const char* Packet, std::size_t Size);
        void (*ServerMessageBlast)(uint16_t PeerID, uint8_t Subchannel, const char* Packet, std::size_t Size);
    };
    callstruct Callbacks;

    //Server configuration
    uint16_t ConnectionsLimit, PeersLimit, ChannelsLimit, PeerChannelsLimit;
    bool GiveNewMaster, LoggingEnabled;
    uint8_t PingInterval;
    std::string WelcomeMessage;
    #ifdef REDRELAY_MULTITHREAD
    volatile bool Running, Destructible;
    #else
    bool Running, Destructible;
    #endif

    //Packet buffer
    RelayPacket packet;
    char UdpBuffer[65536];

    //Data containers for peers, channels
    std::unordered_map<std::string, uint16_t> ChannelNames;
    IndexedPool<Connection> ConnectionsPool;
    IndexedPool<Peer> PeersPool;
    IndexedPool<Channel> ChannelsPool;

    //Network interfaces
    sf::TcpListener TcpListener;
    sf::UdpSocket UdpSocket;
#ifdef REDRELAY_EPOLL
    EpollSelector Selector;
#else
    sf::SocketSelector Selector;
#endif

#ifdef REDRELAY_MULTITHREAD
    void UdpHandler();
#endif

    //Timers
    sf::Clock DeltaClock;
    sf::Clock TimerClock;
    float DeltaTime();
    float Timer();

    //Peers and connections related stuff
    void DropConnection(uint16_t ID);
    void DenyConnection(uint16_t ID, const std::string& Reason);
    void DenyNameChange(uint16_t ID, const std::string& Name, const std::string& Reason);
    void DenyChannelJoin(uint16_t ID, const std::string& Name, const std::string& Reason);
    void PeerLeftChannel(uint16_t Channel, uint16_t Peer);
    void PeerDroppedFromChannel(uint16_t Channel, uint16_t Peer);

    //Handling messages
    void HandleTCP(uint16_t ID, char* Msg, std::size_t Size, uint8_t Type);
    void NewConnection();
    void ReceiveUdp();
    void ReceiveTcp(uint16_t PeerID);
    void HandleConnection(uint16_t ConnectionID);
public:
    RedRelayServer();
    ~RedRelayServer();
    std::string GetVersion() const;
    void Log(std::string message, uint8_t colour=15);
    void SetConnectionsLimit(uint16_t Limit);
    void SetPeersLimit(uint16_t Limit);
    void SetChannelsLimit(uint16_t Limit);
    void SetChannelsPerPeerLimit(uint16_t Limit);
    void SetWelcomeMessage(const std::string& String);
    void SetLogEnabled(bool Flag);
    const Peer& GetPeer(uint16_t PeerID);
    const Channel& GetChannel(uint16_t ChannelID);
    void SetErrorCallback(void(*Error)(const std::string& ErrorMessage));
    void SetStartCallback(void(*ServerStarted)(uint16_t));
    void SetConnectCallback(bool(*PeerConnect)(uint16_t, const sf::IpAddress&, std::string&));
    void SetDisconnectCallback(void(*DisconnectCallback)(uint16_t));
    void SetNameCallback(bool(*NameSet)(uint16_t, std::string&, std::string&));
    void SetChannelJoinCallback(bool(*ChannelJoin)(uint16_t, uint16_t, std::string&));
    void SetChannelLeaveCallback(bool(*ChannelLeave)(uint16_t, uint16_t, std::string&));
    void SetChannelClosedCallback(void(*ChannelClosed)(uint16_t));
    void SetChannelsListRequestCallback(bool(*ChannelsListRequest)(uint16_t, std::string&));
    void SetServerSentCallback(void(*ServerMessageSent)(uint16_t, uint8_t, const char*, std::size_t));
    void SetServerBlastCallback(void(*ServerMessageBlast)(uint16_t, uint8_t, const char*, std::size_t));
    void DropPeer(uint16_t ID);
    void Start(uint16_t Port=6121);
    void Stop(bool Block=false);
    bool IsRunning();
};

}
#endif
