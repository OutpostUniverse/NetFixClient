#include "ErrorLog.h"
#include "GameServer.h"
#include <limits>

#ifndef WIN32
	#define closesocket close
	#define ioctlsocket ioctl
#endif

constexpr std::size_t InvalidGameInfoIndex = (std::numeric_limits<std::size_t>::max)();

const unsigned int UpdateTime = 60;			// Number of seconds before requesting update
const unsigned int RetryTime = 64;			// Number of seconds before requesting update retry
const unsigned int GiveUpTime = 68;			// Number of seconds before clearing dead entries
const unsigned int InitialReplyTime = 4;	// Number of seconds allowed for first update
const GUID gameIdentifier = { 0x5A55CF11, 0xB841, 0x11CE, {0x92, 0x10, 0x00, 0xAA, 0x00, 0x6C, 0x49, 0x72} };



GameServer::GameServer()
{
	// Initialize network socket descriptor
	hostSocket = INVALID_SOCKET;

	// Clear counters
	memset(&counters, 0, sizeof(counters));

	#ifdef WIN32
		bWinsockInitialized = false;
	#endif
}


GameServer::~GameServer()
{
	// Check if a socket exists
	if (hostSocket != INVALID_SOCKET) {
		closesocket(hostSocket);
	}

	#ifdef WIN32
		if (bWinsockInitialized) {
			WSACleanup();
		}
	#endif
}



int GameServer::StartServer(unsigned short port)
{
	int errorCode;

	#ifdef WIN32
		errorCode = InitWinsock();
		if (errorCode != 0) {
			return errorCode;
		}
	#endif

	// Create the host socket
	errorCode = AllocSocket(hostSocket, port);
	if (errorCode != NoError) {
		return errorCode;
	}

	// Create the secondary socket
	errorCode = AllocSocket(secondarySocket, port + 1);
	if (errorCode != NoError) {
		return errorCode;
	}

	gameInfos.clear();

	return NoError;
}

void GameServer::Pump()
{
	sockaddr_in from;
	Packet packet;
	int numBytes;

	// Process all received packets
	for (;;)
	{
		// Check for received packets
		numBytes = ReceiveFrom(packet, from);
		if (numBytes >= 0) {
			ProcessPacket(packet, from);
		}
		else
		{
			if (numBytes != PacketNone)
			{
				#ifdef DEBUG
					LogValue("ReceiveFrom returned error code: ", numBytes);
				#endif
			}
		}

		// Do periodic updates
		DoTimedUpdates();

		// Check if we are done processing packets
		if (numBytes == -1) {
			return;
		}
	}
}


void GameServer::WaitForEvent()
{
	fd_set readfds;
	timeval timeOut = { 1, 0 };

	// Clear the read 
	FD_ZERO(&readfds);
	FD_SET(hostSocket, &readfds);
	FD_SET(secondarySocket, &readfds);
	// Wait for packets or timeout
	select(1, &readfds, 0, 0, &timeOut);
}


int GameServer::AllocSocket(SOCKET& hostSocket, unsigned short port)
{
	// Create the host socket
	hostSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (hostSocket == INVALID_SOCKET) {
		return SocketCreateFailed;
	}

	// Bind the socket to the host port
	sockaddr_in hostAddress;
	// Initialize the struct
	memset(hostAddress.sin_zero, 0, sizeof(hostAddress.sin_zero));
	hostAddress.sin_family = AF_INET;
	hostAddress.sin_port = htons(port);
	hostAddress.sin_addr.s_addr = INADDR_ANY;
	// Bind the socket
	int errorCode = bind(hostSocket, (sockaddr*)&hostAddress, sizeof(hostAddress));

	if (errorCode == SOCKET_ERROR) {
		return SocketBindFailed;
	}

	// Set non-blocking mode
	unsigned long argp = true;
	errorCode = ioctlsocket(hostSocket, FIONBIO, &argp);

	if (errorCode != 0) {
		return SocketNonBlockingModeFailed;
	}

	return NoError;
}

