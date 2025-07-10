#include "stdafx.h"
#include "Map.h"
#include "MagicInstance.h"
#include "../shared/DateTime.h"

using namespace std;

CNpc::CNpc() : Unit(UnitNPC)
{
	Initialize();
}


CNpc::~CNpc()
{
}

/**
* @brief	Initializes this object.
*/
void CNpc::Initialize()
{
	Unit::Initialize();

	m_sSid = 0;
	m_sPid = 0;				// MONSTER(NPC) Picture ID
	m_sSize = 100;				// MONSTER(NPC) Size
	m_strName.clear();			// MONSTER(NPC) Name
	m_iMaxHP = 0;				// �ִ� HP
	m_iHP = 0;					// ���� HP
	m_byState = 0;			// ������ (NPC) �����̻�
	m_tNpcType = 0;				// NPC Type
	// 0 : Normal Monster
	// 1 : NPC
	// 2 : �� �Ա�,�ⱸ NPC
	// 3 : ������
	m_iSellingGroup = 0;
	//m_dwStepDelay = 0;

	m_byDirection = 0;			// npc�� ����,,
	m_iWeapon_1 = 0;
	m_iWeapon_2 = 0;
	m_NpcState = NPC_LIVE;
	m_byGateOpen = true;
	m_byObjectType = NORMAL_OBJECT;

	m_byTrapNumber = 0;
}

void CNpc::Update()
{
	if (m_bType3Flag)
		HPTimeChangeType3();

	// Check for expired type 7/Sleep Skills Remove
	CMagicProcess::CheckExpiredType7Skills(this);
}

/**
* @brief	Adds the NPC to the region.
*
* @param	new_region_x	The new region x coordinate.
* @param	new_region_z	The new region z coordinate.
*/
void CNpc::AddToRegion(int16 new_region_x, int16 new_region_z)
{
	GetRegion()->Remove(this);
	SetRegion(new_region_x, new_region_z);
	GetRegion()->Add(this);
}

/**
* @brief	Sends the movement packet for the NPC.
*
* @param	fPosX 	The position x coordinate.
* @param	fPosY 	The position y coordinate.
* @param	fPosZ 	The position z coordinate.
* @param	fSpeed	The speed.
*/
void CNpc::MoveResult(float fPosX, float fPosY, float fPosZ, float fSpeed)
{
	Packet result(WIZ_NPC_MOVE);

	SetPosition(fPosX, fPosY, fPosZ); 
	RegisterRegion();

	result << GetID() << GetSPosX() << GetSPosZ() << GetSPosY() << uint16(fSpeed * 10);
	SendToRegion(&result);
}

/**
* @brief	Constructs an in/out packet for the NPC.
*
* @param	result	The packet buffer the constructed packet will be stored in.
* @param	bType 	The type (in or out).
*/
void CNpc::GetInOut(Packet & result, uint8 bType)
{
	result.Initialize(WIZ_NPC_INOUT);
	result << bType << GetID();
	if (bType != INOUT_OUT)
		GetNpcInfo(result);

	if (bType == INOUT_IN)
		OnRespawn();
}

/**
* @brief	Constructs and sends an in/out packet for the NPC.
*
* @param	bType	The type (in or out).
* @param	fX   	The x coordinate.
* @param	fZ   	The z coordinate.
* @param	fY   	The y coordinate.
*/
void CNpc::SendInOut(uint8 bType, float fX, float fZ, float fY)
{
	if (GetRegion() == nullptr)
		SetRegion(GetNewRegionX(), GetNewRegionZ());

	if (GetRegion() == nullptr)
		return;

	if (bType == INOUT_OUT)
	{
		GetRegion()->Remove(this);
	}
	else	
	{
		GetRegion()->Add(this);
		SetPosition(fX, fY, fZ);
	}

	Packet result;
	GetInOut(result, bType);
	SendToRegion(&result);
}

/**
* @brief	Gets NPC information for use in various NPC packets.
*
* @param	pkt	The packet the information will be stored in.
*/
void CNpc::GetNpcInfo(Packet & pkt)//duzenlendi
{
	pkt.SByte();
	pkt << GetProtoID()
		<< m_sPid
		<< GetType()
		<< m_iSellingGroup
		<< m_sSize
		<< m_iWeapon_1 << m_iWeapon_2
		<< uint8(isMonster() ? 0 : GetNation())
		<< GetLevel()
		<< GetSPosX() << GetSPosZ() << GetSPosY();
	if (JuraidTempleEventZone() && GetProtoID() == SUMMON_BRIDGE_SSID && g_pMain->pTempleEvent.isSummonsBridge[GetEventRoom()])
		pkt << uint32(0x02);
	else
		pkt << uint32(isGateOpen());
	pkt << m_byObjectType
		<< uint16(0) << uint16(0) // unknown
		<< int8(m_byDirection);
}

