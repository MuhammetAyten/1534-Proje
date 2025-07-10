#include "stdafx.h"
#include "DBAgent.h"

using std::string;
using std::vector;

void CGameServerDlg::SendEventRemainingTime(bool bSendAll, CUser *pUser, uint8 ZoneID)
{
	Packet result(WIZ_BIFROST,uint8(BIFROST_EVENT));
	uint16 nRemainingTime = 0;

	if (ZoneID == ZONE_BATTLE4 || ZoneID == ZONE_BATTLE5)
		nRemainingTime = m_byBattleRemainingTime / 2;
	else if (ZoneID == ZONE_BIFROST || ZoneID ==  ZONE_RONARK_LAND)
		nRemainingTime = m_sBifrostRemainingTime;
	else if (ZoneID == ZONE_BORDER_DEFENSE_WAR || ZoneID == ZONE_JURAID_MOUNTAIN)
		nRemainingTime = g_pMain->pTempleEvent.m_eEventTime;

	result << nRemainingTime;

	if (pUser)
		pUser->Send(&result);
	else if (bSendAll)
	{
		if (ZoneID == ZONE_BATTLE4)
			Send_All(&result, nullptr, 0, ZONE_BATTLE4);
		else if (ZoneID == ZONE_BATTLE5)
			Send_All(&result, nullptr, 0, ZONE_BATTLE5);
		else if (ZoneID == ZONE_BORDER_DEFENSE_WAR || ZONE_JURAID_MOUNTAIN)
		{
			if (ZoneID == ZONE_BORDER_DEFENSE_WAR)
				Send_All(&result, nullptr, 0, ZONE_BORDER_DEFENSE_WAR, true);
			else
				Send_All(&result, nullptr, 0, ZONE_JURAID_MOUNTAIN, true);
		}
		else
		{
			Send_All(&result, nullptr, 0, ZONE_RONARK_LAND);
			Send_All(&result, nullptr, 0, ZONE_BIFROST);
		}
	}
	if (ZoneID == ZONE_BATTLE4)
	{
		result.clear();
		result.Initialize(WIZ_MAP_EVENT);
		result << uint8(0) << uint8(7);

		for(int i = 0; i < 7; i++)
			result << m_sNereidsIslandMonuArray[i];

		if (pUser)
			pUser->Send(&result);

		result.clear();
		result.Initialize(WIZ_MAP_EVENT);
		result << uint8(2) << uint16(m_sElmoMonumentPoint) << uint16(m_sKarusMonumentPoint);

		if (pUser)
			pUser->Send(&result);
	}
}

