#include "stdafx.h"
#include "KnightsManager.h"
#include "../shared/DateTime.h"

void CUser::QuestDataRequest()
{
	DateTime time;

	// Sending this now is probably wrong, but it's cleaner than it was before.
	Packet result(WIZ_QUEST, uint8(1));
	result << uint16(m_QuestMap.size());
	
	foreach (itr, m_QuestMap)
		result << itr->first << itr->second;
	Send(&result);

	foreach (itr, s_QuestMap)
		QuestV2MonsterDataRequest(itr->second->sEventIndex);

	result.clear();
	result.Initialize(WIZ_QUEST);
	result << uint8(8) << time.GetYear() 
		<< time.GetMonth() << time.GetDay() 
		<< time.GetHour() << time.GetMinute() 
		<< time.GetSecond();
	Send(&result);
}

void CUser::QuestV2PacketProcess(Packet & pkt)
{
	uint8 opcode = pkt.read<uint8>();
	uint32 nQuestID = pkt.read<uint32>();

	CNpc *pNpc = g_pMain->GetNpcPtr(m_sEventNid);
	_QUEST_HELPER * pQuestHelper = g_pMain->m_QuestHelperArray.GetData(nQuestID);

	// Does this quest helper exist?
	if (pQuestHelper == nullptr)
		return;

	// If we're the same min. level as the quest requires, 
	// do we have the min. required XP? Seems kind of silly, but OK..
	if (pQuestHelper->bLevel == GetLevel()
		&& pQuestHelper->nExp > m_iExp)
		return;

	switch (opcode)
	{
	case 1:		// ?
		break;	
	case 2:		// Quest Seed
		break;	
	case 3:		// Quest Notify
	case 7:		// Quest Select 
		QuestV2ExecuteHelper(pQuestHelper);
		break;
	case 4:
		QuestV2CheckFulfill(pQuestHelper);
		break;
	case 5:		// Quest Delete
		if (!CheckExistEvent(pQuestHelper->sEventDataIndex, 2))
			SaveEvent(pQuestHelper->sEventDataIndex, 4);

		QuestV2MonsterDataDeleteAll(pQuestHelper->sEventDataIndex);
		QuestV2MonsterDataRequest(pQuestHelper->sEventDataIndex);

		// Kick the user out of the quest zone.
		// Monster suppression squad is the only zone I'm aware of that this should apply to.
		if (GetZoneID() >= 81 && GetZoneID() <= 83)
			KickOutZoneUser(true);
		break;
	case 6:
		if (!CheckExistEvent(pQuestHelper->sEventDataIndex, 2))
			SaveEvent(pQuestHelper->sEventDataIndex, 1);
		break;	
	case 8:		// ?
		break;	
	case 9:		// Quest Change
		break;
	case 10:	// NP Change
		break;	
	case 12:
		if (!CheckExistEvent(pQuestHelper->sEventDataIndex, 1))
			SaveEvent(pQuestHelper->sEventDataIndex, 1);
		break;	
	default:
		printf("Uncatched Num: %d\n", opcode);
		break;
	}
}

void CUser::SaveEvent(uint16 sQuestID, uint8 bQuestState)
{
	if (bQuestState == 1 && GetActiveQuestID())
		return;

	m_QuestMap[sQuestID] = bQuestState;

	// Don't need to handle special/kill quests any further
	if (sQuestID >= QUEST_KILL_GROUP1)
		return;

	Packet result(WIZ_QUEST, uint8(2));
	result << sQuestID << bQuestState;
	Send(&result);

	if (bQuestState == 2)
	{
		QuestV2MonsterDataDeleteAll(sQuestID);
		QuestV2MonsterDataRequest(sQuestID);
	}

	if (bQuestState == 1)
	{
		QuestDataList::iterator itr = s_QuestMap.find(sQuestID);

		if (itr == s_QuestMap.end())
		{
			_QUEST_DATA * pKillData = new _QUEST_DATA();

			pKillData->sEventIndex = sQuestID;

			for (int i = 0; i < QUEST_MOBS_PER_GROUP; i++)
				pKillData->sKillCount[i] = 0;

			s_QuestMap.insert(std::make_pair(pKillData->sEventIndex, pKillData));
		}
		QuestV2MonsterDataRequest(sQuestID);
	}
}

void CUser::DeleteEvent(uint16 sQuestID)
{
	m_QuestMap.erase(sQuestID);
	s_QuestMap.erase(sQuestID);
}

