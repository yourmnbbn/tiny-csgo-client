#ifndef __TINY_CSGO_CLIENT_HEADER__
#define __TINY_CSGO_CLIENT_HEADER__

#ifdef _WIN32
#pragma once
#endif

#include <iostream>
#include <string>
#include <mutex>
#include <vector>
#include <ranges>
#include <asio.hpp>

//from hl2sdk-csgo
#include "common/bitbuf.h"
#include <steam_api.h>
#include <strtools.h>
#include <checksum_crc.h>
#include <mathlib/IceKey.H>
#include <utlmemory.h>
#include <lzss.h>
#include <vstdlib/random.h>

#include "common/proto_oob.h"
#include "common/protocol.h"
#include "common/datafragments.h"
#include "netmessage/netmessages_signon.h"
#include "netmessage/netmessages.h"
#include "netmessage/splitmessage.hpp"
#include "netmessage/subchannel.hpp"

#define BYTES2FRAGMENTS(i) ((i+FRAGMENT_SIZE-1)/FRAGMENT_SIZE)

using namespace asio::ip;

inline asio::io_context g_IoContext;

inline constexpr int NET_CRYPT_KEY_LENGTH = 16;
inline constexpr int NET_COMPRESSION_STACKBUF_SIZE = 4096;
inline constexpr int NET_HEADER_FLAG_SPLITPACKET = -2;

//TO DO: configure this through extern file.
//You can get this value using st_crc plugin in tools
inline constexpr int SEND_TABLE_CRC32 = 0x3C17F0B1;

//Max retry limitation when connection failed at some point
inline constexpr int CONFIG_MAX_RETRY_LIMIT = 30;

class Client
{
	//For handling netmessages
	class CNetMessageHandler
	{
	public:
		static bool HandleNetMessageFromBuffer(Client* client, bf_read& buf, int type);
	};

public:
	Client() :
		m_WriteBuf(m_Buf, sizeof(m_Buf)), 
		m_ReadBuf(m_Buf, sizeof(m_Buf)), 
		m_Datageam(m_DatagramBuf, sizeof(m_DatagramBuf)),
		m_RawDatagram(m_RawDatagramBuf, sizeof(m_RawDatagramBuf))
	{
		for (int i = 0; i < MAX_STREAMS; i++)
		{
			m_ReceiveList[i].buffer = nullptr;
		}
	}
	
	[[nodiscard("Steam api must be confirmed working before running the client!")]]
	bool						PrepareSteamAPI(); 
	void						BindServer(const char* ip, const char* nickname, const char* password, uint16_t port);
	inline void					RunClient() { g_IoContext.run(); }
	void						InitCommandInput();
	
private:
	//Networking
	asio::awaitable<void>		ConnectToServer();
	asio::awaitable<bool>		RetryServer(udp::socket& socket, udp::endpoint& remote_endpoint);
	asio::awaitable<bool>		StartConnectProcess(udp::socket& socket, udp::endpoint& remote_endpoint, bool isRetry = false);
	asio::awaitable<bool>		SendConnectPacket(udp::socket& socket, udp::endpoint& remote_endpoint, uint32_t challenge, uint32_t auth_proto_version, uint32_t connect_proto_ver);
	asio::awaitable<void>		HandleIncomingPacket(udp::socket& socket, udp::endpoint& remote_endpoint);
	asio::awaitable<void>		SendDatagram(udp::socket& socket, udp::endpoint& remote_endpoint);
	asio::awaitable<void>		SendRawDatagramBuffer(udp::socket& socket, udp::endpoint& remote_endpoint);
	asio::awaitable< std::tuple<bool, uint32_t, uint32_t, uint32_t> > GetChallenge(udp::socket& socket, udp::endpoint& remote_endpoint, uint32_t cached_challenge);

	//Process the command we input in the command vector and write to the datagram
	void						HandleStringCommand();
	
	//Packet processing: encryption, decryption, decompression and message handling
	void						ProcessPacket(int packetSize);
	int							ProcessPacketHeader(bf_read& msg);
	bool						CheckReceivingList(int nList);
	bool						ReadSubChannelData(bf_read& buf, int stream);
	bool						ProcessMessages(bf_read& buf, bool wasReliable, size_t length);
	int							EncryptDatagram();
	inline byte*				GetEncryptionKey();
	void						UncompressFragments(dataFragments_t* data);
	bool						BufferToBufferDecompress(char* dest, unsigned int& destLen, char* source, unsigned int sourceLen);
	inline void					ProcessConnectionlessPacket();
	inline unsigned short		BufferToShortChecksum(const void* pvData, size_t nLength);

	//Net messages sending and wrapping
	inline void					SetSignonState(int state, int count) { SendNetMessage(CNETMsg_SignonState_t(state, count)); }
	inline void					SendNetMessage(INetMessage& msg) { msg.WriteToBuffer(m_Datageam); }
	inline void					SendNetMessage(INetMessage&& msg) { msg.WriteToBuffer(m_Datageam); }
	void						SendStringCommand(std::string& command);
	inline void					Disconnect(const char* reason = "Disconnect.") { SendNetMessage(CNETMsg_Disconnect_t(reason)); }

	// Write direct bytes to the datagram instead of using INetMessage::ReadFromBuffer
	// CRC check required
	// Don't use this, it's dangerous, might break the rest of the packet.
	inline void					SendDirectBuffer(const void* data, size_t length) { m_Datageam.WriteBytes(data, length); }

	// Send raw buffer to the datagram, this just add the out sequence and in sequence ahead of buffer as a valid netmessage
	// Message CRC is not garanteed valid
	// Don't use this
	inline void					SendRawDatagram(const void* data, size_t length) { m_RawDatagram.WriteBytes(data, length); }

	//Utility functions
	inline void					BuildUserInfoUpdateMessage(CMsg_CVars& rCvarList);
	inline void					ClearNetchannelInfo();
	inline void					ResetWriteBuffer(){ m_WriteBuf.Reset(); }
	inline void					ResetReadBuffer() { m_ReadBuf.Seek(0); }
	inline int					ReadBufferHeaderInt32() { return *(int*)m_Buf; }
	inline void					PrintRecvBuffer(size_t bytes);

private:
	//Bitbuf for reading and writing data
	char				m_Buf[NET_MAX_PAYLOAD];
	char				m_DatagramBuf[10240];
	char				m_RawDatagramBuf[1024];
	bf_write			m_WriteBuf;
	bf_write			m_Datageam;
	bf_write			m_RawDatagram;
	bf_read				m_ReadBuf;

	//host information
	int					m_HostVersion;
	int					m_Tick;
	int					m_SpawnCount;
	int					m_ServerChallenge = 0;
	int					m_AuthProtocol = 0;
	uint64_t			m_nServerReservationCookie = 0;

