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

#include "RedRelayClient.hpp"

namespace rc{

Peer::Peer(uint16_t PeerID, const std::string& PeerName){
	ID=PeerID;
	Name=PeerName;
}

uint16_t Peer::GetID() const {
	return ID;
}

std::string Peer::GetName() const {
	return Name;
}

Channel::Channel(uint16_t ChannelID, const std::string& ChannelName, uint8_t ChannelFlags){
	ID=ChannelID;
	Name=ChannelName;
	Flags=ChannelFlags;
}

uint16_t Channel::GetID() const {
	return ID;
}

std::string Channel::GetName() const {
	return Name;
}

std::size_t Channel::GetPeerCount() const {
	return Peers.size();
}

const std::vector<Peer>& Channel::GetPeerList() const {
	return Peers;
}

const Peer& Channel::GetPeer(uint16_t ID) const {
	for (const Peer&i : Peers) if (i.ID==ID) return i;
	return defpeer;
}

const Peer& Channel::GetPeer(const std::string& Name) const {
	for (const Peer&i : Peers) if (i.Name==Name) return i;
	return defpeer;
}

uint16_t Channel::GetMasterID() const {
	return Master;
}

uint8_t Channel::GetFlags() const {
	return Flags;
}

bool Channel::IsHidden() const {
	return (Flags&1)!=0;
}

bool Channel::IsAutoClosed() const {
	return (Flags&2)!=0;
}

const Peer Channel::defpeer(0, "");

}