bool CUser::CheckExistEvent(uint16 sQuestID, uint8 bQuestState)
{
	// Attempt to find a quest with that ID in the map
	QuestMap::iterator itr = m_QuestMap.find(sQuestID);

	// If it doesn't exist, it doesn't exist. 
	// Unless of course, we wanted it to not exist, in which case we're right!
	// (this does seem silly, but this behaviour is STILL expected, so do not remove it.)
	if (itr == m_QuestMap.end())
		return bQuestState == 0;
		
	return itr->second == bQuestState;
}

uint8 CUser::GetQuestStatus(uint16 QuestID)
{
	return m_QuestMap[QuestID];
}

void CUser::QuestV2MonsterCountAdd(uint16 sNpcID)
{
	foreach (itr, s_QuestMap) // Bütün aktif görevleri kontrol edelim. gerekiyorsa ekleyelim.
	{
		_QUEST_DATA * pQuestKillMap = itr->second;

		if (pQuestKillMap == nullptr)
			continue;

		uint16 sQuestNum = pQuestKillMap->sEventIndex; // Görev numarasýný aldýk.
		_QUEST_MONSTER *pQuestMonster = g_pMain->m_QuestMonsterArray.GetData(sQuestNum); // Quest monster listesini aldýk.

		if (pQuestMonster == nullptr)  // Eðer yaratýk bilgileri alýnamadýysa atla
			continue;

		// Kontroller saðlandý kesilen yaratýk görev için gerekli ise sayýsýný arttýr client'e bildir.
		AddCountForKillMap(sNpcID, sQuestNum, pQuestMonster, pQuestKillMap);
	}
}

void CUser::AddCountForKillMap(uint16 sNpcID, uint16 sQuestNum, _QUEST_MONSTER * pQuestMonster, _QUEST_DATA * pQuestKillMap)
{
	bool StatusCheck[4];
	memset(StatusCheck, false, sizeof(StatusCheck));

	// TODO: Implement obscure zone ID logic
	for (int group = 0; group < QUEST_MOB_GROUPS; group++)
	{
		for (int i = 0; i < QUEST_MOBS_PER_GROUP; i++)
		{
			if (pQuestMonster->sNum[group][i] != sNpcID)
				continue;

			if (pQuestKillMap->sKillCount[group] + 1 > pQuestMonster->sCount[group])
				return;

			pQuestKillMap->sKillCount[group]++;
			Packet result(WIZ_QUEST, uint8(9));
			result << uint8(2) << uint16(sQuestNum) << uint8(group + 1) << pQuestKillMap->sKillCount[group];
			Send(&result);

			if (pQuestKillMap->sKillCount[group] >= pQuestMonster->sCount[group])
				StatusCheck[group] = true;
		}
	}

	if ((pQuestMonster->sCount[0] <= 0 || StatusCheck[0]) 
		&& (pQuestMonster->sCount[1] <= 0 || StatusCheck[1]) 
		&& (pQuestMonster->sCount[2] <= 0 || StatusCheck[2]) 
		&& 	(pQuestMonster->sCount[3] <= 0 || StatusCheck[3]))
		SaveEvent(sQuestNum, 3); //sabit numara
}

uint8 CUser::QuestV2CheckMonsterCount(uint16 sQuestID, uint8 sRate)
{
	QuestDataList::iterator itr = s_QuestMap.find(sQuestID);

	if(itr == s_QuestMap.end())
		return 0;
	
	return (uint8)itr->second->sKillCount[sRate-1];
}

void CUser::QuestV2MonsterDataDeleteAll(uint16 sQuestID)
{
	s_QuestMap.erase(sQuestID);

	for (int i = QUEST_KILL_GROUP1; i <= QUEST_KILL_GROUP7; i++)
		DeleteEvent(i);
}

void CUser::QuestV2MonsterDataRequest(uint16 sQuestID)
{
	Packet result(WIZ_QUEST, uint8(9));

	QuestDataList::iterator itr = s_QuestMap.find(sQuestID);

	if (itr == s_QuestMap.end())
		return;

	result << uint8(1) << itr->first;

	for (int i = 0; i < QUEST_MOBS_PER_GROUP; i++)
		result << uint8(itr->second->sKillCount[i]);

	Send(&result);
}

void CUser::QuestV2ExecuteHelper(_QUEST_HELPER * pQuestHelper)
{
	if (pQuestHelper == nullptr && pQuestHelper->bQuestType != 3)
		return;

	QuestV2RunEvent(pQuestHelper, pQuestHelper->nEventTriggerIndex); // NOTE: Fulfill will use nEventCompleteIndex
}