void GameServer::ProcessPacket(Packet &packet, sockaddr_in &from)
{
	#ifdef DEBUG
		//LogMessage("ProcessPacket");
		//LogValue("CommandType: ", packet.tlMessage.tlHeader.commandType);
	#endif

	switch(packet.tlMessage.tlHeader.commandType)
	{
	case tlcJoinRequest:
		ProcessJoinRequest(packet, from);
		return;
	case tlcHostedGameSearchQuery:
		ProcessGameSearchQuery(packet, from);
		return;
	case tlcHostedGameSearchReply:
		ProcessGameSearchReply(packet, from);
		return;
	case tlcGameServerPoke:
		ProcessPoke(packet, from);
		return;
	case tlcRequestExternalAddress:
		ProcessRequestExternalAddress(packet, from);
		return;
	// Message types not relevant to the game server
	// Unused messages included to prevent GCC-8 case label warning
	case tlcJoinHelpRequest:
	case tlcEchoExternalAddress:
		return;
	}
}

void GameServer::ProcessJoinRequest(Packet& packet, const sockaddr_in& from)
{
	// Verify the packet size
	if (packet.header.sizeOfPayload != sizeof(JoinRequest)) {
		return;		// Discard (bad size)
	}

	LogEndpoint("Game Join Request from: ", from.sin_addr.s_addr, from.sin_port);

	packet.tlMessage.tlHeader.commandType = tlcJoinHelpRequest;
	packet.tlMessage.joinHelpRequest.clientAddr = from;
	packet.tlMessage.joinHelpRequest.clientAddr.sin_family = 2;		// ** AF_INET

	// Search for a corresponding Session Identifer
	for (std::size_t gameInfoIndex = 0; gameInfoIndex < gameInfos.size(); gameInfoIndex++)
	{
		// Check if the Session Identifier matches
		if (memcmp(&gameInfos[gameInfoIndex].sessionIdentifier, &packet.tlMessage.joinRequest.sessionIdentifier, sizeof(packet.tlMessage.joinRequest.sessionIdentifier)) == 0)
		{
			// Send the Join Help Request
			SendTo(packet, gameInfos[gameInfoIndex].addr);
		}
	}

}

void GameServer::ProcessGameSearchQuery(Packet& packet, sockaddr_in& from)
{
	// Verify packet size
	if (packet.header.sizeOfPayload != sizeof(HostedGameSearchQuery)) {
		return;		// Discard (bad size)
	}
	// Verify the game identifier
	if (packet.tlMessage.searchQuery.gameIdentifier != gameIdentifier) {
		return;		// Discard (wrong game)
	}

	LogEndpoint("Game Search Query from: ", from.sin_addr.s_addr, from.sin_port);

	// Send client a list of games
	// Setup common packet fields
	packet.header.sizeOfPayload = sizeof(HostedGameSearchReply);
	packet.tlMessage.tlHeader.commandType = tlcHostedGameSearchReply;
	// Search games list for suitable games
	for (std::size_t i = 0; i < gameInfos.size(); ++i)
	{
		// Make sure we have valid game data
		if ((gameInfos[i].flags & GameInfoReceived) != 0)
		{
			LogString("  GameCreator: ", gameInfos[i].createGameInfo.gameCreatorName);

			// Consruct a reply packet for this game
			packet.tlMessage.searchReply.sessionIdentifier = gameInfos[i].sessionIdentifier;
			packet.tlMessage.searchReply.createGameInfo = gameInfos[i].createGameInfo;
			packet.tlMessage.searchReply.hostAddress = gameInfos[i].addr;
			packet.tlMessage.searchReply.hostAddress.sin_family = 2;		// ** AF_INET
			// Send the reply
			SendTo(packet, from);
		}
	}
}

void GameServer::ProcessGameSearchReply(Packet& packet, sockaddr_in& from)
{
	// Verify packet size
	if (packet.header.sizeOfPayload != sizeof(HostedGameSearchReply)) {
		return;		// Discard (bad size)
	}

	// Make sure we queried this server
	std::size_t gameInfoIndex = FindGameInfoServer(from, packet.tlMessage.searchReply.timeStamp);
	if (gameInfoIndex == InvalidGameInfoIndex) {
		return;		// Discard (not requested or bad time stamp, possible spam or spoofing attack)
	}
	GameInfo* currentGameInfo = &gameInfos[gameInfoIndex];

	LogEndpoint("Received Host Info from: ", from.sin_addr.s_addr, from.sin_port);

	// Add the game to the list
	currentGameInfo->sessionIdentifier = packet.tlMessage.searchReply.sessionIdentifier;
	currentGameInfo->addr = from;
	currentGameInfo->flags |= GameInfoReceived;
	currentGameInfo->flags &= ~GameInfoExpected & ~GameInfoUpdateRetrySent;
	currentGameInfo->createGameInfo = packet.tlMessage.searchReply.createGameInfo;
	currentGameInfo->time = time(0);
}