void CGameServerDlg::EventRemainingTime(CUser *pUser, uint8 ZoneID)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_EVENT);
	uint32 nOpCode = 0;
	uint16 nRemainingTime = 0;

	if (ZoneID == ZONE_CHAOS_DUNGEON || ZoneID == ZONE_JURAID_MOUNTAIN || ZoneID == ZONE_BORDER_DEFENSE_WAR)
		nOpCode = 1;
	else if (ZoneID == ZONE_STONE1 || ZoneID ==  ZONE_STONE2 || ZoneID == ZONE_STONE3)
		nOpCode = 1;

	result << (uint8) nOpCode;

	if (pUser)
		pUser->Send(&result);

	result.clear();
	result.Initialize(WIZ_EVENT);
	result << uint8(BIFROST_EVENT_COUNT);

	if (ZoneID == ZONE_CHAOS_DUNGEON || ZoneID == ZONE_JURAID_MOUNTAIN || ZoneID == ZONE_BORDER_DEFENSE_WAR)
		nOpCode = uint32(0x05008504);
	else if (ZoneID == ZONE_STONE1 || ZoneID ==  ZONE_STONE2 || ZoneID == ZONE_STONE3)
		nOpCode = uint32(0x05008504);

	result << uint32(0) << uint32(0) << (uint32)nOpCode << uint8(0);

	if (pUser)
		pUser->Send(&result);

	result.clear();
	result.Initialize(WIZ_BIFROST);
	result << uint8(MONSTER_SQUARD);

	if (ZoneID == ZONE_CHAOS_DUNGEON || ZoneID == ZONE_JURAID_MOUNTAIN || ZoneID == ZONE_BORDER_DEFENSE_WAR)
		nRemainingTime = pTempleEvent.m_eEventTime;
	else if (ZoneID == ZONE_STONE1 || ZoneID ==  ZONE_STONE2 || ZoneID == ZONE_STONE3)
		nRemainingTime = pQuestEvent.m_eEventTime;

	if (ZoneID == ZONE_CHAOS_DUNGEON || ZoneID == ZONE_JURAID_MOUNTAIN || ZoneID == ZONE_BORDER_DEFENSE_WAR)
		result << nRemainingTime;
	else if (ZoneID == ZONE_STONE1 || ZoneID ==  ZONE_STONE2 || ZoneID == ZONE_STONE3)
		result << nRemainingTime;

	if (pUser)
		pUser->Send(&result);

	result.clear();
	result.Initialize(WIZ_SELECT_MSG);

	if (ZoneID == ZONE_CHAOS_DUNGEON || ZoneID == ZONE_JURAID_MOUNTAIN || ZoneID == ZONE_BORDER_DEFENSE_WAR)
		nRemainingTime = pTempleEvent.m_eEventTime;
	else if (ZoneID == ZONE_STONE1 || ZoneID ==  ZONE_STONE2 || ZoneID == ZONE_STONE3)
		nRemainingTime = pQuestEvent.m_eEventTime;

	if (ZoneID == ZONE_CHAOS_DUNGEON)
		result << uint16(0) << uint8(7) << uint64(0) << uint8(9) << uint16(0) << uint8(0) << (uint8) g_pMain->pTempleEvent.ActiveEvent << nRemainingTime << uint16(0x00);
	else if (ZoneID == ZONE_BORDER_DEFENSE_WAR)
		result << uint16(0) << uint8(7) << uint64(0) << uint8(8) << uint32(0) << (uint8) g_pMain->pTempleEvent.ActiveEvent << nRemainingTime << uint16(0x00);
	else if (ZoneID == ZONE_JURAID_MOUNTAIN)		
		result << uint16(0) << uint8(7) << uint64(0) << uint8(8) << uint32(0) << (uint8) g_pMain->pTempleEvent.ActiveEvent << nRemainingTime << uint16(0x00);
	else if (ZoneID == ZONE_STONE1 || ZoneID ==  ZONE_STONE2 || ZoneID == ZONE_STONE3)
		result << uint16(0) << uint8(9) << uint64(0) << uint8(9) << uint32(0) << (uint8) g_pMain->pTempleEvent.ActiveEvent << nRemainingTime << uint16(0x00);

	if (pUser)
		pUser->Send(&result);
}

void CUser::CastleSiegeWarProcess(CUser * pUser)
{
	if (pUser == nullptr 
		|| g_pMain->m_KnightsSiegeWarfareArray.GetSize() < 1)
		return;

	_KNIGHTS_SIEGE_WARFARE *pKnightSiegeWar = g_pMain->GetSiegeMasterKnightsPtr(Aktive);
	CKnights * pKnights = g_pMain->GetClanPtr(pUser->GetClanID());

	if (pKnights == nullptr)
		return;

	pKnightSiegeWar->sMasterKnights = pKnights->GetID();
	
	g_pMain->UpdateSiege(pKnightSiegeWar->sCastleIndex, pKnightSiegeWar->sMasterKnights, pKnightSiegeWar->bySiegeType, pKnightSiegeWar->byWarDay, pKnightSiegeWar->byWarTime, pKnightSiegeWar->byWarMinute);
	
	g_pMain->Announcement(IDS_NPC_GUIDON_DESTORY, Nation::ALL, PUBLIC_CHAT);
	g_pMain->m_byBattleSiegeWarMomument = true;
	g_pMain->m_byCastleSýegeWarMonumentTime = 15 * MINUTE;
	g_pMain->m_SiegeWarNoticeTime = 15;
	g_pMain->Announcement(IDS_SIEGE_WAR_CLAN_ANNOUNCEMENT);

	g_pMain->CastleSiegeWarFlag(nullptr, true);
}

void CGameServerDlg::CastleSiegeWarFlag(CUser *pUser, bool SendZonePacket)
{
	_KNIGHTS_SIEGE_WARFARE *pKnightSiege = g_pMain->GetSiegeMasterKnightsPtr(Aktive);
	CKnights *pKnight = g_pMain->GetClanPtr(pKnightSiege->sMasterKnights);

	Packet result(WIZ_SIEGE, uint8(2));

	if (pKnight == nullptr)
		result << uint16(0) << uint16(-1);
	else
		result << pKnight->GetID() << pKnight->m_sMarkVersion;

	if (pUser != nullptr)
		pUser->Send(&result);

	if (SendZonePacket)
		Send_Zone(&result, ZONE_DELOS);
}

