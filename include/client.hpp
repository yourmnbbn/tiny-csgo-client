#ifndef __TINY_CSGO_CLIENT_HEADER__
#define __TINY_CSGO_CLIENT_HEADER__

#ifdef _WIN32
#pragma once
#endif

#include <iostream>
#include <string>
#include <format>
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
//#include "netmessage/netmessageshandler.h"

#define BYTES2FRAGMENTS(i) ((i+FRAGMENT_SIZE-1)/FRAGMENT_SIZE)

using namespace asio::ip;

inline asio::io_context g_IoContext;

constexpr int NET_CRYPT_KEY_LENGTH = 16;
constexpr int NET_COMPRESSION_STACKBUF_SIZE = 4096;

class Client
{
	//For handling netmessages
	class CNetMessageHandler
	{

	public:
		static bool HandleNetMessageFromBuffer(Client* client, bf_read& buf, int type)
		{
			switch (type)
			{
			case net_Tick:
			{
				CNETMsg_Tick_t tick(0, 0.f, 0.f, 0.f);
				tick.ReadFromBuffer(buf);
				
				std::cout << "Receive NetMessage CNETMsg_Tick_t\n";
				std::cout << std::format("Server tick:{}, host_computationtime:{}, host_computationtime_std_deviation: {}, host_framestarttime_std_deviation: {}\n", 
					tick.tick(), tick.host_computationtime(), tick.host_computationtime_std_deviation(), tick.host_framestarttime_std_deviation());
				return true;
			}
			case net_StringCmd:
			{
				CNETMsg_StringCmd_t command("");
				command.ReadFromBuffer(buf);
				std::cout << "Receive NetMessage CNETMsg_StringCmd_t\n";
				return true;
			}
			case net_PlayerAvatarData:
			{
				CNETMsg_PlayerAvatarData_t avatar;
				avatar.ReadFromBuffer(buf);
				std::cout << "Receive NetMessage CNETMsg_PlayerAvatarData_t\n";
				return true;
			}
			case net_SignonState:
			{
				CNETMsg_SignonState_t signonState(SIGNONSTATE_NONE, -1);
				signonState.ReadFromBuffer(buf);
				std::cout << "Receive NetMessage CNETMsg_SignonState_t\n";

				std::cout << std::format("signon_state: {}, spawn_count: {}\n", 
					signonState.signon_state(), signonState.spawn_count());

				//ack server with the same signonstate
				client->SendNetMessage(signonState);
				return true;
			}
			case net_SetConVar:
			{
				CNETMsg_SetConVar_t setConvar;
				setConvar.ReadFromBuffer(buf);
				std::cout << "Receive NetMessage CNETMsg_SetConVar_t\n\n";

				//We don't actually have convar and NetMsgGetCVarUsingDictionary was removed from the souce
				//so far we just simply print them
				for (int i = 0; i < setConvar.convars().cvars_size(); i++)
				{
					auto cvar = setConvar.convars().cvars(i);
					std::cout << std::format("{} {}\n", cvar.has_name() ? cvar.name() : "unknown cvar", cvar.value());
				}

				return true;
			}
			case net_NOP:
			{
				CNETMsg_NOP_t nop;
				nop.ReadFromBuffer(buf);

				if (client->WaitingForMoreFragment(0))
				{
					thread_local int ackFrag = 0;
					thread_local int numFrag = 0;

					dataFragments_t* data = client->GetCurrentFragmentData(0);

					//Data
					if (ackFrag != data->ackedFragments)
					{
						client->ReplyFragmentAck();
						ackFrag = data->ackedFragments;
					}

				}
				return true;
			}
			case net_Disconnect:
			{
				CNETMsg_Disconnect_t disconnect;
				disconnect.ReadFromBuffer(buf);
				std::cout << std::format("Disconnect from server : {}\n", disconnect.has_text() ? disconnect.text() : "unknown");
				return true;
			}
			case net_File:
			{
				CNETMsg_File_t file;
				file.ReadFromBuffer(buf);
				std::cout << "Receive NetMessage CNETMsg_File_t\n";
				return true;
			}
			case net_SplitScreenUser:
			{
				CNETMsg_SplitScreenUser_t splitScreenUser;
				splitScreenUser.ReadFromBuffer(buf);
				std::cout << "Receive NetMessage CNETMsg_SplitScreenUser_t\n";
				return true;
			}
			case svc_ServerInfo:
			{
				CSVCMsg_ServerInfo_t info;
				info.ReadFromBuffer(buf);
				std::cout << "Receive NetMessage CSVCMsg_ServerInfo_t\n";
				return true;
			}
			case svc_SendTable:

				break;
			case svc_ClassInfo:

				break;
			case svc_SetPause:

				break;
			case svc_CreateStringTable:
			{
				CSVCMsg_CreateStringTable_t createStringTable;
				createStringTable.ReadFromBuffer(buf); 
				std::cout << "Receive NetMessage CSVCMsg_CreateStringTable_t\n";

				std::cout << std::format("StringTable name: {}, max_entries: {}, user_data_size_bits: {}, flags: {}\n\n",
					createStringTable.name(), 
					createStringTable.max_entries(), 
					createStringTable.user_data_size_bits(),
					createStringTable.flags());

				return true;
			}

			case svc_UpdateStringTable:

				break;
			case svc_VoiceInit:

				break;
			case svc_VoiceData:

				break;
			case svc_Print:
			{
				CSVCMsg_Print_t print;
				print.ReadFromBuffer(buf);
				if (print.has_text())
				{
					std::cout << print.text() << "\n";
				}
				return true;
			}
				
			case svc_Sounds:

				break;
			case svc_SetView:

				break;
			case svc_FixAngle:

				break;
			case svc_CrosshairAngle:

				break;
			case svc_BSPDecal:

				break;
			case svc_SplitScreen:

				break;
			case svc_UserMessage:

				break;
			case svc_EntityMessage:

				break;
			case svc_GameEvent:

				break;
			case svc_PacketEntities:

				break;
			case svc_TempEntities:

				break;
			case svc_Prefetch:

				break;
			case svc_Menu:

				break;
			case svc_GameEventList:

				break;
			case svc_GetCvarValue:

				break;
			case svc_PaintmapData:

				break;
			case svc_CmdKeyValues:
			{
				CSVCMsg_CmdKeyValues_t kv;
				kv.ReadFromBuffer(buf);

				//Process kv here
				std::cout << "Receive NetMessage CSVCMsg_CmdKeyValues_t\n";
				return true;
			}
			case svc_EncryptedData:

				break;
			case svc_HltvReplay:

				break;
			case svc_Broadcast_Command:

				break;
			}

			return false;
		}
	};//class CNetMessageHandler

public:
	Client() :
		m_WriteBuf(m_Buf, sizeof(m_Buf)), m_ReadBuf(m_Buf, sizeof(m_Buf)), m_Datageam(m_DatagramBuf, sizeof(m_DatagramBuf))
	{
		for (int i = 0; i < MAX_STREAMS; i++)
		{
			m_ReceiveList[i].buffer = nullptr;
		}
	}
	
