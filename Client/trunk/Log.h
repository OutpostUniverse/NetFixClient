#include <fstream.h>
#include "OPUNetTransportLayer.h"

class ofstream;

// Global Debug file
extern ofstream logFile;


void Log(char* string);
void DumpIP(unsigned long ip);
void DumpAddr(sockaddr_in &addr);
void DumpPlayerNetID(int playerNetID);
void DumpAddrList(PeerInfo* peerInfo);
void DumpGuid(GUID &guid);
void DumpPacket(Packet* packet);
