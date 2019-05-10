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

namespace rs{

/////////////
// Channel //
/////////////

std::string Channel::GetName() const {
	return Name;
}

bool Channel::IsHidden() const {
	return HideFromList;
}

bool Channel::IsAutoClosed() const {
	return CloseOnLeave;
}

const std::vector<uint16_t>& Channel::GetPeerList() const {
	return Peers;
}

uint16_t Channel::GetPeersCount() const {
	return Peers.size();
}

uint16_t Channel::GetMasterID() const {
	return Master;
}

//////////
// Peer //
//////////

uint32_t Peer::MessageSize() const {
	if (packetsize>1 && (uint8_t)buffer[buffbegin+1]<254) return (uint8_t)buffer[buffbegin+1];
	if (packetsize>3 && (uint8_t)buffer[buffbegin+1]==254) return (uint8_t)buffer[buffbegin+2] | (uint8_t)buffer[buffbegin+3]<<8;
	if (packetsize>5) return (uint8_t)buffer[buffbegin+2] | (uint8_t)buffer[buffbegin+3]<<8 | (uint8_t)buffer[buffbegin+4]<<16 | (uint8_t)buffer[buffbegin+5]<<24;
	return 65535;
}
uint8_t Peer::SizeOffset() const {
	if (packetsize<2) return 0;
	if ((uint8_t)buffer[buffbegin+1]<254) return 1;
	if ((uint8_t)buffer[buffbegin+1]==254) return 3;
	return 5;
}
bool Peer::MessageReady() const {
	return MessageSize()+SizeOffset()<packetsize;
}

std::string Peer::GetName() const {
	return Name;
}

const std::vector<uint16_t>& Peer::GetJoinedChannels() const {
	return Channels;
}

sf::IpAddress Peer::GetIP() const {
	return Socket->getRemoteAddress();
}

sf::TcpSocket Peer::defsocket;

}