void GameServer::ProcessPoke(Packet& packet, sockaddr_in& from)
{
	// Verify packet size
	if (packet.header.sizeOfPayload != sizeof(GameServerPoke)) {
		return;		// Discard (bad size)
	}

#ifdef DEBUG
	//LogValue("Poke: ", packet.tlMessage.gameServerPoke.statusCode);
#endif

	// Find the current GameInfo
	std::size_t gameInfoIndex = FindGameInfoClient(from, packet.tlMessage.gameServerPoke.randValue);

	// Check what kind of poke this is
	switch (packet.tlMessage.gameServerPoke.statusCode)
	{
	case pscGameHosted: {
		// Check if this game server is not already known
		if (gameInfoIndex == InvalidGameInfoIndex)
		{
			counters.numNewHost++;
			gameInfoIndex = GetNewGameInfo();	// Allocate a new record
		}
		// Make sure we have a record to use
		if (gameInfoIndex == InvalidGameInfoIndex)
		{
			counters.numFailedGameInfoAllocs++;
			return;			// Abort  (failed to allocate new record)
		}
		GameInfo* currentGameInfo = &gameInfos[gameInfoIndex];

		LogEndpoint("Game Hosted from: ", from.sin_addr.s_addr, from.sin_port);

		// Initialize the new record
		currentGameInfo->addr = from;
		currentGameInfo->clientRandValue = packet.tlMessage.gameServerPoke.randValue;
		currentGameInfo->serverRandValue = GetNewRandValue();
		currentGameInfo->flags |= GameInfoExpected;
		currentGameInfo->time = time(0);

		// Send a request for games
		SendGameInfoRequest(from, currentGameInfo->serverRandValue);

		// Update counters
		counters.numGamesHosted++;

		return;
	}
	case pscGameStarted: {
		LogEndpoint("Game Started: ", from.sin_addr.s_addr, from.sin_port);

		// Remove the game from the list
		FreeGameInfo(gameInfoIndex);
		counters.numGamesStarted++;

		return;
	}
	case pscGameCancelled: {
		LogEndpoint("Game Cancelled: ", from.sin_addr.s_addr, from.sin_port);

		// Remove the game from the list
		FreeGameInfo(gameInfoIndex);
		counters.numGamesCancelled++;

		return;
	}
	}
}

void GameServer::ProcessRequestExternalAddress(Packet& packet, sockaddr_in& from)
{
	// Verify packet size
	if (packet.header.sizeOfPayload != sizeof(RequestExternalAddress)) {
		return;		// Discard (bad size)
	}

	// Cache the internal port being used
	unsigned short internalPort = packet.tlMessage.requestExternalAddress.internalPort;

	// Construct a reply
	packet.header.sizeOfPayload = sizeof(EchoExternalAddress);
	packet.tlMessage.tlHeader.commandType = tlcEchoExternalAddress;
	packet.tlMessage.echoExternalAddress.addr = from;
	packet.tlMessage.echoExternalAddress.replyPort = from.sin_port;

	// Send first reply
	SendTo(packet, from);

	// Check if they have a different internal address
	if (from.sin_port != internalPort)
	{
		// Send second reply
		packet.tlMessage.echoExternalAddress.replyPort = internalPort;
		from.sin_port = htons(internalPort);
		SendTo(packet, from);
	}
}

