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

#include "RedRelayServer.hpp"
#include "Platform.hpp"
#include "ConsoleColors.hpp"
#include <streambuf>
#include <iostream>
#include <cstring>
#include <csignal>

#ifdef REDRELAY_DEVBUILD
    #define DebugLog(a) Log(a, 12)
#else
    #define DebugLog
#endif
namespace rs{

float RedRelayServer::DeltaTime(){
	return DeltaClock.restart().asMilliseconds()*0.001f;
}

static std::string DualDigit(uint8_t num){
	std::string str = std::to_string(num);
	return ("00"+str).substr(str.length(), 2);
}

void RedRelayServer::Log(std::string message, uint8_t color){
	if (!LoggingEnabled) return;
	ChangeConsoleColor(15);
	time_t now = time(0);
    struct tm* time = localtime(&now);
	std::cout<<"["<<DualDigit(time->tm_hour)<<":"<<DualDigit(time->tm_min)<<":"<<DualDigit(time->tm_sec)<<"] ";
	ChangeConsoleColor(color);
	std::cout<<message<<std::endl;
}

void RedRelayServer::DropConnection(uint16_t ID){
	if (!ConnectionsPool.Allocated(ID)) return;
	Selector.remove(*ConnectionsPool[ID].Socket);
	delete ConnectionsPool[ID].Socket;
	ConnectionsPool.Deallocate(ID);
}

void RedRelayServer::DenyConnection(uint16_t ID, const std::string& Reason){
	packet.Clear();
	packet.SetType(0);
	packet.AddByte(0);
	packet.AddByte(false);
	packet.AddString(Reason);
	ConnectionsPool[ID].Socket->send(packet.GetPacket(), packet.GetPacketSize());
	DropConnection(ID);
}

void RedRelayServer::DenyNameChange(uint16_t ID, const std::string& Name, const std::string& Reason){
	packet.Clear();
	packet.SetType(0);
	packet.AddByte(1);
	packet.AddByte(false);
	packet.AddByte(Name.length());
	packet.AddString(Name);
	packet.AddString(Reason);
	PeersPool[ID].Socket->send(packet.GetPacket(), packet.GetPacketSize());
}

void RedRelayServer::DenyChannelJoin(uint16_t ID, const std::string& Name, const std::string& Reason){
	packet.Clear();
	packet.SetType(0);
	packet.AddByte(2);
	packet.AddByte(false);
	packet.AddByte(Name.length());
	packet.AddString(Name);
	packet.AddString(Reason);
	PeersPool[ID].Socket->send(packet.GetPacket(), packet.GetPacketSize());
}

void RedRelayServer::PeerLeftChannel(uint16_t Channel, uint16_t Peer){
	packet.Clear();
	packet.SetType(9);
	packet.AddShort(Channel);
	packet.AddShort(Peer);
	for (uint16_t peerID : ChannelsPool[Channel].Peers)
		PeersPool[peerID].Socket->send(packet.GetPacket(), packet.GetPacketSize());
}

void RedRelayServer::PeerDroppedFromChannel(uint16_t Channel, uint16_t Peer){
	packet.Clear();
	packet.SetType(0);
	packet.AddByte(3);
	packet.AddByte(true);
	packet.AddShort(Channel);
	PeersPool[Peer].Socket->send(packet.GetPacket(), packet.GetPacketSize());
}

void RedRelayServer::HandleTCP(uint16_t ID, char* Msg, std::size_t Size, uint8_t Type){
	Peer& Client = PeersPool[ID];
	switch (Type>>4){
	case 0:
		if (Size==0) return;
		switch ((unsigned char)Msg[0]){
		case 1:
			{
				std::string Name = std::string(&Msg[1], (Size<256 ? Size-1:255));
				if (Size==1){
					DenyNameChange(ID, Name, "Can't set a blank name");
					return;
				}
				if (Size>256){
					DenyNameChange(ID, Name, "Name is too long");
					return;
				}
				if (Client.Name!=Name){
					for (uint16_t channelID : Client.Channels) for (uint16_t peerID : ChannelsPool[channelID].Peers)
						if (PeersPool[peerID].Name==Name){
							DenyNameChange(ID, Name, "Name already taken in channel "+ChannelsPool[channelID].Name);
							return;
						}
					if (Callbacks.NameSet!=NULL){
						std::string DenyReason;
						if (!Callbacks.NameSet(ID, Name, DenyReason)){
							DenyNameChange(ID, Name, DenyReason);
							return;
						}
					}
					Log(std::to_string(ID)+" | Peer "+PeersPool[ID].Name+" set name to "+Name, 6);
					for (uint16_t channelID : Client.Channels) for (uint16_t peerID : ChannelsPool[channelID].Peers)
						if (peerID!=ID){
							packet.Clear();
							packet.SetType(9);
							packet.AddShort(channelID);
							packet.AddShort(ID);
							packet.AddByte(ChannelsPool[channelID].Master==ID);
							packet.AddString(Name);
							PeersPool[peerID].Socket->send(packet.GetPacket(), packet.GetPacketSize());
						}
				}
				Client.Name=Name;
				packet.Clear();
				packet.SetType(0);
				packet.AddByte(1);
				packet.AddByte(true);
				packet.AddByte(Name.length());
				packet.AddString(Name);
				Client.Socket->send(packet.GetPacket(), packet.GetPacketSize());
			}
			break;
		case 2:
			{
				if (Size<3){
					DenyChannelJoin(ID, "", "Can't join a channel with a blank name");
					return;
				}
				std::string ChannelName = std::string(&Msg[2], Size-2);
				bool HideFromList=(Msg[1]&1)!=0, CloseOnLeave=(Msg[1]&2)!=0;
				if (Client.Name==""){
					DenyChannelJoin(ID, ChannelName, "Set a name before joining a channel");
					return;
				}
				if (Client.Channels.size()>=PeerChannelsLimit){
					DenyChannelJoin(ID, ChannelName, "You joined too many channels");
					return;
				}
				if (ChannelNames.count(ChannelName) == 0){
					if (ChannelsPool.GetAllocated().size()>=ChannelsLimit){
						DenyChannelJoin(ID, ChannelName, "Channels limit reached");
						return;
					}
					for (uint16_t channelID=0; channelID<ChannelsLimit; ++channelID) if (!ChannelsPool.Allocated(channelID)){
						ChannelsPool.Allocate(channelID);
						ChannelNames[ChannelName]=channelID;
						ChannelsPool[channelID].Name=ChannelName;
						ChannelsPool[channelID].Master=ID;
						ChannelsPool[channelID].HideFromList=HideFromList;
						ChannelsPool[channelID].CloseOnLeave=CloseOnLeave;
						ChannelsPool[channelID].AddPeer(ID);

						if (Callbacks.ChannelJoin!=NULL){
							std::string DenyReason;
							if (!Callbacks.ChannelJoin(ID, channelID, DenyReason)){
								DenyChannelJoin(ID, ChannelName, DenyReason);
								ChannelsPool.Deallocate(channelID);
								ChannelNames.erase(ChannelName);
								return;
							}
						}

						Log("Created channel " + ChannelName + (HideFromList ? std::string(", hidden") : "") + (CloseOnLeave ? std::string(", closed on leave") : ""), 11);
						Log(std::to_string(ID)+" | Peer "+PeersPool[ID].Name+" joined channel "+ChannelName, 3);

						Client.AddChannel(channelID);

						packet.Clear();
						packet.SetType(0);
						packet.AddByte(2);
						packet.AddByte(true);
						packet.AddByte(true);
						packet.AddByte(ChannelName.length());
						packet.AddString(ChannelName);
						packet.AddShort(channelID);
						Client.Socket->send(packet.GetPacket(), packet.GetPacketSize());
						return;
					}
				} else {
					uint16_t channelID=ChannelNames[ChannelName];
					if (Client.IsInChannel(channelID)){
						DenyChannelJoin(ID, ChannelName, "You are in this channel already");
						return;
					}
					for (uint16_t peerID : ChannelsPool[channelID].Peers) if (PeersPool[peerID].Name==Client.Name){
						DenyChannelJoin(ID, ChannelName, "Name already taken");
						return;
					}

					if (Callbacks.ChannelJoin!=NULL){
						std::string DenyReason;
						if (!Callbacks.ChannelJoin(ID, channelID, DenyReason)){
							DenyChannelJoin(ID, ChannelName, DenyReason);
							return;
						}
					}

					Log(std::to_string(ID)+" | Peer "+PeersPool[ID].Name+" joined channel "+ChannelName, 3);

					packet.Clear();
					packet.SetType(9);
					packet.AddShort(channelID);
					packet.AddShort(ID);
					packet.AddByte(false);
					packet.AddString(Client.Name);
					for (uint16_t peerID : ChannelsPool[channelID].Peers)
						PeersPool[peerID].Socket->send(packet.GetPacket(), packet.GetPacketSize());

					packet.Clear();
					packet.SetType(0);
					packet.AddByte(2);
					packet.AddByte(true);
					packet.AddByte(false);
					packet.AddByte(ChannelName.length());
					packet.AddString(ChannelName);
					packet.AddShort(channelID);
					for (uint16_t peerID : ChannelsPool[channelID].Peers){
						packet.AddShort(peerID);
						packet.AddByte(peerID==ChannelsPool[channelID].Master);
						packet.AddByte(PeersPool[peerID].Name.length());
						packet.AddString(PeersPool[peerID].Name);
					}

					Client.AddChannel(channelID);
					ChannelsPool[channelID].AddPeer(ID);

					Client.Socket->send(packet.GetPacket(), packet.GetPacketSize());
				}

			}
			break;
		case 3:
			{
				if (Size<3) return;
				uint16_t channelID=(unsigned char)Msg[1]|(unsigned char)Msg[2]<<8;
				if (!ChannelsPool.Allocated(channelID)) return;
				if (Client.IsInChannel(channelID)){
					if (Callbacks.ChannelLeave!=NULL){
						std::string DenyReason;
						if (!Callbacks.ChannelLeave(ID, channelID, DenyReason)){
							packet.Clear();
							packet.SetType(0);
							packet.AddByte(3);
							packet.AddByte(false);
							packet.AddShort(channelID);
							packet.AddString(DenyReason);
							PeersPool[ID].Socket->send(packet.GetPacket(), packet.GetPacketSize());
							return;
						}
					}
					Log(std::to_string(ID)+" | Peer "+PeersPool[ID].Name+" left the channel "+ChannelsPool[channelID].Name, 8);
					Client.EraseChannel(channelID);
                    ChannelsPool[channelID].ErasePeer(ID);
					if (ChannelsPool[channelID].Peers.size()==0 || (ChannelsPool[channelID].CloseOnLeave && ChannelsPool[channelID].Master==ID)){
						if (Callbacks.ChannelClosed!=NULL) Callbacks.ChannelClosed(channelID);
						Log("Channel "+ChannelsPool[channelID].Name+" closed", 12);
						for (uint16_t peerID : ChannelsPool[channelID].Peers){
                            PeersPool[peerID].EraseChannel(channelID);
							PeerDroppedFromChannel(channelID, peerID);
						}
						PeerDroppedFromChannel(channelID, ID);
						ChannelNames.erase(ChannelsPool[channelID].Name);
						ChannelsPool.Deallocate(channelID);
					} else {
						if (ChannelsPool[channelID].Master==ID){
							if (GiveNewMaster && ChannelsPool[channelID].Peers.size()>0) ChannelsPool[channelID].Master = *ChannelsPool[channelID].Peers.begin();
							else ChannelsPool[channelID].Master=65000;
						}
						PeerDroppedFromChannel(channelID, ID);
						PeerLeftChannel(channelID, ID);
					}
				}
			}
			break;
		case 4:
			if (Callbacks.ChannelsListRequest!=NULL){
				std::string DenyReason;
				if (!Callbacks.ChannelsListRequest(ID, DenyReason)){
					packet.Clear();
					packet.SetType(0);
					packet.AddByte(4);
					packet.AddByte(false);
					packet.AddString(DenyReason);
					Client.Socket->send(packet.GetPacket(), packet.GetPacketSize());
					return;
				}
			}
			Log(std::to_string(ID)+" | Peer "+PeersPool[ID].Name+" requested channel list", 2);
			packet.Clear();
			packet.SetType(0);
			packet.AddByte(4);
			packet.AddByte(true);
			for (IndexedElement<Channel>it : ChannelsPool.GetAllocated()){
				if (!it.element->HideFromList){
					packet.AddShort(it.element->Peers.size());
					packet.AddByte(it.element->Name.length());
					packet.AddString(it.element->Name);
				}
			}
			Client.Socket->send(packet.GetPacket(), packet.GetPacketSize());
			break;
		default:
			break;
		}
		break;
	case 2:
		{
			if (Size<3) return;
			uint16_t channel=(unsigned char)Msg[1]|(unsigned char)Msg[2]<<8;
            if (Client.IsInChannel(channel)){
				char header[11];
				uint8_t headersize;
				header[0]=Type;
				if (Size+2<254){
					header[1]=Size+2;
					headersize=2;
				} else if (Size+2<65535){
					header[1]=(uint8_t)254;
					header[2]=(Size+2)&255;
					header[3]=((Size+2)>>8)&255;
					headersize=4;
				} else {
					header[1]=(uint8_t)255;
					header[2]=(Size+2)&255;
					header[3]=((Size+2)>>8)&255;
					header[4]=((Size+2)>>16)&255;
					header[5]=((Size+2)>>24)&255;
					headersize=6;
				}
				header[headersize++]=Msg[0];
				header[headersize++]=Msg[1];
				header[headersize++]=Msg[2];
				header[headersize++]=ID&255;
				header[headersize++]=(ID>>8)&255;
				for (uint16_t peerID : ChannelsPool[channel].Peers) if (peerID!=ID){
					PeersPool[peerID].Socket->send(header, headersize);
					if (Size>3) PeersPool[peerID].Socket->send(&Msg[3], Size-3);
				}
			}
		}
		break;
	case 3:
		{
			if (Size<5) return;
			uint16_t channel=(unsigned char)Msg[1]|(unsigned char)Msg[2]<<8, peer=(unsigned char)Msg[3]|(unsigned char)Msg[4]<<8;
			if (PeersPool.Allocated(peer) && Client.IsInChannel(channel) && PeersPool[peer].IsInChannel(channel)){
				char header[11];
				uint8_t headersize;
				header[0]=Type;
				if (Size<254){
					header[1]=Size;
					headersize=2;
				} else if (Size<65535){
					header[1]=(uint8_t)254;
					header[2]=Size&255;
					header[3]=(Size>>8)&255;
					headersize=4;
				} else {
					header[1]=(uint8_t)255;
					header[2]=Size&255;
					header[3]=(Size>>8)&255;
					header[4]=(Size>>16)&255;
					header[5]=(Size>>24)&255;
					headersize=6;
				}
				header[headersize++]=Msg[0];
				header[headersize++]=Msg[1];
				header[headersize++]=Msg[2];
				header[headersize++]=ID&255;
				header[headersize++]=(ID>>8)&255;
				PeersPool[peer].Socket->send(header, headersize);
				if (Size>5) PeersPool[peer].Socket->send(&Msg[5], Size-5);
				return;
			}
		}
		break;
	case 9:
        DebugLog(std::to_string(ID)+" | Ping reply");
		PeersPool[ID].PingTries=0;
		break;
	default:
		break;
	}
}

void RedRelayServer::NewConnection(){
	uint16_t connectID = ConnectionsPool.GetAllocated().size();
	if (connectID>=ConnectionsLimit) connectID=0;
	if (ConnectionsPool.Allocated(connectID)) DropConnection(connectID);
	ConnectionsPool.Allocate(connectID);
	sf::TcpSocket* Socket = new sf::TcpSocket;
	if (TcpListener.accept(*Socket) == sf::Socket::Done){
	#ifdef REDRELAY_EPOLL
		Selector.add(*Socket, connectID|0x10000);
	#else
		Selector.add(*Socket);
	#endif
		ConnectionsPool[connectID].Socket = Socket;
	} else {
		delete Socket;
		ConnectionsPool.Deallocate(connectID);
	}
}

void RedRelayServer::ReceiveUdp(){
	sf::IpAddress UdpAddress; uint16_t UdpPort;
	std::size_t received;
	UdpSocket.receive(UdpBuffer, 65536, received, UdpAddress, UdpPort);
	switch (((uint8_t)UdpBuffer[0])>>4){
	case 2: //Identifier 2 means ChannelMessage - broadcast message to all peers in given channel
	{
		if (received<6) break;
		uint16_t peer=(unsigned char)UdpBuffer[1]|(unsigned char)UdpBuffer[2]<<8;
		if (!PeersPool.Allocated(peer) || PeersPool[peer].Socket->getRemoteAddress()!=UdpAddress || PeersPool[peer].UdpPort!=UdpPort) break;
		uint16_t channel=(unsigned char)UdpBuffer[4]|(unsigned char)UdpBuffer[5]<<8;
		if (PeersPool[peer].IsInChannel(channel)){
			UdpBuffer[1]=UdpBuffer[3];
			UdpBuffer[2]=UdpBuffer[4];
			UdpBuffer[3]=UdpBuffer[5];
			UdpBuffer[4]=peer&255;
			UdpBuffer[5]=(peer>>8)&255;
			for (uint16_t peerID : ChannelsPool[channel].Peers) if (peerID!=peer)
				UdpSocket.send(UdpBuffer, received, PeersPool[peerID].Socket->getRemoteAddress(), PeersPool[peerID].UdpPort);
			break;
		}
	}
	break;

	case 3: //Identifier 3 means PeerMessage - send private message to given peer
	{
		if (received<8) break;
		uint16_t peer=(unsigned char)UdpBuffer[1]|(unsigned char)UdpBuffer[2]<<8;
		if (!PeersPool.Allocated(peer) || PeersPool[peer].Socket->getRemoteAddress()!=UdpAddress || PeersPool[peer].UdpPort!=UdpPort) break;
		uint16_t receiver=(unsigned char)UdpBuffer[6]|(unsigned char)UdpBuffer[7]<<8;
		uint16_t channel=(unsigned char)UdpBuffer[4]|(unsigned char)UdpBuffer[5]<<8;
		if (PeersPool.Allocated(receiver) && PeersPool[peer].IsInChannel(channel) && PeersPool[receiver].IsInChannel(channel)){
			UdpBuffer[6]=UdpBuffer[1];
			UdpBuffer[7]=UdpBuffer[2];
			UdpBuffer[2]=UdpBuffer[0];
			UdpSocket.send(&UdpBuffer[2], received-2, PeersPool[receiver].Socket->getRemoteAddress(), PeersPool[receiver].UdpPort);
			break;
		}
	}
	break;

	case 7: //Identifier 7 means UDPHello - respond with UDPWelcome
	{
		if (received<3) break;
		uint16_t peerID=(unsigned char)UdpBuffer[1]|(unsigned char)UdpBuffer[2]<<8;
        DebugLog(std::to_string(peerID)+" | UDPHello");
		if (PeersPool.Allocated(peerID) && (PeersPool[peerID].UdpPort==0 || PeersPool[peerID].UdpPort==UdpPort) && UdpAddress==PeersPool[peerID].Socket->getRemoteAddress()){
			PeersPool[peerID].UdpPort=UdpPort;
			UdpBuffer[0]=(uint8_t)(10<<4);
			UdpSocket.send(UdpBuffer, 1, UdpAddress, UdpPort);
		}
	}
	break;

	default:
	break;
	}
}

void RedRelayServer::ReceiveTcp(uint16_t PeerID){
	Peer& Peer = PeersPool[PeerID];
	std::size_t received;
	switch (Peer.Socket->receive(&Peer.buffer[Peer.buffbegin+Peer.packetsize], 65536-(Peer.buffbegin+Peer.packetsize), received)){
	case sf::Socket::Done:
		Peer.packetsize+=received;
		while (Peer.MessageReady()){
			HandleTCP(PeerID, &Peer.buffer[Peer.buffbegin+1+Peer.SizeOffset()], Peer.MessageSize(), Peer.buffer[Peer.buffbegin]);
			Peer.packetsize -= 1+Peer.SizeOffset()+Peer.MessageSize();
			Peer.buffbegin += 1+Peer.SizeOffset()+Peer.MessageSize();
		}
		if (Peer.buffbegin > 0 && Peer.buffbegin < 65536 && Peer.packetsize != 0){
			memcpy(&Peer.buffer[0], &Peer.buffer[Peer.buffbegin], Peer.packetsize);
		}
		Peer.buffbegin=0;
		break;

	case sf::Socket::Disconnected:
		DropPeer(PeerID);
		break;

	default:
		break;
	}
}

void RedRelayServer::HandleConnection(uint16_t ConnectionID){
	Connection& Connection = ConnectionsPool[ConnectionID];
	std::size_t received;
	switch (Connection.Socket->receive(&Connection.buffer[Connection.received], 14-Connection.received, received)){
	case sf::Socket::Done:
		Connection.received+=received;
		if (Connection.received>0 && Connection.buffer[0]!=0){
			DropConnection(ConnectionID);
			break;
		}
		if (Connection.received==14 && Connection.buffer[1]>>4==0 && Connection.buffer[2]==11 && Connection.buffer[3]==0 && std::string(&Connection.buffer[4], 10)=="revision 3"){
			uint16_t peerID=0;
			for (peerID=0; peerID<PeersLimit; ++peerID) if (!PeersPool.Allocated(peerID)){
				if (Callbacks.PeerConnect!=NULL){
					std::string DenyReason;
					if (!Callbacks.PeerConnect(peerID, Connection.Socket->getRemoteAddress(), DenyReason)){
						DenyConnection(ConnectionID, DenyReason);
						break;
					}
				}
				Log(std::to_string(peerID)+" | Peer connected from "+Connection.Socket->getRemoteAddress().toString(), 14);
				PeersPool.Allocate(peerID);
				PeersPool[peerID].Socket=Connection.Socket;
				packet.Clear();
				packet.SetType(0);
				packet.AddByte(0);
				packet.AddByte(true);
				packet.AddShort(peerID);
				packet.AddString(WelcomeMessage);
			#ifdef REDRELAY_EPOLL
				Selector.mod(*Connection.Socket, peerID|0x20000);
			#endif
				Connection.Socket->send(packet.GetPacket(), packet.GetPacketSize());
				ConnectionsPool.Deallocate(ConnectionID);
				break;
			}
			if (peerID==PeersLimit) DenyConnection(ConnectionID, "Server is full");
		}
		break;

	case sf::Socket::Disconnected:
		DropConnection(ConnectionID);
		break;

	default:
		break;
	}
}



RedRelayServer::RedRelayServer(){
	Callbacks = {NULL};
	ConnectionsLimit=16;
	PeersLimit=128;
	ChannelsLimit=32;
	PeerChannelsLimit=4;
	PingInterval=3;
	GiveNewMaster=true;
	LoggingEnabled=true;
	WelcomeMessage="RedRelay Server #"+std::to_string(REDRELAY_SERVER_BUILD)+" ("+OPERATING_SYSTEM+"/"+ARCHITECTURE+")";
	Running=false;
	Destructible=true;
}

std::string RedRelayServer::GetVersion() const {
	return "RedRelay Server #"+std::to_string(REDRELAY_SERVER_BUILD)+" ("+OPERATING_SYSTEM+"/"+ARCHITECTURE+")";
}

void RedRelayServer::SetPingInterval(uint8_t Interval){
	PingInterval = Interval;
}

void RedRelayServer::SetConnectionsLimit(uint16_t Limit){
	if (Limit>0) ConnectionsLimit=Limit;
}

void RedRelayServer::SetPeersLimit(uint16_t Limit){
	if (Limit>0) PeersLimit=Limit;
}

void RedRelayServer::SetChannelsLimit(uint16_t Limit){
	if (Limit>0) ChannelsLimit=Limit;
}

void RedRelayServer::SetChannelsPerPeerLimit(uint16_t Limit){
	if (Limit>0) PeerChannelsLimit=Limit;
}

void RedRelayServer::SetWelcomeMessage(const std::string& String){
	WelcomeMessage=String;
}

void RedRelayServer::SetLogEnabled(bool Flag){
	LoggingEnabled=Flag;
}

const Peer& RedRelayServer::GetPeer(uint16_t PeerID){
	return PeersPool[PeerID];
}

const Channel& RedRelayServer::GetChannel(uint16_t ChannelID){
	return ChannelsPool[ChannelID];
}

void RedRelayServer::SetErrorCallback(void(*Error)(const std::string& ErrorMessage)){
	Callbacks.Error = Error;
}

void RedRelayServer::SetStartCallback(void(*ServerStarted)(uint16_t)){
	Callbacks.ServerStart = ServerStarted;
}

void RedRelayServer::SetConnectCallback(bool(*PeerConnect)(uint16_t, const sf::IpAddress&, std::string&)){
	Callbacks.PeerConnect = PeerConnect;
}

void RedRelayServer::SetDisconnectCallback(void(*DisconnectCallback)(uint16_t)){
	Callbacks.PeerDisconnect = DisconnectCallback;
}

void RedRelayServer::SetNameCallback(bool(*NameSet)(uint16_t, std::string&, std::string&)){
	Callbacks.NameSet = NameSet;
}

void RedRelayServer::SetChannelJoinCallback(bool(*ChannelJoin)(uint16_t, uint16_t, std::string&)){
	Callbacks.ChannelJoin = ChannelJoin;
}

void RedRelayServer::SetChannelLeaveCallback(bool(*ChannelLeave)(uint16_t, uint16_t, std::string&)){
	Callbacks.ChannelLeave = ChannelLeave;
}

void RedRelayServer::SetChannelClosedCallback(void(*ChannelClosed)(uint16_t)){
	Callbacks.ChannelClosed = ChannelClosed;
}

void RedRelayServer::SetChannelsListRequestCallback(bool(*ChannelsListRequest)(uint16_t, std::string&)){
	Callbacks.ChannelsListRequest = ChannelsListRequest;
}

void RedRelayServer::SetServerSentCallback(void(*ServerMessageSent)(uint16_t, uint8_t, const char*, std::size_t)){
	Callbacks.ServerMessageSent = ServerMessageSent;
}

void RedRelayServer::SetServerBlastCallback(void(*ServerMessageBlast)(uint16_t, uint8_t, const char*, std::size_t)){
	Callbacks.ServerMessageBlast = ServerMessageBlast;
}

void RedRelayServer::DropPeer(uint16_t ID){
	if (!PeersPool.Allocated(ID)) return;
	if (Callbacks.PeerDisconnect!=NULL) Callbacks.PeerDisconnect(ID);
	Log(std::to_string(ID)+" | Peer "+PeersPool[ID].Name+" disconnected", 4);
	for (uint16_t channelID : PeersPool[ID].Channels){
        ChannelsPool[channelID].ErasePeer(ID);
		if (ChannelsPool[channelID].Peers.size()==0 || (ChannelsPool[channelID].CloseOnLeave && ChannelsPool[channelID].Master==ID)){
			if (Callbacks.ChannelClosed!=NULL) Callbacks.ChannelClosed(channelID);
			Log("Channel "+ChannelsPool[channelID].Name+" closed", 12);
			for (uint16_t peerID : ChannelsPool[channelID].Peers) {
                PeersPool[peerID].EraseChannel(channelID);
				PeerDroppedFromChannel(channelID, peerID);
			}
			ChannelNames.erase(ChannelsPool[channelID].Name);
			ChannelsPool.Deallocate(channelID);
		} else {
			if (ChannelsPool[channelID].Master==ID){
				if (GiveNewMaster && ChannelsPool[channelID].Peers.size()>0) ChannelsPool[channelID].Master = *ChannelsPool[channelID].Peers.begin();
				else ChannelsPool[channelID].Master=65535;
			}
			PeerLeftChannel(channelID, ID);
		}
	}
	Selector.remove(*PeersPool[ID].Socket);
	delete PeersPool[ID].Socket;
	PeersPool.Deallocate(ID);
}

#ifdef REDRELAY_MULTITHREAD
void RedRelayServer::UdpHandler(){
    while (Running){
        ReceiveUdp(); //This will block until there's data to receive or socket is closed
    }
}
#endif

void RedRelayServer::Start(uint16_t Port){
	{
		std::streambuf* previous = sf::err().rdbuf();
		sf::err().rdbuf(NULL);
		if (TcpListener.listen(Port) != sf::Socket::Done || UdpSocket.bind(Port) != sf::Socket::Done){
			if (Callbacks.Error!=NULL) Callbacks.Error("Could not bind port "+std::to_string(Port));
			Log("Error: Could not bind port "+std::to_string(Port), 4);
			return;
		}
		sf::err().rdbuf(previous);
	}
        #ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN); //Ignore writes to closed socket
        #endif
	if (Callbacks.ServerStart!=NULL) Callbacks.ServerStart(Port);
	Log(GetVersion() +
    #ifdef REDRELAY_DEVBUILD
        " DEVBUILD"+
    #endif
        " started on port "+std::to_string(Port), 12);

#ifdef REDRELAY_EPOLL
	#ifdef _WIN32
		WelcomeMessage+=", libwepoll";
		Log("Extended polling enabled (github.com/piscisaureus/wepoll)", 12);
	#elif KQUEUE
        WelcomeMessage+=", kqueue";
		Log("Extended polling enabled (kqueue)", 12);
	#else
		WelcomeMessage+=", epoll";
		Log("Extended polling enabled", 12);
	#endif

	Selector.add(TcpListener, 0);

	#ifndef REDRELAY_MULTITHREAD
	Selector.add(UdpSocket, 1);
	#endif
#else
	Selector.add(TcpListener);

	#ifndef REDRELAY_MULTITHREAD
	Selector.add(UdpSocket);
	#endif
#endif

	float WaitTime = PingInterval, LastPing;
	Running = true;
    Destructible = false;

#ifdef REDRELAY_MULTITHREAD
	Log("Multi-threading enabled", 12);
    std::thread UdpThread(&RedRelayServer::UdpHandler, this);
#endif

	while (Running){

	#ifdef REDRELAY_EPOLL
		uint32_t events = Selector.wait(WaitTime*1000);
		for (uint32_t i=0; i<events; ++i){

            #ifndef REDRELAY_MULTITHREAD
			if (Selector.at(i) == 1) ReceiveUdp();
			else
            #endif

			if ((Selector.at(i)&0x20000) != 0) ReceiveTcp(Selector.at(i)&65535);
			else

			if (Selector.at(i) == 0) NewConnection();
			else

			if ((Selector.at(i)&0x10000) != 0) HandleConnection(Selector.at(i)&65535);
		}
	#else
		if (Selector.wait(sf::seconds(WaitTime))){

            #ifndef REDRELAY_MULTITHREAD
			if (Selector.isReady(UdpSocket)) ReceiveUdp();
            #endif

			for (uint32_t i=0; i<PeersPool.Size(); ++i) if (Selector.isReady(*PeersPool.GetAllocated().at(i).element->Socket)) ReceiveTcp(PeersPool.GetAllocated().at(i).index);

			if (Selector.isReady(TcpListener)) NewConnection();

			for (uint32_t i=0; i<ConnectionsPool.Size(); ++i) if (Selector.isReady(*ConnectionsPool.GetAllocated().at(i).element->Socket)) HandleConnection(ConnectionsPool.GetAllocated().at(i).index);
		}
	#endif

		if (PingInterval != 0){
			WaitTime -= DeltaTime();
			if (WaitTime <= 0.1f){
				WaitTime = PingInterval;
				packet.Clear();
				packet.SetType(11);
				const char* tmp = packet.GetPacket();
                DebugLog("Ping tick");
				for (uint32_t i=0; i<PeersPool.Size(); ++i){
                    uint16_t peerID = PeersPool.GetAllocated().at(i).index;
                    if (PeersPool[peerID].PingTries > 2){
                        DebugLog(std::to_string(peerID)+" | Ping timeout");
						DropPeer(peerID);
					} else {
						if (PeersPool[peerID].PingTries > 0) {
							DebugLog(std::to_string(PeersPool.GetAllocated().at(i).index)+" | Ping request");
							UdpSocket.send(tmp, 1, PeersPool[peerID].Socket->getRemoteAddress(), PeersPool[peerID].UdpPort);
							PeersPool[peerID].Socket->send(tmp, packet.GetPacketSize());
						}
						PeersPool[peerID].PingTries++;
					}
				}
			}
		}
	}
	Log("Stopping the server...", 12);
    UdpSocket.unbind(); //this should have unblocked the UDP thread
#ifdef REDRELAY_MULTITHREAD
    UdpThread.join(); //waiting for thread to close
#endif
	for (IndexedElement<Connection>&it : ConnectionsPool.GetAllocated()) delete it.element->Socket;
	for (IndexedElement<Peer>&it : PeersPool.GetAllocated()) delete it.element->Socket;
	ConnectionsPool.Clear();
	PeersPool.Clear();
	ChannelsPool.Clear();
	ChannelNames.clear();
	TcpListener.close();
    Log("Server closed", 12);
    Destructible=true;
}

RedRelayServer::~RedRelayServer(){
    Running=false;
    while(!Destructible) sf::sleep(sf::milliseconds(1));
}

void RedRelayServer::Stop(bool Block){
	Running=false;
	if (Block) while(!Destructible) sf::sleep(sf::milliseconds(1));
}

bool RedRelayServer::IsRunning(){
	return Running;
}

}