	//Net channel
	int					m_nInSequenceNr = 0;
	int					m_nOutSequenceNr = 1;
	int					m_nOutSequenceNrAck = 0;
	int					m_PacketDrop = 0;
	int					m_nOutReliableState = 0;
	// state of incoming reliable data
	int					m_nInReliableState = 0;
	dataFragments_t		m_ReceiveList[MAX_STREAMS]; // receive buffers for streams
	subChannel_s		m_SubChannels[MAX_SUBCHANNELS];

	//server info
	std::string			m_Ip;
	std::string			m_NickName;
	std::string			m_PassWord;
	uint16_t			m_Port;

	//commandline
	std::mutex					m_CommandVecLock;
	std::vector<std::string>	m_VecCommand;

	//Client cvar info
	int					m_clUpdateRate = 128;
	int					m_clCmdRate = 128;
	int					m_clRate = 128000;

	//Flags
	bool				m_flagRetry = false;
	bool				m_flagWaitForDisconnect = false;
};



bool Client::CNetMessageHandler::HandleNetMessageFromBuffer(Client* client, bf_read& buf, int type)
{
	switch (type)
	{
	case net_Tick:
	{
		CNETMsg_Tick_t tick(0, 0.f, 0.f, 0.f);
		tick.ReadFromBuffer(buf);
		client->m_Tick = tick.tick();

		printf("Receive NetMessage CNETMsg_Tick_t\n");
		printf("Server tick:%i, host_computationtime: 0x%X, host_computationtime_std_deviation: 0x%x, host_framestarttime_std_deviation: 0x%X\n",
			tick.tick(), tick.host_computationtime(), tick.host_computationtime_std_deviation(), tick.host_framestarttime_std_deviation());

		return true;
	}
	case net_StringCmd:
	{
		CNETMsg_StringCmd_t command("");
		command.ReadFromBuffer(buf);
		printf("Receive NetMessage CNETMsg_StringCmd_t\n");
		printf("Server wants to execute command %s", command.command().c_str());
		return true;
	}
	case net_PlayerAvatarData:
	{
		CNETMsg_PlayerAvatarData_t avatar;
		avatar.ReadFromBuffer(buf);
		printf("Receive NetMessage CNETMsg_PlayerAvatarData_t\n");
		return true;
	}
	case net_SignonState:
	{
		CNETMsg_SignonState_t signonState(SIGNONSTATE_NONE, -1);
		signonState.ReadFromBuffer(buf);
		printf("Receive NetMessage CNETMsg_SignonState_t\n");
		printf("signon_state: %u, spawn_count: %u\n",
			signonState.signon_state(), signonState.spawn_count());

		client->m_SpawnCount = signonState.spawn_count();

		switch (signonState.signon_state())
		{
		case SIGNONSTATE_NEW:
		{
			//Send client info, or we gonna be kicked for using different send tables
			CCLCMsg_ClientInfo_t info;

			info.set_send_table_crc(SEND_TABLE_CRC32);
			info.set_server_count(signonState.spawn_count());
			info.set_is_hltv(false);
			info.set_is_replay(false);
			info.set_friends_id(SteamUser() ? SteamUser()->GetSteamID().GetAccountID() : 0);
			info.set_friends_name(client->m_NickName);

			client->SendNetMessage(info);
			client->SendNetMessage(signonState);
			break;
		}
		case SIGNONSTATE_PRESPAWN:
		{
			client->SendNetMessage(signonState);
			client->SetSignonState(SIGNONSTATE_SPAWN, signonState.spawn_count());
			//client->SetSignonState(SIGNONSTATE_FULL, signonState.spawn_count());
			break;
		}
		case SIGNONSTATE_SPAWN:
		{
			//client->SendNetMessage(signonState);
			break;
		}
		case SIGNONSTATE_FULL:
		{
			//client->SendNetMessage(signonState);
			break;
		}
		}
		
		return true;
	}
	case net_SetConVar:
	{
		CNETMsg_SetConVar_t setConvar;
		setConvar.ReadFromBuffer(buf);
		printf("Receive NetMessage CNETMsg_SetConVar_t\n\n");

		//We don't actually have convar, so far we just simply print them
		for (int i = 0; i < setConvar.convars().cvars_size(); i++)
		{
			auto cvar = setConvar.convars().cvars(i);
			printf("%s %s\n", NetMsgGetCVarUsingDictionary(cvar), cvar.value().c_str());
		}

		return true;
	}
	case net_NOP:
	{
		CNETMsg_NOP_t nop;
		nop.ReadFromBuffer(buf);
		
		
		return true;
	}
	case net_Disconnect:
	{
		CNETMsg_Disconnect_t disconnect;
		disconnect.ReadFromBuffer(buf);
		printf("Disconnect from server : %s\n", disconnect.has_text() ? disconnect.text().c_str() : "unknown");

		//We wait untill receiving the last packet from the server, then to retry
		if (client->m_flagWaitForDisconnect)
		{
			client->m_flagWaitForDisconnect = false;
			client->m_flagRetry = true;
		}

		return true;
	}
	case net_File:
	{
		CNETMsg_File_t file;
		file.ReadFromBuffer(buf);
		printf("Receive NetMessage CNETMsg_File_t\n");
		return true;
	}
	case net_SplitScreenUser:
	{
		CNETMsg_SplitScreenUser_t splitScreenUser;
		splitScreenUser.ReadFromBuffer(buf);
		printf("Receive NetMessage CNETMsg_SplitScreenUser_t\n");
		return true;
	}
	case svc_ServerInfo:
	{
		CSVCMsg_ServerInfo_t info;
		info.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_ServerInfo_t\n");
		return true;
	}
	case svc_SendTable:
	{
		CSVCMsg_SendTable_t sendTable;
		sendTable.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_SendTable_t\n");
		return true;
	}
	case svc_ClassInfo:
	{
		CSVCMsg_ClassInfo_t classInfo;
		classInfo.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_ClassInfo_t, size: %u\n", classInfo.classes_size());

		for (int i = 0; i < classInfo.classes_size(); i++)
		{
			const CSVCMsg_ClassInfo::class_t& svclass = classInfo.classes(i);
			printf("Server class<datatable> : %s<%s>\n", 
				svclass.class_name().c_str(), svclass.data_table_name().c_str());
		}

		return true;
	}
	case svc_SetPause:
	{
		CSVCMsg_SetPause_t setPause;
		setPause.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_SetPause_t\n");
		return true;
	}
	case svc_CreateStringTable:
	{
		CSVCMsg_CreateStringTable_t createStringTable;
		createStringTable.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_CreateStringTable_t\n");

		printf("StringTable name: %s, max_entries: %i, user_data_size_bits: %i, flags: %i\n\n",
			createStringTable.name().c_str(),
			createStringTable.max_entries(),
			createStringTable.user_data_size_bits(),
			createStringTable.flags());

		return true;
	}
	case svc_UpdateStringTable:
	{
		CSVCMsg_UpdateStringTable_t updateStringTable;
		updateStringTable.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_UpdateStringTable_t\n");
		return true;
	}
	case svc_VoiceInit:
	{
		CSVCMsg_VoiceInit_t voiceInit;
		voiceInit.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_VoiceInit_t\n");
		return true;
	}
	case svc_VoiceData:
	{
		CSVCMsg_VoiceData_t voiceData;
		voiceData.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_VoiceData_t\n");
		return true;
	}
	case svc_Print:
	{
		CSVCMsg_Print_t print;
		print.ReadFromBuffer(buf);
		if (print.has_text())
		{
			printf("%s", print.text().c_str());
		}
		return true;
	}
	case svc_Sounds:
	{
		CSVCMsg_Sounds_t sounds;
		sounds.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_Sounds_t\n");
		return true;
	}
	case svc_SetView:
	{
		CSVCMsg_SetView_t setview;
		setview.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_SetView_t\n");
		return true;
	}
	case svc_FixAngle:
	{
		CSVCMsg_FixAngle_t fixAngle;
		fixAngle.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_FixAngle_t\n");
		return true;
	}
	case svc_CrosshairAngle:
	{
		CSVCMsg_CrosshairAngle_t crossHairAngle;
		crossHairAngle.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_CrosshairAngle_t\n");
		return true;
	}
	case svc_BSPDecal:
	{
		CSVCMsg_BSPDecal_t bspDecal;
		bspDecal.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_BSPDecal_t\n");
		return true;
	}
	break;
	case svc_SplitScreen:
	{
		CSVCMsg_SplitScreen_t splitScreen;
		splitScreen.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_SplitScreen_t\n");
		return true;
	}
	case svc_UserMessage:
	{
		CSVCMsg_UserMessage_t userMsg;
		userMsg.ReadFromBuffer(buf);

		printf("Receive NetMessage CSVCMsg_UserMessage_t\n");
		printf("msg_type: %i, passthrough: %i, size: %i\n",
			userMsg.msg_type(), userMsg.passthrough(), userMsg.msg_data().size());

		return true;
	}
	case svc_EntityMessage:
	{
		CSVCMsg_EntityMsg_t entityMsg;
		entityMsg.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_EntityMessage_t\n");
		return true;
	}
	case svc_GameEvent:
	{
		CSVCMsg_GameEvent_t gameEvent;
		gameEvent.ReadFromBuffer(buf); 

		printf("Receive NetMessage CSVCMsg_GameEvent_t\n");
		printf("Event:%s(%u)", gameEvent.event_name().c_str(), gameEvent.eventid());
		return true;
	}
	case svc_PacketEntities:
	{
		CSVCMsg_PacketEntities_t packetEntity;
		packetEntity.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_PacketEntities_t\n");

		return true;
	}
	case svc_TempEntities:
	{
		CSVCMsg_TempEntities_t tempEntity;
		tempEntity.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_TempEntities_t\n");
		return true;
	}
	case svc_Prefetch:
	{
		CSVCMsg_Prefetch_t prefetch;
		prefetch.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_Prefetch_t\n");
		return true;
	}
	case svc_Menu:
	{
		CSVCMsg_Menu_t menu;
		menu.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_Menu_t\n");
		return true;
	}
	case svc_GameEventList:
	{
		CSVCMsg_GameEventList_t gameEventList;
		gameEventList.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_GameEventList_t\n");
		return true;
	}
	case svc_GetCvarValue:
	{
		CSVCMsg_GetCvarValue_t getCvarValue;
		getCvarValue.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_GetCvarValue_t\n");
		printf("Server wants to get cvar <%s> value\n", getCvarValue.cvar_name().c_str());
		return true;
	}
	case svc_PaintmapData:
	{
		CSVCMsg_PaintmapData_t paintMap;
		paintMap.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_PaintmapData_t\n");
		return true;
	}
	case svc_CmdKeyValues:
	{
		CSVCMsg_CmdKeyValues_t kv;
		kv.ReadFromBuffer(buf);

		//Process kv here
		printf("Receive NetMessage CSVCMsg_CmdKeyValues_t\n");
		return true;
	}
	case svc_EncryptedData:
	{
		CSVCMsg_EncryptedData_t encryptData;
		encryptData.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_EncryptedData_t\n");
		return true;
	}
	case svc_HltvReplay:
	{
		CSVCMsg_HltvReplay_t hltvReplay;
		hltvReplay.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_HltvReplay_t\n");
		return true;
	}
	case svc_Broadcast_Command:
	{
		CSVCMsg_Broadcast_Command_t broadcastCmd;
		broadcastCmd.ReadFromBuffer(buf);
		printf("Receive NetMessage CSVCMsg_Broadcast_Command_t\n");
		return true;
	}
	} //switch(type)

	return false;
} //static bool CNetMessageHandler::HandleNetMessageFromBuffer(Client* client, bf_read& buf, int type)