void CUser::QuestV2CheckFulfill(_QUEST_HELPER * pQuestHelper)
{
	if (pQuestHelper == nullptr || !CheckExistEvent(pQuestHelper->sEventDataIndex, 1))
		return;

	QuestV2RunEvent(pQuestHelper, pQuestHelper->nEventCompleteIndex);
}

bool CUser::QuestV2RunEvent(_QUEST_HELPER * pQuestHelper, uint32 nEventID, int8 bSelectedReward)
{
	// Lookup the corresponding NPC.
	if (pQuestHelper->strLuaFilename == "01_main.lua")
	{
		m_sEventNid = 10000;
	}

	CNpc * pNpc = g_pMain->GetNpcPtr(m_sEventNid);
	bool result = false;

	// Make sure the NPC exists and is not dead (we should also check if it's in range)
	if (pNpc == nullptr || pNpc->isDead())
		return false;

	// Increase the NPC's reference count to ensure it doesn't get freed while executing a script
	pNpc->IncRef();

	m_nQuestHelperID = pQuestHelper->nIndex;
	result = g_pMain->GetLuaEngine()->ExecuteScript(this, pNpc, nEventID, bSelectedReward, pQuestHelper->strLuaFilename.c_str());

	// Decrease it now that we've finished with it + free if necessary
	pNpc->DecRef();

	return result;
}

/* 
These are called by quest scripts. 
*/

void CUser::QuestV2SaveEvent(uint16 sQuestID)
{
	_QUEST_HELPER * pQuestHelper = g_pMain->m_QuestHelperArray.GetData(sQuestID);
	
	if (pQuestHelper == nullptr)
		return;

	SaveEvent(pQuestHelper->sEventDataIndex, pQuestHelper->bEventStatus);
}

void CUser::QuestV2SendNpcMsg(uint32 nQuestID, uint16 sNpcID)
{
	Packet result(WIZ_QUEST, uint8(7));
	result << nQuestID << sNpcID;
	Send(&result);
}

void CUser::QuestV2ShowGiveItem(uint32 nUnk1, uint32 sUnk1, 
								uint32 nUnk2, uint32 sUnk2,
								uint32 nUnk3, uint32 sUnk3,
								uint32 nUnk4, uint32 sUnk4,
								uint32 nUnk5, uint32 sUnk5)
{
	Packet result(WIZ_QUEST, uint8(10));
	result	<< nUnk1 << sUnk1
			<< nUnk2 << sUnk2
			<< nUnk3 << sUnk3
			<< nUnk4 << sUnk4
			<< nUnk5 << sUnk5;
	Send(&result);
}

uint16 CUser::QuestV2SearchEligibleQuest(uint16 sNpcID)
{
	Guard lock(g_pMain->m_questNpcLock);
	QuestNpcList::iterator itr = g_pMain->m_QuestNpcList.find(sNpcID);
	
	if (itr == g_pMain->m_QuestNpcList.end() || itr->second.empty())
		return 0;

	// Loop through all the QuestHelper instances attached to that NPC.
	foreach (itr2, itr->second)
	{
		_QUEST_HELPER * pHelper = (*itr2);
		if (pHelper->bLevel > GetLevel()
			|| (pHelper->bLevel == GetLevel() && pHelper->nExp > m_iExp)
			|| (pHelper->bClass != 5 && !JobGroupCheck(pHelper->bClass))
			|| (pHelper->bNation != 3 && pHelper->bNation != GetNation())
			|| (pHelper->sEventDataIndex == 0)
			|| (pHelper->bEventStatus < 0 || CheckExistEvent(pHelper->sEventDataIndex, 2)) //gorev tamamlanmamis ise
			|| !CheckExistEvent(pHelper->sEventDataIndex, pHelper->bEventStatus)) //gorev tamamlanma kontrolu
			continue;
		
		return 2;
	}
	
	return 0;
}

void CUser::QuestV2ShowMap(uint32 nQuestHelperID)
{
	Packet result(WIZ_QUEST, uint8(11));
	result << nQuestHelperID;
	Send(&result);
}

uint8 CUser::CheckMonsterCount(uint8 bGroup)
{
	_QUEST_MONSTER * pQuestMonster = g_pMain->m_QuestMonsterArray.GetData(m_sEventDataIndex);
	
	if (pQuestMonster == nullptr || bGroup == 0 || bGroup >= QUEST_MOB_GROUPS)
		return 0;

	return m_bKillCounts[bGroup];
}

