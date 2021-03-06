#ifndef NETMESSAGES_H
#define NETMESSAGES_H

#ifdef _WIN32
#pragma once
#pragma warning(disable : 4100)	// unreferenced formal parameter
#pragma comment(linker, "/STACK:9216000") // For NET_MAX_PAYLOAD update
#endif

#include <inetmessage.h>
#include <netmessages.pb.h>

//The interface in the hl2sdk is outdated, this is from the cstrike15_src
#include "inetchannelinfo.h"

//Updated from engine binary(13835)
#define	NET_MAX_PAYLOAD			0xFFFFB 

template< int msgType, typename PB_OBJECT_TYPE, int groupType = INetChannelInfo::GENERIC, bool bReliable = true >
class CNetMessagePB : public INetMessage, public PB_OBJECT_TYPE
{
public:
	typedef CNetMessagePB< msgType, PB_OBJECT_TYPE, groupType, bReliable > MyType_t;
	typedef PB_OBJECT_TYPE PBType_t;
	static const int sk_Type = msgType;

	CNetMessagePB() :
		m_bReliable(bReliable)
	{
	}

	virtual ~CNetMessagePB()
	{
	}

	virtual bool ReadFromBuffer(bf_read& buffer)
	{
		int size = buffer.ReadVarInt32();
		if (size < 0 || size > NET_MAX_PAYLOAD)
		{
			return false;
		}

		// Check its valid
		if (size > buffer.GetNumBytesLeft())
		{
			return false;
		}

		// If the read buffer is byte aligned, we can parse right out of it
		if ((buffer.GetNumBitsRead() % 8) == 0)
		{
			bool parseResult = PB_OBJECT_TYPE::ParseFromArray(buffer.GetBasePointer() + buffer.GetNumBytesRead(), size);
			buffer.SeekRelative(size * 8);
			return parseResult;
		}

		// otherwise we have to do a temp allocation so we can read it all shifted
#ifdef NET_SHOW_UNALIGNED_MSGS
		printf("Warning: unaligned read of protobuf message %s (%d bytes)\n", PB_OBJECT_TYPE::GetTypeName().c_str(), size);
#endif

		void* parseBuffer = stackalloc(size);
		if (!buffer.ReadBytes(parseBuffer, size))
		{
			return false;
		}

		if (!PB_OBJECT_TYPE::ParseFromArray(parseBuffer, size))
		{
			return false;
		}

		return true;
	}

	virtual bool WriteToBuffer(bf_write& buffer) 
	{
		if (!PB_OBJECT_TYPE::IsInitialized())
		{
			printf("WriteToBuffer Message %s is not initialized! Probably missing required fields!\n", PB_OBJECT_TYPE::GetTypeName().c_str());
		}

		int size = PB_OBJECT_TYPE::ByteSize();

		// If the write is byte aligned we can go direct
		if ((buffer.GetNumBitsWritten() % 8) == 0)
		{
			int sizeWithHeader = size + 1 + buffer.ByteSizeVarInt32(GetType()) + buffer.ByteSizeVarInt32(size);

			if (buffer.GetNumBytesLeft() >= sizeWithHeader)
			{
				buffer.WriteVarInt32(GetType());
				buffer.WriteVarInt32(size);

				if (!PB_OBJECT_TYPE::SerializeWithCachedSizesToArray((google::protobuf::uint8*)buffer.GetData() + buffer.GetNumBytesWritten()))
				{
					return false;
				}

				// Tell the buffer we just splatted into it
				buffer.SeekToBit(buffer.GetNumBitsWritten() + (size * 8));
				return true;
			}

			// Won't fit
			return false;
		}

		// otherwise we have to do a temp allocation so we can write it all shifted

		void* serializeBuffer = stackalloc(size);

		if (!PB_OBJECT_TYPE::SerializeWithCachedSizesToArray((google::protobuf::uint8*)serializeBuffer))
		{
			return false;
		}

		buffer.WriteVarInt32(GetType());
		buffer.WriteVarInt32(size);
		return buffer.WriteBytes(serializeBuffer, size);
	}

	virtual const char* ToString() const
	{
		m_toString = PB_OBJECT_TYPE::DebugString();
		return m_toString.c_str();
	}

	virtual int GetType() const
	{
		return msgType;
	}

	virtual size_t GetSize() const
	{
		return sizeof(*this);
	}

	virtual const char* GetName() const
	{
		if (s_typeName.empty())
		{
			s_typeName = PB_OBJECT_TYPE::GetTypeName();
		}
		return s_typeName.c_str();
	}