void CUser::EventTrapProcess(float x, float z, CUser * pUser)
{
	if (pUser == nullptr)
		return;

	switch (pUser->GetZoneID())
	{
	case ZONE_KROWAZ_DOMINION:
		{
			foreach_stlmap(itr, g_pMain->m_ObjectEventArray)
			{
				if (itr->second->sZoneID != pUser->GetZoneID())
					continue;

				if (itr->second->sType != OBJECT_POISONGAS)
					continue;

				float m_sNewX = itr->second->fPosX, m_sOldX = itr->second->fPosX; 
				float m_sNewZ = itr->second->fPosZ, m_sOldZ = itr->second->fPosZ;

				m_sNewX -= 7, m_sOldX += 7;
				m_sNewZ += 3, m_sOldZ -= 3;

				if ((x >= m_sNewX && x <= m_sOldX) && (z >= m_sOldZ && z <= m_sNewZ))
					pUser->HpChange(-1500, pUser);
			}
		}
		break;
	default:
		break;
	}
}

void CUser::BifrostProcess(CUser * pUser)
{
	if (pUser == nullptr)
		return;

	if (g_pMain->m_BifrostVictory == 0 && g_pMain->m_bAttackBifrostMonument)
	{
		g_pMain->m_sBifrostTime = g_pMain->m_xBifrostTime;
		g_pMain->m_sBifrostRemainingTime = g_pMain->m_sBifrostTime;
		g_pMain->m_BifrostVictory = pUser->GetNation();
		g_pMain->SendFormattedResource(pUser->GetNation() == ELMORAD ? IDS_BEEF_ROAST_VICTORY_ELMORAD : IDS_BEEF_ROAST_VICTORY_KARUS, Nation::ALL,false);
		g_pMain->SendEventRemainingTime(true, nullptr, ZONE_BIFROST);

		if (g_pMain->m_bAttackBifrostMonument)
			g_pMain->m_bAttackBifrostMonument = false;
	}
	else if (g_pMain->m_BifrostVictory == 1 || g_pMain->m_BifrostVictory == 2) 
	{
		if (pUser->GetNation() != g_pMain->m_BifrostVictory && g_pMain->m_bAttackBifrostMonument)
		{
			g_pMain->m_BifrostVictory = 3;
			g_pMain->SendFormattedResource(pUser->GetNation() == ELMORAD ? IDS_BEEF_ROAST_VICTORY_ELMORAD : IDS_BEEF_ROAST_VICTORY_KARUS, Nation::ALL,false);

			if (g_pMain->m_bAttackBifrostMonument)
				g_pMain->m_bAttackBifrostMonument = false;
		}
	}
}

void CUser::TempleProcess(Packet &pkt )
{
	uint8 opcode = pkt.read<uint8>();
	
	switch (opcode)
	{
	case MONSTER_STONE:
		MonsterStoneProcess(opcode);
		break;
	case TEMPLE_EVENT_JOIN:
		TempleOperations(opcode);
		break;
	case TEMPLE_EVENT_DISBAND:
		TempleOperations(opcode);
		break;
	}
}

