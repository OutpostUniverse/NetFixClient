#include "Log.h"
#include "OPUNetTransportLayer.h"
#include "FileSystemHelper.h"
namespace op2ext {
#include "op2ext.h"
}
#include <winsock2.h>
#include <objbase.h>
#include <iostream>
#include <sstream>


std::string FormatAddress(const sockaddr_in& address)
{
	std::stringstream ss;

	ss << "(AF:" << address.sin_family << ") ";
	ss << FormatIP4Address(address.sin_addr.s_addr);
	ss << ":" << ntohs(address.sin_port);

	return ss.str();
}

std::string FormatIP4Address(unsigned long ip)
{
	std::stringstream ss;

	ss << (ip & 255) << "." 
		<< ((ip >> 8) & 255) << "." 
		<< ((ip >> 16) & 255) << "." 
		<< ((ip >> 24) & 255);

	return ss.str();
}

std::string FormatPlayerList(const PeerInfo* peerInfo)
{
	std::stringstream ss;

	for (int i = 0; i < MaxRemotePlayers; ++i)
	{
		ss << " " << i << ") {" << peerInfo[i].status << ", ";
		ss << FormatAddress(peerInfo[i].address);
		ss << ", ";
		ss << FormatPlayerNetID(peerInfo[i].playerNetID);
		ss << "}";
	}

	return ss.str();
}

std::string FormatPlayerNetID(int playerNetID)
{
	std::stringstream ss;

	ss << "[" << (playerNetID & ~7) << "." << (playerNetID & 7) << "]";

	return ss.str();
}

std::string FormatGuid(const GUID& guid)
{
	std::stringstream ss;

	ss << std::hex << "{" << guid.Data1 << "-" << guid.Data2 << "-" << guid.Data3 << "-";
	for (int i = 0; i < 8; ++i)	{
		ss << (int)guid.Data4[i];
	}
	ss << "}" << std::dec;

	return ss.str();
}

std::string FormatPacket(const OP2Internal::Packet& packet)
{
	std::stringstream ss;

	ss << " Source: " << packet.header.sourcePlayerNetID << std::endl;
	ss << " Dest  : " << packet.header.destPlayerNetID << std::endl;
	ss << " Size  : " << static_cast<unsigned int>(packet.header.sizeOfPayload) << std::endl;
	ss << " type  : " << static_cast<unsigned int>(packet.header.type) << std::endl;
	ss << " checksum : " << std::hex << packet.Checksum() << std::dec << std::endl;
	ss << " commandType : " << packet.tlMessage.tlHeader.commandType; //Final endl adding by Log function

	return ss.str();
}


void Log(const std::string& message)
{
	op2ext::Log(message.c_str());
}