	virtual int GetGroup() const
	{
		return groupType;
	}

	virtual void SetReliable(bool state)
	{
		m_bReliable = state;
	}

	virtual bool IsReliable() const
	{
		return m_bReliable;
	}

	virtual INetMessage* Clone() const
	{
		MyType_t* pClone = new MyType_t;
		pClone->CopyFrom(*this);
		pClone->m_bReliable = m_bReliable;

		return pClone;
	}

	virtual void SetNetChannel(INetChannel* netchan){}
	virtual bool Process(void) { return true; }
	virtual INetChannel* GetNetChannel(void) const { return nullptr; }

protected:
	bool m_bReliable; // true if message should be sent reliable
	mutable std::string	m_toString; // cached copy of ToString()
	static std::string s_typeName;
};

template< int msgType, typename PB_OBJECT_TYPE, int groupType, bool bReliable >
std::string CNetMessagePB< msgType, PB_OBJECT_TYPE, groupType, bReliable >::s_typeName;


class CNETMsg_Tick_t : public CNetMessagePB< net_Tick, CNETMsg_Tick >
{
public:
	static float FrametimeToFloat(uint32 frametime) { return (float)frametime / 1000000.0f; }

	CNETMsg_Tick_t(int tick, float host_computationtime, float host_computationtime_stddeviation, float host_framestarttime_std_deviation)
	{
		SetReliable(false);
		set_tick(tick);
		set_host_computationtime(MIN((uint32)(1000000.0 * host_computationtime), 1000000u));
		set_host_computationtime_std_deviation(MIN((uint32)(1000000.0 * host_computationtime_stddeviation), 1000000u));
		set_host_framestarttime_std_deviation(MIN((uint32)(1000000.0 * host_framestarttime_std_deviation), 1000000u));
	}
};

class CNETMsg_StringCmd_t : public CNetMessagePB< net_StringCmd, CNETMsg_StringCmd, INetChannelInfo::STRINGCMD >
{
public:
	CNETMsg_StringCmd_t(const char* command)
	{
		set_command(command);
	}
};

class CNETMsg_PlayerAvatarData_t : public CNetMessagePB< net_PlayerAvatarData, CNETMsg_PlayerAvatarData, INetChannelInfo::PAINTMAP >
{
	// 12 KB player avatar 64x64 rgb only no alpha
	// WARNING-WARNING-WARNING
	// This message is extremely large for our net channels
	// and must be pumped through special fragmented waiting list
	// via chunk-based ack mechanism!
	// See: INetChannel::EnqueueVeryLargeAsyncTransfer
	// WARNING-WARNING-WARNING
public:
	CNETMsg_PlayerAvatarData_t() {}
	CNETMsg_PlayerAvatarData_t(uint32 unAccountID, void const* pvData, uint32 cbData)
	{
		set_accountid(unAccountID);
		set_rgb((const char*)pvData, cbData);
	}
};

class CNETMsg_SignonState_t : public CNetMessagePB< net_SignonState, CNETMsg_SignonState, INetChannelInfo::SIGNON >
{
public:
	CNETMsg_SignonState_t(int state, int spawncount)
	{
		set_signon_state(state);
		set_spawn_count(spawncount);
		set_num_server_players(0);
	}
};

//As long as we set the correct cvar name, there is no need to use dictionary
inline void NetMsgSetCVarUsingDictionary(CMsg_CVars::CVar* convar, char const* name, char const* value)
{
	convar->set_value(value);
	convar->set_name(name);
}

class CNETMsg_SetConVar_t : public CNetMessagePB< net_SetConVar, CNETMsg_SetConVar, INetChannelInfo::STRINGCMD >
{
public:
	CNETMsg_SetConVar_t() {}
	CNETMsg_SetConVar_t(const char* name, const char* value)
	{
		AddToTail(name, value);
	}
	void AddToTail(const char* name, const char* value)
	{
		NetMsgSetCVarUsingDictionary(mutable_convars()->add_cvars(), name, value);
	}
};