bool Client::PrepareSteamAPI()
{
	if (!SteamAPI_IsSteamRunning())
	{
		printf("Steam is not running!\n");
		return false;
	}

	if (!SteamAPI_Init())
	{
		printf("Cannot initialize SteamAPI\n");
		return false;
	}
	if (!SteamUser())
	{
		printf("Cannot initialize ISteamUser interface\n");
		return false;
	}

	return true;
}

void Client::BindServer(const char* ip, const char* nickname, const char* password, uint16_t port)
{
	m_Ip = ip;
	m_NickName = nickname;
	m_PassWord = password;
	m_Port = port;

	asio::co_spawn(g_IoContext, ConnectToServer(), asio::detached);
}

void Client::InitCommandInput()
{
	thread_local bool run = false;
	if (run)
		return;

	std::thread([this]() {
		std::string command;
		while (std::getline(std::cin, command))
		{
			std::lock_guard<std::mutex> lock(m_CommandVecLock);
			m_VecCommand.push_back(std::string(command));
		}

		}).detach();

		run = true;
}

asio::awaitable<void> Client::ConnectToServer()
{
	printf("Connecting to server %s:%u, using nickname %s, using password %s\n",
		m_Ip.c_str(), m_Port, m_NickName.c_str(), m_PassWord.c_str());

	udp::socket socket(g_IoContext, udp::endpoint(udp::v4(), m_Port));
	auto remote_endpoint = udp::endpoint(make_address(m_Ip), m_Port);

	co_await StartConnectProcess(socket, remote_endpoint);
}