/**
* @brief	Sends a gate status.
*
* @param	ObjectType  object type
* @param	bFlag  	The flag (open or shut).
* @param	bSendAI	true to update the AI server.
*/
void CNpc::SendGateFlag(uint8 bFlag /*= -1*/, bool bSendAI /*= true*/)
{
	 uint8 objectType = OBJECT_FLAG_LEVER;
	 
	 _OBJECT_EVENT * pObjectEvent = GetMap()->GetObjectEvent(GetProtoID());
	 
	 if (pObjectEvent)
		 objectType = (uint8)pObjectEvent->sType;
	 else
		 objectType = GetType();
	 
	 Packet result(WIZ_OBJECT_EVENT, objectType);

	// If there's a flag to set, set it now.
	if (bFlag >= 0)
		m_byGateOpen = (bFlag == 1);

	// Tell everyone nearby our new status.
	result << uint8(1) << GetID() << m_byGateOpen;
	SendToRegion(&result);

	// Tell the AI server our new status
	if (bSendAI)
	{
		result.Initialize(AG_NPC_GATE_OPEN);
		result << GetID() << GetProtoID() << m_byGateOpen;
		Send_AIServer(&result);
	}
}

/**
* @brief	Changes an NPC's hitpoints.
*
* @param	amount   	The amount to adjust the HP by.
* @param	pAttacker	The attacker.
* @param	bSendToAI	true to update the AI server.
*/
void CNpc::HpChange(int amount, Unit *pAttacker /*= nullptr*/, bool bSendToAI /*= true*/) 
{
	uint16 tid = (pAttacker != nullptr ? pAttacker->GetID() : -1);
	bool bySendPacket = false;

	// Implement damage/HP cap.
	if (amount < -MAX_DAMAGE)
		amount = -MAX_DAMAGE;
	else if (amount > MAX_DAMAGE)
		amount = MAX_DAMAGE;

	// Glorious copypasta.
	if (amount < 0 && -amount > m_iHP)
		m_iHP = 0;
	else if (amount >= 0 && m_iHP + amount > m_iMaxHP)
		m_iHP = m_iMaxHP;
	else
		m_iHP += amount;

	// NOTE: Sleep System
	if (m_NpcState == NPC_SLEEPING)
	{
		m_NpcState = NPC_TRACING;
		bySendPacket = true;
	}

	if (bySendPacket)
		StateChangeServerDirect(1, m_NpcState);

	// NOTE: This will handle the death notification/looting.
	if (bSendToAI)
		SendHpChangeToAI(tid, amount);

	if (pAttacker != nullptr
		&& pAttacker->isPlayer())
		TO_USER(pAttacker)->SendTargetHP(0, GetID(), amount);
}

void CNpc::HpChangeMagic(int amount, Unit *pAttacker /*= nullptr*/, AttributeType attributeType /*= AttributeNone*/)
{
	uint16 tid = (pAttacker != nullptr ? pAttacker->GetID() : -1);
	bool bySendPacket = false;

	// Implement damage/HP cap.
	if (amount < -MAX_DAMAGE)
		amount = -MAX_DAMAGE;
	else if (amount > MAX_DAMAGE)
		amount = MAX_DAMAGE;

	// NOTE: Sleep System
	if (m_NpcState == NPC_SLEEPING)
	{
		m_NpcState = NPC_TRACING;
		bySendPacket = true;
	}

	if (bySendPacket)
		StateChangeServerDirect(1, m_NpcState);

	HpChange(amount, pAttacker, false);
	SendHpChangeToAI(tid, amount, attributeType);
}

void CNpc::SendHpChangeToAI(uint16 sTargetID, int amount, AttributeType attributeType /*= AttributeNone*/)
{
	Packet result(AG_NPC_HP_CHANGE);
	result << GetID() << sTargetID << m_iHP << amount << uint8(attributeType);
	Send_AIServer(&result);
}

/**
* @brief	Changes an NPC's mana.
*
* @param	amount	The amount to adjust the mana by.
*/
void CNpc::MSpChange(int amount)
{

}