//This thing is easy to find in the binary, why hidding this valve?
inline const char* NetMsgGetCVarUsingDictionary(CMsg_CVars::CVar const& convar)
{
	if (convar.has_name())
		return convar.name().c_str();
	switch (convar.dictionary_name())
	{
	case 0: return "";
	case 1: return "accountid";
	case 2: return "password";
	case 3: return "cl_use_opens_buy_menu";
	case 4: return "tv_nochat";
	case 5: return "cl_clanid";
	case 6: return "name";
	case 7: return "cl_interp_ratio";
	case 8: return "cl_predict";
	case 9: return "cl_updaterate";
	case 10: return "cl_session";
	case 11: return "voice_loopback";
	case 12: return "cl_lagcompensation";
	case 13: return "cl_color";
	case 14: return "cl_cmdrate";
	case 15: return "net_maxroutable";
	case 16: return "rate";
	case 17: return "cl_predictweapons";
	case 18: return "cl_autohelp";
	case 19: return "cl_interp";
	case 20: return "cl_autowepswitch";
	case 21: return "cl_spec_mode";
	case 22: return "tv_relay";
	case 23: return "hltv_slots";
	case 24: return "hltv_clients";
	case 25: return "hltv_addr";
	case 26: return "hltv_proxies";
	case 27: return "sv_bot_difficulty_kbm";
	case 28: return "hltv_sdr";
	case 29: return "steamworks_sessionid_client";
	case 30: return "sdr_routing";
	default:
		printf("Invalid dictionary entry for cvar # %d\n", convar.dictionary_name());
		return "undefined";
	}
}
class CNETMsg_Disconnect_t : public CNetMessagePB< net_Disconnect, CNETMsg_Disconnect >
{
public:
	CNETMsg_Disconnect_t(){}
	CNETMsg_Disconnect_t(const char* reason)
	{
		set_text(reason);
	}
};

typedef CNetMessagePB< net_NOP, CNETMsg_NOP >											CNETMsg_NOP_t;
typedef CNetMessagePB< net_File, CNETMsg_File >											CNETMsg_File_t;
typedef CNetMessagePB< net_SplitScreenUser, CNETMsg_SplitScreenUser >					CNETMsg_SplitScreenUser_t;

///////////////////////////////////////////////////////////////////////////////////////
// Client messages: Sent from the client to the server
///////////////////////////////////////////////////////////////////////////////////////

typedef CNetMessagePB< clc_SplitPlayerConnect, CCLCMsg_SplitPlayerConnect >							CCLCMsg_SplitPlayerConnect_t;
typedef CNetMessagePB< clc_Move, CCLCMsg_Move, INetChannelInfo::MOVE, false >						CCLCMsg_Move_t;
typedef CNetMessagePB< clc_ClientInfo, CCLCMsg_ClientInfo > 										CCLCMsg_ClientInfo_t;
typedef CNetMessagePB< clc_VoiceData, CCLCMsg_VoiceData, INetChannelInfo::VOICE, false >			CCLCMsg_VoiceData_t;
typedef CNetMessagePB< clc_BaselineAck, CCLCMsg_BaselineAck >										CCLCMsg_BaselineAck_t;
typedef CNetMessagePB< clc_ListenEvents, CCLCMsg_ListenEvents >										CCLCMsg_ListenEvents_t;
typedef CNetMessagePB< clc_RespondCvarValue, CCLCMsg_RespondCvarValue >								CCLCMsg_RespondCvarValue_t;
typedef CNetMessagePB< clc_LoadingProgress, CCLCMsg_LoadingProgress >								CCLCMsg_LoadingProgress_t;
typedef CNetMessagePB< clc_CmdKeyValues, CCLCMsg_CmdKeyValues >										CCLCMsg_CmdKeyValues_t;
typedef CNetMessagePB< clc_HltvReplay, CCLCMsg_HltvReplay >											CCLCMsg_HltvReplay_t;

class CCLCMsg_FileCRCCheck_t : public CNetMessagePB< clc_FileCRCCheck, CCLCMsg_FileCRCCheck >
{
public:
	// Warning: These routines may use the va() function...
	static void SetPath(CCLCMsg_FileCRCCheck& msg, const char* path);
	static const char* GetPath(const CCLCMsg_FileCRCCheck& msg);
	static void SetFileName(CCLCMsg_FileCRCCheck& msg, const char* fileName);
	static const char* GetFileName(const CCLCMsg_FileCRCCheck& msg);
};

///////////////////////////////////////////////////////////////////////////////////////
// Server messages: Sent from the server to the client
///////////////////////////////////////////////////////////////////////////////////////