asio::awaitable<bool> Client::RetryServer(udp::socket& socket, udp::endpoint& remote_endpoint)
{
	//Clrear netchannel information
	ClearNetchannelInfo();
	
	printf("Retrying to server %s:%u, using nickname %s, using password %s\n",
		m_Ip.c_str(), m_Port, m_NickName.c_str(), m_PassWord.c_str());

	co_return co_await StartConnectProcess(socket, remote_endpoint, true);
}

asio::awaitable<bool> Client::StartConnectProcess(udp::socket& socket, udp::endpoint& remote_endpoint, bool isRetry)
{
	int challenge_retry = 0;

	while (true)
	{
		auto [success, challenge, auth_proto_ver, connect_proto_ver] = co_await GetChallenge(socket, remote_endpoint, m_ServerChallenge);
		if (success)
		{
			m_HostVersion = connect_proto_ver;
			m_ServerChallenge = challenge;
			m_AuthProtocol = auth_proto_ver;
			break;
		}

		printf("Connection failed after challenge response! retry...(#%i)\n", ++challenge_retry);

		if (challenge_retry > CONFIG_MAX_RETRY_LIMIT)
		{
			printf("Error: Max retry limit(%i) exceeded\n", CONFIG_MAX_RETRY_LIMIT);
			co_return false;
		}
	}

	int auth_retry = 0;
	while (!(co_await SendConnectPacket(socket, remote_endpoint, m_ServerChallenge, m_AuthProtocol, m_HostVersion)))
	{
		printf("Authentication failed! retry...(#%i)\n ", ++auth_retry);

		if (auth_retry > CONFIG_MAX_RETRY_LIMIT)
		{
			printf("Error: Max retry limit(%i) exceeded\n", CONFIG_MAX_RETRY_LIMIT);
			co_return false;
		}
	}
	
	//If it's a retry connect, don't call this because we only have one running and suspended, will be resumed when return true
	if(!isRetry)
		co_await HandleIncomingPacket(socket, remote_endpoint);
	
	co_return true;
}

asio::awaitable<bool> Client::SendConnectPacket(
	udp::socket& socket,
	udp::endpoint& remote_endpoint,
	uint32_t challenge,
	uint32_t auth_proto_version,
	uint32_t connect_proto_ver)
{
	ResetWriteBuffer();
	m_WriteBuf.WriteLong(-1);
	m_WriteBuf.WriteByte(C2S_CONNECT);
	m_WriteBuf.WriteLong(connect_proto_ver);
	m_WriteBuf.WriteLong(auth_proto_version);
	m_WriteBuf.WriteLong(challenge);
	m_WriteBuf.WriteString("");	//We send nickname by client info cvar
	m_WriteBuf.WriteString(m_PassWord.c_str());
	m_WriteBuf.WriteByte(1);	//numbers of players to connect, no spilt screen players

	//Send main client information
	CCLCMsg_SplitPlayerConnect_t splitMsg; 
	BuildUserInfoUpdateMessage(*splitMsg.mutable_convars());
	splitMsg.WriteToBuffer(m_WriteBuf);

	m_WriteBuf.WriteOneBit(0); //low violence setting
	m_WriteBuf.WriteLongLong(m_nServerReservationCookie); //reservation cookie
	m_WriteBuf.WriteByte(1); //platform

	unsigned char steamkey[STEAM_KEYSIZE];
	unsigned int keysize = 0;

	CSteamID localsid = SteamUser()->GetSteamID();
	SteamUser()->GetAuthSessionTicket(steamkey, STEAM_KEYSIZE, &keysize);

	m_WriteBuf.WriteLong(0); //Encryption key index
	m_WriteBuf.WriteShort(keysize + sizeof(uint64));
	m_WriteBuf.WriteLongLong(localsid.ConvertToUint64());
	m_WriteBuf.WriteBytes(steamkey, keysize);

	co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), remote_endpoint, asio::use_awaitable);

	//Wait for connect response, TO DO: add timeout support
	co_await socket.async_wait(socket.wait_read, asio::use_awaitable);
	co_await socket.async_receive_from(asio::buffer(m_Buf), remote_endpoint, asio::use_awaitable);
	
	ResetReadBuffer();
	if (m_ReadBuf.ReadLong() != -1)
		co_return false;

	auto c = m_ReadBuf.ReadByte();
	switch (c)
	{
	case S2C_CONNREJECT:
		char err[256];
		m_ReadBuf.ReadString(err, sizeof(err));

		printf("Connection refused! - %s\n", err);
		co_return false;

	case S2C_CONNECTION:
		//Tell server that we are connected, ready to receive netmessages
		SetSignonState(SIGNONSTATE_CONNECTED, -1);
		co_return true;

	default:
		printf("Connection error! Got response header - %u\n", c);
		co_return false;
	}

}

asio::awaitable<std::tuple<bool, uint32_t, uint32_t, uint32_t>> Client::GetChallenge(
	udp::socket& socket,
	udp::endpoint& remote_endpoint,
	uint32_t cached_challenge)
{
	//Write request challenge packet
	ResetWriteBuffer();
	m_WriteBuf.WriteLong(-1);
	m_WriteBuf.WriteByte(A2S_GETCHALLENGE);

	auto numBytesWritten = m_WriteBuf.GetNumBytesWritten();
	auto len = snprintf((char*)(m_Buf + numBytesWritten), sizeof(m_Buf) - numBytesWritten, "connect0x%08X", cached_challenge);

	co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten() + len + 1), remote_endpoint, asio::use_awaitable);

	//Wait for challenge response, TO DO: add timeout support
	co_await socket.async_wait(socket.wait_read, asio::use_awaitable);
	co_await socket.async_receive_from(asio::buffer(m_Buf), remote_endpoint, asio::use_awaitable);

	auto failed_obj = std::make_tuple<bool, uint32_t, uint32_t, uint32_t>(false, 0, 0, 0);

	//Read information from buffer
	ResetReadBuffer();
	if (m_ReadBuf.ReadLong() != -1 || m_ReadBuf.ReadByte() != S2C_CHALLENGE)
		co_return failed_obj;

	auto challenge = m_ReadBuf.ReadLong();
	auto auth_protocol_version = m_ReadBuf.ReadLong();
	auto encrypt_key = m_ReadBuf.ReadShort();
	auto server_steamid = m_ReadBuf.ReadLongLong();
	auto vac = m_ReadBuf.ReadByte();

	char buf[48];
	m_ReadBuf.ReadString(buf, sizeof(buf));
	if (StringHasPrefix(buf, "connect"))
	{
		if (StringHasPrefix(buf, "connect-retry"))
		{
			co_return co_await GetChallenge(socket, remote_endpoint, challenge);
		}
		else if (StringHasPrefix(buf, "connect-lan-only"))
		{
			printf("You cannot connect to this CS:GO server because it is restricted to LAN connections only.\n");
			co_return failed_obj;
		}
		else if (StringHasPrefix(buf, "connect-matchmaking-only"))
		{
			printf("You must use matchmaking to connect to this CS:GO server.\n");
			co_return failed_obj;
		}
		//else if... don't cover other circumstances for now
	}
	else
	{
		printf("Corrupted challenge response!\n");
		co_return failed_obj;
	}

	auto connect_protocol_version = m_ReadBuf.ReadLong();
	m_ReadBuf.ReadString(buf, sizeof(buf)); //lobby name
	auto require_pw = m_ReadBuf.ReadByte();
	auto lobby_id = m_ReadBuf.ReadLongLong();
	auto dcFriendsReqd = m_ReadBuf.ReadByte() != 0;
	auto officialValveServer = m_ReadBuf.ReadByte() != 0;
	
	printf("Server using '%s' lobbies, requiring pw %s, lobby id %llx\n",
		buf, require_pw ? "yes" : "no", lobby_id);

	printf("Get server challenge number : 0x%X, auth prorocol: 0x%02X, server steam: %llu, vac %s, dcFriendsReqd %u, officialValveServer %u\n",
		challenge, auth_protocol_version, server_steamid, vac ? "on" : "off", dcFriendsReqd, officialValveServer);

	co_return std::make_tuple<bool, uint32_t, uint32_t, uint32_t>(true,
		(uint32_t)challenge, (uint32_t)auth_protocol_version, (uint32_t)connect_protocol_version);
}

