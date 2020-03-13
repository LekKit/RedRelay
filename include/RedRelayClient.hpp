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

#ifndef REDRELAY_CLIENT
#define REDRELAY_CLIENT

#include <string>
#include <cstring>
#include <vector>
#include <SFML/Network.hpp>

#define REDRELAY_CLIENT_BUILD 10

namespace rc{

//Forward declaration (some compilers might need it)
class RedRelayClient;
class RelayPacket;
class Channel;

class Peer{
friend class RedRelayClient;
friend class Channel;
private:
    Peer(uint16_t PeerID, const std::string& PeerName);
    uint16_t ID;
    std::string Name;
public:
    uint16_t GetID() const;
    std::string GetName() const;
};

class Channel{
friend class RedRelayClient;
private:
    static const Peer defpeer;
    uint16_t ID;
    std::string Name;
    std::vector<Peer> Peers;
    uint8_t Flags;
    uint16_t Master;
    Channel(uint16_t ChannelID, const std::string& ChannelName, uint8_t ChannelFlags);
public:
    //Valid channel flags (only used when creating a new channel)
    enum ChannelFlags{
        HideFromList=1, //Hide the channel from the server channels list
        CloseOnLeave=2  //Close the channel when its creator (channel master) leaves
    };
    uint16_t GetID() const;
    std::string GetName() const;
    std::size_t GetPeerCount() const;
    const std::vector<Peer>& GetPeerList() const;
    const Peer& GetPeer(uint16_t ID) const;
    const Peer& GetPeer(const std::string& Name) const;
    uint16_t GetMasterID() const;
    uint8_t GetFlags() const;
    bool IsHidden() const;
    bool IsAutoClosed() const;
};

//Event class containing it's type and specific data, new events will be pushed into RedRelayClient::Events on RedRelayClient::Update()
class Event{
friend class RedRelayClient;
private:
    uint16_t m_short1, m_short2, m_short3;
    std::string m_string;
    Event(uint8_t EventType, const std::string& Message="", uint16_t Short1=0, uint16_t Short2=0, uint16_t Short3=0);
public:
    Event();
    uint8_t Type;
    enum Type{
        Error,              //An error occurred
        Connected,          //TCP handshake completed
        Established,        //UDP handshake completed
        ConnectDenied,      //Connection denied - server may be full, may have banned you, etc
        Disconnected,       //Disconnected from the server - on purpose, or the server dropped the connection
        NameSet,            //Successful name set
        NameDenied,         //Name set denied - the name conflicted with other peer name, contained illegal characters, etc
        ChannelJoin,        //Successful channel join
        ChannelDenied,      //Channel join denied  - the client has no name, it's name conflicts with other peer in channel, etc
        ChannelLeave,       //You left the channel - on purpose, or the server kicked you from there
        ChannelLeaveDenied, //Channel leave denied - there was no such channel to leave
        ListReceived,       //Channels list received - will be followed by Event::ListEntry
        ListEntry,          //A single channel from the list
        ListDenied,         //Channels list request denied - server may have this option disabled, or you requested it too many times
        PeerJoined,         //A peer joined one of the channels you were in
        PeerLeft,           //A peer from of the channels you were in left
        PeerChangedName,    //A peer from of the channels you were in changed its name
        ChannelBlast,       //A peer from of the channels you were in issued a blast (UDP) broadcast to the channel
        ChannelSent,        //A peer from of the channels you were in issued a send (TCP) broadcast to the channel
        PeerBlast,          //A peer from of the channels you were in blast you a private message
        PeerSent            //A peer from of the channels you were in sent you a private message
    };
    std::string ErrorMessage() const;
    std::string DenyMessage() const;
    std::string WelcomeMessage() const;
    std::string DisconnectAddress() const;
    uint16_t ChannelsCount() const;
    std::string ChannelName() const;
    uint16_t ChannelID() const;
    uint16_t PeersCount() const;
    std::string PeerName() const;
    uint16_t PeerID() const;
    bool PeerWasMaster() const;
    const char* Address() const;
    uint32_t Size() const;
    uint8_t Subchannel() const;
    uint8_t Variant() const;
    uint8_t UByte(uint32_t Index) const;
    int8_t Byte(uint32_t Index) const;
    uint16_t UShort(uint32_t Index) const;
    int16_t Short(uint32_t Index) const;
    uint32_t UInt(uint32_t Index) const;
    int32_t Int(uint32_t Index) const;
    uint64_t ULong(uint32_t Index) const;
    int64_t Long(uint32_t Index) const;
    float Float(uint32_t Index) const;
    double Double(uint32_t Index) const;
    std::string String(uint32_t Index) const;
    std::string String(uint32_t Index, uint32_t Size) const;
};

//Packet building class providing dynamic buffer for you data, also manages endianness properly
class Binary{
friend class RelayPacket;
private:
    std::size_t capacity;
    std::size_t size;
    char* buffer;
public:
    Binary(std::size_t Size=64);
    ~Binary();
    void Reallocate(std::size_t newcapacity);
    void Resize(std::size_t Size);
    const char* GetAddress() const;
    std::size_t GetSize() const;
    void Clear();
    void AddByte(uint8_t Byte);
    void AddShort(uint16_t Short);
    void AddInt(uint32_t Int);
    void AddLong(uint64_t Long);
    void AddFloat(float Float);
    void AddDouble(double Double);
    void AddString(const std::string& String);
    void AddNullString(const std::string& String);
    void AddBinary(const void* Data, std::size_t Size);
};

class RelayPacket : public Binary{
private:
    uint8_t type;
public:
    void SetType(const uint8_t Type);
    void SetVariant(const uint8_t Variant);
    const char* GetPacket();
    std::size_t GetPacketSize() const;
    void Clear();
};

//TCP stream reader for internal usage
class PacketReader{
private:
    char* buffer;
    std::size_t capacity;
    std::size_t received;
    uint32_t packetbegin;
    uint8_t SizeOffset() const;
public:
    PacketReader(std::size_t size=64);
    ~PacketReader();
    void Reallocate(std::size_t newcapacity);
    void CheckBounds();
    void* GetReceiveAddr();
    std::size_t GetReceiveSize() const;
    void Received(uint32_t size);
    bool PacketReady();
    char* GetPacket() const;
    std::size_t PacketSize() const;
    uint8_t GetPacketType() const;
    void NextPacket();
    void Clear();
};

class RedRelayClient{
private:
    uint8_t ConnectState=0;
    static const Channel defchannel;
    uint16_t PeerID;
    std::string Name;
    std::vector<Channel> Channels;
    std::vector<Event> nextevents;
    uint16_t SelectedChannel;
    PacketReader reader;
    RelayPacket packet;
    char UdpBuffer[65536];
    sf::TcpSocket TcpSocket;
    sf::UdpSocket UdpSocket;
    sf::Clock TimerClock;
    float LastTimer=0;