void CUser::MonsterStoneProcess(uint8 bType)
{
	uint16 nActiveEvent = (uint16)bType;
	uint8 bResult = 0;

	if(nActiveEvent == MONSTER_STONE && !isQuestEventUser())
	{
		g_pMain->pQuestEvent.ActiveEvent = TEMPLE_EVENT_MONSTER_STONE;

		if (nActiveEvent == MONSTER_STONE)
		{
			if (!CheckExistItem(ITEM_MONSTER_STONE))
				bResult = 0;
			else if (isTrading() 
				|| isMerchanting() 
				|| isStoreOpen())
				bResult = 2;
			else if (isMining())
				bResult = 3; 	
			else
				bResult = 1;
		}

		if (bResult == 1) 
		{
			if (!isMonsterStoneActive())
				m_bMonsterStatus = true;
			if (!isMonsterSummonActive())
				m_aMonsterStatus = true;
			if (!isMonsterStoneMonsterKill() || isMonsterStoneMonsterKill())
				m_tMonsterStatus = false;

			m_bMonsterStoneTime = UNIXTIME;
			m_MonsterSummonTime = UNIXTIME;
			RobItem(ITEM_MONSTER_STONE);
			GetNation() == KARUS ? g_pMain->pQuestEvent.KarusUserCount++ :g_pMain->pQuestEvent.ElMoradUserCount++;
			g_pMain->pQuestEvent.AllUserCount = (g_pMain->pQuestEvent.KarusUserCount + g_pMain->pQuestEvent.ElMoradUserCount);
			g_pMain->AddQuestEventUser(this);
			g_pMain->TempleQuestEventTeleportUsers(this);
		}
	}
	else if (nActiveEvent == TEMPLE_EVENT_DISBAND && isQuestEventUser())
	{
		GetNation() == KARUS ? g_pMain->pQuestEvent.KarusUserCount-- : g_pMain->pQuestEvent.ElMoradUserCount--;
		g_pMain->pQuestEvent.AllUserCount = g_pMain->pQuestEvent.KarusUserCount + g_pMain->pQuestEvent.ElMoradUserCount;

		if (g_pMain->pQuestEvent.LastEventRoom <= 1)
		{
			g_pMain->pQuestEvent.ActiveEvent = -1;
			g_pMain->pQuestEvent.StartTime = (uint32)UNIXTIME;
			g_pMain->pQuestEvent.m_eEventTime = 0;
			g_pMain->pQuestEvent.LastEventRoom = 1;
			g_pMain->pQuestEvent.AllUserCount = 0;
			g_pMain->pQuestEvent.ElMoradUserCount = 0;
			g_pMain->pQuestEvent.KarusUserCount = 0;
			g_pMain->pQuestEvent.isActive = false;
			g_pMain->pQuestEvent.isAttackable = false;	
		}
		if (g_pMain->pQuestEvent.LastEventRoom > 1)
			g_pMain->pQuestEvent.LastEventRoom--;

		if (isMonsterStoneActive())
			m_bMonsterStatus = false;
		if (isMonsterSummonActive())
			m_aMonsterStatus = false;
		if (!isMonsterStoneMonsterKill() || isMonsterStoneMonsterKill())
			m_tMonsterStatus = false;

		g_pMain->KillNpc(GetSocketID());
		m_bMonsterStoneTime = UNIXTIME;
		m_MonsterSummonTime = UNIXTIME;
		m_MonsterStoneFamily = 0;
		g_pMain->UpdateQuestEventUser(this,0);
		g_pMain->RemoveQuestEventUser(this);
	}
} 