bool CNpc::CastSkill(Unit * pTarget, uint32 nSkillID)
{
	if (pTarget == nullptr)
		return false;

	MagicInstance instance;

	instance.bSendFail = false;
	instance.nSkillID = nSkillID;
	instance.sCasterID = GetID();
	instance.sTargetID = pTarget->GetID();

	instance.Run();

	return (instance.bSkillSuccessful);
}

float CNpc::GetRewardModifier(uint8 byLevel)
{
	int iLevelDifference = GetLevel() - byLevel;

	if (iLevelDifference <= -14)	
		return 0.2f;
	else if (iLevelDifference <= -8 && iLevelDifference >= -13)
		return 0.5f;
	else if (iLevelDifference <= -2 && iLevelDifference >= -7)
		return 0.8f;

	return 1.0f;
}

float CNpc::GetPartyRewardModifier(uint32 nPartyLevel, uint32 nPartyMembers)
{
	int iLevelDifference = GetLevel() - (nPartyLevel / nPartyMembers);

	if (iLevelDifference >= 8)
		return 2.0f;
	else if (iLevelDifference >= 5)
		return 1.5f;
	else if (iLevelDifference >= 2)
		return 1.2f;

	return 1.0f;
}

void CNpc::StateChangeServerDirect(uint8 bType, uint32 nBuff)
{
	uint8 buff = *(uint8 *)&nBuff; // don't ask
	bool bySendPacket = false;
	Packet result;
	switch (bType)
	{
	case 1:
		m_NpcState = buff;
		bySendPacket = true;
		break;
	}

	if (bySendPacket)
		SendAIServerNpcUpdate(bType, GetSPosX(), GetSPosY() ,GetSPosZ(), 0.0f);

	result.Initialize(WIZ_STATE_CHANGE);
	result << GetID() << bType << nBuff; 
	SendToRegion(&result);
}

void CNpc::SendAIServerNpcUpdate(uint8 bType, float fPosX, float fPosY, float fPosZ, float fSpeed)
{
	Packet result(AI_NPC_UPDATE);
	result << GetID() << bType << m_NpcState << fPosX << fPosY << fPosZ << fSpeed;
	Send_AIServer(&result);
}

/**
* @brief	Executes the death action.
*
* @param	pKiller	The killer.
*/
void CNpc::OnDeath(Unit *pKiller)
{
	if (m_NpcState == NPC_DEAD)
		return;

	m_NpcState = NPC_DEAD;
	m_sACPercent = 100;

	if (m_byObjectType == SPECIAL_OBJECT && GetMap() != nullptr)
	{
		_OBJECT_EVENT *pEvent = GetMap()->GetObjectEvent(GetProtoID());
		if (pEvent != nullptr)
			pEvent->byLife = 0;
	}

	Unit::OnDeath(pKiller);
	OnDeathProcess(pKiller);

	InitType3();
	InitType4();

	if (GetRegion() != nullptr)
		GetRegion()->Remove(TO_NPC(this));

	SetRegion();
}

