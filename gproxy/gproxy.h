/*

   Copyright 2010 Trevor Hogan

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

#ifndef GPROXY_H
#define GPROXY_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include "config.h"

typedef std::vector<unsigned char> BYTEARRAY;

// time

uint32_t GetTime();  // seconds
uint32_t GetTicks(); // milliseconds

#define MILLISLEEP(x) Sleep(x)

// network

#undef FD_SETSIZE
#define FD_SETSIZE 512

// output

void LOG_Print(std::string message);
void CONSOLE_Print(std::string message, bool log = true);
void CONSOLE_PrintNoCRLF(std::string message, bool log = true);
void CONSOLE_ChangeChannel(std::string channel);
void CONSOLE_AddChannelUser(std::string name);
void CONSOLE_RemoveChannelUser(std::string name);
void CONSOLE_RemoveChannelUsers();
void CONSOLE_Draw();
void CONSOLE_Resize();

enum GAME_STATUS
{
    LOBBY,
    LOADING,
    LOADED,
    REHOSTING,
    OFFLINE
};

//
// CGProxy
//

class CTCPServer;
class CTCPSocket;
class CTCPClient;
class CUDPSocket;
class CIncomingGameHost;
class CGameProtocol;
class CGPSProtocol;
class CCommandPacket;

class CGProxy
{
public:
    std::string m_Version;
    CConfig *m_Config;
    CTCPServer *m_LocalServer;
    CTCPSocket *m_LocalSocket;
    CTCPClient *m_RemoteSocket;
    CUDPSocket *m_UDPSocket;
    std::vector<CTCPClient *> m_GameInfoSockets;
    std::vector<CIncomingGameHost *> m_Games;
    CGameProtocol *m_GameProtocol;
    CGPSProtocol *m_GPSProtocol;
    std::queue<CCommandPacket *> m_LocalPackets;
    std::queue<CCommandPacket *> m_RemotePackets;
    std::queue<CCommandPacket *> m_PacketBuffer;
    std::vector<unsigned char> m_Laggers;
    uint32_t m_TotalPacketsReceivedFromLocal;
    uint32_t m_TotalPacketsReceivedFromRemote;
    bool m_Exiting;
    uint32_t m_War3Version;
    uint16_t m_Port;
    uint32_t m_LastConnectionAttemptTime;
    uint32_t m_LastRefreshTime;
    std::string m_RemoteServerIP;
    uint16_t m_RemoteServerPort;
    bool m_GameIsReliable;
    bool m_GameStarted;
    bool m_LeaveGameSent;
    bool m_ActionReceived;
    bool m_Synchronized;
    uint16_t m_ReconnectPort;
    unsigned char m_PID;
    unsigned char m_ChatPID;
    uint32_t m_ReconnectKey;
    unsigned char m_NumEmptyActions;
    unsigned char m_NumEmptyActionsUsed;
    uint32_t m_LastAckTime;
    uint32_t m_LastActionTime;
    std::string m_JoinedName;
    std::string m_HostName;

    CGProxy(uint32_t nWar3Version, uint16_t nPort, CConfig *nConfig);
    ~CGProxy();

    // processing functions

    bool Update(long usecBlock);

    void ExtractLocalPackets();
    void ProcessLocalPackets();
    void ExtractRemotePackets();
    void ProcessRemotePackets();

    bool AddGame(CIncomingGameHost *game);
    void SendLocalChat(std::string message);
    void SendEmptyAction();
};

#endif