typedef CNetMessagePB< svc_ServerInfo, CSVCMsg_ServerInfo, INetChannelInfo::SIGNON >					CSVCMsg_ServerInfo_t;
typedef CNetMessagePB< svc_ClassInfo, CSVCMsg_ClassInfo, INetChannelInfo::SIGNON >						CSVCMsg_ClassInfo_t;
typedef CNetMessagePB< svc_SendTable, CSVCMsg_SendTable, INetChannelInfo::SIGNON >					    CSVCMsg_SendTable_t;
typedef CNetMessagePB< svc_Print, CSVCMsg_Print, INetChannelInfo::GENERIC, false >						CSVCMsg_Print_t;
typedef CNetMessagePB< svc_SetPause, CSVCMsg_SetPause >													CSVCMsg_SetPause_t;
typedef CNetMessagePB< svc_SetView, CSVCMsg_SetView >												    CSVCMsg_SetView_t;
typedef CNetMessagePB< svc_CreateStringTable, CSVCMsg_CreateStringTable, INetChannelInfo::SIGNON >	    CSVCMsg_CreateStringTable_t;
typedef CNetMessagePB< svc_UpdateStringTable, CSVCMsg_UpdateStringTable, INetChannelInfo::STRINGTABLE >	CSVCMsg_UpdateStringTable_t;
typedef CNetMessagePB< svc_VoiceInit, CSVCMsg_VoiceInit, INetChannelInfo::SIGNON >						CSVCMsg_VoiceInit_t;
typedef CNetMessagePB< svc_VoiceData, CSVCMsg_VoiceData, INetChannelInfo::VOICE, false >				CSVCMsg_VoiceData_t;
typedef CNetMessagePB< svc_FixAngle, CSVCMsg_FixAngle, INetChannelInfo::GENERIC, false >			    CSVCMsg_FixAngle_t;
typedef CNetMessagePB< svc_Prefetch, CSVCMsg_Prefetch, INetChannelInfo::SOUNDS >					    CSVCMsg_Prefetch_t;
typedef CNetMessagePB< svc_CrosshairAngle, CSVCMsg_CrosshairAngle >									    CSVCMsg_CrosshairAngle_t;
typedef CNetMessagePB< svc_BSPDecal, CSVCMsg_BSPDecal >												    CSVCMsg_BSPDecal_t;
typedef CNetMessagePB< svc_SplitScreen, CSVCMsg_SplitScreen >										    CSVCMsg_SplitScreen_t;
typedef CNetMessagePB< svc_GetCvarValue, CSVCMsg_GetCvarValue >										    CSVCMsg_GetCvarValue_t;
typedef CNetMessagePB< svc_Menu, CSVCMsg_Menu, INetChannelInfo::GENERIC, false >					    CSVCMsg_Menu_t;
typedef CNetMessagePB< svc_UserMessage, CSVCMsg_UserMessage, INetChannelInfo::USERMESSAGES, false >		CSVCMsg_UserMessage_t;
typedef CNetMessagePB< svc_PaintmapData, CSVCMsg_PaintmapData, INetChannelInfo::PAINTMAP >			    CSVCMsg_PaintmapData_t;
typedef CNetMessagePB< svc_GameEvent, CSVCMsg_GameEvent, INetChannelInfo::EVENTS >					    CSVCMsg_GameEvent_t;
typedef CNetMessagePB< svc_GameEventList, CSVCMsg_GameEventList >									    CSVCMsg_GameEventList_t;
typedef CNetMessagePB< svc_TempEntities, CSVCMsg_TempEntities, INetChannelInfo::TEMPENTS, false >	    CSVCMsg_TempEntities_t;
typedef CNetMessagePB< svc_PacketEntities, CSVCMsg_PacketEntities, INetChannelInfo::ENTITIES >		    CSVCMsg_PacketEntities_t;
typedef CNetMessagePB< svc_Sounds, CSVCMsg_Sounds, INetChannelInfo::SOUNDS >							CSVCMsg_Sounds_t;
typedef CNetMessagePB< svc_EntityMessage, CSVCMsg_EntityMsg, INetChannelInfo::ENTMESSAGES, false >		CSVCMsg_EntityMsg_t;
typedef CNetMessagePB< svc_CmdKeyValues, CSVCMsg_CmdKeyValues >											CSVCMsg_CmdKeyValues_t;
typedef CNetMessagePB< svc_EncryptedData, CSVCMsg_EncryptedData, INetChannelInfo::ENCRYPTED >			CSVCMsg_EncryptedData_t;
typedef CNetMessagePB< svc_HltvReplay, CSVCMsg_HltvReplay, INetChannelInfo::ENTITIES >					CSVCMsg_HltvReplay_t;
typedef CNetMessagePB< svc_Broadcast_Command, CSVCMsg_Broadcast_Command, INetChannelInfo::STRINGCMD >	CSVCMsg_Broadcast_Command_t;


#endif	// !NETMESSAGES_H
