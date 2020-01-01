#include "gproxy.h"
#include "util.h"
#include "incominggamehost.h"

//
// CIncomingGameHost
//

uint32_t CIncomingGameHost :: NextUniqueGameID = 1;

CIncomingGameHost :: CIncomingGameHost( uint16_t nGameType, uint16_t nParameter, uint32_t nLanguageID, uint16_t nPort, BYTEARRAY &nIP, uint32_t nStatus, uint32_t nElapsedTime, string nGameName, unsigned char nSlotsTotal, uint32_t nHostCounter, BYTEARRAY &nStatString )
{
	m_GameType = nGameType;
	m_Parameter = nParameter;
	m_LanguageID = nLanguageID;
	m_Port = nPort;
	m_IP = nIP;
	m_Status = nStatus;
	m_ElapsedTime = nElapsedTime;
	m_GameName = nGameName;
	m_OpenSlots = 12;
	m_SlotsTotal = nSlotsTotal;
	m_HostCounter = nHostCounter;
	m_StatString = nStatString;
	m_UniqueGameID = NextUniqueGameID++;
	m_ReceivedTime = GetTime( );

	// decode stat string

	BYTEARRAY StatString = UTIL_DecodeStatString( m_StatString );
	BYTEARRAY MapFlags;
	BYTEARRAY MapWidth;
	BYTEARRAY MapHeight;
	BYTEARRAY MapCRC;
	BYTEARRAY MapPath;
	BYTEARRAY HostName;

	if( StatString.size( ) >= 14 )
	{
		unsigned int i = 13;
		MapFlags = BYTEARRAY( StatString.begin( ), StatString.begin( ) + 4 );
		MapWidth = BYTEARRAY( StatString.begin( ) + 5, StatString.begin( ) + 7 );
		MapHeight = BYTEARRAY( StatString.begin( ) + 7, StatString.begin( ) + 9 );
		MapCRC = BYTEARRAY( StatString.begin( ) + 9, StatString.begin( ) + 13 );
		MapPath = UTIL_ExtractCString( StatString, 13 );
		i += MapPath.size( ) + 1;

		m_MapFlags = UTIL_ByteArrayToUInt32( MapFlags, false );
		m_MapWidth = UTIL_ByteArrayToUInt16( MapWidth, false );
		m_MapHeight = UTIL_ByteArrayToUInt16( MapHeight, false );
		m_MapCRC = MapCRC;
		m_MapPath = string( MapPath.begin( ), MapPath.end( ) );

		if( StatString.size( ) >= i + 1 )
		{
			HostName = UTIL_ExtractCString( StatString, i );
			m_HostName = string( HostName.begin( ), HostName.end( ) );
		}
	}
}

CIncomingGameHost :: ~CIncomingGameHost( )
{

}

string CIncomingGameHost :: GetIPString( )
{
	string Result;

	if( m_IP.size( ) >= 4 )
	{
		for( unsigned int i = 0; i < 4; i++ )
		{
			Result += UTIL_ToString( (unsigned int)m_IP[i] );

			if( i < 3 )
				Result += ".";
		}
	}

	return Result;
} 
