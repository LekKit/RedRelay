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

#include "Platform.hpp"
#include "RedRelayClient.hpp"

namespace rc{

RedRelayClient::RedRelayClient(){
	TcpSocket.setBlocking(false);
	UdpSocket.setBlocking(false);
	TcpSocket.connect(sf::IpAddress(), 0);
}

void RedRelayClient::SendTcp(const void* data, std::size_t size){
	std::size_t sent;
	while (TcpSocket.send(data, size, sent) == sf::Socket::Partial) {
		data=(char*)data+sent;
		size-=sent;
	}
}

void RedRelayClient::HandleTCP(const char* Msg, std::size_t Size, uint8_t Type){
	switch (Type>>4){
	case 0:
		if (Size<2){
			nextevents.push_back(Event(Event::Error, "Reader error - message too short for a response"));
			return;
		}
		switch (Msg[0]){
            case 0:
			if (Msg[1]){
				if (Size<4){
					nextevents.push_back(Event(Event::Error, "Reader error - message too short for a connect response"));
					return;
				}
				PeerID=(uint8_t)Msg[2]|(uint8_t)Msg[3]<<8;
				Events.push_back(Event(Event::Connected, std::string(&Msg[4], Size-4)));
				ConnectState=RequestingUdp;
				LastTimer=Timer();
				UdpBuffer[0]=7<<4;
				UdpBuffer[1]=PeerID&255;
				UdpBuffer[2]=(PeerID>>8)&255;
				UdpSocket.send(UdpBuffer, 3, TcpSocket.getRemoteAddress(), TcpSocket.getRemotePort());
			} else {
				Events.push_back(Event(Event::ConnectDenied, std::string(&Msg[2], Size-2)));
				TcpSocket.disconnect();
			}
			break;
		case 1:
			if (Size<4){
				nextevents.push_back(Event(Event::Error, "Reader error - message too short for a rename response"));
				return;
			}
			if (Msg[1]){
				Name=std::string(&Msg[3], (uint8_t)Msg[2]);
				Events.push_back(Event(Event::NameSet));
			} else {
				Events.push_back(Event(Event::NameDenied, std::string(&Msg[3+(uint8_t)Msg[2]], Size-3-(uint8_t)Msg[2])));
			}
			break;
		case 2:
			if (Size<6){
				nextevents.push_back(Event(Event::Error, "Reader error - message too short for a channel response"));
				return;
			}
			if (Msg[1]){
				uint16_t ChannelID = (uint8_t)Msg[4+(uint8_t)Msg[3]]|(uint8_t)Msg[5+(uint8_t)Msg[3]]<<8;
				for (Channel&i : Channels) if (i.ID == ChannelID) break;
				Channels.push_back(Channel(ChannelID, std::string(&Msg[4], (uint8_t)Msg[3]), Msg[2]));
				SelectedChannel=ChannelID;
				Channels.at(Channels.size()-1).Master=SelfID();
				if (uint32_t(6+(uint8_t)Msg[3])<Size) Channels.at(Channels.size()-1).Master=65535;
				for (uint32_t i=6+(uint8_t)Msg[3]; i<Size; i+=(uint8_t)Msg[i+3]+4){
					Channels.at(Channels.size()-1).Peers.push_back(Peer((uint8_t)Msg[i]|(uint8_t)Msg[i+1]<<8, std::string(&Msg[i+4], (uint8_t)Msg[i+3])));
					if ((Msg[i+2]&1)!=0) Channels.at(Channels.size()-1).Master=(uint8_t)Msg[i]|(uint8_t)Msg[i+1]<<8;
				}
				Events.push_back(Event(Event::ChannelJoin,  Channels.at(Channels.size()-1).Name, 0, SelectedChannel));
			} else {
				Events.push_back(Event(Event::ChannelDenied, std::string(&Msg[3+(uint8_t)Msg[2]], Size-(uint8_t)Msg[2]-3)));
			}
			break;
		case 3:
			if (Size<4){
				nextevents.push_back(Event(Event::Error, "Reader error - message too short for a leave response"));
				return;
			}
			if (Msg[1]){
				uint16_t ChannelID = (uint8_t)Msg[2]|(uint8_t)Msg[3]<<8;
				for (uint32_t i=0; i<Channels.size(); ++i) if (Channels.at(i).ID == ChannelID){
					Events.push_back(Event(Event::ChannelLeave, Channels.at(i).Name, 0, Channels.at(i).ID));
					Channels.erase(Channels.begin()+i);
					if (i>0) SelectedChannel=Channels.at(i-1).ID;
				}
			} else {
				Events.push_back(Event(Event::ChannelLeaveDenied, std::string(&Msg[4], Size-4), 0, (uint8_t)Msg[2]|(uint8_t)Msg[3]<<8));
			}
			break;
		case 4:
			if (Msg[1]){
				uint16_t ChannelsCount=0;
				for (uint32_t i=2; i<Size; i+=(uint8_t)Msg[i+2]+3) ++ChannelsCount;
				Events.push_back(Event(Event::ListReceived, "", ChannelsCount));
				for (uint32_t i=2; i<Size; i+=(uint8_t)Msg[i+2]+3) Events.push_back(Event(Event::ListEntry, std::string(&Msg[i+3], (uint8_t)Msg[i+2]), 0, 0, (uint8_t)Msg[i]|(uint8_t)Msg[i+1]<<8));
			} else Events.push_back(Event(Event::ListDenied, std::string(&Msg[2], Size-2)));
		default:
			break;
		}
		break;
	case 2:
		if (Size<6) break;
		Events.push_back(Event(Event::ChannelSent, std::string(&Msg[5], Size-5), (uint8_t)Msg[3]|(uint8_t)Msg[4]<<8, (uint8_t)Msg[1]|(uint8_t)Msg[2]<<8, (uint8_t)Msg[0]|(Type&15)<<8));
		break;
	case 3:
		if (Size<6) break;
		Events.push_back(Event(Event::PeerSent, std::string(&Msg[5], Size-5), (uint8_t)Msg[3]|(uint8_t)Msg[4]<<8, (uint8_t)Msg[1]|(uint8_t)Msg[2]<<8, (uint8_t)Msg[0]|(Type&15)<<8));
		break;
	case 9:
		{
			if (Size<4) break;
			uint16_t channel=(uint8_t)Msg[0]|(uint8_t)Msg[1]<<8, peer=(uint8_t)Msg[2]|(uint8_t)Msg[3]<<8;
			bool quit=false;
			for (Channel&i : Channels) if (i.ID==channel){
				for (uint32_t j=0; j<i.Peers.size(); ++j) if (i.Peers.at(j).ID==peer){
					if (Size==4){
						if (i.Peers.at(j).ID==i.Master) i.Master=65535;
						Events.push_back(Event(Event::PeerLeft, i.Peers.at(j).Name, i.Peers.at(j).ID, i.ID, i.Master==65535));
						i.Peers.erase(i.Peers.begin()+j);
						quit=true;
						break;
					}
					if (Size==5 && Msg[4]){
						i.Master=peer;
						quit=true;
						break;
					}
					if (Size>5){
						Events.push_back(Event(Event::PeerChangedName, i.Peers.at(j).Name, i.Peers.at(j).ID,  i.ID));
						i.Peers.at(j).Name=std::string(&Msg[5], Size-5);
						quit=true;
						break;
					}
				}
				if (quit || Size<5) break;
				i.Peers.push_back(Peer(peer, std::string(&Msg[5], Size-5)));
				Events.push_back(Event(Event::PeerJoined, std::string(&Msg[5], Size-5), peer, i.ID));
				break;
			}
		}
		break;
	case 11:
		packet.Clear();
		packet.SetType(9);
		SendTcp(packet.GetPacket(), packet.GetPacketSize());
		break;
	default:
		break;
	}
}

void RedRelayClient::HandleUDP(std::size_t received){
	switch (((unsigned char)UdpBuffer[0])>>4){
	case 2:
		if (received<6) return;
		Events.push_back(Event(Event::ChannelBlast, std::string(&UdpBuffer[6], received-6), (uint8_t)UdpBuffer[4]|(uint8_t)UdpBuffer[5]<<8, (uint8_t)UdpBuffer[2]|(uint8_t)UdpBuffer[3]<<8, (uint8_t)UdpBuffer[1]|((uint8_t)UdpBuffer[0]&15)<<8));
		break;
	case 3:
		if (received<6) return;
		Events.push_back(Event(Event::PeerBlast, std::string(&UdpBuffer[6], received-6), (uint8_t)UdpBuffer[4]|(uint8_t)UdpBuffer[5]<<8, (uint8_t)UdpBuffer[2]|(uint8_t)UdpBuffer[3]<<8, (uint8_t)UdpBuffer[1]|((uint8_t)UdpBuffer[0]&15)<<8));
	case 10:
		if (ConnectState==RequestingUdp) Events.push_back(Event::Established);
		ConnectState=Established;
		break;
	default:
		break;
	}
}

float RedRelayClient::Timer(){
	return (float)(TimerClock.getElapsedTime().asMilliseconds()*0.001);
}

std::string RedRelayClient::GetVersion() const {
	return "RedRelay Client #"+std::to_string(REDRELAY_CLIENT_BUILD)+" ("+OPERATING_SYSTEM+"/"+ARCHITECTURE+")";
}

void RedRelayClient::Connect(const std::string& Address, uint16_t Port){
	if (ConnectState>Disconnected){
		nextevents.push_back(Event(Event::Error, "Socket error - Already connected to a server"));
		return;
	}
	reader.Clear();
	TcpSocket.connect(sf::IpAddress(Address), Port);
	LastTimer=Timer();
	ConnectState=Connecting;
}

void RedRelayClient::Disconnect(){
	if (ConnectState==Disconnected) return;
	nextevents.push_back(Event(Event::Disconnected, TcpSocket.getRemoteAddress().toString()+":"+std::to_string(TcpSocket.getRemotePort())));
	TcpSocket.disconnect();
	Channels.clear();
	reader.Clear();
	ConnectState=Disconnected;
}

std::string RedRelayClient::GetHostAddress() const {
	return TcpSocket.getRemoteAddress().toString();
}

uint16_t RedRelayClient::GetHostPort() const {
	return TcpSocket.getRemotePort();
}

uint8_t RedRelayClient::GetConnectState() const {
	return ConnectState;
}

void RedRelayClient::Update(){
	for (Event&i : nextevents) Events.push_back(i);
	nextevents.clear();
	std::size_t received;
	sf::Socket::Status status;
	do{
		reader.CheckBounds();
		status = TcpSocket.receive(reader.GetReceiveAddr(), reader.GetReceiveSize(), received);
		if (status == sf::Socket::Done){
			reader.Received(received);
			while (reader.PacketReady()){
				HandleTCP(reader.GetPacket(), reader.PacketSize(), reader.GetPacketType());
				reader.NextPacket();
			}
		}
	} while (status==sf::Socket::Done);
	if (status==sf::Socket::Disconnected && ConnectState>Connecting) Disconnect();
	sf::IpAddress UdpAddress; uint16_t UdpPort;
	while (UdpSocket.receive(UdpBuffer, 65536, received, UdpAddress, UdpPort) == sf::Socket::Done) if (UdpAddress==TcpSocket.getRemoteAddress()) HandleUDP(received);
	if (ConnectState<Established && ConnectState>Disconnected){
		switch (ConnectState){
		case Connecting:
			if (TcpSocket.receive(&received, 0, received)==sf::Socket::Disconnected || TcpSocket.getRemotePort()==0){
				if (Timer()>LastTimer+3){
					Events.push_back(Event(Event::Error, "Socket error - Error connecting"));
					Disconnect();
				}
				return;
			} else {
				char tmp=0;
				SendTcp(&tmp, 1);
				sf::sleep(sf::milliseconds(10));
				packet.Clear();
				packet.SetType(0);
				packet.AddByte(0);
				packet.AddString("revision 3");
				SendTcp(packet.GetPacket(), packet.GetPacketSize());
				ConnectState=RequestingTcp;
			}
			break;
		case RequestingUdp:
			if (Timer()<LastTimer+1) return;
			LastTimer=Timer();
			UdpBuffer[0]=7<<4;
			UdpBuffer[1]=PeerID&255;
			UdpBuffer[2]=(PeerID>>8)&255;
			UdpSocket.send(UdpBuffer, 3, TcpSocket.getRemoteAddress(), TcpSocket.getRemotePort());
			break;
		default:
			break;
		}
	}
}

uint16_t RedRelayClient::SelfID() const {
	return PeerID;
}

void RedRelayClient::SetName(const std::string& Name){
	if (ConnectState<RequestingUdp) return;
	packet.Clear();
	packet.SetType(0);
	packet.AddByte(1);
	packet.AddString(Name);
	SendTcp(packet.GetPacket(), packet.GetPacketSize());
}

std::string RedRelayClient::SelfName() const {
	return Name;
}

void RedRelayClient::JoinChannel(const std::string& ChannelName, uint8_t Flags){
	if (ConnectState<RequestingUdp) return;
	packet.Clear();
	packet.SetType(0);
	packet.AddByte(2);
	packet.AddByte(Flags);
	packet.AddString(ChannelName);
	SendTcp(packet.GetPacket(), packet.GetPacketSize());
}

const std::vector<Channel>& RedRelayClient::GetJoinedChannels() const {
	return Channels;
}

const Channel& RedRelayClient::GetChannel(const std::string& Name) const {
	if (Name=="" && Channels.size()>0) return GetChannel(SelectedChannel);
	for (const Channel&i : Channels) if (i.Name==Name) return i;
	return defchannel;
}

const Channel& RedRelayClient::GetChannel(uint16_t ID) const {
	for (const Channel&i : Channels) if (i.ID==ID) return i;
	return defchannel;
}

void RedRelayClient::LeaveChannel(const std::string& Name){
	if (ConnectState<RequestingUdp) return;
	if (Name=="" && Channels.size()>0){
		LeaveChannel(SelectedChannel);
		return;
	} else
	for (Channel&i : Channels) if (i.Name==Name){
		packet.Clear();
		packet.SetType(0);
		packet.AddByte(3);
		packet.AddShort(i.ID);
		SendTcp(packet.GetPacket(), packet.GetPacketSize());
	}
}

void RedRelayClient::LeaveChannel(uint16_t ID){
	for (Channel&i : Channels) if (i.ID==ID){
		packet.Clear();
		packet.SetType(0);
		packet.AddByte(3);
		packet.AddShort(ID);
		SendTcp(packet.GetPacket(), packet.GetPacketSize());
	}
}

void RedRelayClient::RequestChannelsList(){
	if (ConnectState<RequestingUdp) return;
	packet.Clear();
	packet.SetType(0);
	packet.AddByte(4);
	SendTcp(packet.GetPacket(), packet.GetPacketSize());
}

void RedRelayClient::SelectChannel(const std::string& Name){
	for (Channel&i : Channels) if (i.Name==Name) SelectedChannel=i.ID;
}

void RedRelayClient::SelectChannel(uint16_t ID){
	for (Channel&i : Channels) if (i.ID==ID) SelectedChannel=ID;
}

void RedRelayClient::ChannelSend(const void* Data, std::size_t Size, uint8_t Subchannel, uint8_t Variant, uint16_t ChannelID){
	if (ConnectState<RequestingUdp) return;
	if (ChannelID==65535) ChannelID=SelectedChannel;
	for (const Channel&i : Channels) if (i.ID==ChannelID){
		packet.Clear();
		packet.SetType(2);
		packet.SetVariant(Variant);
		packet.AddByte(Subchannel);
		packet.AddShort(ChannelID);
		packet.AddBinary(Data, Size);
		SendTcp(packet.GetPacket(), packet.GetPacketSize());
		return;
	}
}

void RedRelayClient::ChannelSend(const Binary& Binary, uint8_t Subchannel, uint8_t Variant, uint16_t ChannelID){
	ChannelSend(Binary.GetAddress(), Binary.GetSize(), Subchannel, Variant, ChannelID);
}

void RedRelayClient::PeerSend(const void* Data, std::size_t Size, uint16_t PeerID, uint8_t Subchannel, uint8_t Variant, uint16_t ChannelID){
	if (ConnectState<RequestingUdp) return;
	if (ChannelID==65535) ChannelID=SelectedChannel;
	for (const Channel&i : Channels) if (i.ID==ChannelID) for (const Peer&j : i.Peers) if (j.ID==PeerID){
		packet.Clear();
		packet.SetType(3);
		packet.SetVariant(Variant);
		packet.AddByte(Subchannel);
		packet.AddShort(ChannelID);
		packet.AddShort(PeerID);
		packet.AddBinary(Data, Size);
		SendTcp(packet.GetPacket(), packet.GetPacketSize());
		return;
	}
}

void RedRelayClient::PeerSend(const Binary& Binary, uint16_t PeerID, uint8_t Subchannel, uint8_t Variant, uint16_t ChannelID){
	PeerSend(Binary.GetAddress(), Binary.GetSize(), PeerID, Subchannel, Variant, ChannelID);
}

void RedRelayClient::ChannelBlast(const void* Data, std::size_t Size, uint8_t Subchannel, uint8_t Variant, uint16_t ChannelID){
	if (ConnectState<RequestingUdp) return;
	if (ChannelID==65535) ChannelID=SelectedChannel;
	if (Size > 65530) Size = 65530;
	for (const Channel&i : Channels) if (i.ID==ChannelID){
		UdpBuffer[0]=(2<<4)|(Variant&15);
		UdpBuffer[1]=PeerID&255;
		UdpBuffer[2]=(PeerID>>8)&255;
		UdpBuffer[3]=Subchannel;
		UdpBuffer[4]=ChannelID&255;
		UdpBuffer[5]=(ChannelID>>8)&255;
		memcpy(&UdpBuffer[6], Data, Size);
		UdpSocket.send(UdpBuffer, 6+Size, TcpSocket.getRemoteAddress(), TcpSocket.getRemotePort());
		return;
	}
}

void RedRelayClient::ChannelBlast(const Binary& Binary, uint8_t Subchannel, uint8_t Variant, uint16_t ChannelID){
	ChannelBlast(Binary.GetAddress(), Binary.GetSize(), Subchannel, Variant, ChannelID);
}

void RedRelayClient::PeerBlast(const void* Data, std::size_t Size, uint16_t PeerID, uint8_t Subchannel, uint8_t Variant, uint16_t ChannelID){
	if (ConnectState<RequestingUdp) return;
	if (ChannelID==65535) ChannelID=SelectedChannel;
	if (Size > 65528) Size = 65528;
	for (const Channel&i : Channels) if (i.ID==ChannelID) for (const Peer&j : i.Peers) if (j.ID==PeerID){
		UdpBuffer[0]=(3<<4)|(Variant&15);
		UdpBuffer[1]=this->PeerID&255;
		UdpBuffer[2]=(this->PeerID>>8)&255;
		UdpBuffer[3]=Subchannel;
		UdpBuffer[4]=ChannelID&255;
		UdpBuffer[5]=(ChannelID>>8)&255;
		UdpBuffer[6]=PeerID&255;
		UdpBuffer[7]=(PeerID>>8)&255;
		memcpy(&UdpBuffer[8], Data, Size);
		UdpSocket.send(UdpBuffer, 8+Size, TcpSocket.getRemoteAddress(), TcpSocket.getRemotePort());
		return;
	}
}

void RedRelayClient::PeerBlast(const Binary& Binary, uint16_t PeerID, uint8_t Subchannel, uint8_t Variant, uint16_t ChannelID){
	PeerBlast(Binary.GetAddress(), Binary.GetSize(), PeerID, Subchannel, Variant, ChannelID);
}

const Channel RedRelayClient::defchannel(0, "", 0);

}
