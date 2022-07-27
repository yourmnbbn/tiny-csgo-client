#ifndef __TINY_CSGO_CLIENT_USERMSGHANDLER_HPP__
#define __TINY_CSGO_CLIENT_USERMSGHANDLER_HPP__

#ifdef _WIN32
#pragma once
#endif

#include <ranges>
#include "cstrike15_usermessages.pb.h"
#include "../common/bitbuf.h"

class CUserMsgHandler
{
public:
	static bool HandleUserMessage(int msgType, const void* data, size_t length);

private:
	template<typename MsgObjType>
	static MsgObjType usermsg_cast(const void* data, size_t length);
	static const std::string& RemoveColorCode(const std::string& message);
};

template<typename MsgObjType>
MsgObjType CUserMsgHandler::usermsg_cast(const void* data, size_t length)
{
	MsgObjType obj;
	obj.ParseFromArray(data, length);
	return obj;
}

const std::string& CUserMsgHandler::RemoveColorCode(const std::string& message)
{
	auto [first, end] = std::ranges::remove_if(const_cast<std::string&>(message), [](char c) {return c > 0 && c <= 0x0F; });
	const_cast<std::string&>(message).erase(first, end);
	return message;
}

bool CUserMsgHandler::HandleUserMessage(int msgType, const void* data, size_t length)
{
	//So far we just handle some of the common seen usermessages in the tiny client
	switch (msgType)
	{
	case CS_UM_VGUIMenu:
	{
		auto guiMenu = usermsg_cast<CCSUsrMsg_VGUIMenu>(data, length);
		printf("Receive  CCSUsrMsg_VGUIMenu, menu name:%s \n", guiMenu.name().c_str());
		for (int i = 0; i < guiMenu.subkeys().size(); i++)
		{
			auto subKey = guiMenu.subkeys().Get(i);
			printf("Subkey<%i>: %s\n", i, subKey.name().c_str());
		}
		return true;
	}
	case CS_UM_SayText:
	{
		auto sayText = usermsg_cast<CCSUsrMsg_SayText>(data, length);
		int client = sayText.ent_idx();
		printf("SayText: <Client %d> %s\n", client, RemoveColorCode(sayText.text()).c_str());
		return true;
	}
	case CS_UM_SayText2:
	{
		auto sayText2 = usermsg_cast<CCSUsrMsg_SayText2>(data, length);
		int client = sayText2.ent_idx();
		printf("SayText2<type: %s> Playere: %s<index %d> : %s\n", 
			RemoveColorCode(sayText2.msg_name()).c_str(), RemoveColorCode(sayText2.params(0)).c_str(), 
			client, RemoveColorCode(sayText2.params(1)).c_str());
		auto var = sayText2.params(0);
		
		return true;
	}
	case CS_UM_TextMsg:
	{
		auto textMsg = usermsg_cast<CCSUsrMsg_TextMsg>(data, length);
		for (int i = 0; i < 5; ++i)
		{
			if(textMsg.params(i).c_str()[0])
				printf("%s\n", RemoveColorCode(textMsg.params(i)).c_str());
		}
		return true;
	}
	case CS_UM_ShowMenu: //For handling sourcemod menu
	{
		auto menu = usermsg_cast<CCSUsrMsg_ShowMenu>(data, length);
		int displayTime = menu.display_time();
		int bitsValidSlots = menu.bits_valid_slots();
		
		if (bitsValidSlots)
		{
			printf("ShowMenu<time: %ds> \n%s\n", displayTime, menu.menu_string().c_str());
		}
		return true;
	}
	case CS_UM_MatchEndConditions:
	{
		auto matchEndCondEvent = usermsg_cast<CCSUsrMsg_MatchEndConditions>(data, length);
		printf("Event match_end_conditions:\n\tfrags: %d\n\tmax_rounds: %d\n\twin_rounds: %d\n\ttime: %d\n\n",
			(int)matchEndCondEvent.fraglimit(), (int)matchEndCondEvent.mp_maxrounds(),
			(int)matchEndCondEvent.mp_winlimit(), (int)matchEndCondEvent.mp_timelimit());
		return true;
	}
	case CS_UM_HintText:
	{
		//hint is sometimes annoying in our client
		//auto hintText = usermsg_cast<CCSUsrMsg_HintText>(data, length);
		//printf("Hint text: %s", hintText.text().c_str());
		return true;
	}
	case CS_UM_StopSpectatorMode:
	case CS_UM_KillCam:
	case CS_UM_Fade:
	case CS_UM_Rumble:
	case CS_UM_ResetHud:
	case CS_UM_Train:
	case CS_UM_RequestState:
	case CS_UM_ProcessSpottedEntityUpdate:
		//Do nothing
		return true;
	default: return false;
	}
}

#endif // !__TINY_CSGO_CLIENT_USERMSGHANDLER_HPP__