/**
* @brief	Executes the death process.
*
* @param	pKiller	The killer.
*/
void CNpc::OnDeathProcess(Unit *pKiller)
{
	if (TO_NPC(this) == nullptr && !pKiller->isPlayer())
		return;

	CUser * pUser = TO_USER(pKiller);

	if (pUser == nullptr)
		return;

	if (!m_bMonster)
	{
		switch (m_tNpcType)
		{
		case NPC_BIFROST_MONUMENT:
			pUser->BifrostProcess(pUser);
			break;
		case NPC_PVP_MONUMENT:
			PVPMonumentProcess(pUser);
			break;
		case NPC_BATTLE_MONUMENT:
			BattleMonumentProcess(pUser);
			break;
		case NPC_HUMAN_MONUMENT:
			NationMonumentProcess(pUser);
			break;
		case NPC_KARUS_MONUMENT:
			NationMonumentProcess(pUser);
			break;
		case NPC_DESTROYED_ARTIFACT:
			if (g_pMain->m_byBattleOpen == SIEGE_BATTLE && pUser->isInClan())
				pUser->CastleSiegeWarProcess(pUser);
			break;
		case NPC_BORDER_MONUMENT:
			BDWMonumentProcess(pUser);
			break;
		}
	}
	else if (m_bMonster)
	{
		if (GetProtoID() == 700 || GetProtoID() == 750 || GetProtoID() == 701 || GetProtoID() == 751)
		{
			if (pUser->CheckExistEvent(STARTER_SEED_QUEST, 1))
			{
				_QUEST_HELPER * pQuestHelper ;
				if (pUser->GetNation() == ELMORAD)
					pQuestHelper = g_pMain->m_QuestHelperArray.GetData(5005);
				else
					pQuestHelper = g_pMain->m_QuestHelperArray.GetData(5002);

				pUser->QuestV2RunEvent(pQuestHelper,pQuestHelper->nEventTriggerIndex);
			}
		}

		if (g_pMain->pTempleEvent.ActiveEvent == TEMPLE_EVENT_JURAD_MOUNTAIN && pUser->JuraidTempleEventZone())
		{
			if (JuraidTempleEventZone())
			{
				_MONSTER_RESPAWN_LIST * pMonsterRespawn = g_pMain->m_MonsterRespawnListArray.GetData(GetProtoID());

				if (pMonsterRespawn != nullptr)
					g_pMain->SpawnEventNpc(pMonsterRespawn->sSid, pMonsterRespawn->sType == 0 ? true : false, GetZoneID(), GetX(), GetY(), GetZ(), 0, pMonsterRespawn->sCount, 5, 60 * MINUTE, 0, GetID(), pUser->GetEventRoom());

				if (pMonsterRespawn != nullptr && (GetProtoID() == pMonsterRespawn->sIndex || GetProtoID() == pMonsterRespawn->sSid))
				{
					if (GetProtoID() == pMonsterRespawn->sIndex)
						g_pMain->pTempleEvent.isMonsterKilledCount[GetEventRoom()]++;
					else if (GetProtoID() == pMonsterRespawn->sSid)
						g_pMain->pTempleEvent.isMonsterAddKilledCount[GetEventRoom()]++;

					if (g_pMain->pTempleEvent.isMonsterKilledCount[GetEventRoom()] > 4 && g_pMain->pTempleEvent.isMonsterAddKilledCount[GetEventRoom()] > 24)
						g_pMain->pTempleEvent.isSummonsBridge[GetEventRoom()] = true;

					if (g_pMain->pTempleEvent.isSummonsBridge[GetEventRoom()] && (g_pMain->pTempleEvent.isMonsterKilledCount[GetEventRoom()] > 4 || g_pMain->pTempleEvent.isMonsterAddKilledCount[GetEventRoom()] > 24))
					{
						if (g_pMain->pTempleEvent.isMonsterKilledCount[GetEventRoom()] > 4 || g_pMain->pTempleEvent.isMonsterAddKilledCount[GetEventRoom()] > 24)
						{
							if (g_pMain->pTempleEvent.isMonsterKilledCount[GetEventRoom()] > 4)
								g_pMain->pTempleEvent.isMonsterKilledCount[GetEventRoom()] = 1;
							else if (g_pMain->pTempleEvent.isMonsterAddKilledCount[GetEventRoom()] > 24)
								g_pMain->pTempleEvent.isMonsterAddKilledCount[GetEventRoom()] = 1;
						}
					}
				}

				if (GetProtoID() == DEVA_BIRD) // Deva
				{
					g_pMain->pTempleEvent.isDevaControl[pUser->GetEventRoom()] = true;
					g_pMain->pTempleEvent.isDevaFlag[pUser->GetEventRoom()] = true;
				}
			}
		}

		if (m_tNpcType == NPC_CHAOS_STONE && pUser->isInPKZone())
			ChaosStoneProcess(pUser,5);

		if (g_pMain->m_MonsterRespawnListArray.GetSize() > 0 && !isInTempleEventZone())
		{
			_MONSTER_RESPAWN_LIST * pMonsterRespawn = g_pMain->m_MonsterRespawnListArray.GetData(GetProtoID());

			if (pMonsterRespawn != nullptr)
				g_pMain->SpawnEventNpc(pMonsterRespawn->sSid, pMonsterRespawn->sType == 0 ? true : false, GetZoneID(), GetX(), GetY(), GetZ(), 0, pMonsterRespawn->sCount, 5, 60 * MINUTE, 0, GetID(), pUser->GetEventRoom());
		}

		if (g_pMain->m_bForgettenTempleIsActive && GetZoneID() == ZONE_FORGOTTEN_TEMPLE)
			g_pMain->m_ForgettenTempleMonsterList.erase(GetProtoID());

		if (pUser->GetZoneID() >= ZONE_STONE1 && pUser->GetZoneID() <= ZONE_STONE3 
			&& pUser->isMonsterStoneActive() && !pUser->isMonsterStoneMonsterKill())
		{
			if (GetProtoID() == 8718 
				|| GetProtoID() == 8728
				|| GetProtoID() == 8746
				|| GetProtoID() == 8770
				|| GetProtoID() == 8708
				|| GetProtoID() == 8737
				|| GetProtoID() == 8754
				|| GetProtoID() == 8784
				|| GetProtoID() == 8792
				|| GetProtoID() == 8762
				|| GetProtoID() == 8776
				|| GetProtoID() == 8800
				|| GetProtoID() == 8809
				|| GetProtoID() == 8728)
			{
				pUser->m_MonsterSummonTime = UNIXTIME;
				pUser->m_tMonsterStatus = true;

				Packet result(WIZ_EVENT);
				result << uint8(TEMPLE_EVENT_FINISH)
					<< uint8(0x11) << uint8(0x00)  
					<< uint8(0x65) << uint8(0x14) << uint32(0x00);

				pUser->Send(&result);
			}
		}

		if (GetType() == NPC_BORDER_MONUMENT && isInTempleEventZone())
			BDWMonumentProcess(pUser);

		if (pUser->isInParty())
		{
			_PARTY_GROUP * pParty = g_pMain->GetPartyPtr(pUser->GetPartyID());

			if (pParty == nullptr)
				return;

			short partyUsers[MAX_PARTY_USERS];

			for (int i = 0; i < MAX_PARTY_USERS; i++)
				partyUsers[i] = pParty->uid[i];

			for (int i = 0; i < MAX_PARTY_USERS; i++)
			{
				CUser * PartyUser = g_pMain->GetUserPtr(partyUsers[i]);
				
				if (PartyUser == nullptr)
					continue;

				if (!isInRangeSlow(PartyUser, RANGE_50M)
					|| PartyUser->isDead() || !PartyUser->isInGame())
					continue;

				if (PartyUser->isPlayer())
					PartyUser->QuestV2MonsterCountAdd(GetProtoID());
			}
		}
		else if (!pUser->isInParty() && pUser->isPlayer())
			pUser->QuestV2MonsterCountAdd(GetProtoID());
	}

	DateTime time;
	string pKillerPartyUsers;

	if (pUser->isInParty())
	{
		CUser *pPartyUser;
		_PARTY_GROUP *pParty = g_pMain->GetPartyPtr(pUser->GetPartyID());
		if (pParty)
		{
			for (int i = 0; i < MAX_PARTY_USERS; i++)
			{
				pPartyUser = g_pMain->GetUserPtr(pParty->uid[i]);
				if (pPartyUser)
					pKillerPartyUsers += string_format("%s,",pPartyUser->GetName().c_str());
			}
		}

		if (!pKillerPartyUsers.empty())
			pKillerPartyUsers = pKillerPartyUsers.substr(0,pKillerPartyUsers.length() - 1);
	}

	if (pKillerPartyUsers.empty())
		g_pMain->WriteDeathNpcLogFile(string_format("[ %s - %d:%d:%d ] Killer=%s,SID=%d,Target=%s,Zone=%d,X=%d,Z=%d\n",m_bMonster ? "MONSTER" : "NPC",time.GetHour(),time.GetMinute(),time.GetSecond(),pKiller->GetName().c_str(),GetProtoID(),GetName().c_str(),GetZoneID(),uint16(GetX()),uint16(GetZ())));
	else
		g_pMain->WriteDeathNpcLogFile(string_format("[ %s - %d:%d:%d ] Killer=%s,KillerParty=%s,SID=%d,Target=%s,Zone=%d,X=%d,Z=%d\n",m_bMonster ? "MONSTER" : "NPC",time.GetHour(),time.GetMinute(),time.GetSecond(),pKiller->GetName().c_str(),pKillerPartyUsers.c_str(),GetProtoID(),GetName().c_str(),GetZoneID(),uint16(GetX()),uint16(GetZ())));
}