void GameServer::DoTimedUpdates()
{
	#ifdef DEBUG
		//LogMessage("DoTimedUpdates()");
	#endif

	// Get the current time
	time_t currentTime = time(0);
	// Check for timed out game entries
	for (std::size_t i = gameInfos.size(); i-- > 0; )
	{
		// Get the current time difference
		time_t timeDiff = currentTime - gameInfos[i].time;

		// Check for no initial update within required time
		if ((timeDiff >= InitialReplyTime) && ((gameInfos[i].flags & GameInfoReceived) == 0))
		{
			LogEndpoint("Dropping Game: No initial Host Info from: ", gameInfos[i].addr.sin_addr.s_addr, gameInfos[i].addr.sin_port);

			// Clear bogus entry
			FreeGameInfo(i);
			// Update counters
			counters.numDroppedHostedPokes++;
		}
		// Check if no updates have occured for a while
		else if ((timeDiff >= UpdateTime) && ((gameInfos[i].flags & GameInfoReceived) != 0))
		{
			// Entry is old and requires update
			// --------------------------------

			// Check if enough time has elapsed to give up
			if (timeDiff >= GiveUpTime)
			{
				LogEndpoint("Dropping Game: Lost contact with host: ", gameInfos[i].addr.sin_addr.s_addr, gameInfos[i].addr.sin_port);

				// Give up
				FreeGameInfo(i);
				counters.numGamesDropped++;
			}
			else if ((gameInfos[i].flags & GameInfoExpected) == 0)
			{
				LogEndpoint("Requesting Game info update 1 (periodic): ", gameInfos[i].addr.sin_addr.s_addr, gameInfos[i].addr.sin_port);

				// Game info is stale, request update
				SendGameInfoRequest(gameInfos[i].addr, gameInfos[i].serverRandValue);
				gameInfos[i].flags |= GameInfoExpected;
				counters.numUpdateRequestSent++;
			}
			else if ((timeDiff >= RetryTime) && ((gameInfos[i].flags & GameInfoUpdateRetrySent) == 0))
			{
				LogEndpoint("Requesting Game info update 2 (retry): ", gameInfos[i].addr.sin_addr.s_addr, gameInfos[i].addr.sin_port);

				// Assume the packet was dropped. Retry.
				SendGameInfoRequest(gameInfos[i].addr, gameInfos[i].serverRandValue);
				gameInfos[i].flags |= GameInfoUpdateRetrySent;
				counters.numRetrySent++;
			}
		}
	}

	// Check if we should reduce memory usage **TODO**

	LogCounters(counters);
}

std::size_t GameServer::FindGameInfoClient(const sockaddr_in &from, unsigned int clientRandValue)
{
	// Search games list
	for (std::size_t i = 0; i < gameInfos.size(); ++i)
	{
		// Check if this address matches
		if ((gameInfos[i].clientRandValue == clientRandValue) && (memcmp(&gameInfos[i].addr, &from, sizeof(sockaddr_in)) == 0))
		{
			// Return the GameInfo
			return i;
		}
	}

	// GameInfo not found
	return InvalidGameInfoIndex;
}


std::size_t GameServer::FindGameInfoServer(const sockaddr_in &from, unsigned int serverRandValue)
{
	// Search games list
	for (std::size_t i = 0; i < gameInfos.size(); ++i)
	{
		// Check if this address matches
		if ((gameInfos[i].serverRandValue == serverRandValue) && (memcmp(&gameInfos[i].addr, &from, sizeof(sockaddr_in)) == 0))
		{
			// Return the GameInfo
			return i;
		}
	}

	// GameInfo not found
	return InvalidGameInfoIndex;
}

int GameServer::GetNewGameInfo()
{
	gameInfos.push_back(GameInfo());

	// Clear the flags of the new entry
	gameInfos[gameInfos.size() - 1].flags = 0;

	return gameInfos.size();
}


void GameServer::FreeGameInfo(std::size_t index)
{
	// Make sure it's a valid index
	if (index >= gameInfos.size())
	{
		LogMessage("Internal Error: Tried to free a non-existent GameInfo record");
		return;
	}

	gameInfos.erase(gameInfos.begin() + index);
}


unsigned int GameServer::GetNewRandValue()
{
	return 0;		// **TODO**
}


