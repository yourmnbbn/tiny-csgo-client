#ifndef __TINY_CSGO_CLIENT_HEADER__
#define __TINY_CSGO_CLIENT_HEADER__

#ifdef _WIN32
#pragma once
#endif

#include <iostream>
#include <string>
#include <format>
#include <asio.hpp>

//from hl2sdk-csgo
#include <bitbuf.h>
#include <steam_api.h>

//from cstrike15_src
#include "proto_oob.h"	

using namespace asio::ip;

inline asio::io_context g_IoContext;

class Client
{
public:
	Client() :
		m_WriteBuf(m_Buf, sizeof(m_Buf)), m_ReadBuf(m_Buf, sizeof(m_Buf))
	{
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

protected:
	asio::awaitable<void> ConnectToServer()
	{
		std::cout << std::format("Connecting to server {}:{}, using nickname {}, using password {}\n", 
			m_Ip, m_Port, m_NickName, m_PassWord);
		
		udp::socket socket(g_IoContext, udp::endpoint(udp::v4(), m_Port));
		auto remote_endpoint = udp::endpoint(make_address(m_Ip), m_Port);
		
		auto [success, challenge, auth_proto_ver, connect_proto_ver] = co_await GetChallenge(socket, remote_endpoint);
		if (!success)
		{
			std::cout << "Can't get challenge from target server!\n";
			co_return;
		}

		if(!(co_await SendConnectPacket(socket, remote_endpoint, challenge, auth_proto_ver, connect_proto_ver)))
			co_return;

		co_await HandleIncomingPacket(socket, remote_endpoint);

		co_return;
	}
	
	asio::awaitable<bool> SendConnectPacket(udp::socket& socket, 
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

		constexpr auto STEAM_KEYSIZE = 2048;
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
			co_return true;

		default:
			std::cout << std::format("Connection error! Got response header - {:#04x}\n", c);
			co_return false;
		}
		
	}

	asio::awaitable<std::tuple<bool, uint32_t, uint32_t, uint32_t>> GetChallenge(udp::socket& socket, udp::endpoint& remote_endpoint)
	{
		//Write request challenge packet
		ResetWriteBuffer();
		m_WriteBuf.WriteLong(-1);
		m_WriteBuf.WriteByte(A2S_GETCHALLENGE);
		m_WriteBuf.WriteString("connect0x00000000");

		co_await socket.async_send_to(asio::buffer(m_Buf, m_WriteBuf.GetNumBytesWritten()), remote_endpoint, asio::use_awaitable);

		//Wait for challenge response, TO DO: add timeout support
		co_await socket.async_wait(socket.wait_read, asio::use_awaitable);
		co_await socket.async_receive_from(asio::buffer(m_Buf), remote_endpoint, asio::use_awaitable);
		
		//Read information from buffer
		ResetReadBuffer();
		if (m_ReadBuf.ReadLong() != -1 || m_ReadBuf.ReadByte() != S2C_CHALLENGE)
			co_return std::make_tuple<bool, uint32_t, uint32_t, uint32_t>(false, 0, 0, 0);
		
		auto challenge = m_ReadBuf.ReadLong();
		auto auth_protocol_version = m_ReadBuf.ReadLong();
		auto encrypt_key = m_ReadBuf.ReadShort();
		auto server_steamid = m_ReadBuf.ReadLongLong();
		auto vac = m_ReadBuf.ReadByte();

		char buf[48];
		m_ReadBuf.ReadString(buf, sizeof(buf)); //always will be "connect-retry" ?
		auto connect_protocol_version = m_ReadBuf.ReadLong();
		m_ReadBuf.ReadString(buf, sizeof(buf)); //lobby name
		auto require_pw = m_ReadBuf.ReadByte();
		auto lobby_id = m_ReadBuf.ReadLongLong();
		
		std::cout << std::format("Server using '{}' lobbies, requiring pw {}, lobby id {:#010x}\n", 
			buf, require_pw ? "yes" : "no", (uint64_t)lobby_id);

		std::cout << std::format("Get server challenge number : {:#010x}, auth prorocol: {:#04x}, server steam: {}, vac {}\n",
			challenge, auth_protocol_version, server_steamid, vac ? "on" : "off");

		co_return std::make_tuple<bool, uint32_t, uint32_t, uint32_t>(true, 
			(uint32_t)challenge, (uint32_t)auth_protocol_version, (uint32_t)connect_protocol_version);
	}

	asio::awaitable<void> HandleIncomingPacket(udp::socket& socket, udp::endpoint& remote_endpoint)
	{
		//Process all incoming packet here after we established connection.
		while (true)
		{
			co_await socket.async_wait(socket.wait_read, asio::use_awaitable);
			size_t n = co_await socket.async_receive_from(asio::buffer(m_Buf), remote_endpoint, asio::use_awaitable);

			//TO DO : Process all other packets here
			ResetReadBuffer();
			PrintRecvBuffer(n);
		}

		co_return;
	}

private:

	inline void ResetWriteBuffer()
	{
		memset(m_Buf, 0, m_WriteBuf.GetNumBytesWritten());
		m_WriteBuf.Reset();
	}

	inline void ResetReadBuffer()
	{
		m_ReadBuf.Seek(0);
	}

	inline void PrintRecvBuffer(size_t bytes)
	{
		for (size_t i = 0; i < bytes; i++)
		{
			printf("%02X ", m_Buf[i] & 0xFF);
		}
		printf("\n");
	}

private:
	char m_Buf[1024];
	bf_write m_WriteBuf;
	bf_read m_ReadBuf;

	//server info
	std::string m_Ip;
	std::string m_NickName;
	std::string m_PassWord;
	uint16_t m_Port;
};

#endif // !__TINY_CSGO_CLIENT_HEADER__