/**
* @brief	Executes the Npc respawn.
*/
void CNpc::OnRespawn()
{
	if (GetProtoID() == BORDER_WAR_MONUMENT_SID
		|| (g_pMain->m_byBattleOpen == NATION_BATTLE
		&& (GetProtoID() == ELMORAD_MONUMENT_SID
		|| GetProtoID() == ASGA_VILLAGE_MONUMENT_SID
		|| GetProtoID() == RAIBA_VILLAGE_MONUMENT_SID
		|| GetProtoID() == DODO_CAMP_MONUMENT_SID
		|| GetProtoID() == LUFERSON_MONUMENT_SID
		|| GetProtoID() == LINATE_MONUMENT_SID
		|| GetProtoID() == BELLUA_MONUMENT_SID
		|| GetProtoID() == LAON_CAMP_MONUMENT_SID)))
	{
		_MONUMENT_INFORMATION * pData = new	_MONUMENT_INFORMATION();
		pData->sSid = GetProtoID();
		pData->sNid = m_sNid;
		pData->RepawnedTime = int32(UNIXTIME);

		if (GetProtoID() == DODO_CAMP_MONUMENT_SID || GetProtoID() == LAON_CAMP_MONUMENT_SID)
			g_pMain->m_bMiddleStatueNation = m_bNation; 

		if (!g_pMain->m_NationMonumentInformationArray.PutData(pData->sSid, pData))
			delete pData;
	}
	else if (g_pMain->m_bForgettenTempleIsActive && GetZoneID() == ZONE_FORGOTTEN_TEMPLE)
		g_pMain->m_ForgettenTempleMonsterList.insert(std::make_pair(GetID(), GetProtoID()));

}

