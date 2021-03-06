#ifndef INCOMINGGAMEHOSTL_H
#define INCOMINGGAMEHOSTL_H

//
// CIncomingGameHost
//

class CIncomingGameHost
{
public:
	static uint32_t NextUniqueGameID;

private:
	uint16_t m_GameType;
	uint16_t m_Parameter;
	uint16_t m_Port;
	unsigned long m_IP;
	uint32_t m_Status;
	uint32_t m_ElapsedTime;
	std::string m_GameName;
	unsigned char m_OpenSlots;
	unsigned char m_SlotsTotal;
	uint32_t m_HostCounter;
	BYTEARRAY m_StatString;
	uint32_t m_UniqueGameID;
	uint32_t m_ReceivedTime;
	GAME_STATUS m_GameStatus;

	// decoded from stat string:

	uint32_t m_MapFlags;
	uint16_t m_MapWidth;
	uint16_t m_MapHeight;
	BYTEARRAY m_MapCRC;
	std::string m_MapPath;
	std::string m_HostName;

public:
	CIncomingGameHost(uint16_t nGameType, uint16_t nParameter, uint16_t nPort, unsigned long nIP, uint32_t nElapsedTime, std::string nGameName, unsigned char nOpenSlots, unsigned char nSlotsTotal, uint32_t nHostCounter, BYTEARRAY &nStatString);
	~CIncomingGameHost();

	uint16_t GetGameType() { return m_GameType; }
	uint16_t GetParameter() { return m_Parameter; }
	uint16_t GetPort() { return m_Port; }
	unsigned long GetIP() { return m_IP; }
	std::string GetIPString();
	uint32_t GetElapsedTime() { return m_ElapsedTime; }
	std::string GetGameName() { return m_GameName; }
	unsigned char GetOpenSlots() { return m_OpenSlots; }
	unsigned char GetSlotsTotal() { return m_SlotsTotal; }
	uint32_t GetHostCounter() { return m_HostCounter; }
	BYTEARRAY GetStatString() { return m_StatString; }
	uint32_t GetUniqueGameID() { return m_UniqueGameID; }
	uint32_t GetReceivedTime() { return m_ReceivedTime; }
	uint32_t GetMapFlags() { return m_MapFlags; }
	uint16_t GetMapWidth() { return m_MapWidth; }
	uint16_t GetMapHeight() { return m_MapHeight; }
	BYTEARRAY GetMapCRC() { return m_MapCRC; }
	std::string GetMapPath() { return m_MapPath; }
	std::string GetHostName() { return m_HostName; }
	GAME_STATUS GetGameStatus() { return m_GameStatus; }

	void SetOpenSlots(unsigned char count) { m_OpenSlots = count; }
	void SetGameStatus(GAME_STATUS status) { m_GameStatus = status; }
};

#endif