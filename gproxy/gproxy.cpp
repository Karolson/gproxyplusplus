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

// todotodo: GHost++ may drop the player even after they reconnect if they run out of time and haven't caught up yet

#define UNICODE
#define _UNICODE

#include "gproxy.h"
#include "util.h"
#include "config.h"
#include "socket.h"
#include "commandpacket.h"
#include "gameprotocol.h"
#include "gpsprotocol.h"
#include "incominggamehost.h"
#include <cstring>
#include <tchar.h>

#include <signal.h>
#include <stdlib.h>

#include <windows.h>

#include <time.h>

CGProxy *gGProxy = NULL;
std::string gLogFile = "gproxy.log";
std::string dotsURL = "thdots.ru";
const uint16_t gameInfoPorts[] = {6151, 6152, 6153};
unsigned long ip;

uint32_t GetTime()
{
    return GetTicks() / 1000;
}

uint32_t GetTicks()
{
    return timeGetTime();
    uint32_t ticks;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    ticks = t.tv_sec * 1000;
    ticks += t.tv_nsec / 1000000;
    return ticks;
}

void LOG_Print(std::string message)
{
    if (!gLogFile.empty())
    {
        std::ofstream Log;
        Log.open(gLogFile.c_str(), std::ios::app);

        if (!Log.fail())
        {
            time_t Now = time(NULL);
            std::string Time = asctime(localtime(&Now));

            // erase the newline

            Time.erase(Time.size() - 1);
            Log << "[" << Time << "] " << message << std::endl;
            Log.close();
        }
    }
}

void CONSOLE_Print(std::string message, bool log)
{
    if (log)
        LOG_Print(message);

    std::cout << message << std::endl;
}

void shutdown()
{
    CONSOLE_Print("[GPROXY] shutting down");
    delete gGProxy;
    gGProxy = NULL;

    // shutdown winsock
    CONSOLE_Print("[GPROXY] shutting down winsock");
    WSACleanup();
}

void UpdateRegistry()
{
    HKEY hKey;
    DWORD value = 1;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Blizzard Entertainment\\Warcraft III", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS)
    {
        CONSOLE_Print("Failed to open Warcraft III registry key", true);
    }
    else
    {
        if (RegSetValueExA(hKey, "Allow Local Files", 0, REG_DWORD, (const BYTE *)&value, sizeof(value)) != ERROR_SUCCESS)
        {
            CONSOLE_Print("Failed to set Allow Local Files registry key", true);
        }
    }
    RegCloseKey(hKey);
}

//
// main
//

HANDLE ThreadHandle;

DWORD WINAPI ThreadMain(LPVOID lpParam)
{
    std::ofstream Log;
    Log.open(gLogFile.c_str(), std::ios::trunc);
    Log.close();

    CONSOLE_Print("[GPROXY] starting up");

    // initialize winsock

    CONSOLE_Print("[GPROXY] starting winsock");
    WSADATA wsadata;

    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
    {
        CONSOLE_Print("[GPROXY] error starting winsock");
        return 1;
    }

    // increase process priority

    CONSOLE_Print("[GPROXY] setting process priority to \"above normal\"");
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

    CONSOLE_Print("", false);
    CONSOLE_Print("  Welcome to DodsGProxy++.", false);
    CONSOLE_Print("", false);

    // initialize gproxy

    CConfig *Config = new CConfig();
    Config->Read("dods.cfg");

    if (Config->Exists("url"))
    {
        dotsURL = Config->GetString("url", dotsURL);
    }
    CONSOLE_Print("Using URL: " + dotsURL, true);

    uint32_t War3Version = 27;
    uint16_t Port = 6125;
    gGProxy = new CGProxy(War3Version, Port, Config);

    struct in_addr addr;
    struct hostent *remoteHost = gethostbyname(dotsURL.c_str());
    if (remoteHost == NULL)
    {
        CONSOLE_Print("Can't connect to " + dotsURL, true);
        return 1;
    }
    ip = *(u_long *)remoteHost->h_addr_list[0];

    while (1)
    {
        if (gGProxy->Update(40000))
            break;
    }

    // shutdown gproxy
    shutdown();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    bool bSuccess = TRUE;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        //UpdateRegistry();
        DWORD ThreadID;
        ThreadHandle = CreateThread(NULL, 0, ThreadMain, NULL, 0, &ThreadID);
        break;
    }
    case DLL_PROCESS_DETACH:
    {
        TerminateThread(ThreadHandle, 0);
        shutdown();
        break;
    }
    }

    return bSuccess;
}

//
// CGProxy
//

CGProxy::CGProxy(uint32_t nWar3Version, uint16_t nPort, CConfig *nConfig)
{
    m_Version = "Public Test Release 1.0 (March 11, 2010) THDOTS EDITION";
    m_Config = nConfig;
    m_LocalServer = new CTCPServer();
    m_LocalSocket = NULL;
    m_RemoteSocket = new CTCPClient();
    m_RemoteSocket->SetNoDelay(true);
    m_UDPSocket = new CUDPSocket();
    m_UDPSocket->SetBroadcastTarget("127.0.0.1");
    m_GameProtocol = new CGameProtocol(this);
    m_GPSProtocol = new CGPSProtocol();
    m_TotalPacketsReceivedFromLocal = 0;
    m_TotalPacketsReceivedFromRemote = 0;
    m_Exiting = false;
    m_War3Version = nWar3Version;
    m_Port = nPort;
    m_LastConnectionAttemptTime = 0;
    m_LastRefreshTime = 0;
    m_RemoteServerPort = 0;
    m_GameIsReliable = false;
    m_GameStarted = false;
    m_LeaveGameSent = false;
    m_ActionReceived = false;
    m_Synchronized = true;
    m_ReconnectPort = 0;
    m_PID = 255;
    m_ChatPID = 255;
    m_ReconnectKey = 0;
    m_NumEmptyActions = 0;
    m_NumEmptyActionsUsed = 0;
    m_LastAckTime = 0;
    m_LastActionTime = 0;
    m_LocalServer->Listen(std::string(), m_Port);
    CONSOLE_Print("[GPROXY] GProxy++ Version " + m_Version);
}