void CUser::TempleOperations(uint8 bType)
{
	uint16 nActiveEvent = (uint16)g_pMain->pTempleEvent.ActiveEvent;

	uint8 bResult = 1;
	Packet result;

	if(bType == TEMPLE_EVENT_JOIN && !isEventUser() && !isQuestEventUser())
	{
		if (nActiveEvent == TEMPLE_EVENT_CHAOS)
		{
			if (CheckExistItem(CHAOS_MAP,1))
				bResult = 1;
			else if (m_sItemArray[RIGHTHAND].nNum == MATTOCK || m_sItemArray[RIGHTHAND].nNum == GOLDEN_MATTOCK || isMining())
				bResult = 4; 
			else
				bResult = 3;
		}

		else if (nActiveEvent == TEMPLE_EVENT_BORDER_DEFENCE_WAR 
			|| nActiveEvent == TEMPLE_EVENT_JURAD_MOUNTAIN)
		{
			if (nActiveEvent == TEMPLE_EVENT_BORDER_DEFENCE_WAR)
			{
				if (GetLevel() < MIN_LEVEL_BORDER_DEFANCE)
					bResult = WarpListMinLevel;
			}
			
			if (nActiveEvent == TEMPLE_EVENT_JURAD_MOUNTAIN)
			{
				if (GetLevel() < MIN_LEVEL_JURAD_MOUNTAIN)
					bResult = WarpListMinLevel;
			}
		}

		if (bResult == 1) 
		{
			GetNation() == KARUS ? g_pMain->pTempleEvent.KarusUserCount++ :g_pMain->pTempleEvent.ElMoradUserCount++;
			g_pMain->pTempleEvent.AllUserCount = (g_pMain->pTempleEvent.KarusUserCount + g_pMain->pTempleEvent.ElMoradUserCount);
			g_pMain->AddEventUser(this);
			TempleOperations(TEMPLE_EVENT_COUNTER);
		}
	}
	else if (bType == TEMPLE_EVENT_DISBAND && isEventUser())
	{
		GetNation() == KARUS ? g_pMain->pTempleEvent.KarusUserCount-- : g_pMain->pTempleEvent.ElMoradUserCount--;
		g_pMain->pTempleEvent.AllUserCount = g_pMain->pTempleEvent.KarusUserCount + g_pMain->pTempleEvent.ElMoradUserCount;
		g_pMain->RemoveEventUser(this);
		TempleOperations(TEMPLE_EVENT_COUNTER);
	}
	else if (bType == TEMPLE_EVENT_COUNTER)
	{
		result.clear();
		result.Initialize(WIZ_SELECT_MSG);
		result << uint16(0x00) << uint8(0x07) << uint64(0x00) << uint32(0x06) << g_pMain->pTempleEvent.KarusUserCount << uint16(0x00) << g_pMain->pTempleEvent.ElMoradUserCount << uint16(0x00) << g_pMain->m_nTempleEventRemainSeconds << uint16(0x00);
			
		if (!g_pMain->m_TempleEventUserArray.GetData(GetSocketID()))
		{
			Send(&result);
			return;
		}

		foreach_stlmap (itr, g_pMain->m_TempleEventUserArray)
		{
			CUser * pUser = g_pMain->GetUserPtr(itr->second->m_socketID);

			if (pUser == nullptr || 
				!pUser->isInGame())
				continue;

			pUser->Send(&result);
		}
	}
	else if (bType == TEMPLE_EVENT_COUNTER2)
	{
		result.clear();
		result.Initialize(WIZ_EVENT);
		result << bType << nActiveEvent;

		if(nActiveEvent == TEMPLE_EVENT_CHAOS)
			result << g_pMain->pTempleEvent.AllUserCount;
		else
			result << g_pMain->pTempleEvent.KarusUserCount << g_pMain->pTempleEvent.ElMoradUserCount;

		g_pMain->Send_All(&result, nullptr, Nation::ALL, 0, true, 0);
	}
}

void CGameServerDlg::AddEventUser(CUser *pUser)
{
	if (pUser == nullptr)
	{
		TRACE("#### AddEventUser : pUser == nullptr ####\n");
		return;
	}

	_TEMPLE_EVENT_USER * pEventUser = new _TEMPLE_EVENT_USER;

	pEventUser->m_socketID =  pUser->GetSocketID();
	pEventUser->m_bEventRoom = pUser->GetEventRoom();

	if (!g_pMain->m_TempleEventUserArray.PutData(pEventUser->m_socketID, pEventUser))
		delete pEventUser;
}

void CGameServerDlg::AddQuestEventUser(CUser *pUser)
{
	if (pUser == nullptr)
	{
		TRACE("#### AddEventUser : pUser == nullptr ####\n");
		return;
	}

	_TEMPLE_QUEST_EVENT_USER * pQuestEventUser = new _TEMPLE_QUEST_EVENT_USER;

	pQuestEventUser->m_socketID =  pUser->GetSocketID();
	pQuestEventUser->m_bEventRoom = pUser->GetEventRoom();

	if (!g_pMain->m_TempleQuestEventUserArray.PutData(pQuestEventUser->m_socketID, pQuestEventUser))
		delete pQuestEventUser;
}

void CUser::isEventSoccerMember(uint8 TeamColours, float x, float z)
{
	bool bIsNeutralZone = (GetZoneID() >= ZONE_MORADON && GetZoneID() <= ZONE_MORADON5); 

	if(g_pMain->pSoccerEvent.m_SoccerRedColour > 11 
		&& TeamColours == TeamColourRed)
		return;

	if(g_pMain->pSoccerEvent.m_SoccerBlueColour > 11 
		&& TeamColours == TeamColourBlue)
		return;

	if (TeamColours > TeamColourRed 
		|| TeamColours < TeamColourBlue)
		return;

	if (!bIsNeutralZone)
		return;

	_TEMPLE_SOCCER_EVENT_USER * pSoccerEventUser = new _TEMPLE_SOCCER_EVENT_USER;

	pSoccerEventUser->m_socketID = GetSocketID();
	pSoccerEventUser->m_teamColour = TeamColours;

	if(!g_pMain->m_TempleSoccerEventUserArray.PutData(pSoccerEventUser->m_socketID, pSoccerEventUser))
	{
		delete pSoccerEventUser;
		return;
	}

	if (TeamColours == TeamColourBlue)
		g_pMain->pSoccerEvent.m_SoccerBlueColour++;

	if (TeamColours == TeamColourRed)
		g_pMain->pSoccerEvent.m_SoccerRedColour++;

	Packet result(WIZ_MINING);
	result << uint8(16) << uint8(2) << g_pMain->pSoccerEvent.m_SoccerTime;
	Send(&result);

	if (x == 0.0f && z == 0.0f)
	{
		if(TeamColours == TeamColourBlue)
			x = 672.0f, z = 166.0f;
		else
			x = 672.0f, z = 154.0f;
	}

	ZoneChange(GetZoneID(), x, z);

	StateChangeServerDirect(11, uint32(TeamColours));
}