	[[nodiscard("Steam api must be confirmed working before running the client!")]]
	bool PrepareSteamAPI()
	{
		if (!SteamAPI_IsSteamRunning())
		{
			std::cout << "Steam is not running!\n";
			return false;
		}
		
		if (!SteamAPI_Init())
		{
			std::cout << "Cannot initialize SteamAPI\n";
			return false;
		}
		if (!SteamUser())
		{
			std::cout << "Cannot initialize ISteamUser interface\n";
			return false;
		}

		return true;
	}

	void BindServer(const char* ip, const char* nickname, const char* password, uint16_t port)
	{
		m_Ip = ip;
		m_NickName = nickname;
		m_PassWord = password;
		m_Port = port;
		
		asio::co_spawn(g_IoContext, ConnectToServer(), asio::detached);
	}

	inline void RunClient() { g_IoContext.run(); }

	void InitCommandInput()
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

protected:
	asio::awaitable<void> ConnectToServer()
	{
		std::cout << std::format("Connecting to server {}:{}, using nickname {}, using password {}\n", 
			m_Ip, m_Port, m_NickName, m_PassWord);
		
		udp::socket socket(g_IoContext, udp::endpoint(udp::v4(), m_Port));
		auto remote_endpoint = udp::endpoint(make_address(m_Ip), m_Port);
		
		auto [success, challenge, auth_proto_ver, connect_proto_ver] = co_await GetChallenge(socket, remote_endpoint, 0);
		if (!success)
		{
			std::cout << "Connection failed after challenge response!\n";
			co_return;
		}
		m_HostVersion = connect_proto_ver;

		if(!(co_await SendConnectPacket(socket, remote_endpoint, challenge, auth_proto_ver, connect_proto_ver)))
			co_return;
		
		co_await HandleIncomingPacket(socket, remote_endpoint);

		co_return;
	}