asio::awaitable<void> Client::HandleIncomingPacket(udp::socket& socket, udp::endpoint& remote_endpoint)
{
	//Process all incoming packet here after we established connection.
	while (true)
	{
		//Suspend packet receiving here for a retry, resume when successfully retrying server
		//HandleIncomingPacket will not be called again if it's a retry
		if (m_flagRetry)
		{
			if (!(co_await RetryServer(socket, remote_endpoint)))
			{
				printf("Retrying server failed! \n");
				co_return;
			}
			m_flagRetry = false;
		}

		HandleStringCommand();
		co_await SendDatagram(socket, remote_endpoint);
		co_await SendRawDatagramBuffer(socket, remote_endpoint);

		co_await socket.async_wait(socket.wait_read, asio::use_awaitable);
		size_t length = co_await socket.async_receive_from(asio::buffer(m_Buf), remote_endpoint, asio::use_awaitable);

		ResetReadBuffer();

		if (ReadBufferHeaderInt32() == -1)
		{
			m_ReadBuf.ReadLong(); //-1

			ProcessConnectionlessPacket();
			continue;
		}

		// Check for split message
		if (ReadBufferHeaderInt32() == NET_HEADER_FLAG_SPLITPACKET)
		{
			if (!NET_GetLong(m_ReadBuf, length))
				continue;
		}

		ProcessPacket(length);
	}

	co_return;
}

asio::awaitable<void> Client::SendDatagram(udp::socket& socket, udp::endpoint& remote_endpoint)
{
	ResetWriteBuffer();
	m_WriteBuf.WriteLong(m_nOutSequenceNr++); //out sequence
	m_WriteBuf.WriteLong(m_nInSequenceNr); //In sequence
	bf_write flagsPos = m_WriteBuf;
	m_WriteBuf.WriteByte(0); //Flag place holder
	m_WriteBuf.WriteShort(0);//checksum place holder

	int nCheckSumStart = m_WriteBuf.GetNumBytesWritten();

	m_WriteBuf.WriteByte(m_nInReliableState); //InReliableState

	//Write datagram
	m_WriteBuf.WriteBytes(m_Datageam.GetData(), m_Datageam.GetNumBytesWritten());
	m_Datageam.Reset();

	flagsPos.WriteByte(0);

	constexpr auto MIN_ROUTABLE_PAYLOAD = 16;
	constexpr auto NETMSG_TYPE_BITS = 8;

	// Deal with packets that are too small for some networks
	while (m_WriteBuf.GetNumBytesWritten() < MIN_ROUTABLE_PAYLOAD)
	{
		// Go ahead and pad some bits as long as needed
		CNETMsg_NOP_t nop;
		nop.WriteToBuffer(m_WriteBuf);
	}

	// Make sure we have enough bits to read a final net_NOP opcode before compressing 
	int nRemainingBits = m_WriteBuf.GetNumBitsWritten() % 8;
	if (nRemainingBits > 0 && nRemainingBits <= (8 - NETMSG_TYPE_BITS))
	{
		CNETMsg_NOP_t nop;
		nop.WriteToBuffer(m_WriteBuf);
	}

	const void* pvData = m_WriteBuf.GetData() + nCheckSumStart;
	int nCheckSumBytes = m_WriteBuf.GetNumBytesWritten() - nCheckSumStart;
	unsigned short usCheckSum = BufferToShortChecksum(pvData, nCheckSumBytes);
	flagsPos.WriteUBitLong(usCheckSum, 16);

	auto length = EncryptDatagram();
	co_await socket.async_send_to(asio::buffer(m_Buf, length), remote_endpoint, asio::use_awaitable);
}

asio::awaitable<void> Client::SendRawDatagramBuffer(udp::socket& socket, udp::endpoint& remote_endpoint)
{
	if (m_RawDatagram.GetNumBytesWritten() <= 0)
		co_return;

	ResetWriteBuffer();
	m_WriteBuf.WriteLong(m_nOutSequenceNr++); //out sequence
	m_WriteBuf.WriteLong(m_nInSequenceNr); //In sequence

	m_WriteBuf.WriteBytes(m_RawDatagram.GetData(), m_RawDatagram.GetNumBytesWritten());
	m_RawDatagram.Reset();

	int length = EncryptDatagram();
	co_await socket.async_send_to(asio::buffer(m_Buf, length), remote_endpoint, asio::use_awaitable);
}

void Client::HandleStringCommand()
{
	auto trim = [](std::string str) {
		str.erase(0, str.find_first_not_of(" "));
		str.erase(str.find_last_not_of(" ") + 1);
		return str;
	};

	auto isEmpty = [](std::string str) { return !str.empty(); };

	{
		std::lock_guard<std::mutex> lock(m_CommandVecLock);

		for (std::string command : m_VecCommand | std::views::transform(trim) | std::views::filter(isEmpty))
		{
			SendStringCommand(command);
		}

		m_VecCommand.clear();
	}
}