int GameServer::ReceiveFrom(Packet &packet, const sockaddr_in &from)
{
	// Read any received packets
	std::size_t numBytes;

	if (!ReadSocketData(numBytes, hostSocket, packet, from))
	{
		if (!ReadSocketData(numBytes, secondarySocket, packet, from))
		{
			// Check if not a would block error **TODO**
			return PacketNone;				// No Packet (would block)
		}
	}

	#ifdef DEBUG
		//LogMessage("Packet Received");
	#endif

	// Do simple error checks common to all packets
	// Check the packet min size
	if (numBytes < sizeof(packet.header) + sizeof(packet.tlMessage.tlHeader))
	{
		counters.numMinSizeErrors++;
		return PacketSizeBad;			// Invalid packet (bad size)
	}
	// Check the packet size field
	if ((packet.header.sizeOfPayload + sizeof(packet.header)) != numBytes)
	{
		counters.numSizeFieldErrors++;
		return PacketSizeFieldBad;		// Invalid packet (bad size field)
	}
	// Check the packet type
	if (packet.header.type != 1)
	{
		counters.numTypeFieldErrors++;
		return PacketTypeBad;			// Invalid packet (format)
	}
	// Check the packet checksum
	if (packet.header.checksum != packet.Checksum())
	{
		counters.numChecksumFieldErrors++;
		return PacketChecksumBad;		// Invalid packet (checksum)
	}

	#ifdef DEBUG
		//LogMessage("Received packet passes all common tests");
	#endif

	counters.numPacketsReceived++;
	counters.numBytesReceived += numBytes;

	return numBytes;
}

// Hide implementation details of recvfrom library function
// On failure, byteCountOut is set to 0. Returns false is SOCKET_ERROR encountered.
bool GameServer::ReadSocketData(std::size_t& byteCountOut, SOCKET& socket, Packet& packetBuffer, const sockaddr_in& from)
{
	socklen_t addrLen = sizeof(from);

	// recvfrom's return type is different on MSVC (int) and Linux (ssize_t) compilations
	auto byteCount = recvfrom(socket, (char*)&packetBuffer, sizeof(packetBuffer), 0, (sockaddr*)&from, &addrLen);

	if (byteCount == SOCKET_ERROR) {
		byteCountOut = 0u;
		return false;
	}

	byteCountOut = static_cast<std::size_t>(byteCount);
	return true;
}

void GameServer::SendTo(Packet &packet, const sockaddr_in &to)
{
	int errorCode;
	int size = packet.header.sizeOfPayload + sizeof(packet.header);
	int toLen = sizeof(to);

	// Calculate the packet checksum
	packet.header.checksum = packet.Checksum();

	// Send the packet
	errorCode = sendto(hostSocket, (char*)&packet, size, 0, (sockaddr*)&to, toLen);

	if (errorCode == SOCKET_ERROR)
	{
		// Error  **TODO** Update counters
		#ifdef DEBUG
			LogMessage("Error: SendTo socket error");
		#endif
	}
	else
	{
		counters.numPacketsSent++;
		counters.numBytesSent += size;
	}
}


void GameServer::SendGameInfoRequest(sockaddr_in &to, unsigned int serverRandValue)
{
	Packet packet;

	// Consntruct a Hosted Game Search Query packet
	packet.header.sourcePlayerNetID = 0;
	packet.header.destPlayerNetID = 0;
	packet.header.type = 1;
	packet.header.sizeOfPayload = sizeof(HostedGameSearchQuery);
	packet.tlMessage.tlHeader.commandType = tlcHostedGameSearchQuery;
	packet.tlMessage.searchQuery.gameIdentifier = gameIdentifier;
	packet.tlMessage.searchQuery.timeStamp = serverRandValue;
	memset(packet.tlMessage.searchQuery.password, 0, sizeof(packet.tlMessage.searchQuery.password));

	// Send the packet
	SendTo(packet, to);
}


#ifdef WIN32
	int GameServer::InitWinsock()
	{
		// Make sure Winsock was not already initialized
		if (bWinsockInitialized != false)
		{
			return WinsockInitFailed;			// Error: Already initialized
		}

		// Initialize Winsock
		WSADATA wsaData;
		int errorCode;
		WORD version = MAKEWORD(2, 2);

		// Initialize Winsock
		errorCode = WSAStartup(version, &wsaData);

		if (errorCode == 0)
		{
			// Check if we got the right version
			if (wsaData.wVersion != version)
			{
				// Wrong version
				WSACleanup();
				return WinsockVersionFailed;	// Error: Wrong Winsock version
			}
		}

		// Winsock Initialized successfully
		bWinsockInitialized = true;
		return NoError;
	}
#endif
