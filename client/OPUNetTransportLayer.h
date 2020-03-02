#pragma once

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

#include <OP2Internal.h>
using namespace OP2Internal;


// Max number of remote players
const int MaxRemotePlayers = 6;
const int JoinTimeOut = 3000;		// 3 seconds

// Default Ports
const int DefaultGameServerPort = 47800;
const int DefaultClientPort = 47800;


struct HostedGameInfo
{
	CreateGameInfo createGameInfo;
	unsigned int ping;
	GUID sessionIdentifier;
	sockaddr_in address;
};


struct PeerInfo
{
	int playerNetID;
	int status;
	sockaddr_in address;
	bool bReturnJoinPacket;
};


class OPUNetTransportLayer : public NetTransportLayer
{
public:
	// Public member functions
	// -----------------------
	static OPUNetTransportLayer* Create();		// Public static constructor [returns NULL on failure]

	// Following functions: Return true on success, false on failure
	bool CreateSocket();
	bool HostGame(USHORT port, const char* password, const char* creatorName, int maxPlayers, int gameType);
	bool SearchForGames(char* hostAddress, USHORT defaultHostPort);
	bool JoinGame(HostedGameInfo &game, const char* password);
	// Externally triggered events
	void OnJoinAccepted(Packet &packet);
	// Properties
	int GetNumPlayers();
	void GetGameServerAddressString(char* gameServerAddressString, int maxLength);
	int GetPort();
	bool GetAddress(sockaddr_in& addr);
	bool GetExternalAddress();

	// Virtual member functions
	// ------------------------
	virtual ~OPUNetTransportLayer() override;
	virtual int GetHostPlayerNetID() override;
	virtual void ShutDownInvite() override;
	virtual int ReplicatePlayersList() override;
	virtual int GetOpponentNetIDList(int netIDList[], int maxNumID) override;
	virtual void RemovePlayer(int playerNetID) override;
	virtual int Send(Packet& packet) override;
	virtual int Receive(Packet& packet) override;
	virtual int IsHost() override;
	virtual int IsValidPlayer() override;
	virtual int F1()  override;
	virtual int GetAddressString(int playerNetID, char* addressString, int bufferSize) override;
	virtual int ResetTrafficCounters() override;
	virtual int GetTrafficCounts(TrafficCounters& trafficCounters) override;

private:
	// Private member functions
	// ------------------------
	OPUNetTransportLayer();			// Private Constructor  [Prevent object creation]
	bool InitializeWinsock();
	int GetHostAddress(char* addrString, sockaddr_in &hostAddress);
	int AddPlayer(sockaddr_in& from);
	int ReadSocket(SOCKET sourceSocket, Packet& packet, sockaddr_in& from);
	bool SendTo(Packet& packet, sockaddr_in& to);
	bool SendStatusUpdate();
	bool SendUntilStatusUpdate(Packet& packet, int untilStatus, int maxTries, int repeatDelay);
	bool DoImmediateProcessing(Packet& packet, sockaddr_in& fromAddress);
	bool PokeGameServer(PokeStatusCode status);
	bool GetGameServerAddress(sockaddr_in &gameServerAddress);
	void CheckSourcePort(Packet& packet, sockaddr_in& from);

private:
	// Member variables
	// ----------------
	// Gameplay variables
	unsigned int numPlayers;
	bool bInitialized;
	SOCKET netSocket;
	SOCKET hostSocket;
	int forcedPort;
	// Peer Info
	PeerInfo peerInfo[MaxRemotePlayers];
	// Traffic counters
	TrafficCounters trafficCounters;
	// State variables
	bool bInvite;
	bool bGameStarted;
	// Hosted Game variables
	HostedGameInfo hostedGameInfo;
	char password[12];
	// Joining Game variables
	HostedGameInfo* joiningGameInfo;
	int numJoining;
	// Game server random security  (prevents spoofing attacks)
	int randValue;
};


const unsigned int RandValueXor = 0xC37A4219;