/**
* @brief	Executes the death process.
*
* @param	pUser	The User.
* @param	MonsterCount The Respawn boss count.
*/
void CNpc::ChaosStoneProcess(CUser *pUser, uint16 MonsterCount)
{
	if (pUser == nullptr)
		return;

	std::vector<uint32> MonsterSpawned;
	std::vector<uint32> MonsterSpawnedFamily;
	bool bLoopBack = true;

	if (g_pMain->m_MonsterSummonListZoneArray.GetSize() > 0)
	{
		foreach_stlmap (itr, g_pMain->m_MonsterSummonListZoneArray)
		{
			if (itr->second->ZoneID == GetZoneID())
			{
				if (std::find(MonsterSpawned.begin(),MonsterSpawned.end(),itr->second->nIndex) == MonsterSpawned.end())
					MonsterSpawned.push_back(itr->second->nIndex);
			}
			else
				continue;
		}
	}
	if (MonsterSpawned.size() > 0)
	{
		for (uint8 i = 0; i < MonsterCount;i++)
		{
			uint32 nMonsterNum = myrand(0, (int32) MonsterSpawned.size());
			_MONSTER_SUMMON_LIST_ZONE * pMonsterSummonListZone = g_pMain->m_MonsterSummonListZoneArray.GetData(nMonsterNum);
			if (pMonsterSummonListZone != nullptr)
			{
				if (std::find(MonsterSpawnedFamily.begin(), MonsterSpawnedFamily.end(), pMonsterSummonListZone->byFamily) == MonsterSpawnedFamily.end())
				{
					g_pMain->SpawnEventNpc(pMonsterSummonListZone->sSid, true, GetZoneID(), GetX(), GetY(), GetZ(), 0, 1, CHAOS_STONE_MONSTER_RESPAWN_RADIUS, CHAOS_STONE_MONSTER_LIVE_TIME, GetNation(), GetID());
					MonsterSpawnedFamily.push_back(pMonsterSummonListZone->byFamily);
					bLoopBack = false;
				}
			}
			if (bLoopBack)
				i--;
			else
				bLoopBack = true;
		}
	}
}

/*
* @brief	Executes the pvp monument process.
*
* @param	pUser	The User.
*/
void CNpc::PVPMonumentProcess(CUser *pUser)
{
	if (pUser == nullptr)
		return;

	Packet result(WIZ_CHAT, uint8(MONUMENT_NOTICE));
	result << uint8(FORCE_CHAT) << pUser->GetNation() << pUser->GetName().c_str();
	g_pMain->Send_Zone(&result, GetZoneID(), nullptr, Nation::ALL);

	g_pMain->m_nPVPMonumentNation[pUser->GetZoneID()] = pUser->GetNation();
	g_pMain->NpcUpdate(GetProtoID(), m_bMonster, pUser->GetNation(), pUser->GetNation() == KARUS ? MONUMENT_KARUS_SPID : MONUMENT_ELMORAD_SPID);
}