void CUser::isEventSoccerStard()
{
	if (g_pMain->pSoccerEvent.isSoccerAktive())
		return;

	if (!isSoccerEventUser())
		return;

	if(g_pMain->pSoccerEvent.m_SoccerBlueColour == TeamColourNone)
		return;

	if(g_pMain->pSoccerEvent.m_SoccerRedColour == TeamColourNone)
		return;

	if(g_pMain->pSoccerEvent.m_SoccerBlueColour > TeamColourNone 
		&& g_pMain->pSoccerEvent.m_SoccerRedColour > TeamColourNone)
	{
		g_pMain->pSoccerEvent.m_SoccerTime = 600;
		g_pMain->pSoccerEvent.m_SoccerActive = true;
	}
}

void CUser::isEventSoccerEnd()
{
	if(!isSoccerEventUser())
		return;
	
	uint8 nWinnerTeam = TeamColourNone;
	Packet result(WIZ_MINING, uint8(16));

	if (g_pMain->pSoccerEvent.m_SoccerRedGool > g_pMain->pSoccerEvent.m_SoccerBlueGool)
		nWinnerTeam = TeamColourRed;
	else if (g_pMain->pSoccerEvent.m_SoccerRedGool < g_pMain->pSoccerEvent.m_SoccerBlueGool)
		nWinnerTeam = TeamColourBlue;
	else
		nWinnerTeam = TeamColourNone;

	result << uint8(4) << nWinnerTeam << g_pMain->pSoccerEvent.m_SoccerBlueGool << g_pMain->pSoccerEvent.m_SoccerRedGool;

	if(g_pMain->pSoccerEvent.m_SoccerBlueColour > TeamColourNone 
		&& m_teamColour == TeamColourBlue)
		g_pMain->pSoccerEvent.m_SoccerBlueColour--;

	if(g_pMain->pSoccerEvent.m_SoccerRedColour > TeamColourNone 
		&& m_teamColour == TeamColourRed)
		g_pMain->pSoccerEvent.m_SoccerRedColour--;

	StateChangeServerDirect(11, TeamColourNone);

	Send(&result);
}

void CGameServerDlg::RemoveEventUser(CUser *pUser)
{
	if (pUser == nullptr)
	{
		TRACE("#### RemoveEventUser : pUser == nullptr ####\n");
		return;
	}

	if (g_pMain->m_TempleEventUserArray.GetData(pUser->GetSocketID()) != nullptr)
		g_pMain->m_TempleEventUserArray.DeleteData(pUser->GetSocketID());

	pUser->m_bEventRoom = 0;
}

void CGameServerDlg::RemoveQuestEventUser(CUser *pUser)
{
	if (pUser == nullptr)
	{
		TRACE("#### RemoveQuestEventUser : pUser == nullptr ####\n");
		return;
	}

	if (g_pMain->m_TempleQuestEventUserArray.GetData(pUser->GetSocketID()) != nullptr)
		g_pMain->m_TempleQuestEventUserArray.DeleteData(pUser->GetSocketID());

	pUser->m_bEventRoom = 0;
}

void CGameServerDlg::UpdateEventUser(CUser *pUser, uint16 nEventRoom)
{
	if (pUser == nullptr)
	{
		TRACE("#### UpdateEventUser : pUser == nullptr ####\n");
		return;
	}

	_TEMPLE_EVENT_USER * pEventUser = g_pMain->m_TempleEventUserArray.GetData(pUser->GetSocketID());

	if (pEventUser == nullptr)
	{
		TRACE("#### UpdateEventUser : pEventUser == nullptr ####\n");
		return;
	}

	pEventUser->m_bEventRoom = nEventRoom;
	pUser->m_bEventRoom = nEventRoom;
}

