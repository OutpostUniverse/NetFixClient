#pragma once

#include "PlayerNetID.h"
#include <OP2Internal.h>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <array>

// Type alias to handle different type names used by winsock and POSIX socket implementations
#ifdef WIN32
using Port = u_short;
#else
using Port = in_port_t;
#endif

using namespace OP2Internal;

const int HostPlayerIndex = 0;
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
	PeerStatus status;
	sockaddr_in address;
	bool bReturnJoinPacket;

	void Clear()
	{
		playerNetID = 0;
		status = PeerStatus::EmptySlot;
		address.sin_addr.s_addr = INADDR_ANY;
	}
};


class OPUNetTransportLayer : public NetTransportLayer
{
public:
	static OPUNetTransportLayer* Create();		// returns NULL on failure

	// Following functions: Return true on success, false on failure
	bool CreateSocket();
	bool HostGame(Port port, const char* hostPassword, const char* creatorName, int maxPlayers, int gameType);
	bool SearchForGames(char* hostAddressString, Port defaultHostPort);
	bool JoinGame(HostedGameInfo &game, const char* joinRequestPassword);
	// Externally triggered events
	void OnJoinAccepted(Packet &packet);
	// Properties
	int GetNumPlayers();
	void GetGameServerAddressString(char* gameServerAddressString, int maxLength);
	int GetPort();
	bool GetAddress(sockaddr_in& addr);
	bool GetExternalAddress();

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
	enum class HostAddressCode
	{
		Success = -1,
		InvalidAddress = 0,
		NoAddressSpecified = 1
	};

	OPUNetTransportLayer();			// Private Constructor  [Prevent object creation]
	bool InitializeWinsock();
	HostAddressCode GetHostAddress(char* addrString, sockaddr_in &hostAddress);
	int AddPlayer(const sockaddr_in& from);
	int ReadSocket(SOCKET sourceSocket, Packet& packet, sockaddr_in& from);
	bool SendTo(Packet& packet, const sockaddr_in& to);
	bool SendStatusUpdate();
	bool SendUntilStatusUpdate(Packet& packet, PeerStatus untilStatus, int maxTries, int repeatDelay);
	bool OnImmediatePacketProcess(Packet& packet, const sockaddr_in& fromAddress);
	void OnJoinRequest(Packet& packet, const sockaddr_in& fromAddress, TransportLayerMessage& tlMessage);
	void OnHostedGameSearchQuery(Packet& packet, const sockaddr_in& fromAddress, TransportLayerMessage& tlMessage);
	bool OnJoinHelpRequest(const Packet& packet, const sockaddr_in& fromAddress, TransportLayerMessage& tlMessage);
	bool OnSetPlayersList(Packet& packet, const TransportLayerMessage& tlMessage);
	void OnSetPlayersListFailed(Packet& packet);
	void OnUpdateStatus(const Packet& packet, const TransportLayerMessage& tlMessage);
	bool OnHostedGameSearchReply(Packet& packet, const sockaddr_in& fromAddress);
	bool PokeGameServer(PokeStatusCode status);
	bool GetGameServerAddress(sockaddr_in &gameServerAddress);
	void CheckSourcePort(Packet& packet, sockaddr_in& from);

	void SendBroadcast(Packet& packet, int packetSize);
	void SendSinglecast(Packet& packet, int packetSize);
	void ClearPlayers();

	// Gameplay variables
	unsigned int numPlayers;
	bool bInitialized;
	SOCKET netSocket;
	SOCKET hostSocket;
	int forcedPort;
	// Peer Info
	std::array<PeerInfo, MaxRemotePlayers> peerInfos;
	// Traffic counters
	TrafficCounters trafficCounters;
	// State variables
	bool bInvite;
	bool bGameStarted;
	// Hosted Game variables
	HostedGameInfo hostedGameInfo;
	char hostPassword[12];
	// Joining Game variables
	HostedGameInfo* joiningGameInfo;
	int numJoining;
	// Game server random security  (prevents spoofing attacks)
	int randValue;
};


const unsigned int RandValueXor = 0xC37A4219;