/*
* @brief	Executes the bdw monument process.
*
* @param	pUser	The User.
*/
void CNpc::BDWMonumentProcess(CUser *pUser)
{
	if (pUser == nullptr)
		return;

	if( pUser->GetZoneID() != ZONE_BORDER_DEFENSE_WAR ||
		g_pMain->pTempleEvent.m_sBorderMonumentNation[pUser->GetEventRoom()] == pUser->GetNation())
		return;

	Packet result;
	pUser->m_tBorderCapure = UNIXTIME;
	uint16 nSeconds = 360;

	g_pMain->pTempleEvent.m_sBorderMonumentTimer[pUser->GetEventRoom()] = nSeconds;
	g_pMain->pTempleEvent.m_sBorderMonumentNation[pUser->GetEventRoom()] = pUser->GetNation();
	g_pMain->pTempleEvent.m_MonumentFinish[pUser->GetEventRoom()] = true;

	pUser->GetNation() == ELMORAD ? g_pMain->pTempleEvent.ElmoDeathCount[pUser->GetEventRoom()] = g_pMain->pTempleEvent.ElmoDeathCount[pUser->GetEventRoom()] + 2 : g_pMain->pTempleEvent.KarusDeathCount[pUser->GetEventRoom()] = g_pMain->pTempleEvent.KarusDeathCount[pUser->GetEventRoom()] + 2;

	if (pUser->isInParty())
	{
		_PARTY_GROUP * pParty = g_pMain->GetPartyPtr(pUser->GetPartyID());

		if (pParty == nullptr)
			return;

		short partyUsers[MAX_PARTY_USERS];

		for (int i = 0; i < MAX_PARTY_USERS; i++)
			partyUsers[i] = pParty->uid[i];

		for (int i = 0; i < MAX_PARTY_USERS; i++)
		{
			CUser * pTUser = g_pMain->GetUserPtr(partyUsers[i]);
			if (pTUser == nullptr)
				continue;

			if (pUser->GetEventRoom() != pTUser->GetEventRoom())
				continue;

				pTUser->m_iLoyaltyDaily += 1 ;
				pTUser->UpdatePlayerRank();
		}
	}

	if (!pUser->isInParty())
	{
		pUser->m_iLoyaltyDaily += 1 ;
		pUser->UpdatePlayerRank();
	}
	result.Initialize(WIZ_EVENT);
	result << uint8(TEMPLE_EVENT_BORDER_COUNTER);
	result << g_pMain->pTempleEvent.KarusDeathCount[pUser->GetEventRoom()] << uint16(0x00) << g_pMain->pTempleEvent.ElmoDeathCount[pUser->GetEventRoom()] << uint16(0x00);
	g_pMain->SpawnEventNpc(GetProtoID(), m_bMonster, GetZoneID(), GetX(), GetY(), GetZ(), (uint8) m_byDirection, 1, 0, 0, pUser->GetNation(), pUser->GetSocketID(), pUser->GetEventRoom());
	g_pMain->Send_Zone(&result, GetZoneID(), pUser, Nation::ALL,pUser->GetEventRoom());
}

/*
* @brief	Executes the battle monument process.
*
* @param	pUser	The User.
*/
void CNpc::BattleMonumentProcess(CUser *pUser)
{
	if (pUser == nullptr)
		return;

	if (g_pMain->m_byBattleOpen == NATION_BATTLE)
	{
		g_pMain->NpcUpdate(GetProtoID(), m_bMonster, pUser->GetNation(), pUser->GetNation() == KARUS ? MONUMENT_KARUS_SPID : MONUMENT_ELMORAD_SPID);
		g_pMain->Announcement(DECLARE_BATTLE_MONUMENT_STATUS, Nation::ALL, m_byTrapNumber, pUser);

		if (pUser->GetNation() == KARUS)
		{	
			g_pMain->m_sKarusMonumentPoint +=2;
			g_pMain->m_sKarusMonuments++;

			if (g_pMain->m_sKarusMonuments >= 7)
				g_pMain->m_sKarusMonumentPoint +=10;

			if (g_pMain->m_sElmoMonuments != 0)
				g_pMain->m_sElmoMonuments--;
		}
		else
		{
			g_pMain->m_sElmoMonumentPoint += 2;
			g_pMain->m_sElmoMonuments++;

			if (g_pMain->m_sElmoMonuments >= 7)
				g_pMain->m_sElmoMonumentPoint +=10;

			if (g_pMain->m_sKarusMonuments != 0)
				g_pMain->m_sKarusMonuments--;
		}

		if (pUser->GetZoneID() == ZONE_BATTLE4 && g_pMain->isWarOpen())
		{
			uint32	m_byNereidsIslandRemainingTime = 0;

			if (g_pMain->m_sKarusMonuments >= 7 || g_pMain->m_sElmoMonuments >= 7)
				m_byNereidsIslandRemainingTime = 900;
			else
				m_byNereidsIslandRemainingTime = g_pMain->m_byBattleRemainingTime / 2;

			Packet result(WIZ_MAP_EVENT, uint8(6)); // time left
			result << uint16(m_byNereidsIslandRemainingTime) << uint16(0);
			g_pMain->Send_All(&result, nullptr, 0, ZONE_BATTLE4);

			g_pMain->m_sNereidsIslandMonuArray[m_byTrapNumber-1] = pUser->GetNation();
			result.clear();
			result.Initialize(WIZ_MAP_EVENT);
			result << uint8(0) << uint8(7);

			for(int i = 0; i < 7; i++)
				result << g_pMain->m_sNereidsIslandMonuArray[i];

			g_pMain->Send_All(&result, nullptr, 0, ZONE_BATTLE4);
		}
	}
}

