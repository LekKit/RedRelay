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
#include <cstring>

namespace rs{

RelayPacket::RelayPacket(std::size_t Size){
	size=0;
	type=0;
	capacity=Size;
	buffer = new char[Size];
}

RelayPacket::~RelayPacket(){
	delete[] buffer;
}

void RelayPacket::Reallocate(std::size_t newcapacity){
	if (newcapacity<=size+6 || newcapacity==capacity) return;
	char* tmp = new char[newcapacity];
	memcpy(&tmp[6], &buffer[6], size);
	delete[] buffer;
	capacity = newcapacity;
	buffer = tmp;
}

void RelayPacket::SetType(const uint8_t Type){
	type=Type<<4;
}

void RelayPacket::AddByte(const uint8_t Byte){
	while (size+6+1>capacity) Reallocate(capacity*2);
	buffer[6+size++]=Byte;
}

void RelayPacket::AddShort(const uint16_t Short){
	while (size+6+2>capacity) Reallocate(capacity*2);
	buffer[6+size++]=Short&255;
	buffer[6+size++]=(Short>>8)&255;
}

void RelayPacket::AddInt(const uint32_t Int){
	while (size+6+4>capacity) Reallocate(capacity*2);
	buffer[6+size++]=Int&255;
	buffer[6+size++]=(Int>>8)&255;
	buffer[6+size++]=(Int>>16)&255;
	buffer[6+size++]=(Int>>24)&255;
}

void RelayPacket::AddString(const std::string& String){
	while (size+6+String.length()>capacity) Reallocate(capacity*2);
	memcpy(&buffer[6+size], String.c_str(), String.length());
	size+=String.length();
}

const char* RelayPacket::GetPacket(){
	if (size<254){
		buffer[4]=type;
		buffer[5]=size&255;
		return &buffer[4];
	} else if (size<65535){
		buffer[2]=type;
		buffer[3]=(uint8_t)254;
		buffer[4]=size&255;
		buffer[5]=(size>>8)&255;
		return &buffer[2];
	} else {
		buffer[0]=type;
		buffer[1]=(uint8_t)254;
		buffer[2]=size&255;
		buffer[3]=(size>>8)&255;
		buffer[4]=(size>>16)&255;
		buffer[5]=(size>>24)&255;
		return &buffer[0];
	}
}

std::size_t RelayPacket::GetPacketSize() const {
	if (size<254){
		return 2+size;
	} else if (size<65535){
		return 4+size;
	} else {
		return 6+size;
	}
}

std::size_t RelayPacket::GetSize() const {
	return size;
}

void RelayPacket::Clear(){
	type=0;
	size=0;
}

}