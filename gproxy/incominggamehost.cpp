#include "gproxy.h"
#include "util.h"
#include "incominggamehost.h"
#include <winsock2.h>

//
// CIncomingGameHost
//

uint32_t CIncomingGameHost::NextUniqueGameID = 1;

CIncomingGameHost::CIncomingGameHost(uint16_t nGameType, uint16_t nParameter, uint16_t nPort, unsigned long nIP, uint32_t nElapsedTime, std::string nGameName, unsigned char nOpenSlots, unsigned char nSlotsTotal, uint32_t nHostCounter, BYTEARRAY &nStatString)
{
	m_GameType = nGameType;
	m_Parameter = nParameter;
	m_Port = nPort;
	m_IP = nIP;
	m_ElapsedTime = nElapsedTime;
	m_GameName = nGameName;
	m_OpenSlots = nOpenSlots;
	m_SlotsTotal = nSlotsTotal;
	m_HostCounter = nHostCounter;
	m_StatString = nStatString;
	m_UniqueGameID = NextUniqueGameID++;
	m_ReceivedTime = GetTime();
	m_GameStatus = GAME_STATUS::LOBBY;

	// decode stat string

	BYTEARRAY StatString = UTIL_DecodeStatString(m_StatString);
	BYTEARRAY MapFlags;
	BYTEARRAY MapWidth;
	BYTEARRAY MapHeight;
	BYTEARRAY MapCRC;
	BYTEARRAY MapPath;
	BYTEARRAY HostName;

	if (StatString.size() >= 14)
	{
		unsigned int i = 13;
		MapFlags = BYTEARRAY(StatString.begin(), StatString.begin() + 4);
		MapWidth = BYTEARRAY(StatString.begin() + 5, StatString.begin() + 7);
		MapHeight = BYTEARRAY(StatString.begin() + 7, StatString.begin() + 9);
		MapCRC = BYTEARRAY(StatString.begin() + 9, StatString.begin() + 13);
		MapPath = UTIL_ExtractCString(StatString, 13);
		i += MapPath.size() + 1;

		m_MapFlags = UTIL_ByteArrayToUInt32(MapFlags, false);
		m_MapWidth = UTIL_ByteArrayToUInt16(MapWidth, false);
		m_MapHeight = UTIL_ByteArrayToUInt16(MapHeight, false);
		m_MapCRC = MapCRC;
		m_MapPath = std::string(MapPath.begin(), MapPath.end());

		if (StatString.size() >= i + 1)
		{
			HostName = UTIL_ExtractCString(StatString, i);
			m_HostName = std::string(HostName.begin(), HostName.end());
		}
	}
}

CIncomingGameHost::~CIncomingGameHost()
{
}

std::string CIncomingGameHost::GetIPString()
{
	struct in_addr addr;
	addr.s_addr = m_IP;
	char *ipaddress = inet_ntoa(addr);
	unsigned char a, b, c, d;
	sscanf(ipaddress, "%hu.%hu.%hu.%hu", &a, &b, &c, &d);
	return ipaddress;
}