void Client::ProcessPacket(int packetSize)
{
	//Message decryption, for now we only support default encryption key
	CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memDecryptedAll(NET_COMPRESSION_STACKBUF_SIZE);
	IceKey iceKey(2);
	iceKey.set(GetEncryptionKey());

	if ((packetSize % iceKey.blockSize()) == 0)
	{
		// Decrypt the message
		memDecryptedAll.EnsureCapacity(packetSize);
		unsigned char* pchCryptoBuffer = (unsigned char*)stackalloc(iceKey.blockSize());
		for (int k = 0; k < (int)packetSize; k += iceKey.blockSize())
		{
			iceKey.decrypt((const unsigned char*)(m_Buf + k), pchCryptoBuffer);
			Q_memcpy(memDecryptedAll.Base() + k, pchCryptoBuffer, iceKey.blockSize());
		}

		// Check how much random fudge we have
		int numRandomFudgeBytes = *memDecryptedAll.Base();
		if ((numRandomFudgeBytes > 0) && (int(numRandomFudgeBytes + 1 + sizeof(int32)) < packetSize))
		{
			// Fetch the size of the encrypted message
			int32 numBytesWrittenWire = 0;
			Q_memcpy(&numBytesWrittenWire, memDecryptedAll.Base() + 1 + numRandomFudgeBytes, sizeof(int32));
			int32 const numBytesWritten = BigLong(numBytesWrittenWire);	// byteswap from the wire

			// Make sure the total size of the message matches the expectations
			if (int(numRandomFudgeBytes + 1 + sizeof(int32) + numBytesWritten) == packetSize)
			{
				// Fix the packet to point at decrypted data!
				packetSize = numBytesWritten;
				Q_memcpy(m_Buf, memDecryptedAll.Base() + 1 + numRandomFudgeBytes + sizeof(int32), packetSize);
			}
		}
	}

	//Is this message compressed?
	constexpr auto NET_HEADER_FLAG_COMPRESSEDPACKET = -3;
	if (ReadBufferHeaderInt32() == NET_HEADER_FLAG_COMPRESSEDPACKET)
	{
		byte* pCompressedData = (byte*)m_Buf + sizeof(unsigned int);

		CLZSS lzss;
		// Decompress
		int actualSize = lzss.GetActualSize(pCompressedData);
		if (actualSize <= 0 || actualSize > NET_MAX_PAYLOAD)
			return;

		MEM_ALLOC_CREDIT();
		CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memDecompressed(NET_COMPRESSION_STACKBUF_SIZE);
		memDecompressed.EnsureCapacity(actualSize);

		unsigned int uDecompressedSize = lzss.SafeUncompress(pCompressedData, memDecompressed.Base(), actualSize);
		if (uDecompressedSize == 0 || ((unsigned int)actualSize) != uDecompressedSize)
		{
			return;
		}

		// packet->wiresize is already set
		Q_memcpy(m_Buf, memDecompressed.Base(), uDecompressedSize);

		packetSize = uDecompressedSize;
	}

	//PrintRecvBuffer(packetSize);
	bf_read msg(m_Buf, packetSize);

	int flags = ProcessPacketHeader(msg);

	if (flags == -1)
		return;

	//Handle netmessages
	if (flags & PACKET_FLAG_RELIABLE)
	{
		int i, bit = 1 << msg.ReadUBitLong(3);


		for (i = 0; i < MAX_STREAMS; i++)
		{
			if (msg.ReadOneBit() != 0)
			{
				if (!ReadSubChannelData(msg, i))
				{
					printf("Error reading packet! stream %i\n", i);
					return; // error while reading fragments, drop whole packet
				}
			}
		}

		// flip subChannel bit to signal successfull receiving
		FLIPBIT(m_nInReliableState, bit);

		for (i = 0; i < MAX_STREAMS; i++)
		{
			if (!CheckReceivingList(i))
				return; // error while processing 
		}
	}

	// Is there anything left to process?
	if (msg.GetNumBitsLeft() > 0)
	{
		// parse and handle all messeges 
		if (!ProcessMessages(msg, false, packetSize))
		{
			return;	// disconnect or error
		}
	}
}

bool Client::CheckReceivingList(int nList)
{
	dataFragments_t* data = &m_ReceiveList[nList]; // get list

	if (data->buffer == NULL)
		return true;

	if (data->ackedFragments < data->numFragments)
		return true;

	if (data->ackedFragments > data->numFragments)
	{
		printf("Receiving failed: too many fragments %i/%i\n", data->ackedFragments, data->numFragments);
		return false;
	}

	// got all fragments
	printf("Receiving complete: %i fragments, %i bytes\n", data->numFragments, data->bytes);

	if (data->isCompressed)
	{
		UncompressFragments(data);
	}

	if (!data->filename[0])
	{
		bf_read buffer(data->buffer, data->bytes);

		if (!ProcessMessages(buffer, true, data->bytes)) // parse net message
		{
			return false; // stop reading any further
		}
	}
	else
	{
		//...
	}

	// clear receiveList
	if (data->buffer)
	{
		delete[] data->buffer;
		data->buffer = NULL;
	}

	return true;
}