    void SendTcp(const void* data, std::size_t size);
    void HandleTCP(const char* Msg, std::size_t Size, uint8_t Type);
    void HandleUDP(std::size_t received);
    float Timer();
public:
    enum ConnectState{
        Disconnected,   //Not connected to server
        Connecting,     //Connecting through TCP
        RequestingTcp,  //Waiting for connect response from the server
        RequestingUdp,  //Waiting for UDP handshake completion, TCP may be used now
        Established     //Connection sequence completed
    };
    std::vector<Event> Events;
    RedRelayClient();
    std::string GetVersion() const;
    void Connect(const std::string& Address, uint16_t Port=6121);
    void Disconnect();
    std::string GetHostAddress() const;
    uint16_t GetHostPort() const;
    uint8_t GetConnectState() const;
    void Update();
    uint16_t SelfID() const;
    void SetName(const std::string& Name);
    std::string SelfName() const;
    void JoinChannel(const std::string& ChannelName, uint8_t Flags=0);
    const std::vector<Channel>& GetJoinedChannels() const;
    const Channel& GetChannel(const std::string& Name="") const;
    const Channel& GetChannel(uint16_t ID) const ;
    void LeaveChannel(const std::string& Name="");
    void LeaveChannel(uint16_t ID);
    void RequestChannelsList();
    void SelectChannel(const std::string& Name);
    void SelectChannel(uint16_t ID);
    void ChannelSend(const void* Data, std::size_t Size, uint8_t Subchannel, uint8_t Variant=2, uint16_t ChannelID=65535);
    void ChannelSend(const Binary& Binary, uint8_t Subchannel, uint8_t Variant=2, uint16_t ChannelID=65535);
    void PeerSend(const void* Data, std::size_t Size, uint16_t PeerID, uint8_t Subchannel, uint8_t Variant=2, uint16_t ChannelID=65535);
    void PeerSend(const Binary& Binary, uint16_t PeerID, uint8_t Subchannel, uint8_t Variant=2, uint16_t ChannelID=65535);
    void ChannelBlast(const void* Data, std::size_t Size, uint8_t Subchannel, uint8_t Variant=2, uint16_t ChannelID=65535);
    void ChannelBlast(const Binary& Binary, uint8_t Subchannel, uint8_t Variant=2, uint16_t ChannelID=65535);
    void PeerBlast(const void* Data, std::size_t Size, uint16_t PeerID, uint8_t Subchannel, uint8_t Variant=2, uint16_t ChannelID=65535);
    void PeerBlast(const Binary& Binary, uint16_t PeerID, uint8_t Subchannel, uint8_t Variant=2, uint16_t ChannelID=65535);
};

}

#endif