	asio::awaitable<bool> SendConnectPacket(
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
		m_WriteBuf.WriteString(m_NickName.c_str());
		m_WriteBuf.WriteString(m_PassWord.c_str());
		m_WriteBuf.WriteByte(0);
		m_WriteBuf.WriteOneBit(0); //low violence setting
		m_WriteBuf.WriteLongLong(0); //reservation cookie
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

			std::cout << std::format("Connection refused! - {}\n", err);
			co_return false;

		case S2C_CONNECTION:
			//Tell server that we are connected, ready to receive netmessages
			SetSignonState(SIGNONSTATE_CONNECTED, -1);
			co_return true;

		default:
			std::cout << std::format("Connection error! Got response header - {:#04x}\n", c);
			co_return false;
		}
		
	}

	asio::awaitable<std::tuple<bool, uint32_t, uint32_t, uint32_t>> GetChallenge(
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
		
		co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten() + len+1), remote_endpoint, asio::use_awaitable);

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
				std::cout << "You cannot connect to this CS:GO server because it is restricted to LAN connections only.\n";
				co_return failed_obj;
			}
			else if (StringHasPrefix(buf, "connect-matchmaking-only"))
			{
				std::cout << "You must use matchmaking to connect to this CS:GO server.\n";
				co_return failed_obj;
			}
			//else if... don't cover other circumstances for now
		}
		else
		{
			std::cout << "Corrupted challenge response!\n";
			co_return failed_obj;
		}

		auto connect_protocol_version = m_ReadBuf.ReadLong();
		m_ReadBuf.ReadString(buf, sizeof(buf)); //lobby name
		auto require_pw = m_ReadBuf.ReadByte();
		auto lobby_id = m_ReadBuf.ReadLongLong();
		auto dcFriendsReqd = m_ReadBuf.ReadByte() != 0;
		auto officialValveServer = m_ReadBuf.ReadByte() != 0;
		
		std::cout << std::format("Server using '{}' lobbies, requiring pw {}, lobby id {:#010x}\n", 
			buf, require_pw ? "yes" : "no", (uint64_t)lobby_id);

		std::cout << std::format("Get server challenge number : {:#010x}, auth prorocol: {:#04x}, server steam: {}, vac {}, dcFriendsReqd {}, officialValveServer {}\n",
			challenge, auth_protocol_version, server_steamid, vac ? "on" : "off", dcFriendsReqd, officialValveServer);

		co_return std::make_tuple<bool, uint32_t, uint32_t, uint32_t>(true, 
			(uint32_t)challenge, (uint32_t)auth_protocol_version, (uint32_t)connect_protocol_version);
	}

	asio::awaitable<void> HandleIncomingPacket(udp::socket& socket, udp::endpoint& remote_endpoint)
	{
		//Process all incoming packet here after we established connection.
		while (true)
		{
			HandleStringCommand();
			co_await SendDatagram(socket, remote_endpoint);

			co_await socket.async_wait(socket.wait_read, asio::use_awaitable);
			size_t n = co_await socket.async_receive_from(asio::buffer(m_Buf), remote_endpoint, asio::use_awaitable);

			//TO DO : Process all other packets here
			ResetReadBuffer();

			if (ReadBufferHeaderInt32() == -1)
			{
				m_ReadBuf.ReadLong(); //-1

				ProcessConnectionlessPacket();
				continue;
			}

			ProcessPacket(n);
		}

		co_return;
	}

	void HandleStringCommand()
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
	

	void ProcessPacket(int packetSize)
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
						std::cout << "Error reading packet! stream " << i << "\n";
						return; // error while reading fragments, drop whole packet
					}
				}
			}

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
			if (!ProcessMessages(msg, false))
			{
				return;	// disconnect or error
			}
		}
	}

	bool CheckReceivingList(int nList)
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

			if (!ProcessMessages(buffer, true)) // parse net message
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

	bool ReadSubChannelData(bf_read& buf, int stream)
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
				printf("Received fragment out of order: %i/%i, offset %i\n", startFragment, numFragments, offset );
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
		printf("Received fragments: offset %i start %i, num %i, length:%i\n", offset, startFragment, numFragments, length);
		printf("Total fragment needed: %i, received: %i\n", data->numFragments, data->ackedFragments);
		return true;
	}

	bool ProcessMessages(bf_read& buf, bool wasReliable)
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
				printf("Got unhandle message type %X\n", cmd);
			}
		}

		return true;
	}
	
	int ProcessPacketHeader(bf_read& msg)
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

		//if (usDataCheckSum != usCheckSum)
		//{
		//	std::cout << std::format("corrupted packet {} at {}, crc check failed\n", sequence, m_nInSequenceNr);
		//	return -1;
		//}

		int relState = msg.ReadByte();	// reliable state of 8 subchannels
		int nChoked = 0;	// read later if choked flag is set

		if (flags & PACKET_FLAG_CHOKED)
			nChoked = msg.ReadByte();
		
		// discard stale or duplicated packets
		if (sequence <= m_nInSequenceNr)
		{
			if (sequence == m_nInSequenceNr)
			{
				std::cout << std::format("duplicate packet {} at {}\n", sequence, m_nInSequenceNr);
			}
			else
			{
				std::cout << std::format("out of order packet {} at {}\n", sequence, m_nInSequenceNr);
			}

			return -1;
		}