bool Client::ReadSubChannelData(bf_read& buf, int stream)
{
	dataFragments_t* data = &m_ReceiveList[stream]; // get list
	int startFragment = 0;
	int numFragments = 0;
	unsigned int offset = 0;
	unsigned int length = 0;

	bool bSingleBlock = buf.ReadOneBit() == 0; // is single block ?

	if (!bSingleBlock)
	{
		startFragment = buf.ReadUBitLong(MAX_FILE_SIZE_BITS - FRAGMENT_BITS); // 16 MB max
		numFragments = buf.ReadUBitLong(3);  // 8 fragments per packet max
		offset = startFragment * FRAGMENT_SIZE;
		length = numFragments * FRAGMENT_SIZE;
	}

	if (offset == 0) // first fragment, read header info
	{
		data->filename[0] = 0;
		data->isCompressed = false;
		data->isReplayDemo = false;
		data->transferID = 0;

		if (bSingleBlock)
		{
			// data compressed ?
			if (buf.ReadOneBit())
			{
				data->isCompressed = true;
				data->nUncompressedSize = buf.ReadUBitLong(MAX_FILE_SIZE_BITS);
			}
			else
			{
				data->isCompressed = false;
			}

			data->bytes = buf.ReadUBitLong(NET_MAX_PAYLOAD_BITS);
		}
		else
		{

			if (buf.ReadOneBit()) // is it a file ?
			{
				data->transferID = buf.ReadUBitLong(32);
				buf.ReadString(data->filename, MAX_OSPATH);

				// replay demo?
				if (buf.ReadOneBit())
				{
					data->isReplayDemo = true;
				}
			}

			// data compressed ?
			if (buf.ReadOneBit())
			{
				data->isCompressed = true;
				data->nUncompressedSize = buf.ReadUBitLong(MAX_FILE_SIZE_BITS);
			}
			else
			{
				data->isCompressed = false;
			}

			data->bytes = buf.ReadUBitLong(MAX_FILE_SIZE_BITS);
		}

		if (data->buffer)
		{
			// last transmission was aborted, free data
			delete[] data->buffer;
			data->buffer = NULL;
			printf("Fragment transmission aborted at %i/%i\n", data->ackedFragments, data->numFragments);
		}

		data->bits = data->bytes * 8;
		data->asTCP = false;
		data->numFragments = BYTES2FRAGMENTS(data->bytes);
		data->ackedFragments = 0;
		data->file = FILESYSTEM_INVALID_HANDLE;

		if (bSingleBlock)
		{
			numFragments = data->numFragments;
			length = numFragments * FRAGMENT_SIZE;
		}

		if (data->bytes > MAX_FILE_SIZE)
		{
			// This can happen with the compressed path above, which uses VarInt32 rather than MAX_FILE_SIZE_BITS
			printf("Net message exceeds max size (%u / %u)\n", MAX_FILE_SIZE, data->bytes);
			// Subsequent packets for this transfer will treated as invalid since we never setup a buffer.
			return false;
		}

		if (data->isCompressed && data->nUncompressedSize > MAX_FILE_SIZE)
		{
			// This can happen with the compressed path above, which uses VarInt32 rather than MAX_FILE_SIZE_BITS
			printf("Net message uncompressed size exceeds max size (%u / compressed %u / uncompressed %u)\n", MAX_FILE_SIZE, data->bytes, data->nUncompressedSize);
			// Subsequent packets for this transfer will treated as invalid since we never setup a buffer.
			return false;
		}

		data->buffer = new char[PAD_NUMBER(data->bytes, 4)];
	}
	else
	{
		if (data->buffer == NULL)
		{
			// This can occur if the packet containing the "header" (offset == 0) is dropped.  Since we need the header to arrive we'll just wait
			//  for a retry
			printf("Received fragment out of order: %i/%i, offset %i\n", startFragment, numFragments, offset);
			return false;
		}
	}

	if ((startFragment + numFragments) == data->numFragments)
	{
		// we are receiving the last fragment, adjust length
		int rest = FRAGMENT_SIZE - (data->bytes % FRAGMENT_SIZE);
		if (rest < FRAGMENT_SIZE)
			length -= rest;
	}
	else if ((startFragment + numFragments) > data->numFragments)
	{
		// a malicious client can send a fragment beyond what was arranged in fragment#0 header
		// old code will overrun the allocated buffer and likely cause a server crash
		// it could also cause a client memory overrun because the offset can be anywhere from 0 to 16MB range
		// drop the packet and wait for client to retry
		printf("Received fragment chunk out of bounds: %i+%i>%i\n", startFragment, numFragments, data->numFragments);
		return false;
	}

	buf.ReadBytes(data->buffer + offset, length); // read data

	data->ackedFragments += numFragments;

	//if (net_showfragments.GetBool())
	//printf("Received fragments: offset %i start %i, num %i, length:%i\n", offset, startFragment, numFragments, length);
	//printf("Total fragment needed: %i, received: %i\n", data->numFragments, data->ackedFragments);
	return true;
}

bool Client::ProcessMessages(bf_read& buf, bool wasReliable, size_t length)
{
	int startbit = buf.GetNumBitsRead();

	while (true)
	{
		if (buf.IsOverflowed())
		{
			printf("ProcessMessages: incoming buffer overflow!\n");
			return false;
		}

		// Are we at the end?
		if (buf.GetNumBitsLeft() < 8) // Minimum bits for message header encoded using VarInt32
		{
			break;
		}

		unsigned char cmd = buf.ReadVarInt32();

		if (!CNetMessageHandler::HandleNetMessageFromBuffer(this, buf, cmd))
		{
			int size = buf.ReadVarInt32();
			printf("Error: Got unhandled message type 0x%X\n", cmd);
			//PrintRecvBuffer(length);
			if (size < 0 || size > NET_MAX_PAYLOAD)
			{
				printf("Unknown message size %i exceed the limit %i\n", size, NET_MAX_PAYLOAD);
				return false;
			}

			// Check its valid
			if (size > buf.GetNumBytesLeft())
			{
				printf("Unknown message size(%i) greater than bytes left(%i) in the message, abort parsing\n", size, buf.GetNumBytesLeft());
				return false;
			}

			//Skip this unknown message
			buf.SeekRelative(size * 8);
		}
	}

	return true;
}

int Client::ProcessPacketHeader(bf_read& msg)
{
	ResetReadBuffer();
	int sequence = msg.ReadLong();
	int sequence_ack = msg.ReadLong();
	int flags = msg.ReadByte();

	unsigned short usCheckSum = (unsigned short)msg.ReadUBitLong(16);
	int nOffset = msg.GetNumBitsRead() >> 3;
	int nCheckSumBytes = msg.TotalBytesAvailable() - nOffset;

	const void* pvData = msg.GetBasePointer() + nOffset;
	unsigned short usDataCheckSum = BufferToShortChecksum(pvData, nCheckSumBytes);

	if (usDataCheckSum != usCheckSum)
	{
		printf("corrupted packet %u at %u, crc check failed\n", sequence, m_nInSequenceNr);
		return -1;
	}

	int relState = msg.ReadByte();	// reliable state of 8 subchannels
	int nChoked = 0;	// read later if choked flag is set

	if (flags & PACKET_FLAG_CHOKED)
		nChoked = msg.ReadByte();

	// discard stale or duplicated packets
	if (sequence <= m_nInSequenceNr)
	{
		if (sequence == m_nInSequenceNr)
		{
			printf("duplicate packet %u at %u\n", sequence, m_nInSequenceNr);
		}
		else
		{
			printf("out of order packet %u at %u\n", sequence, m_nInSequenceNr);
		}

		return -1;
	}

	//
	// dropped packets don't keep the message from being used
	//
	m_PacketDrop = sequence - (m_nInSequenceNr + nChoked + 1);

	if (m_PacketDrop > 0)
	{
		printf("Dropped %u packets at %u\n", m_PacketDrop, sequence);
	}

	m_nInSequenceNr = sequence;
	m_nOutSequenceNrAck = sequence_ack;

	return flags;
}

void Client::SendStringCommand(std::string& command)
{
	CNETMsg_StringCmd_t stringCmd(command.c_str());
	SendNetMessage(stringCmd);

	if (command == "disconnect")
	{
		Disconnect();
	}
	else if (command == "retry")
	{
		m_flagWaitForDisconnect = true;
		Disconnect();
	}
}