void CGameServerDlg::UpdateQuestEventUser(CUser *pUser, uint16 nEventRoom)
{
	if (pUser == nullptr)
	{
		TRACE("#### UpdateQuestEventUser : pUser == nullptr ####\n");
		return;
	}

	_TEMPLE_QUEST_EVENT_USER * pQuestEventUser = g_pMain->m_TempleQuestEventUserArray.GetData(pUser->GetSocketID());

	if (pQuestEventUser == nullptr)
	{
		TRACE("#### UpdateQuestEventUser : pQuestEventUser == nullptr ####\n");
		return;
	}

	pQuestEventUser->m_bEventRoom = nEventRoom;
	pUser->m_bEventRoom = nEventRoom;
}

void CGameServerDlg::SetEventUser(CUser *pUser)
{
	if (pUser == nullptr)
	{
		TRACE("#### SetEventUser : pUser == nullptr ####\n");
		return;
	}

	uint8 nMaxUserCount = 0;

	switch (g_pMain->pTempleEvent.ActiveEvent)
	{
	case TEMPLE_EVENT_BORDER_DEFENCE_WAR:
		nMaxUserCount = 16;
		break;
	case TEMPLE_EVENT_CHAOS:
		nMaxUserCount = 10;
		break;
	case TEMPLE_EVENT_JURAD_MOUNTAIN:
		nMaxUserCount = 16;
		break;
	}

	if (g_pMain->TempleEventGetRoomUsers(g_pMain->pTempleEvent.LastEventRoom) >= nMaxUserCount)
		g_pMain->pTempleEvent.LastEventRoom++;

	if (g_pMain->TempleEventGetRoomUsers(g_pMain->pTempleEvent.LastEventRoom) < nMaxUserCount)
		g_pMain->UpdateEventUser(pUser, g_pMain->pTempleEvent.LastEventRoom);
}

void CGameServerDlg::SetQuestEventUser(CUser *pUser)
{
	if (pUser == nullptr)
	{
		TRACE("#### SetQuestEventUser : pUser == nullptr ####\n");
		return;
	}

	uint8 nMaxUserCount = 0;

	switch (g_pMain->pQuestEvent.ActiveEvent)
	{
	case TEMPLE_EVENT_MONSTER_STONE:
		nMaxUserCount = 1;
		break;
	}

	if (g_pMain->TempleQuestEventGetRoomUsers(g_pMain->pQuestEvent.LastEventRoom) >= nMaxUserCount)
		g_pMain->pQuestEvent.LastEventRoom++;

	if (g_pMain->TempleQuestEventGetRoomUsers(g_pMain->pQuestEvent.LastEventRoom) < nMaxUserCount)
		g_pMain->UpdateQuestEventUser(pUser, g_pMain->pQuestEvent.LastEventRoom);
}

bool CUser::isEventUser()
{
	_TEMPLE_EVENT_USER * pEventUser = g_pMain->m_TempleEventUserArray.GetData(GetSocketID());

	if (pEventUser != nullptr)
		return true;

	return false;
}

bool CUser::isQuestEventUser()
{
	_TEMPLE_QUEST_EVENT_USER * pQuestEventUser = g_pMain->m_TempleQuestEventUserArray.GetData(GetSocketID());

	if (pQuestEventUser != nullptr)
		return true;

	return false;
}

bool CUser::isSoccerEventUser()
{
	_TEMPLE_SOCCER_EVENT_USER * pSoccerEvent = g_pMain->m_TempleSoccerEventUserArray.GetData(GetSocketID());

	if(pSoccerEvent != nullptr)
		return true;

	return false;
}

bool CUser::isInSoccerEvent()
{
	bool bIsNeutralZone = (GetZoneID() >= ZONE_MORADON && GetZoneID() <= ZONE_MORADON5); 

	if(!bIsNeutralZone)
		return false;
	
	if (!isSoccerEventUser())
		return false;

	return ((GetX() > 644.0f && GetX() < 699.0f) 
		&& ((GetZ() > 120.0f && GetZ() < 200.0f)));
}