//
// dropped packets don't keep the message from being used
//
		m_PacketDrop = sequence - (m_nInSequenceNr + nChoked + 1);

		if (m_PacketDrop > 0)
		{
			std::cout << std::format("Dropped {} packets at {}\n", m_PacketDrop, sequence);
		}

		m_nInSequenceNr = sequence;
		m_nOutSequenceNrAck = sequence_ack;

		return flags;
	}

	void ProcessConnectionlessPacket()
	{
		char c = m_ReadBuf.ReadByte();
		std::cout << std::format("Get connectionless packet : {:#04x}\n", (uint8_t)c);
	}

	void SetSignonState(int state, int count)
	{
		CNETMsg_SignonState_t signonState(state, count);
		SendNetMessage(signonState);
	}

	void SendStringCommand(std::string& command)
	{
		CNETMsg_StringCmd_t stringCmd(command.c_str());
		SendNetMessage(stringCmd);

		if (command == "disconnect")
		{
			CNETMsg_Disconnect_t disconnect;
			disconnect.set_text("Disconnect.");
			SendNetMessage(disconnect);
		}
	}
	
	asio::awaitable<void> SendDatagram(udp::socket& socket, udp::endpoint& remote_endpoint)
	{
		if (m_Datageam.GetNumBytesWritten() <= 0)
			co_return;

		ResetWriteBuffer();
		m_WriteBuf.WriteLong(m_nOutSequenceNr++); //out sequence
		m_WriteBuf.WriteLong(m_nInSequenceNr); //In sequence
		bf_write flagsPos = m_WriteBuf;
		m_WriteBuf.WriteByte(0); //Flag place holder
		m_WriteBuf.WriteShort(0);//checksum place holder
		
		int nCheckSumStart = m_WriteBuf.GetNumBytesWritten();

		if (m_Datageam.GetNumBytesWritten() == 4 && *(int*)m_Datageam.GetData() == 0)
		{
			//Hack to write direct data 00 D7 3C FF 00 00 00 00
			m_WriteBuf.WriteByte(0xFF);
		}
		else
		{
			m_WriteBuf.WriteByte(0); //InReliableState
		}

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

	asio::awaitable<void> SendDirectDatagramBuffer(
		udp::socket& socket, 
		udp::endpoint& remote_endpoint,
		const char* data,
		size_t length)
	{
		ResetWriteBuffer();
		m_WriteBuf.WriteLong(m_nOutSequenceNr++); //out sequence
		m_WriteBuf.WriteLong(m_nInSequenceNr); //In sequence

		m_WriteBuf.WriteBytes(data, length);
		length = EncryptDatagram();
		co_await socket.async_send_to(asio::buffer(m_Buf, length), remote_endpoint, asio::use_awaitable);
	}

	void SendNetMessage(INetMessage& msg)
	{
		msg.WriteToBuffer(m_Datageam);
	}

	//Don't use this, it's dangerous, might break the rest of the packet!!!
	void SendDirectBuffer(const void* data, size_t length)
	{
		m_Datageam.WriteBytes(data, length);
	}

	//can't use stackalloc in a awaiter function, extract it out
	int EncryptDatagram()
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

private:

	void UncompressFragments(dataFragments_t* data)
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

	bool BufferToBufferDecompress(char* dest, unsigned int& destLen, char* source, unsigned int sourceLen)
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

	inline bool WaitingForMoreFragment(int stream)
	{
		dataFragments_t* data = &m_ReceiveList[stream]; 

		if (data->ackedFragments < data->numFragments)
			return true;

		return false;
	}

	inline dataFragments_t* GetCurrentFragmentData(int stream)
	{
		return &m_ReceiveList[stream];
	}

	inline void ReplyFragmentAck()
	{
		thread_local bool flag = true;

		if (flag)
		{
			SendDirectBuffer("\x00\x00\x00\x00", 4);
		}
		else
		{
			CNETMsg_NOP_t nop;
			SendNetMessage(nop);
		}
		flag = flag ? false : true;
	}

	inline byte* GetEncryptionKey()
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

	inline unsigned short BufferToShortChecksum(const void* pvData, size_t nLength)
	{
		CRC32_t crc = CRC32_ProcessSingleBuffer(pvData, nLength);

		unsigned short lowpart = (crc & 0xffff);
		unsigned short highpart = ((crc >> 16) & 0xffff);

		return (unsigned short)(lowpart ^ highpart);
	}

	inline void ResetWriteBuffer()
	{
		m_WriteBuf.Reset();
	}

	inline void ResetReadBuffer()
	{
		m_ReadBuf.Seek(0);
	}

	inline int ReadBufferHeaderInt32()
	{
		return *(int*)m_Buf;
	}

	inline void PrintRecvBuffer(size_t bytes)
	{
		for (size_t i = 0; i < bytes; i++)
		{
			printf("%02X ", m_Buf[i] & 0xFF);
		}
		printf("\n\n");
	}

private:
	char m_Buf[NET_MAX_PAYLOAD];
	char m_DatagramBuf[10240];
	bf_write m_WriteBuf;
	bf_write m_Datageam;
	bf_read m_ReadBuf;


	//commandline
	std::mutex m_CommandVecLock;
	std::vector<std::string> m_VecCommand;

	//host information
	int m_HostVersion;

	//Net channel
	int m_nInSequenceNr = 0;
	int m_nOutSequenceNr = 1;
	int m_nOutSequenceNrAck = 0;
	int m_PacketDrop = 0;
	dataFragments_t	 m_ReceiveList[MAX_STREAMS]; // receive buffers for streams

	//server info
	std::string m_Ip;
	std::string m_NickName;
	std::string m_PassWord;
	uint16_t m_Port;
};


#endif // !__TINY_CSGO_CLIENT_HEADER__