// First job change; you're a [novice], Harry!
bool CUser::PromoteUserNovice()
{
	uint8 bNewClasses[] = { ClassWarriorNovice, ClassRogueNovice, ClassMageNovice, ClassPriestNovice };
	uint8 bOldClass = GetClassType() - 1; // convert base class 1,2,3,4 to 0,1,2,3 to align with bNewClasses

	// Make sure it's a beginner class.
	if (!isBeginner())
		return false;

	Packet result(WIZ_CLASS_CHANGE, uint8(6));

	// Build the new class.
	uint16 sNewClass = (GetNation() * 100) + bNewClasses[bOldClass];
	result << sNewClass << GetID();
	SendToRegion(&result);

	// Change the class & update party.
	result.clear();
	result << uint8(2) << sNewClass;
	ClassChange(result, false); // TODO: Clean this up. Shouldn't need to build a packet for this.

	// Update the clan.
	result.clear();
	result << uint16(0);
	CKnightsManager::CurrentKnightsMember(this, result); // TODO: Clean this up too.
	return true;
}

// From novice to master.
bool CUser::PromoteUser()
{
	/* unlike the official, the checks & item removal should be handled in the script, not here */
	uint8 bOldClass = GetClassType();

	// We must be a novice before we can be promoted to master.
	if (!isNovice()) 
		return false;

	Packet result(WIZ_CLASS_CHANGE, uint8(6));

	// Build the new class.
	uint16 sNewClass = (GetNation() * 100) + bOldClass + 1;
	result << sNewClass << GetID();
	SendToRegion(&result);

	// Change the class & update party.
	result.clear();
	result << uint8(2) << sNewClass;
	ClassChange(result, false); // TODO: Clean this up. Shouldn't need to build a packet for this.

	// use integer division to get from 5/7/9/11 (novice classes) to 1/2/3/4 (base classes)
	uint8 bBaseClass = (bOldClass / 2) - 1; 

	// this should probably be moved to the script
	SaveEvent(bBaseClass, 2); 

	// Update the clan.
	result.clear();
	result << uint16(0);
	CKnightsManager::CurrentKnightsMember(this, result); // TODO: Clean this up too.
	return true;
}

void CUser::PromoteClan(ClanTypeFlag byFlag)
{
	if (!isInClan())
		return;

	CKnightsManager::UpdateKnightsGrade(GetClanID(), byFlag);
}

void CUser::SendClanPointChange(int32 nChangeAmount)
{
	if (!isInClan())
		return;

	CKnightsManager::UpdateClanPoint(GetClanID(), nChangeAmount);
}

uint8 CUser::GetClanGrade()
{
	if (!isInClan())
		return 0;

	CKnights * pClan = g_pMain->GetClanPtr(GetClanID());
	if (pClan == nullptr)
		return 0;

	return pClan->m_byGrade;
}

uint32 CUser::GetClanPoint()
{
	if (!isInClan())
		return 0;

	CKnights * pClan = g_pMain->GetClanPtr(GetClanID());
	if (pClan == nullptr)
		return 0;

	return pClan->m_nClanPointFund;
}

uint8 CUser::GetClanRank()
{
	if (!isInClan())
		return ClanTypeNone;

	CKnights * pClan = g_pMain->GetClanPtr(GetClanID());
	if (pClan == nullptr)
		return ClanTypeNone;

	return pClan->m_byFlag;
}

uint8 CUser::GetBeefRoastVictory()
{
	if( g_pMain->m_sBifrostTime <= 90 * MINUTE && g_pMain->m_BifrostVictory != ALL )
		return g_pMain->m_sBifrostVictoryAll; 
	else
		return g_pMain->m_BifrostVictory; 
}

uint8 CUser::GetWarVictory() { return g_pMain->m_bVictory; }

uint8 CUser::CheckMiddleStatueCapture() { return g_pMain->m_bMiddleStatueNation == GetNation() ? 1 : 0; }

void CUser::MoveMiddleStatue() { Warp((uint16)((GetNation() == KARUS ? DODO_CAMP_WARP_X : LAON_CAMP_WARP_X) + myrand(0, DODO_LAON_WARP_RADIUS)) * 10,(uint16)((GetNation() == KARUS ? DODO_CAMP_WARP_Z : LAON_CAMP_WARP_Z) + myrand(0, DODO_LAON_WARP_RADIUS)) * 10); }

uint8 CUser::GetPVPMonumentNation() { return g_pMain->m_nPVPMonumentNation[GetZoneID()]; }