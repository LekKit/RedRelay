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

Event::Event(uint8_t EventType, const std::string& Message, uint16_t Short1, uint16_t Short2, uint16_t Short3){
	Type = EventType;
	m_string = Message;
	m_short1 = Short1;
	m_short2 = Short2;
	m_short3 = Short3;
}

Event::Event(){
	Type = Error;
}

std::string Event::ErrorMessage() const {
	if (Type==Error) return m_string;
	return "";
}

std::string Event::DenyMessage() const {
	if (Type==ConnectDenied || Type==NameDenied || Type==ChannelDenied || Type==ChannelLeaveDenied) return m_string;
	return "";
}

std::string Event::WelcomeMessage() const {
	if (Type==Connected) return m_string;
	return "";
}

std::string Event::DisconnectAddress() const {
	if (Type==Disconnected) return m_string;
	return "";
}

uint16_t Event::ChannelsCount() const {
	if (Type==ListReceived) return m_short1;
	return 0;
}

std::string Event::ChannelName() const {
	if (Type==ChannelJoin || Type==ChannelLeave || Type==ListEntry) return m_string;
	return "";
}

uint16_t Event::ChannelID() const {
	return m_short2;
}

uint16_t Event::PeersCount() const {
	if (Type==ListEntry) return m_short3;
	return 0;
}

std::string Event::PeerName() const {
	if (Type==PeerJoined || Type==PeerLeft || Type==PeerChangedName) return m_string;
	return "";
}

uint16_t Event::PeerID() const {
	return m_short1;
}

const char* Event::Address() const {
	return m_string.c_str();
}

uint32_t Event::Size() const {
	return m_string.length();
}

uint8_t Event::Subchannel() const {
	if (Type>=ChannelBlast) return m_short3&255;
	return 0;
}

uint8_t Event::Variant() const {
	if (Type>=ChannelBlast) return (m_short3>>8)&255;
	return 0;
}

uint8_t Event::UByte(uint32_t Index) const {
	if (m_string.length()<Index+1) return 0;
	return (uint8_t)m_string[Index];
}

int8_t Event::Byte(uint32_t Index) const {
	if (m_string.length()<Index+1) return 0;
	return m_string[Index];
}

uint16_t Event::UShort(uint32_t Index) const {
	if (m_string.length()<Index+2) return 0;
	return (uint8_t)m_string[Index]|(uint8_t)m_string[Index+1]<<8;
}

int16_t Event::Short(uint32_t Index) const {
	if (m_string.length()<Index+2) return 0;
	return (uint8_t)m_string[Index]|(uint8_t)m_string[Index+1]<<8;
}

uint32_t Event::UInt(uint32_t Index) const {
	if (m_string.length()<Index+4) return 0;
	return (uint8_t)m_string[Index]|(uint8_t)m_string[Index+1]<<8|(uint8_t)m_string[Index+2]<<16|(uint8_t)m_string[Index+3]<<24;
}

int32_t Event::Int(uint32_t Index) const {
	if (m_string.length()<Index+4) return 0;
	return (uint8_t)m_string[Index]|(uint8_t)m_string[Index+1]<<8|(uint8_t)m_string[Index+2]<<16|(uint8_t)m_string[Index+3]<<24;
}

float Event::Float(uint32_t Index) const {
	if (m_string.length()<Index+4) return 0;
	float output;
	memcpy(&output, &m_string[Index], 4);
	return output;
}

std::string Event::String(uint32_t Index) const {
	if (m_string.length()<Index+1) return "";
	return std::string(&m_string[Index]);
}

std::string Event::String(uint32_t Index, uint32_t Size) const {
	if (m_string.length()<Index+Size) return "";
	return std::string(&m_string[Index], Size);
}

}