uint8 CNpc::isInSoccerEvent()
{
	bool bIsNeutralZone = (GetZoneID() >= ZONE_MORADON && GetZoneID() <= ZONE_MORADON5); 

	bool b_isSoccerOutside = ((GetX() > 644.0f && GetX() < 699.0f) && ((GetZ() > 120.0f && GetZ() < 200.0f)));

	bool b_isSoccerRedside = ((GetX() > 661.0f && GetX() < 681.0f) && ((GetZ() > 108.0f && GetZ() < 120.0f)));

	bool b_isSoccerBlueside = ((GetX() > 661.0f && GetX() < 681.0f) && ((GetZ() > 199.0f && GetZ() < 208.0f)));

	if (!bIsNeutralZone)
		return TeamColourMap;
	
	if (b_isSoccerBlueside)
		return TeamColourBlue;

	if (b_isSoccerRedside)
		return TeamColourRed;

	if (!b_isSoccerOutside)
		return TeamColourOutside;

	return TeamColourNone;
}

void CUser::HandleCapture(Packet & pkt)
{
	if ((UNIXTIME - m_tBorderCapure) < 10)
		return;

	uint16 nSeconds = 360;
	
	g_pMain->pTempleEvent.m_sBorderMonumentTimer[GetEventRoom()] = nSeconds;
	g_pMain->pTempleEvent.m_sBorderMonumentNation[GetEventRoom()] = GetNation();
	g_pMain->pTempleEvent.m_MonumentFinish[GetEventRoom()] = true;

	GetNation() == ELMORAD ? g_pMain->pTempleEvent.ElmoDeathCount[GetEventRoom()] = g_pMain->pTempleEvent.ElmoDeathCount[GetEventRoom()] + 2 : g_pMain->pTempleEvent.KarusDeathCount[GetEventRoom()] = g_pMain->pTempleEvent.KarusDeathCount[GetEventRoom()] + 2;

	Packet result(WIZ_CAPTURE, uint8(CAPURE_TIME_HUMAN));
	result << GetNation() << GetName().c_str();
	g_pMain->Send_Zone(&result, GetZoneID(), this, Nation::ALL,GetEventRoom());

	result.Initialize(WIZ_CAPTURE);
	result << uint8(CAPURE_TIME_KARUS) << GetNation() << uint16(nSeconds);;
	g_pMain->Send_Zone(&result, GetZoneID(), this, Nation::ALL,GetEventRoom());

	result.Initialize(WIZ_EVENT);
	result << uint8(TEMPLE_EVENT_BORDER_COUNTER);
	
	if (isInParty())
	{
		_PARTY_GROUP * pParty = g_pMain->GetPartyPtr(GetPartyID());

		if (pParty == nullptr)
			return;

		short partyUsers[MAX_PARTY_USERS];

		for (int i = 0; i < MAX_PARTY_USERS; i++)
			partyUsers[i] = pParty->uid[i];

		for (int i = 0; i < MAX_PARTY_USERS; i++)
		{
			CUser * pUser = g_pMain->GetUserPtr(partyUsers[i]);
			if (pUser == nullptr)
				continue;

			if (GetEventRoom() != pUser->GetEventRoom())
				continue;

				pUser->m_iLoyaltyDaily += 1 ;
				pUser->UpdatePlayerRank();
		}
	}

	if (!isInParty())
	{
		m_iLoyaltyDaily += 1 ;
		UpdatePlayerRank();
	}

	result << g_pMain->pTempleEvent.KarusDeathCount[GetEventRoom()] << uint16(0x00) << g_pMain->pTempleEvent.ElmoDeathCount[GetEventRoom()] << uint16(0x00);
	g_pMain->Send_Zone(&result, GetZoneID(), this, Nation::ALL,GetEventRoom());
}

uint8 CUser::GetMonsterChallengeTime() 
{ 
	if (g_pMain->m_bForgettenTempleIsActive
		&& g_pMain->m_nForgettenTempleLevelMin != 0 
		&& g_pMain->m_nForgettenTempleLevelMax != 0
		&& GetLevel() >= g_pMain->m_nForgettenTempleLevelMin 
		&& GetLevel() <= g_pMain->m_nForgettenTempleLevelMax
		&& !g_pMain->m_bForgettenTempleSummonMonsters)
		return g_pMain->m_nForgettenTempleChallengeTime; 

	return 0;
}

uint8 CUser::GetMonsterChallengeUserCount() { return (uint8) g_pMain->m_nForgettenTempleUsers.size(); }
