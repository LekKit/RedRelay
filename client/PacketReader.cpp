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

uint8_t PacketReader::SizeOffset() const {
	if (received<2) return 0;
	if ((unsigned char)buffer[packetbegin+1]<254) return 1;
	if ((unsigned char)buffer[packetbegin+1]==254) return 3;
	return 5;
}

PacketReader::PacketReader(std::size_t size){
	capacity=size;
	buffer=new char[size];
	received=0;
	packetbegin=0;
}

PacketReader::~PacketReader(){
	delete[] buffer;
}

void PacketReader::Reallocate(std::size_t newcapacity){
	if (newcapacity<=received || newcapacity==capacity) return;
	char* tmp = new char[newcapacity];
	memcpy(tmp, &buffer[packetbegin], received);
	delete[] buffer;
	capacity = newcapacity;
	packetbegin = 0;
	buffer = tmp;
}

void PacketReader::CheckBounds(){
	while (!GetReceiveSize()) Reallocate(capacity*2);
}

void* PacketReader::GetReceiveAddr(){
	return &buffer[packetbegin+received];
}

std::size_t PacketReader::GetReceiveSize() const {
	return capacity-(received+packetbegin);
}

void PacketReader::Received(uint32_t size){
	received+=size;
}

bool PacketReader::PacketReady(){
	return PacketSize()+SizeOffset()<received;
}

char* PacketReader::GetPacket() const {
	return &buffer[packetbegin+SizeOffset()+1];
}

std::size_t PacketReader::PacketSize() const {
	if (received>1 && (unsigned char)buffer[packetbegin+1]<254) return (unsigned char)buffer[packetbegin+1];
	if (received>3 && (unsigned char)buffer[packetbegin+1]==254) return (unsigned char)buffer[packetbegin+2]|(unsigned char)buffer[packetbegin+3]<<8;
	if (received>5) return (unsigned char)buffer[packetbegin+2]|(unsigned char)buffer[packetbegin+3]<<8|(unsigned char)buffer[packetbegin+4]<<16|(unsigned char)buffer[packetbegin+5]<<24;
	return 65535;
}

uint8_t PacketReader::GetPacketType() const {
        return buffer[packetbegin];
}

void PacketReader::NextPacket(){
	received-=PacketSize()+SizeOffset()+1;
	packetbegin+=PacketSize()+SizeOffset()+1;
	if (packetbegin>0 && !PacketReady()){
		memmove(buffer, &buffer[packetbegin], received);
		packetbegin=0;
	}
}

void PacketReader::Clear(){
	received=0;
	packetbegin=0;
}

}