int Client::EncryptDatagram()
{
	CUtlMemoryFixedGrowable< byte, NET_COMPRESSION_STACKBUF_SIZE > memEncryptedAll(NET_COMPRESSION_STACKBUF_SIZE);
	int length = m_WriteBuf.GetNumBytesWritten();

	IceKey iceKey(2);
	iceKey.set(GetEncryptionKey());

	// Generate some random fudge, ICE operates on 64-bit blocks, so make sure our total size is a multiple of 8 bytes
	int numRandomFudgeBytes = RandomInt(16, 72);
	int numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof(int32) + length;
	numRandomFudgeBytes += iceKey.blockSize() - (numTotalEncryptedBytes % iceKey.blockSize());
	numTotalEncryptedBytes = 1 + numRandomFudgeBytes + sizeof(int32) + length;

	char* pchRandomFudgeBytes = (char*)stackalloc(numRandomFudgeBytes);
	for (int k = 0; k < numRandomFudgeBytes; ++k)
		pchRandomFudgeBytes[k] = RandomInt(16, 250);

	// Prepare the encrypted memory
	memEncryptedAll.EnsureCapacity(numTotalEncryptedBytes);
	*memEncryptedAll.Base() = numRandomFudgeBytes;
	Q_memcpy(memEncryptedAll.Base() + 1, pchRandomFudgeBytes, numRandomFudgeBytes);

	int32 const numBytesWrittenWire = BigLong(length);	// byteswap for the wire
	Q_memcpy(memEncryptedAll.Base() + 1 + numRandomFudgeBytes, &numBytesWrittenWire, sizeof(numBytesWrittenWire));
	Q_memcpy(memEncryptedAll.Base() + 1 + numRandomFudgeBytes + sizeof(int32), m_Buf, length);

	// Encrypt the message
	unsigned char* pchCryptoBuffer = (unsigned char*)stackalloc(iceKey.blockSize());
	for (int k = 0; k < numTotalEncryptedBytes; k += iceKey.blockSize())
	{
		iceKey.encrypt((const unsigned char*)(memEncryptedAll.Base() + k), pchCryptoBuffer);
		Q_memcpy(memEncryptedAll.Base() + k, pchCryptoBuffer, iceKey.blockSize());
	}

	Q_memcpy(m_Buf, memEncryptedAll.Base(), numTotalEncryptedBytes);

	return numTotalEncryptedBytes;
}

void Client::UncompressFragments(dataFragments_t* data)
{
	if (!data->isCompressed)
		return;

	// allocate buffer for uncompressed data, align to 4 bytes boundary
	char* newbuffer = new char[PAD_NUMBER(data->nUncompressedSize, 4)];
	unsigned int uncompressedSize = data->nUncompressedSize;

	// uncompress data
	BufferToBufferDecompress(newbuffer, uncompressedSize, data->buffer, data->bytes);

	// free old buffer and set new buffer
	delete[] data->buffer;
	data->buffer = newbuffer;
	data->bytes = uncompressedSize;
	data->isCompressed = false;
}

bool Client::BufferToBufferDecompress(char* dest, unsigned int& destLen, char* source, unsigned int sourceLen)
{
	CLZSS s;
	if (s.IsCompressed((byte*)source))
	{
		unsigned int uDecompressedLen = s.GetActualSize((byte*)source);
		if (uDecompressedLen > destLen)
		{
			Warning("NET_BufferToBufferDecompress with improperly sized dest buffer (%u in, %u needed)\n", destLen, uDecompressedLen);
			return false;
		}
		else
		{
			destLen = s.SafeUncompress((byte*)source, (byte*)dest, destLen);
		}
	}
	else
	{
		if (sourceLen > destLen)
		{
			Warning("NET_BufferToBufferDecompress with improperly sized dest buffer (%u in, %u needed)\n", destLen, sourceLen);
			return false;
		}

		Q_memcpy(dest, source, sourceLen);
		destLen = sourceLen;
	}

	return true;
}

inline void Client::ProcessConnectionlessPacket()
{
	printf("Get connectionless packet : %02X\n", m_ReadBuf.ReadByte());
}


inline byte* Client::GetEncryptionKey()
{
	static uint32 unHostVersion = m_HostVersion;
	static byte pubEncryptionKey[NET_CRYPT_KEY_LENGTH] = {
		'C', 'S', 'G', 'O',
		byte((unHostVersion >> 0) & 0xFF), byte((unHostVersion >> 8) & 0xFF), byte((unHostVersion >> 16) & 0xFF), byte((unHostVersion >> 24) & 0xFF),
		byte((unHostVersion >> 2) & 0xFF), byte((unHostVersion >> 10) & 0xFF), byte((unHostVersion >> 18) & 0xFF), byte((unHostVersion >> 26) & 0xFF),
		byte((unHostVersion >> 4) & 0xFF), byte((unHostVersion >> 12) & 0xFF), byte((unHostVersion >> 20) & 0xFF), byte((unHostVersion >> 28) & 0xFF),
	};

	return pubEncryptionKey;
}

inline unsigned short Client::BufferToShortChecksum(const void* pvData, size_t nLength)
{
	CRC32_t crc = CRC32_ProcessSingleBuffer(pvData, nLength);

	unsigned short lowpart = (crc & 0xffff);
	unsigned short highpart = ((crc >> 16) & 0xffff);

	return (unsigned short)(lowpart ^ highpart);
}

inline void Client::PrintRecvBuffer(size_t bytes)
{
	for (size_t i = 0; i < bytes; i++)
	{
		printf("%02X ", m_Buf[i] & 0xFF);
	}
	printf("\n\n");
}

inline void Client::BuildUserInfoUpdateMessage(CMsg_CVars& rCvarList)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%u", SteamUser()->GetSteamID().GetAccountID());
	NetMsgSetCVarUsingDictionary(rCvarList.add_cvars(), "accountid", buf);
	
	NetMsgSetCVarUsingDictionary(rCvarList.add_cvars(), "name", m_NickName.c_str());

	snprintf(buf, sizeof(buf), "%u", m_clUpdateRate);
	NetMsgSetCVarUsingDictionary(rCvarList.add_cvars(), "cl_updaterate", buf);

	snprintf(buf, sizeof(buf), "%u", m_clCmdRate);
	NetMsgSetCVarUsingDictionary(rCvarList.add_cvars(), "cl_cmdrate", buf);

	snprintf(buf, sizeof(buf), "%u", m_clRate);
	NetMsgSetCVarUsingDictionary(rCvarList.add_cvars(), "rate", buf);

	snprintf(buf, sizeof(buf), "$%llx", m_nServerReservationCookie);
	NetMsgSetCVarUsingDictionary(rCvarList.add_cvars(), "cl_session", buf);
}

inline void Client::ClearNetchannelInfo()
{
	m_nInSequenceNr = 0;
	m_nOutSequenceNr = 1;
	m_nOutSequenceNrAck = 0;
	m_PacketDrop = 0;
	m_nInReliableState = 0;
	m_nOutReliableState = 0;
}

#endif // !__TINY_CSGO_CLIENT_HEADER__
