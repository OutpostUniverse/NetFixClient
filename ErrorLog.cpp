#include "ErrorLog.h"


std::ostream &logFile = std::cerr;


void LogMessage(const char* message)
{
	logFile << message << std::endl;
}

void LogString(const char* message, const char* string)
{
	logFile << message << string << std::endl;
}

void LogValue(const char* message, int value)
{
	logFile << message << std::dec << value << std::endl;
}

void LogValueHex(const char* message, int value)
{
	logFile << message << std::hex << value << std::endl;
}

void LogEndpoint(const char* message, unsigned long ipAddr, unsigned short port)
{
	union
	{
		unsigned long ipAddr;
		struct
		{
			unsigned char b1;
			unsigned char b2;
			unsigned char b3;
			unsigned char b4;
		};
	} ip;

	ip.ipAddr = ipAddr;

	logFile << message << (unsigned int)ip.b1 << "." << (unsigned int)ip.b2 << "." << (unsigned int)ip.b3 << "." << (unsigned int)ip.b4 
			<< ":" << port << std::endl;
}

void LogCounters(GameServerCounters &counters)
{
	static GameServerCounters oldCounters = { 0 };

	// Check if updates need to be printed
	if (memcmp(&counters, &oldCounters, sizeof(counters)) == 0) {
		return;
	}

	// Store the current counters
	oldCounters = counters;

	// Output the counters
	// Game counters
	logFile << " Games: (Hosted: " << counters.numGamesHosted
			<< ", Started: " << counters.numGamesStarted
			<< ", Cancelled: " << counters.numGamesCancelled
			<< ", Dropped: " << counters.numGamesDropped << ")" << std::endl;
	// Traffic counters
	logFile << " Traffic: (PacketRecv: " << counters.numPacketsReceived
			<< ", ByteRecv: " << counters.numBytesReceived
			<< ", PacketSent: " << counters.numPacketsSent
			<< ", ByteSent: " << counters.numBytesSent << ")" << std::endl;
	// Performance counters
	logFile << " Performance: (DropHostPoke:" << counters.numDroppedHostedPokes
			<< ", UpdateRequestSent:" << counters.numUpdateRequestSent
			<< ", RetrySent:" << counters.numRetrySent << ")" << std::endl;
}


/*
void LogCounters(GameServerCounters &counters)
{
	static GameServerCounters oldCounters = { 0 };

	// Check if updates need to be printed
	if (memcmp(&counters, &oldCounters, sizeof(counters)) == 0) {
		return;
	}

	// Store the current counters
	oldCounters = counters;

	// Output the counters
	// Game counters
	logFile << std::endl;
	logFile << "counters.numGamesHosted   : " << counters.numGamesHosted << std::endl;
	logFile << "counters.numGamesStarted  : " << counters.numGamesStarted << std::endl;
	logFile << "counters.numGamesCancelled: " << counters.numGamesCancelled << std::endl;
	logFile << "counters.numGamesDropped  : " << counters.numGamesDropped << std::endl;
	// Traffic counters
	logFile << "counters.numPacketsReceived: " << counters.numPacketsReceived << std::endl;
	logFile << "counters.numBytesReceived  : " << counters.numBytesReceived << std::endl;
	logFile << "counters.numPacketsSent    : " << counters.numPacketsSent << std::endl;
	logFile << "counters.numBytesSent      : " << counters.numBytesSent << std::endl;
	// Performance counters
	logFile << "counters.numDroppedHostedPokes :" << counters.numDroppedHostedPokes << std::endl;
	logFile << "counters.numUpdateRequestSent  :" << counters.numUpdateRequestSent << std::endl;
	logFile << "counters.numRetrySent          :" << counters.numRetrySent << std::endl;
	logFile << "counters.numNewHost            :" << counters.numNewHost << std::endl;
	// Receive Error counts
	logFile << "counters.numMinSizeErrors      :" << counters.numMinSizeErrors << std::endl;
	logFile << "counters.numSizeFieldErrors    :" << counters.numSizeFieldErrors << std::endl;
	logFile << "counters.numTypeFieldErrors    :" << counters.numTypeFieldErrors << std::endl;
	logFile << "counters.numChecksumFieldErrors:" << counters.numChecksumFieldErrors << std::endl;
	// Error counts
	logFile << "counters.numFailedGameInfoAllocs:" << counters.numFailedGameInfoAllocs << std::endl;
	logFile << std::endl;
}
*/