/*
* @brief  Executes the nation monument process.
*
* @param  pUser  The User.
*/
void CNpc::NationMonumentProcess(CUser *pUser)
{
	if (pUser == nullptr)
		return;

	if (g_pMain->m_byBattleOpen == NATION_BATTLE)
	{
		g_pMain->NpcUpdate(GetProtoID(), m_bMonster, pUser->GetNation());
		g_pMain->Announcement(DECLARE_NATION_MONUMENT_STATUS, Nation::ALL, GetProtoID(), pUser);

		uint16 sSid = 0;

		foreach_stlmap (itr, g_pMain->m_NationMonumentInformationArray)
			if (itr->second->sSid == (pUser->GetNation() == KARUS ? GetProtoID() + 10000 : GetProtoID() - 10000))
				sSid = itr->second->sSid;

		if (sSid != 0)
			g_pMain->m_NationMonumentInformationArray.DeleteData(sSid);
	}
	else
	{
		g_pMain->NpcUpdate(GetProtoID(), m_bMonster, pUser->GetNation());

		uint16 sSid = 0;

		foreach_stlmap (itr, g_pMain->m_NationMonumentInformationArray)
			if (itr->second->sSid == (pUser->GetNation() == KARUS ? GetProtoID() + 10000 : GetProtoID() - 10000))
				sSid = itr->second->sSid;

		if (sSid != 0)
			g_pMain->m_NationMonumentInformationArray.DeleteData(sSid);
	}
}

void CNpc::HPTimeChangeType3()
{
	if (isDead()
		|| !m_bType3Flag)
		return;

	uint16	totalActiveDurationalSkills = 0, 
		totalActiveDOTSkills = 0;

	bool bIsDOT = false;
	for (int i = 0; i < MAX_TYPE3_REPEAT; i++)
	{
		MagicType3 * pEffect = &m_durationalSkills[i];
		if (!pEffect->m_byUsed)
			continue;

		// Has the required interval elapsed before using this skill?
		if ((UNIXTIME - pEffect->m_tHPLastTime) >= pEffect->m_bHPInterval)
		{
			Unit * pUnit = g_pMain->GetUnitPtr(pEffect->m_sSourceID);

			if(pUnit == nullptr)
				continue;

			// Reduce the HP 
			HpChange(pEffect->m_sHPAmount, pUnit); // do we need to specify the source of the DOT?
			pEffect->m_tHPLastTime = UNIXTIME;

			if (pEffect->m_sHPAmount < 0)
				bIsDOT = true;

			// Has the skill expired yet?
			if (++pEffect->m_bTickCount == pEffect->m_bTickLimit)
			{
				Packet result(WIZ_MAGIC_PROCESS, uint8(MAGIC_DURATION_EXPIRED));

				// Healing-over-time skills require the type 100
				if (pEffect->m_sHPAmount > 0)
					result << uint8(100);
				else // Damage-over-time requires 200.
					result << uint8(200);

				SendToRegion(&result);

				pEffect->Reset();
			}
		}

		if (pEffect->m_byUsed)
		{
			totalActiveDurationalSkills++;
			if (pEffect->m_sHPAmount < 0)
				totalActiveDOTSkills++;
		}
	}

	// Have all the skills expired?
	if (totalActiveDurationalSkills == 0)
		m_bType3Flag = false;
}

void CNpc::Type4Duration()
{
	Guard lock(m_buffLock);
	if (m_buffMap.empty())
		return;

	foreach (itr, m_buffMap)
	{
		if (itr->second.m_tEndTime > UNIXTIME)
			continue;

		CMagicProcess::RemoveType4Buff(itr->first, this, true, false);
		break; // only ever handle one at a time with the current logic
	}
}