CGProxy::~CGProxy()
{
    for (std::vector<CIncomingGameHost *>::iterator i = m_Games.begin(); i != m_Games.end(); i++)
        m_UDPSocket->Broadcast(6112, m_GameProtocol->SEND_W3GS_DECREATEGAME((*i)->GetUniqueGameID()));

    delete m_Config;
    delete m_LocalServer;
    delete m_LocalSocket;
    delete m_RemoteSocket;
    delete m_UDPSocket;

    for (std::vector<CTCPClient *>::iterator i = m_GameInfoSockets.begin(); i != m_GameInfoSockets.end(); i++)
        delete *i;

    for (std::vector<CIncomingGameHost *>::iterator i = m_Games.begin(); i != m_Games.end(); i++)
        delete *i;

    delete m_GameProtocol;
    delete m_GPSProtocol;

    while (!m_LocalPackets.empty())
    {
        delete m_LocalPackets.front();
        m_LocalPackets.pop();
    }

    while (!m_RemotePackets.empty())
    {
        delete m_RemotePackets.front();
        m_RemotePackets.pop();
    }

    while (!m_PacketBuffer.empty())
    {
        delete m_PacketBuffer.front();
        m_PacketBuffer.pop();
    }
}

bool CGProxy::Update(long usecBlock)
{
    unsigned int NumFDs = 0;

    // take every socket we own and throw it in one giant select statement so we can block on all sockets

    int nfds = 0;
    fd_set fd;
    fd_set send_fd;
    FD_ZERO(&fd);
    FD_ZERO(&send_fd);

    // 1. the local server

    m_LocalServer->SetFD(&fd, &send_fd, &nfds);
    NumFDs++;

    // 2. the local socket

    if (m_LocalSocket)
    {
        m_LocalSocket->SetFD(&fd, &send_fd, &nfds);
        NumFDs++;
    }

    // 3. the remote socket

    if (!m_RemoteSocket->HasError() && m_RemoteSocket->GetConnected())
    {
        m_RemoteSocket->SetFD(&fd, &send_fd, &nfds);
        NumFDs++;
    }

    // 4. game info sockets

    for (CTCPClient *gameInfoSocket : m_GameInfoSockets)
    {
        gameInfoSocket->SetFD(&fd, &send_fd, &nfds);
        NumFDs++;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = usecBlock;

    struct timeval send_tv;
    send_tv.tv_sec = 0;
    send_tv.tv_usec = 0;

    select(1, &fd, NULL, NULL, &tv);
    select(1, NULL, &send_fd, NULL, &send_tv);

    if (NumFDs == 0)
        MILLISLEEP(50);

    //
    // accept new connections
    //

    CTCPSocket *NewSocket = m_LocalServer->Accept(&fd);

    if (NewSocket)
    {
        if (m_LocalSocket)
        {
            // someone's already connected, reject the new connection
            // we only allow one person to use the proxy at a time

            delete NewSocket;
        }
        else
        {
            CONSOLE_Print("[GPROXY] local player connected");
            m_LocalSocket = NewSocket;
            m_LocalSocket->SetNoDelay(true);
            m_TotalPacketsReceivedFromLocal = 0;
            m_TotalPacketsReceivedFromRemote = 0;
            m_GameIsReliable = false;
            m_GameStarted = false;
            m_LeaveGameSent = false;
            m_ActionReceived = false;
            m_Synchronized = true;
            m_ReconnectPort = 0;
            m_PID = 255;
            m_ChatPID = 255;
            m_ReconnectKey = 0;
            m_NumEmptyActions = 0;
            m_NumEmptyActionsUsed = 0;
            m_LastAckTime = 0;
            m_LastActionTime = 0;
            m_JoinedName.clear();
            m_HostName.clear();

            while (!m_PacketBuffer.empty())
            {
                delete m_PacketBuffer.front();
                m_PacketBuffer.pop();
            }

            for (std::vector<CTCPClient *>::iterator i = m_GameInfoSockets.begin(); i != m_GameInfoSockets.end(); i++)
                (*i)->Disconnect();
        }
    }

    if (m_LocalSocket)
    {
        //
        // handle proxying (reconnecting, etc...)
        //

        if (m_LocalSocket->HasError() || !m_LocalSocket->GetConnected())
        {
            CONSOLE_Print("[GPROXY] local player disconnected");

            delete m_LocalSocket;
            m_LocalSocket = NULL;

            // ensure a leavegame message was sent, otherwise the server may wait for our reconnection which will never happen
            // if one hasn't been sent it's because Warcraft III exited abnormally

            if (m_GameIsReliable && !m_LeaveGameSent)
            {
                // note: we're not actually 100% ensuring the leavegame message is sent, we'd need to check that DoSend worked, etc...

                BYTEARRAY LeaveGame;
                LeaveGame.push_back(0xF7);
                LeaveGame.push_back(0x21);
                LeaveGame.push_back(0x08);
                LeaveGame.push_back(0x00);
                UTIL_AppendByteArray(LeaveGame, (uint32_t)PLAYERLEAVE_GPROXY, false);
                m_RemoteSocket->PutBytes(LeaveGame);
                m_RemoteSocket->DoSend(&send_fd);
            }

            m_RemoteSocket->Reset();
            m_RemoteSocket->SetNoDelay(true);
            m_RemoteServerIP.clear();
            m_RemoteServerPort = 0;
        }
        else
        {
            m_LocalSocket->DoRecv(&fd);
            ExtractLocalPackets();
            ProcessLocalPackets();

            if (!m_RemoteServerIP.empty())
            {
                if (m_GameIsReliable && m_ActionReceived && GetTime() - m_LastActionTime >= 60)
                {
                    if (m_NumEmptyActionsUsed < m_NumEmptyActions)
                    {
                        SendEmptyAction();
                        m_NumEmptyActionsUsed++;
                    }
                    else
                    {
                        SendLocalChat("GProxy++ ran out of time to reconnect, Warcraft III will disconnect soon.");
                        CONSOLE_Print("[GPROXY] ran out of time to reconnect");
                    }

                    m_LastActionTime = GetTime();
                }

                if (m_RemoteSocket->HasError())
                {
                    CONSOLE_Print("[GPROXY] disconnected from remote server due to socket error");

                    if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0)
                    {
                        SendLocalChat("You have been disconnected from the server due to a socket error.");
                        uint32_t TimeRemaining = (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60 - (GetTime() - m_LastActionTime);

                        if (GetTime() - m_LastActionTime > (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60)
                            TimeRemaining = 0;

                        SendLocalChat("GProxy++ is attempting to reconnect... (" + UTIL_ToString(TimeRemaining) + " seconds remain)");
                        CONSOLE_Print("[GPROXY] attempting to reconnect");
                        m_RemoteSocket->Reset();
                        m_RemoteSocket->SetNoDelay(true);
                        m_RemoteSocket->Connect(std::string(), m_RemoteServerIP, m_ReconnectPort);
                        m_LastConnectionAttemptTime = GetTime();
                    }
                    else
                    {
                        m_LocalSocket->Disconnect();
                        delete m_LocalSocket;
                        m_LocalSocket = NULL;
                        m_RemoteSocket->Reset();
                        m_RemoteSocket->SetNoDelay(true);
                        m_RemoteServerIP.clear();
                        m_RemoteServerPort = 0;
                        return false;
                    }
                }

                if (!m_RemoteSocket->GetConnecting() && !m_RemoteSocket->GetConnected())
                {
                    CONSOLE_Print("[GPROXY] disconnected from remote server");

                    if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0)
                    {
                        SendLocalChat("You have been disconnected from the server.");
                        uint32_t TimeRemaining = (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60 - (GetTime() - m_LastActionTime);

                        if (GetTime() - m_LastActionTime > (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60)
                            TimeRemaining = 0;

                        SendLocalChat("GProxy++ is attempting to reconnect... (" + UTIL_ToString(TimeRemaining) + " seconds remain)");
                        CONSOLE_Print("[GPROXY] attempting to reconnect");
                        m_RemoteSocket->Reset();
                        m_RemoteSocket->SetNoDelay(true);
                        m_RemoteSocket->Connect(std::string(), m_RemoteServerIP, m_ReconnectPort);
                        m_LastConnectionAttemptTime = GetTime();
                    }
                    else
                    {
                        m_LocalSocket->Disconnect();
                        delete m_LocalSocket;
                        m_LocalSocket = NULL;
                        m_RemoteSocket->Reset();
                        m_RemoteSocket->SetNoDelay(true);
                        m_RemoteServerIP.clear();
                        m_RemoteServerPort = 0;
                        return false;
                    }
                }

                if (m_RemoteSocket->GetConnected())
                {
                    if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0 && GetTime() - m_RemoteSocket->GetLastRecv() >= 20)
                    {
                        CONSOLE_Print("[GPROXY] disconnected from remote server due to 20 second timeout");
                        SendLocalChat("You have been timed out from the server.");
                        uint32_t TimeRemaining = (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60 - (GetTime() - m_LastActionTime);

                        if (GetTime() - m_LastActionTime > (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60)
                            TimeRemaining = 0;

                        SendLocalChat("GProxy++ is attempting to reconnect... (" + UTIL_ToString(TimeRemaining) + " seconds remain)");
                        CONSOLE_Print("[GPROXY] attempting to reconnect");
                        m_RemoteSocket->Reset();
                        m_RemoteSocket->SetNoDelay(true);
                        m_RemoteSocket->Connect(std::string(), m_RemoteServerIP, m_ReconnectPort);
                        m_LastConnectionAttemptTime = GetTime();
                    }
                    else
                    {
                        m_RemoteSocket->DoRecv(&fd);
                        ExtractRemotePackets();
                        ProcessRemotePackets();

                        if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0 && GetTime() - m_LastAckTime >= 10)
                        {
                            m_RemoteSocket->PutBytes(m_GPSProtocol->SEND_GPSC_ACK(m_TotalPacketsReceivedFromRemote));
                            m_LastAckTime = GetTime();
                        }

                        m_RemoteSocket->DoSend(&send_fd);
                    }
                }

                if (m_RemoteSocket->GetConnecting())
                {
                    // we are currently attempting to connect

                    if (m_RemoteSocket->CheckConnect())
                    {
                        // the connection attempt completed

                        if (m_GameIsReliable && m_ActionReceived)
                        {
                            // this is a reconnection, not a new connection
                            // if the server accepts the reconnect request it will send a GPS_RECONNECT back requesting a certain number of packets

                            SendLocalChat("GProxy++ reconnected to the server!");
                            SendLocalChat("==================================================");
                            CONSOLE_Print("[GPROXY] reconnected to remote server");

                            // note: even though we reset the socket when we were disconnected, we haven't been careful to ensure we never queued any data in the meantime
                            // therefore it's possible the socket could have data in the send buffer
                            // this is bad because the server will expect us to send a GPS_RECONNECT message first
                            // so we must clear the send buffer before we continue
                            // note: we aren't losing data here, any important messages that need to be sent have been put in the packet buffer
                            // they will be requested by the server if required

                            m_RemoteSocket->ClearSendBuffer();
                            m_RemoteSocket->PutBytes(m_GPSProtocol->SEND_GPSC_RECONNECT(m_PID, m_ReconnectKey, m_TotalPacketsReceivedFromRemote));

                            // we cannot permit any forwarding of local packets until the game is synchronized again
                            // this will disable forwarding and will be reset when the synchronization is complete

                            m_Synchronized = false;
                        }
                        else
                            CONSOLE_Print("[GPROXY] connected to remote server");
                    }
                    else if (GetTime() - m_LastConnectionAttemptTime >= 10)
                    {
                        // the connection attempt timed out (10 seconds)

                        CONSOLE_Print("[GPROXY] connect to remote server timed out");

                        if (m_GameIsReliable && m_ActionReceived && m_ReconnectPort > 0)
                        {
                            uint32_t TimeRemaining = (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60 - (GetTime() - m_LastActionTime);

                            if (GetTime() - m_LastActionTime > (m_NumEmptyActions - m_NumEmptyActionsUsed + 1) * 60)
                                TimeRemaining = 0;

                            SendLocalChat("GProxy++ is attempting to reconnect... (" + UTIL_ToString(TimeRemaining) + " seconds remain)");
                            CONSOLE_Print("[GPROXY] attempting to reconnect");
                            m_RemoteSocket->Reset();
                            m_RemoteSocket->SetNoDelay(true);
                            m_RemoteSocket->Connect(std::string(), m_RemoteServerIP, m_ReconnectPort);
                            m_LastConnectionAttemptTime = GetTime();
                        }
                        else
                        {
                            m_LocalSocket->Disconnect();
                            delete m_LocalSocket;
                            m_LocalSocket = NULL;
                            m_RemoteSocket->Reset();
                            m_RemoteSocket->SetNoDelay(true);
                            m_RemoteServerIP.clear();
                            m_RemoteServerPort = 0;
                            return false;
                        }
                    }
                }
            }

            m_LocalSocket->DoSend(&send_fd);
        }
    }
    else
    {
        //
        // handle game listing
        //

        if (m_GameInfoSockets.size() == 0)
        {
            for (const uint16_t port : gameInfoPorts)
            {
                CTCPClient *tcpClient = new CTCPClient();
                tcpClient->Reset();
                tcpClient->SetNoDelay(true);
                tcpClient->Connect(std::string(), std::string(dotsURL), port);
                gGProxy->m_GameInfoSockets.push_back(tcpClient);
            }
        }

        for (CTCPClient *gameInfoSocket : m_GameInfoSockets)
        {
            if (gameInfoSocket->HasError() || (!gameInfoSocket->GetConnected() && !gameInfoSocket->GetConnecting()))
            {
                gameInfoSocket->Reset();
                gameInfoSocket->SetNoDelay(true);
                gameInfoSocket->Connect(std::string(), std::string(dotsURL), gameInfoSocket->GetPort());
            }
            else if (gameInfoSocket->GetConnecting())
            {
                if (gameInfoSocket->CheckConnect())
                {
                }
            }
            else if (gameInfoSocket->GetConnected())
            {
                gameInfoSocket->DoRecv(&fd);
                std::string *RecvBuffer = gameInfoSocket->GetBytes();
                BYTEARRAY Bytes = UTIL_CreateByteArray((unsigned char *)RecvBuffer->c_str(), RecvBuffer->size());
                gameInfoSocket->ClearRecvBuffer();

                if (Bytes.size() >= 4)
                {
                    std::string head = std::string(Bytes.begin(), Bytes.begin() + 4);
                    uint32_t i = 4;
                    if (head == "GAME")
                    {
                        uint16_t port = UTIL_ByteArrayToUInt16(Bytes, false, i);
                        i += sizeof(port);
                        uint8_t open_slots = Bytes.at(i);
                        i += sizeof(open_slots);
                        uint8_t slots_total = Bytes.at(i);
                        i += sizeof(slots_total);
                        uint8_t game_name_size = Bytes.at(i);
                        i += sizeof(game_name_size);
                        std::string game_name = std::string(Bytes.begin() + i, Bytes.begin() + i + game_name_size);
                        i += game_name_size;
                        uint8_t stat_string_size = Bytes.at(i);
                        i += sizeof(stat_string_size);
                        BYTEARRAY stat_string = BYTEARRAY(Bytes.begin() + i, Bytes.begin() + i + stat_string_size);
                        gGProxy->AddGame(new CIncomingGameHost(0, 0, port, ip, 0, game_name, open_slots, slots_total, 0x10000001, stat_string));
                    }
                }
            }
        }

        if (GetTime() - m_LastRefreshTime >= 2)
        {
            for (std::vector<CIncomingGameHost *>::iterator i = m_Games.begin(); i != m_Games.end(); i++)
            {
                BYTEARRAY MapGameType;
                UTIL_AppendByteArray(MapGameType, (*i)->GetGameType(), false);
                UTIL_AppendByteArray(MapGameType, (*i)->GetParameter(), false);
                BYTEARRAY MapFlags = UTIL_CreateByteArray((*i)->GetMapFlags(), false);
                BYTEARRAY MapWidth = UTIL_CreateByteArray((*i)->GetMapWidth(), false);
                BYTEARRAY MapHeight = UTIL_CreateByteArray((*i)->GetMapHeight(), false);

                bool m_TFT = true;

                m_UDPSocket->Broadcast(6112, m_GameProtocol->SEND_W3GS_GAMEINFO(m_TFT, m_War3Version, MapGameType, MapFlags, MapWidth, MapHeight, (*i)->GetGameName(), (*i)->GetHostName(), (*i)->GetElapsedTime(), (*i)->GetMapPath(), (*i)->GetMapCRC(), (*i)->GetSlotsTotal(), (*i)->GetOpenSlots(), m_Port, (*i)->GetUniqueGameID(), (*i)->GetUniqueGameID()));
            }

            m_LastRefreshTime = GetTime();
        }
    }

    return m_Exiting;
}

void CGProxy::ExtractLocalPackets()
{
    if (!m_LocalSocket)
        return;

    std::string *RecvBuffer = m_LocalSocket->GetBytes();
    BYTEARRAY Bytes = UTIL_CreateByteArray((unsigned char *)RecvBuffer->c_str(), RecvBuffer->size());

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while (Bytes.size() >= 4)
    {
        // byte 0 is always 247

        if (Bytes[0] == W3GS_HEADER_CONSTANT)
        {
            // bytes 2 and 3 contain the length of the packet

            uint16_t Length = UTIL_ByteArrayToUInt16(Bytes, false, 2);

            if (Length >= 4)
            {
                if (Bytes.size() >= Length)
                {
                    // we have to do a little bit of packet processing here
                    // this is because we don't want to forward any chat messages that start with a "/" as these may be forwarded to battle.net instead
                    // in fact we want to pretend they were never even received from the proxy's perspective

                    bool Forward = true;
                    BYTEARRAY Data = BYTEARRAY(Bytes.begin(), Bytes.begin() + Length);

                    if (Bytes[1] == CGameProtocol::W3GS_CHAT_TO_HOST)
                    {
                        if (Data.size() >= 5)
                        {
                            unsigned int i = 5;
                            unsigned char Total = Data[4];

                            if (Total > 0 && Data.size() >= i + Total)
                            {
                                i += Total;
                                unsigned char Flag = Data[i + 1];
                                i += 2;

                                std::string MessageString;

                                if (Flag == 16 && Data.size() >= i + 1)
                                {
                                    BYTEARRAY Message = UTIL_ExtractCString(Data, i);
                                    MessageString = std::string(Message.begin(), Message.end());
                                }
                                else if (Flag == 32 && Data.size() >= i + 5)
                                {
                                    BYTEARRAY Message = UTIL_ExtractCString(Data, i + 4);
                                    MessageString = std::string(Message.begin(), Message.end());
                                }

                                std::string Command = MessageString;
                                transform(Command.begin(), Command.end(), Command.begin(), (int (*)(int))toupper);

                                if (Command.size() >= 1 && Command.substr(0, 1) == "/")
                                {
                                    Forward = false;

                                    if (Command.size() >= 5 && Command.substr(0, 4) == "/re ")
                                    {
                                        SendLocalChat("You are not connected to battle.net.");
                                    }
                                    else if (Command == "/sc" || Command == "/spoof" || Command == "/spoofcheck" || Command == "/spoof check")
                                    {
                                        SendLocalChat("You are not connected to battle.net.");
                                    }
                                    else if (Command == "/status")
                                    {
                                        if (m_LocalSocket)
                                        {
                                            if (m_GameIsReliable && m_ReconnectPort > 0)
                                                SendLocalChat("GProxy++ disconnect protection: Enabled");
                                            else
                                                SendLocalChat("GProxy++ disconnect protection: Disabled");
                                        }
                                    }
                                    else if (Command.size() >= 4 && Command.substr(0, 3) == "/w ")
                                    {
                                        SendLocalChat("You are not connected to battle.net.");
                                    }
                                }
                            }
                        }
                    }

                    if (Forward)
                    {
                        m_LocalPackets.push(new CCommandPacket(W3GS_HEADER_CONSTANT, Bytes[1], Data));
                        m_PacketBuffer.push(new CCommandPacket(W3GS_HEADER_CONSTANT, Bytes[1], Data));
                        m_TotalPacketsReceivedFromLocal++;
                    }

                    *RecvBuffer = RecvBuffer->substr(Length);
                    Bytes = BYTEARRAY(Bytes.begin() + Length, Bytes.end());
                }
                else
                    return;
            }
            else
            {
                CONSOLE_Print("[GPROXY] received invalid packet from local player (bad length)");
                m_Exiting = true;
                return;
            }
        }
        else
        {
            CONSOLE_Print("[GPROXY] received invalid packet from local player (bad header constant)");
            m_Exiting = true;
            return;
        }
    }
}

void CGProxy::ProcessLocalPackets()
{
    if (!m_LocalSocket)
        return;

    while (!m_LocalPackets.empty())
    {
        CCommandPacket *Packet = m_LocalPackets.front();
        m_LocalPackets.pop();
        BYTEARRAY Data = Packet->GetData();

        if (Packet->GetPacketType() == W3GS_HEADER_CONSTANT)
        {
            if (Packet->GetID() == CGameProtocol::W3GS_REQJOIN)
            {
                if (Data.size() >= 20)
                {
                    // parse

                    uint32_t HostCounter = UTIL_ByteArrayToUInt32(Data, false, 4);
                    uint32_t EntryKey = UTIL_ByteArrayToUInt32(Data, false, 8);
                    unsigned char Unknown = Data[12];
                    uint16_t ListenPort = UTIL_ByteArrayToUInt16(Data, false, 13);
                    uint32_t PeerKey = UTIL_ByteArrayToUInt32(Data, false, 15);
                    BYTEARRAY Name = UTIL_ExtractCString(Data, 19);
                    std::string NameString = std::string(Name.begin(), Name.end());
                    BYTEARRAY Remainder = BYTEARRAY(Data.begin() + Name.size() + 20, Data.end());

                    if ((Remainder.size() == 18 && m_War3Version <= 28) || (Remainder.size() == 19 && m_War3Version >= 29))
                    {
                        // lookup the game in the main list

                        bool GameFound = false;

                        for (std::vector<CIncomingGameHost *>::iterator i = m_Games.begin(); i != m_Games.end(); i++)
                        {
                            if ((*i)->GetUniqueGameID() == EntryKey)
                            {
                                CONSOLE_Print("[GPROXY] local player requested game name [" + (*i)->GetGameName() + "]");

                                CONSOLE_Print("[GPROXY] connecting to remote server [" + (*i)->GetIPString() + "] on port " + UTIL_ToString((*i)->GetPort()));
                                m_RemoteServerIP = (*i)->GetIPString();
                                m_RemoteServerPort = (*i)->GetPort();
                                m_RemoteSocket->Reset();
                                m_RemoteSocket->SetNoDelay(true);
                                m_RemoteSocket->Connect(std::string(), m_RemoteServerIP, m_RemoteServerPort);
                                m_LastConnectionAttemptTime = GetTime();
                                m_GameIsReliable = ((*i)->GetMapWidth() == 1984 && (*i)->GetMapHeight() == 1984);
                                m_GameStarted = false;

                                // rewrite packet

                                BYTEARRAY DataRewritten;
                                DataRewritten.push_back(W3GS_HEADER_CONSTANT);
                                DataRewritten.push_back(Packet->GetID());
                                DataRewritten.push_back(0);
                                DataRewritten.push_back(0);
                                UTIL_AppendByteArray(DataRewritten, (*i)->GetHostCounter(), false);
                                UTIL_AppendByteArray(DataRewritten, (uint32_t)0, false);
                                DataRewritten.push_back(Unknown);
                                UTIL_AppendByteArray(DataRewritten, ListenPort, false);
                                UTIL_AppendByteArray(DataRewritten, PeerKey, false);
                                UTIL_AppendByteArray(DataRewritten, NameString);
                                UTIL_AppendByteArrayFast(DataRewritten, Remainder);
                                BYTEARRAY LengthBytes;
                                LengthBytes = UTIL_CreateByteArray((uint16_t)DataRewritten.size(), false);
                                DataRewritten[2] = LengthBytes[0];
                                DataRewritten[3] = LengthBytes[1];
                                Data = DataRewritten;

                                // save the hostname for later (for manual spoof checking)

                                m_JoinedName = NameString;
                                m_HostName = (*i)->GetHostName();
                                GameFound = true;
                                break;
                            }
                        }

                        if (!GameFound)
                        {
                            CONSOLE_Print("[GPROXY] local player requested unknown game (expired?)");
                            m_LocalSocket->Disconnect();
                        }
                    }
                    else
                        CONSOLE_Print("[GPROXY] received invalid join request from local player (invalid remainder)");
                }
                else
                    CONSOLE_Print("[GPROXY] received invalid join request from local player (too short)");
            }
            else if (Packet->GetID() == CGameProtocol::W3GS_LEAVEGAME)
            {
                m_LeaveGameSent = true;
                m_LocalSocket->Disconnect();
            }
            else if (Packet->GetID() == CGameProtocol::W3GS_CHAT_TO_HOST)
            {
                // handled in ExtractLocalPackets (yes, it's ugly)
            }
        }

        // warning: do not forward any data if we are not synchronized (e.g. we are reconnecting and resynchronizing)
        // any data not forwarded here will be cached in the packet buffer and sent later so all is well

        if (m_RemoteSocket && m_Synchronized)
            m_RemoteSocket->PutBytes(Data);

        delete Packet;
    }
}

void CGProxy::ExtractRemotePackets()
{
    std::string *RecvBuffer = m_RemoteSocket->GetBytes();
    BYTEARRAY Bytes = UTIL_CreateByteArray((unsigned char *)RecvBuffer->c_str(), RecvBuffer->size());

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while (Bytes.size() >= 4)
    {
        if (Bytes[0] == W3GS_HEADER_CONSTANT || Bytes[0] == GPS_HEADER_CONSTANT)
        {
            // bytes 2 and 3 contain the length of the packet

            uint16_t Length = UTIL_ByteArrayToUInt16(Bytes, false, 2);

            if (Length >= 4)
            {
                if (Bytes.size() >= Length)
                {
                    m_RemotePackets.push(new CCommandPacket(Bytes[0], Bytes[1], BYTEARRAY(Bytes.begin(), Bytes.begin() + Length)));

                    if (Bytes[0] == W3GS_HEADER_CONSTANT)
                        m_TotalPacketsReceivedFromRemote++;

                    *RecvBuffer = RecvBuffer->substr(Length);
                    Bytes = BYTEARRAY(Bytes.begin() + Length, Bytes.end());
                }
                else
                    return;
            }
            else
            {
                CONSOLE_Print("[GPROXY] received invalid packet from remote server (bad length)");
                m_Exiting = true;
                return;
            }
        }
        else
        {
            CONSOLE_Print("[GPROXY] received invalid packet from remote server (bad header constant)");
            m_Exiting = true;
            return;
        }
    }
}

void CGProxy::ProcessRemotePackets()
{
    if (!m_LocalSocket || !m_RemoteSocket)
        return;

    while (!m_RemotePackets.empty())
    {
        CCommandPacket *Packet = m_RemotePackets.front();
        m_RemotePackets.pop();

        if (Packet->GetPacketType() == W3GS_HEADER_CONSTANT)
        {
            if (Packet->GetID() == CGameProtocol::W3GS_SLOTINFOJOIN)
            {
                BYTEARRAY Data = Packet->GetData();

                if (Data.size() >= 6)
                {
                    uint16_t SlotInfoSize = UTIL_ByteArrayToUInt16(Data, false, 4);

                    if (Data.size() >= 7 + SlotInfoSize)
                        m_ChatPID = Data[6 + SlotInfoSize];
                }

                // send a GPS_INIT packet
                // if the server doesn't recognize it (e.g. it isn't GHost++) we should be kicked

                CONSOLE_Print("[GPROXY] join request accepted by remote server");

                if (m_GameIsReliable)
                {
                    CONSOLE_Print("[GPROXY] detected reliable game, starting GPS handshake");
                    m_RemoteSocket->PutBytes(m_GPSProtocol->SEND_GPSC_INIT(1));
                }
                else
                    CONSOLE_Print("[GPROXY] detected standard game, disconnect protection disabled");
            }
            else if (Packet->GetID() == CGameProtocol::W3GS_COUNTDOWN_END)
            {
                if (m_GameIsReliable && m_ReconnectPort > 0)
                    CONSOLE_Print("[GPROXY] game started, disconnect protection enabled");
                else
                {
                    if (m_GameIsReliable)
                        CONSOLE_Print("[GPROXY] game started but GPS handshake not complete, disconnect protection disabled");
                    else
                        CONSOLE_Print("[GPROXY] game started");
                }

                m_GameStarted = true;
            }
            else if (Packet->GetID() == CGameProtocol::W3GS_INCOMING_ACTION)
            {
                if (m_GameIsReliable)
                {
                    // we received a game update which means we can reset the number of empty actions we have to work with
                    // we also must send any remaining empty actions now
                    // note: the lag screen can't be up right now otherwise the server made a big mistake, so we don't need to check for it

                    BYTEARRAY EmptyAction;
                    EmptyAction.push_back(0xF7);
                    EmptyAction.push_back(0x0C);
                    EmptyAction.push_back(0x06);
                    EmptyAction.push_back(0x00);
                    EmptyAction.push_back(0x00);
                    EmptyAction.push_back(0x00);

                    for (unsigned char i = m_NumEmptyActionsUsed; i < m_NumEmptyActions; i++)
                        m_LocalSocket->PutBytes(EmptyAction);

                    m_NumEmptyActionsUsed = 0;
                }

                m_ActionReceived = true;
                m_LastActionTime = GetTime();
            }
            else if (Packet->GetID() == CGameProtocol::W3GS_START_LAG)
            {
                if (m_GameIsReliable)
                {
                    BYTEARRAY Data = Packet->GetData();

                    if (Data.size() >= 5)
                    {
                        unsigned char NumLaggers = Data[4];

                        if (Data.size() == 5 + NumLaggers * 5)
                        {
                            for (unsigned char i = 0; i < NumLaggers; i++)
                            {
                                bool LaggerFound = false;

                                for (std::vector<unsigned char>::iterator j = m_Laggers.begin(); j != m_Laggers.end(); j++)
                                {
                                    if (*j == Data[5 + i * 5])
                                        LaggerFound = true;
                                }

                                if (LaggerFound)
                                    CONSOLE_Print("[GPROXY] warning - received start_lag on known lagger");
                                else
                                    m_Laggers.push_back(Data[5 + i * 5]);
                            }
                        }
                        else
                            CONSOLE_Print("[GPROXY] warning - unhandled start_lag (2)");
                    }
                    else
                        CONSOLE_Print("[GPROXY] warning - unhandled start_lag (1)");
                }
            }
            else if (Packet->GetID() == CGameProtocol::W3GS_STOP_LAG)
            {
                if (m_GameIsReliable)
                {
                    BYTEARRAY Data = Packet->GetData();

                    if (Data.size() == 9)
                    {
                        bool LaggerFound = false;

                        for (std::vector<unsigned char>::iterator i = m_Laggers.begin(); i != m_Laggers.end();)
                        {
                            if (*i == Data[4])
                            {
                                i = m_Laggers.erase(i);
                                LaggerFound = true;
                            }
                            else
                                i++;
                        }

                        if (!LaggerFound)
                            CONSOLE_Print("[GPROXY] warning - received stop_lag on unknown lagger");
                    }
                    else
                        CONSOLE_Print("[GPROXY] warning - unhandled stop_lag");
                }
            }
            else if (Packet->GetID() == CGameProtocol::W3GS_INCOMING_ACTION2)
            {
                if (m_GameIsReliable)
                {
                    // we received a fractured game update which means we cannot use any empty actions until we receive the subsequent game update
                    // we also must send any remaining empty actions now
                    // note: this means if we get disconnected right now we can't use any of our buffer time, which would be very unlucky
                    // it still gives us 60 seconds total to reconnect though
                    // note: the lag screen can't be up right now otherwise the server made a big mistake, so we don't need to check for it

                    BYTEARRAY EmptyAction;
                    EmptyAction.push_back(0xF7);
                    EmptyAction.push_back(0x0C);
                    EmptyAction.push_back(0x06);
                    EmptyAction.push_back(0x00);
                    EmptyAction.push_back(0x00);
                    EmptyAction.push_back(0x00);

                    for (unsigned char i = m_NumEmptyActionsUsed; i < m_NumEmptyActions; i++)
                        m_LocalSocket->PutBytes(EmptyAction);

                    m_NumEmptyActionsUsed = m_NumEmptyActions;
                }
            }

            // forward the data

            m_LocalSocket->PutBytes(Packet->GetData());

            // we have to wait until now to send the status message since otherwise the slotinfojoin itself wouldn't have been forwarded

            if (Packet->GetID() == CGameProtocol::W3GS_SLOTINFOJOIN)
            {
                if (m_GameIsReliable)
                    SendLocalChat("This is a reliable game. Requesting GProxy++ disconnect protection from server...");
                else
                    SendLocalChat("This is an unreliable game. GProxy++ disconnect protection is disabled.");
            }
        }
        else if (Packet->GetPacketType() == GPS_HEADER_CONSTANT)
        {
            if (m_GameIsReliable)
            {
                BYTEARRAY Data = Packet->GetData();

                if (Packet->GetID() == CGPSProtocol::GPS_INIT && Data.size() == 12)
                {
                    m_ReconnectPort = UTIL_ByteArrayToUInt16(Data, false, 4);
                    m_PID = Data[6];
                    m_ReconnectKey = UTIL_ByteArrayToUInt32(Data, false, 7);
                    m_NumEmptyActions = Data[11];
                    SendLocalChat("GProxy++ disconnect protection is ready (" + UTIL_ToString((m_NumEmptyActions + 1) * 60) + " second buffer).");
                    CONSOLE_Print("[GPROXY] handshake complete, disconnect protection ready (" + UTIL_ToString((m_NumEmptyActions + 1) * 60) + " second buffer)");
                }
                else if (Packet->GetID() == CGPSProtocol::GPS_RECONNECT && Data.size() == 8)
                {
                    uint32_t LastPacket = UTIL_ByteArrayToUInt32(Data, false, 4);
                    uint32_t PacketsAlreadyUnqueued = m_TotalPacketsReceivedFromLocal - m_PacketBuffer.size();

                    if (LastPacket > PacketsAlreadyUnqueued)
                    {
                        uint32_t PacketsToUnqueue = LastPacket - PacketsAlreadyUnqueued;

                        if (PacketsToUnqueue > m_PacketBuffer.size())
                        {
                            CONSOLE_Print("[GPROXY] received GPS_RECONNECT with last packet > total packets sent");
                            PacketsToUnqueue = m_PacketBuffer.size();
                        }

                        while (PacketsToUnqueue > 0)
                        {
                            delete m_PacketBuffer.front();
                            m_PacketBuffer.pop();
                            PacketsToUnqueue--;
                        }
                    }

                    // send remaining packets from buffer, preserve buffer
                    // note: any packets in m_LocalPackets are still sitting at the end of this buffer because they haven't been processed yet
                    // therefore we must check for duplicates otherwise we might (will) cause a desync

                    std::queue<CCommandPacket *> TempBuffer;

                    while (!m_PacketBuffer.empty())
                    {
                        if (m_PacketBuffer.size() > m_LocalPackets.size())
                            m_RemoteSocket->PutBytes(m_PacketBuffer.front()->GetData());

                        TempBuffer.push(m_PacketBuffer.front());
                        m_PacketBuffer.pop();
                    }

                    m_PacketBuffer = TempBuffer;

                    // we can resume forwarding local packets again
                    // doing so prior to this point could result in an out-of-order stream which would probably cause a desync

                    m_Synchronized = true;
                }
                else if (Packet->GetID() == CGPSProtocol::GPS_ACK && Data.size() == 8)
                {
                    uint32_t LastPacket = UTIL_ByteArrayToUInt32(Data, false, 4);
                    uint32_t PacketsAlreadyUnqueued = m_TotalPacketsReceivedFromLocal - m_PacketBuffer.size();

                    if (LastPacket > PacketsAlreadyUnqueued)
                    {
                        uint32_t PacketsToUnqueue = LastPacket - PacketsAlreadyUnqueued;

                        if (PacketsToUnqueue > m_PacketBuffer.size())
                        {
                            CONSOLE_Print("[GPROXY] received GPS_ACK with last packet > total packets sent");
                            PacketsToUnqueue = m_PacketBuffer.size();
                        }

                        while (PacketsToUnqueue > 0)
                        {
                            delete m_PacketBuffer.front();
                            m_PacketBuffer.pop();
                            PacketsToUnqueue--;
                        }
                    }
                }
                else if (Packet->GetID() == CGPSProtocol::GPS_REJECT && Data.size() == 8)
                {
                    uint32_t Reason = UTIL_ByteArrayToUInt32(Data, false, 4);

                    if (Reason == REJECTGPS_INVALID)
                        CONSOLE_Print("[GPROXY] rejected by remote server: invalid data");
                    else if (Reason == REJECTGPS_NOTFOUND)
                        CONSOLE_Print("[GPROXY] rejected by remote server: player not found in any running games");

                    m_LocalSocket->Disconnect();
                }
            }
        }

        delete Packet;
    }
}

bool CGProxy::AddGame(CIncomingGameHost *game)
{
    // check for duplicates and rehosted games

    bool DuplicateFound = false;
    uint32_t OldestReceivedTime = GetTime();

    for (std::vector<CIncomingGameHost *>::iterator i = m_Games.begin(); i != m_Games.end(); i++)
    {
        if (game->GetIP() == (*i)->GetIP() && game->GetPort() == (*i)->GetPort())
        {
            // duplicate or rehosted game, delete the old one and add the new one
            // don't forget to remove the old one from the LAN list first

            m_UDPSocket->Broadcast(6112, m_GameProtocol->SEND_W3GS_DECREATEGAME((*i)->GetUniqueGameID()));
            delete *i;
            *i = game;
            DuplicateFound = true;
            break;
        }

        if (game->GetReceivedTime() < OldestReceivedTime)
            OldestReceivedTime = game->GetReceivedTime();
    }

    if (!DuplicateFound)
        m_Games.push_back(game);

    // the game list cannot hold more than 20 games (warcraft 3 doesn't handle it properly and ignores any further games)
    // if this game puts us over the limit, remove the oldest game
    // don't remove the "search game" since that's probably a pretty important game
    // note: it'll get removed automatically by the 60 second timeout in the main loop when appropriate

    if (m_Games.size() > 20)
    {
        for (std::vector<CIncomingGameHost *>::iterator i = m_Games.begin(); i != m_Games.end(); i++)
        {
            if (game->GetReceivedTime() == OldestReceivedTime)
            {
                m_UDPSocket->Broadcast(6112, m_GameProtocol->SEND_W3GS_DECREATEGAME((*i)->GetUniqueGameID()));
                delete *i;
                m_Games.erase(i);
                break;
            }
        }
    }

    return !DuplicateFound;
}

void CGProxy::SendLocalChat(std::string message)
{
    if (m_LocalSocket)
    {
        if (m_GameStarted)
        {
            if (message.size() > 127)
                message = message.substr(0, 127);

            m_LocalSocket->PutBytes(m_GameProtocol->SEND_W3GS_CHAT_FROM_HOST(m_ChatPID, UTIL_CreateByteArray(m_ChatPID), 32, UTIL_CreateByteArray((uint32_t)0, false), message));
        }
        else
        {
            if (message.size() > 254)
                message = message.substr(0, 254);

            m_LocalSocket->PutBytes(m_GameProtocol->SEND_W3GS_CHAT_FROM_HOST(m_ChatPID, UTIL_CreateByteArray(m_ChatPID), 16, BYTEARRAY(), message));
        }
    }
}

void CGProxy::SendEmptyAction()
{
    // we can't send any empty actions while the lag screen is up
    // so we keep track of who the lag screen is currently showing (if anyone) and we tear it down, send the empty action, and put it back up

    for (std::vector<unsigned char>::iterator i = m_Laggers.begin(); i != m_Laggers.end(); i++)
    {
        BYTEARRAY StopLag;
        StopLag.push_back(0xF7);
        StopLag.push_back(0x11);
        StopLag.push_back(0x09);
        StopLag.push_back(0);
        StopLag.push_back(*i);
        UTIL_AppendByteArray(StopLag, (uint32_t)60000, false);
        m_LocalSocket->PutBytes(StopLag);
    }

    BYTEARRAY EmptyAction;
    EmptyAction.push_back(0xF7);
    EmptyAction.push_back(0x0C);
    EmptyAction.push_back(0x06);
    EmptyAction.push_back(0x00);
    EmptyAction.push_back(0x00);
    EmptyAction.push_back(0x00);
    m_LocalSocket->PutBytes(EmptyAction);

    if (!m_Laggers.empty())
    {
        BYTEARRAY StartLag;
        StartLag.push_back(0xF7);
        StartLag.push_back(0x10);
        StartLag.push_back(0);
        StartLag.push_back(0);
        StartLag.push_back(m_Laggers.size());

        for (std::vector<unsigned char>::iterator i = m_Laggers.begin(); i != m_Laggers.end(); i++)
        {
            // using a lag time of 60000 ms means the counter will start at zero
            // hopefully warcraft 3 doesn't care about wild variations in the lag time in subsequent packets

            StartLag.push_back(*i);
            UTIL_AppendByteArray(StartLag, (uint32_t)60000, false);
        }

        BYTEARRAY LengthBytes;
        LengthBytes = UTIL_CreateByteArray((uint16_t)StartLag.size(), false);
        StartLag[2] = LengthBytes[0];
        StartLag[3] = LengthBytes[1];
        m_LocalSocket->PutBytes(StartLag);
    }
}
