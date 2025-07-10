#include "stdafx.h"
#include "Map.h"
#include "KnightsManager.h"
#include "KingSystem.h"
using std::string;

void CUser::ItemRepair(Packet & pkt)
{
	if (isDead())
		return;

	Packet result(WIZ_ITEM_REPAIR);
	uint32 money, itemid;
	uint16 durability, quantity, sNpcID;
	_ITEM_TABLE* pTable = nullptr;
	CNpc *pNpc = nullptr;
	uint8 sPos, sSlot;

	pkt >> sPos >> sSlot >> sNpcID >> itemid;
	if (sPos == 1 ) 
	{	// SLOT
		if (sSlot >= SLOT_MAX) 
			goto fail_return;

		if (m_sItemArray[sSlot].nNum != itemid) 
			goto fail_return;
	}
	else if (sPos == 2 ) 
	{	// INVEN
		if (sSlot >= HAVE_MAX) 
			goto fail_return;

		if (m_sItemArray[SLOT_MAX+sSlot].nNum != itemid)
			goto fail_return;
	}

	pNpc = g_pMain->GetNpcPtr(sNpcID);
	if (pNpc == nullptr || !isInRange(pNpc, MAX_NPC_RANGE))
		return;

	if (pNpc->GetType() == NPC_TINKER || pNpc->GetType() == NPC_MERCHANT)
	{
		pTable = g_pMain->GetItemPtr( itemid );
		if (pTable == nullptr
			|| pTable->m_iSellPrice == SellTypeNoRepairs) 
			goto fail_return;

		durability = pTable->m_sDuration;
		if(durability == 1) 
			goto fail_return;

		if(sPos == 1)
			quantity = pTable->m_sDuration - m_sItemArray[sSlot].sDuration;
		else if(sPos == 2) 
			quantity = pTable->m_sDuration - m_sItemArray[SLOT_MAX+sSlot].sDuration;

		money = (unsigned int)((((pTable->m_iBuyPrice-10) / 10000.0f) + pow((float)pTable->m_iBuyPrice, 0.75f)) * quantity / (double)durability);

		if (GetPremiumProperty(PremiumRepairDiscountPercent) > 0)
			money = money * GetPremiumProperty(PremiumRepairDiscountPercent) / 100;

		if (!GoldLose(money, false))
			goto fail_return;

		if (sPos == 1)
			m_sItemArray[sSlot].sDuration = durability;
		else if(sPos == 2)
			m_sItemArray[SLOT_MAX+sSlot].sDuration = durability;

		SetUserAbility(true);
		result << uint8(1) << GetCoins();
		Send(&result);
		return;
	}
	
fail_return:
	result << uint8(0) << GetCoins();
	Send(&result);
}

void CUser::ClientEvent(uint16 sNpcID)
{
	// Ensure AI's loaded
	if (!g_pMain->m_bPointCheckFlag
		|| isDead())
		return;

	int32 iEventID = 0;
	CNpc *pNpc = g_pMain->GetNpcPtr(sNpcID);
	if (pNpc == nullptr
		|| !isInRange(pNpc, MAX_NPC_RANGE))
		return;

	m_sEventNid = sNpcID;
	m_sEventSid = pNpc->GetProtoID(); // For convenience purposes with Lua scripts.

	if (pNpc->GetProtoID() == SAW_BLADE_SSID)
	{
		if (GetZoneID() == ZONE_CHAOS_DUNGEON)
			HpChange(-5000 / 10);
		else if (GetZoneID() == ZONE_KROWAZ_DOMINION)
			HpChange(-370, pNpc);
		return;
	}
	else if (pNpc->GetProtoID() == CHAOS_CUBE_SSID && !pNpc->isDead())
	{ 
		uint8 nEventRoomUserCount = g_pMain->TempleEventGetRoomUsers(GetEventRoom());
		uint8 nItemRewardRankFirst = nEventRoomUserCount / 3;
		uint8 nItemRewardRankSecond = (nEventRoomUserCount  - 1) * 2;

		int32 nUserRank = GetPlayerRank(RANK_TYPE_CHAOS_DUNGEON);
		uint32 nItemID = 0;

		if (nUserRank == 0)
			nItemID = ITEM_KILLING_BLADE;
		else if (nUserRank < nItemRewardRankFirst)
			nItemID = ITEM_LIGHT_PIT;
		else if (nUserRank >= nItemRewardRankFirst && nUserRank <= nItemRewardRankSecond)
			nItemID = ITEM_DRAIN_RESTORE;
		else if (nUserRank > nItemRewardRankSecond)
			nItemID = ITEM_KILLING_BLADE;

		GiveItem(nItemID,1);
		g_pMain->ShowNpcEffect(GetSocketID(),251,GetZoneID());
		g_pMain->KillNpc(sNpcID);
		return;
	}
	else if (pNpc->GetType() == NPC_KISS)
	{
		KissUser();
		return;
	}
	else if (pNpc->GetType() == NPC_ROLLINGSTONE)
	{
		HpChange(-GetMaxHealth(), pNpc);
		return;
	}
	Guard lock(g_pMain->m_questNpcLock);
	QuestNpcList::iterator itr = g_pMain->m_QuestNpcList.find(pNpc->GetProtoID());
	if (itr == g_pMain->m_QuestNpcList.end())
		return;

	QuestHelperList & pList = itr->second;
	_QUEST_HELPER * pHelper = nullptr;
	foreach (itr, pList)
	{
		if ((*itr) == nullptr
			|| (*itr)->sEventDataIndex
			|| (*itr)->bEventStatus
			|| ((*itr)->bNation != 3 && (*itr)->bNation != GetNation())
			|| ((*itr)->bClass != 5 && !JobGroupCheck((*itr)->bClass)))
			continue;

		pHelper = (*itr);
		break;
	}

	if (pHelper == nullptr)
		return;

	QuestV2RunEvent(pHelper, pHelper->nEventTriggerIndex);
}

void CUser::KissUser()
{
	Packet result(WIZ_KISS);
	result << uint32(GetID()) << m_sEventNid;
	GiveItem(910014000); // aw, you got a 'Kiss'. How literal.
	SendToRegion(&result);
}

void CUser::ClassChange(Packet & pkt, bool bFromClient /*= true */)
{
	Packet result(WIZ_CLASS_CHANGE);
	bool bSuccess = false;
	uint8 opcode = pkt.read<uint8>();
	if (opcode == CLASS_CHANGE_REQ)	
	{
		ClassChangeReq();
		return;
	}
	else if (opcode == ALL_POINT_CHANGE)	
	{
		AllPointChange(false);
		return;
	}
	else if (opcode == ALL_SKILLPT_CHANGE)	
	{
		AllSkillPointChange(false);
		return;
	}
	else if (opcode == CHANGE_MONEY_REQ)	
	{
		uint8 sub_type = pkt.read<uint8>(); // type is irrelevant
		uint32 money = (uint32)pow((GetLevel() * 2.0f), 3.4f);

		if (GetLevel() < 30)	
			money = (uint32)(money * 0.4f);
		else if (GetLevel() >= 60)
			money = (uint32)(money * 1.5f);

		// If nation discounts are enabled (1), and this nation has won the last war, get it half price.
		// If global discounts are enabled (2), everyone can get it for half price.
		if ((g_pMain->m_sDiscount == 1 && g_pMain->m_byOldVictory == GetNation())
			|| g_pMain->m_sDiscount == 2)
			money /= 2;

		result << uint8(CHANGE_MONEY_REQ) << money;
		Send(&result);
		return;
	}
	// If this packet was sent from the client, ignore it.
	else if (bFromClient)
		return;

	uint8 classcode = pkt.read<uint8>();
	switch (m_sClass)
	{
	case KARUWARRIOR:
		if( classcode == BERSERKER || classcode == GUARDIAN )
			bSuccess = true;
		break;
	case KARUROGUE:
		if( classcode == HUNTER || classcode == PENETRATOR )
			bSuccess = true;
		break;
	case KARUWIZARD:
		if( classcode == SORSERER || classcode == NECROMANCER )
			bSuccess = true;
		break;
	case KARUPRIEST:
		if( classcode == SHAMAN || classcode == DARKPRIEST )
			bSuccess = true;
		break;
	case ELMORWARRRIOR:
		if( classcode == BLADE || classcode == PROTECTOR )
			bSuccess = true;
		break;
	case ELMOROGUE:
		if( classcode == RANGER || classcode == ASSASSIN )
			bSuccess = true;
		break;
	case ELMOWIZARD:
		if( classcode == MAGE || classcode == ENCHANTER )
			bSuccess = true;
		break;
	case ELMOPRIEST:
		if( classcode == CLERIC || classcode == DRUID )
			bSuccess = true;
		break;
	case BERSERKER:
		if (classcode == GUARDIAN)
			bSuccess = true;
    break;
	case HUNTER:
		if (classcode == PENETRATOR)
		  bSuccess = true;
    break;
	case SORSERER:
		if (classcode == NECROMANCER)
		  bSuccess = true;
    break;
	case SHAMAN:
		if (classcode == DARKPRIEST)
		  bSuccess = true;
    break;
	case BLADE:
		if (classcode == PROTECTOR)
		  bSuccess = true;
    break;
	case RANGER:
		if (classcode == ASSASSIN)
		  bSuccess = true;
    break;
	case MAGE:
		if (classcode == ENCHANTER)
		  bSuccess = true;
    break;
	case CLERIC:
		if (classcode == DRUID)
		  bSuccess = true;
    break;

	}

	// Not allowed this job change
	if (!bSuccess)
	{
		result << uint8(CLASS_CHANGE_RESULT) << uint8(0);
		Send(&result);
		return;
	}

	m_sClass = classcode;
	if (isInParty())
	{
		// TO-DO: Move this somewhere better.
		result.SetOpcode(WIZ_PARTY);
		result << uint8(PARTY_CLASSCHANGE) << GetSocketID() << uint16(classcode);
		g_pMain->Send_PartyMember(GetPartyID(), &result);
	}
}

void CUser::RecvSelectMsg(Packet & pkt)	// Receive menu reply from client.
{
	bMenuID = pkt.read<uint8>();
	string szLuaFilename;
	bySelectedReward = -1;
	pkt.SByte();
	pkt >> szLuaFilename >> bySelectedReward;

	if (!AttemptSelectMsg(bMenuID, bySelectedReward))
		memset(&m_iSelMsgEvent, -1, sizeof(m_iSelMsgEvent));
}

void CUser::RecvMapEvent(Packet & pkt)
{
	uint8 bType = pkt.read<uint8>();
	
	Packet result;
	result.Initialize(WIZ_MAP_EVENT);

	if (g_pMain->isWarOpen() && GetZoneID() == ZONE_BATTLE4)
		result << bType << g_pMain->m_sKarusMonumentPoint << g_pMain->m_sElmoMonumentPoint;

	if (g_pMain->isWarOpen() && GetZoneID() == ZONE_BATTLE6)
		result << bType << g_pMain->m_sKarusDead << g_pMain->m_sElmoradDead;

	Send(&result);
}

bool CUser::AttemptSelectMsg(uint8 bMenuID, int8 bySelectedReward)
{
	_QUEST_HELPER * pHelper = nullptr;
	if (bMenuID >= MAX_MESSAGE_EVENT
		|| isDead()
		|| m_nQuestHelperID == 0)
		return false;

	// Get the event number that needs to be processed next.
	int32 selectedEvent = m_iSelMsgEvent[bMenuID];
	if (selectedEvent < 0
		|| (pHelper = g_pMain->m_QuestHelperArray.GetData(m_nQuestHelperID)) == nullptr
		|| !QuestV2RunEvent(pHelper, selectedEvent, bySelectedReward))
		return false;

	return true;
}

void CUser::SendSay(int32 nTextID[8])
{
	Packet result(WIZ_NPC_SAY);
	result << int32(-1) << int32(-1);
	foreach_array_n(i, nTextID, 8)
		result << nTextID[i];
	Send(&result);
}

void CUser::SelectMsg(uint8 bFlag, int32 nQuestID, int32 menuHeaderText, 
					  int32 menuButtonText[MAX_MESSAGE_EVENT], int32 menuButtonEvents[MAX_MESSAGE_EVENT])
{
	_QUEST_HELPER * pHelper = g_pMain->m_QuestHelperArray.GetData(m_nQuestHelperID);
	if (pHelper == nullptr)
		return;

	// Send the menu to the client
	Packet result(WIZ_SELECT_MSG);
	result.SByte();

	result << m_sEventSid << bFlag << nQuestID << menuHeaderText;
	foreach_array_n(i, menuButtonText, MAX_MESSAGE_EVENT)
		result << menuButtonText[i];
	result << pHelper->strLuaFilename;
	Send(&result);

	// and store the corresponding event IDs.
	memcpy(&m_iSelMsgEvent, menuButtonEvents, sizeof(int32) * MAX_MESSAGE_EVENT);
}

void CUser::NpcEvent(Packet & pkt)
{
	// Ensure AI is loaded first
	if (!g_pMain->m_bPointCheckFlag
		|| isDead())
		return;	

	Packet result;
	uint16 sNpcID = pkt.read<uint16>();
	int32 nQuestID = pkt.read<int32>();

	CNpc *pNpc = g_pMain->GetNpcPtr(sNpcID);
	if (pNpc == nullptr
		|| !isInRange(pNpc, MAX_NPC_RANGE))
		return;

	switch (pNpc->GetType())
	{
	case NPC_MERCHANT:
	case NPC_TINKER:
		result.SetOpcode(pNpc->GetType() == NPC_MERCHANT ? WIZ_TRADE_NPC : WIZ_REPAIR_NPC);
		result << pNpc->m_iSellingGroup;
		Send(&result);
		break;

	case NPC_MARK:
		result.SetOpcode(WIZ_KNIGHTS_PROCESS);
		result << uint8(KNIGHTS_CAPE_NPC);
		Send(&result);
		break;

	case NPC_RENTAL:
		result.SetOpcode(WIZ_RENTAL);
		result	<< uint8(RENTAL_NPC) 
			<< uint16(1) // 1 = enabled, -1 = disabled 
			<< pNpc->m_iSellingGroup;
		Send(&result);
		break;

	case NPC_ELECTION:
	case NPC_TREASURY:
		{
			CKingSystem * pKingSystem = g_pMain->m_KingSystemArray.GetData(GetNation());
			result.SetOpcode(WIZ_KING);
			if (pNpc->GetType() == NPC_ELECTION)
			{
				// Ensure this still works as per official without a row in the table.
				string strKingName = (pKingSystem == nullptr ? "" : pKingSystem->m_strKingName);
				result.SByte();
				result	<< uint8(KING_NPC) << strKingName;
			}
			else
			{
				// Ensure this still works as per official without a row in the table.
				uint32 nTribute = (pKingSystem == nullptr ? 0 : pKingSystem->m_nTribute + pKingSystem->m_nTerritoryTax);
				uint32 nTreasury = (pKingSystem == nullptr ? 0 : pKingSystem->m_nNationalTreasury);
				result	<< uint8(KING_TAX) << uint8(1) // success
					<< uint16(isKing() ? 1 : 2) // 1 enables king-specific stuff (e.g. scepter), 2 is normal user stuff
					<< nTribute << nTreasury;
			}
			Send(&result);
		} break;

	case NPC_SIEGE:
		{
			_KNIGHTS_SIEGE_WARFARE *pKnightSiegeWarFare = g_pMain->GetSiegeMasterKnightsPtr(Aktive);
			result.SetOpcode(WIZ_SIEGE);
			result << uint8(3) << uint8(7);
			Send(&result);
		}
		break;
	case NPC_SIEGE_1:
		{
			_KNIGHTS_SIEGE_WARFARE *pKnightSiegeWarFare = g_pMain->GetSiegeMasterKnightsPtr(Aktive);
			if (isInClan())
			{
				if (pKnightSiegeWarFare->sMasterKnights != GetClanID())
					return;

				result.SetOpcode(WIZ_SIEGE);
				result << uint8(4) << uint8(1) 

				<< pKnightSiegeWarFare->nDungeonCharge 
				<< pKnightSiegeWarFare->nMoradonTax 
				<< pKnightSiegeWarFare->nDellosTax;
				Send(&result);
			}
		}
		break;

	case NPC_VICTORY_GATE:
		switch(GetWarVictory())
		{
		case KARUS:
			if(GetNation() == KARUS)
				ZoneChange(2,222,1846);
			   break;
		case ELMORAD:
			if(GetNation() == ELMORAD)
				ZoneChange(1,1865,168);
			   break;
		}
		break;

	case NPC_CAPTAIN:
		result.SetOpcode(WIZ_CLASS_CHANGE);
		result << uint8(CLASS_CHANGE_REQ);
		Send(&result);
		break;

	case NPC_WAREHOUSE:
		result.SetOpcode(WIZ_WAREHOUSE);
		result << uint8(WAREHOUSE_REQ);
		Send(&result);
		break;

	case NPC_CHAOTIC_GENERATOR:
	case NPC_CHAOTIC_GENERATOR2:
		SendAnvilRequest(sNpcID, ITEM_BIFROST_REQ);
		break;

	case NPC_BORDER_MONUMENT:
		{
			if( GetZoneID() != ZONE_BORDER_DEFENSE_WAR ||
				g_pMain->pTempleEvent.m_sBorderMonumentNation[GetEventRoom()] == GetNation())
				return;

			m_tBorderCapure = UNIXTIME;

			result.Initialize(WIZ_QUEST);
			result << uint8(CAPURE_TIME) << uint32(9993);
			Send(&result);

			result.clear();

			result.Initialize(WIZ_CAPTURE);
			result << uint8(CAPURE_RIGHT_CLICK) << GetSocketID() << GetName();
			Send(&result);
		}
		break;

	case NPC_CLAN: // this HAS to go.
		result << uint16(0); // page 0
		CKnightsManager::AllKnightsList(this, result);
	default:
		ClientEvent(sNpcID);
	}
}

// NPC Shop
void CUser::ItemTrade(Packet & pkt)
{
	DateTime time;
	Packet result(WIZ_ITEM_TRADE);
	uint32 transactionPrice;
	int itemid = 0, money = 0, group = 0;
	uint16 npcid;
	uint16 count, real_count = 0;
	_ITEM_TABLE* pTable = nullptr;
	CNpc* pNpc = nullptr;
	uint8 type, pos, destpos, errorCode = 1;
	bool bSuccess = false;

	if (isDead())
	{
		errorCode = 1;
		goto fail_return;
	}

	pkt >> type;
	// Buy == 1, Sell == 2
	if (type == 1 || type == 2)
	{
		pkt >> group >> npcid;
		if (!g_pMain->m_bPointCheckFlag
			|| (pNpc = g_pMain->GetNpcPtr(npcid)) == nullptr
			|| (pNpc->GetType() != NPC_MERCHANT && pNpc->GetType() != NPC_TINKER)
			|| pNpc->m_iSellingGroup != group
			|| !isInRange(pNpc, MAX_NPC_RANGE))
			goto fail_return;
	}

	pkt >> itemid >> pos;

	if (type == 3) 	// Move only (this is so useless mgame -- why not just handle it with the CUser::ItemMove(). Gah.)
		pkt >> destpos;
	else
		pkt >> count;

	// Moving an item in the inventory
	if (type == 3)
	{
		if (pos >= HAVE_MAX || destpos >= HAVE_MAX || itemid != m_sItemArray[SLOT_MAX+pos].nNum)
		{
			errorCode = 4;
			goto fail_return;
		}

		short duration = m_sItemArray[SLOT_MAX+pos].sDuration;
		short itemcount = m_sItemArray[SLOT_MAX+pos].sCount; 
		uint32 nExpirationTime = m_sItemArray[SLOT_MAX+pos].nExpirationTime;

		m_sItemArray[SLOT_MAX+pos].nNum = m_sItemArray[SLOT_MAX+destpos].nNum;
		m_sItemArray[SLOT_MAX+pos].sDuration = m_sItemArray[SLOT_MAX+destpos].sDuration;
		m_sItemArray[SLOT_MAX+pos].sCount = m_sItemArray[SLOT_MAX+destpos].sCount;
		m_sItemArray[SLOT_MAX+pos].nExpirationTime = m_sItemArray[SLOT_MAX+destpos].nExpirationTime;

		m_sItemArray[SLOT_MAX+destpos].nNum = itemid;
		m_sItemArray[SLOT_MAX+destpos].sDuration = duration;
		m_sItemArray[SLOT_MAX+destpos].sCount = itemcount;
		m_sItemArray[SLOT_MAX+destpos].nExpirationTime = nExpirationTime;

		result << uint8(3);
		Send(&result);
		return;
	}

	if (isTrading()
		|| (type == 2 && pNpc->m_iSellingGroup == 249000)
		|| (pTable = g_pMain->GetItemPtr(itemid)) == nullptr
		|| (type == 2 // if we're selling an item...
		&& (itemid >= ITEM_NO_TRADE // Cannot be traded, sold or stored.
		|| pTable->m_bRace == RACE_UNTRADEABLE))) // Cannot be traded or sold.
		goto fail_return;

	if (pos >= HAVE_MAX
		|| count <= 0 || count > MAX_ITEM_COUNT)
	{
		errorCode = 2;
		goto fail_return;
	}

	// Buying from an NPC
	if (type == 1 && pNpc->m_iSellingGroup != 249000)
	{	
		if (m_sItemArray[SLOT_MAX+pos].nNum != 0)
		{
			if (m_sItemArray[SLOT_MAX+pos].nNum != itemid)
			{
				errorCode = 2;
				goto fail_return;
			}

			if (!pTable->m_bCountable || count <= 0)
			{
				errorCode = 2;
				goto fail_return;
			}

			if (pTable->m_bCountable 
				&& (count + m_sItemArray[SLOT_MAX+pos].sCount) > MAX_ITEM_COUNT)
			{
				errorCode = 4;
				goto fail_return;				
			}
		}
		
		transactionPrice = ((uint32)pTable->m_iBuyPrice * count);

		if (m_bPremiumType > 0 && pTable->m_iSellPrice != SellTypeFullPrice)
			transactionPrice -= (((uint32)pTable->m_iBuyPrice / 8) * count);

		if(pTable->m_bSellingGroup == 0)
		{
			errorCode = 3;
			goto fail_return;
		}

		if (pNpc->m_iSellingGroup != (pTable->m_bSellingGroup * 1000))
		{
			if (pNpc->m_iSellingGroup != (pTable->m_bSellingGroup * 1000 + 1))
			{
				if (pNpc->m_iSellingGroup != (pTable->m_bSellingGroup * 1000 + 2))
				{
					if (pNpc->m_iSellingGroup != (pTable->m_bSellingGroup * 1000 + 72))
					{
						printf("Gold Items Paketten Gelen %u Npc Selling Grubu %u Item Tablosu Selling Gurubu %u \n",group,pNpc->m_iSellingGroup, pTable->m_bSellingGroup);
						errorCode = 3;
						goto fail_return;
					}
				}
			}
		}

		if (!hasCoins(transactionPrice))
		{
			errorCode = 3;
			goto fail_return;
		}

		if (((pTable->m_sWeight * count) + m_sItemWeight) > m_sMaxWeight)
		{
			errorCode = 4;
			goto fail_return;
		}
		
		m_sItemArray[SLOT_MAX+pos].nNum = itemid;
		m_sItemArray[SLOT_MAX+pos].sDuration = pTable->m_sDuration;
		m_sItemArray[SLOT_MAX+pos].sCount += count;

		g_pMain->WriteItemTransactionLogFile(string_format("%d:%d:%d || ZoneID = %d(%d,%d),UserID=%s,Task=BuyItem NPC,ItemName=%s,ItemID=%d,m_iBuyPrice=%d,count_array=%d\n",
		time.GetHour(),time.GetMinute(),time.GetSecond(),GetZoneID(),uint16(GetX()),uint16(GetZ()),GetName().c_str(),pTable->m_sName.c_str(),pTable->m_iNum,pTable->m_iBuyPrice, count));

		m_iGold -= transactionPrice;

		if (!pTable->m_bCountable)
		{
			m_sItemArray[SLOT_MAX+pos].nSerialNum = g_pMain->GenerateItemSerial();
			m_sItemArray[SLOT_MAX+pos].nUserSeal = (uint32) g_pMain->GenerateItemSerial();
		}

		SetUserAbility(false);
		SendItemWeight();

		g_pMain->WriteItemTransactionLogFile(string_format("%d:%d:%d || ZoneID = %d(%d,%d),UserID=%s,Task=BuyItem NPC,transactionPrice=%d\n",
		time.GetHour(),time.GetMinute(),time.GetSecond(),GetZoneID(),uint16(GetX()),uint16(GetZ()),GetName().c_str(),transactionPrice));
	}
	else if (type == 1 && pNpc->m_iSellingGroup == 249000)
	{	
		if (m_sItemArray[SLOT_MAX+pos].nNum != 0)
		{
			if (m_sItemArray[SLOT_MAX+pos].nNum != itemid)
			{
				errorCode = 2;
				goto fail_return;
			}

			if (!pTable->m_bCountable || count <= 0)
			{
				errorCode = 2;
				goto fail_return;
			}

			if (pTable->m_bCountable 
				&& (count + m_sItemArray[SLOT_MAX+pos].sCount) > MAX_ITEM_COUNT)
			{
				errorCode = 4;
				goto fail_return;				
			}
		}
		transactionPrice = ((uint32)pTable->m_iNPBuyPrice * count);

		if (m_bPremiumType > 0 && pTable->m_iSellPrice != SellTypeFullPrice)
			transactionPrice -= (((uint32)pTable->m_iNPBuyPrice / 8) * count);

		if(pTable->m_bSellingGroup == 0)
		{
			errorCode = 3;
			goto fail_return;
		}

		if (pNpc->m_iSellingGroup != (pTable->m_bSellingGroup * 1000))
		{
			if (pNpc->m_iSellingGroup != (pTable->m_bSellingGroup * 1000 + 1))
			{
				if (pNpc->m_iSellingGroup != (pTable->m_bSellingGroup * 1000 + 2))
				{
					if (pNpc->m_iSellingGroup != (pTable->m_bSellingGroup * 1000 + 72))
					{
						printf("Loyalty Items Paketten Gelen %u Npc Selling Grubu %u Item Tablosu Selling Gurubu %u \n",group,pNpc->m_iSellingGroup, pTable->m_bSellingGroup);
						errorCode = 3;
						goto fail_return;
					}
				}
			}
		}

		if (!hasLoyalty(transactionPrice))
		{
			errorCode = 3;
			goto fail_return;
		}

		if (((pTable->m_sWeight * count) + m_sItemWeight) > m_sMaxWeight)
		{
			errorCode = 4;
			goto fail_return;
		}

		m_sItemArray[SLOT_MAX+pos].nNum = itemid;
		m_sItemArray[SLOT_MAX+pos].sDuration = pTable->m_sDuration;
		m_sItemArray[SLOT_MAX+pos].sCount += count;

		g_pMain->WriteItemTransactionLogFile(string_format("%d:%d:%d || ZoneID = %d(%d,%d),UserID=%s,Task=BuyItem NPC,ItemName=%s,ItemID=%d,m_iBuyPrice=%d,count_array=%d\n",
		time.GetHour(),time.GetMinute(),time.GetSecond(),GetZoneID(),uint16(GetX()),uint16(GetZ()),GetName().c_str(),pTable->m_sName.c_str(),pTable->m_iNum,pTable->m_iBuyPrice, count));

		m_iLoyalty -= transactionPrice;

		if (!pTable->m_bCountable)
		{
			m_sItemArray[SLOT_MAX+pos].nSerialNum = g_pMain->GenerateItemSerial();
			m_sItemArray[SLOT_MAX+pos].nUserSeal = (uint32) g_pMain->GenerateItemSerial();
		}

		SetUserAbility(false);
		SendItemWeight();

		g_pMain->WriteItemTransactionLogFile(string_format("%d:%d:%d || ZoneID = %d(%d,%d),UserID=%s,Task=BuyItem NPC,transactionPrice=%d\n",
		time.GetHour(),time.GetMinute(),time.GetSecond(),GetZoneID(),uint16(GetX()),uint16(GetZ()),GetName().c_str(),transactionPrice));
	}
	// Selling an item to an NPC
	if (type == 2 && pNpc->m_iSellingGroup != 249000)
	{
		_ITEM_DATA *pItem = &m_sItemArray[SLOT_MAX+pos];
		if (pItem->nNum != itemid
			|| pItem->isSealed() // need to check the error codes for these
			|| pItem->isRented())
		{
			errorCode = 2;
			goto fail_return;
		}

		if (pItem->sCount < count)
		{
			errorCode = 3;
			goto fail_return;
		}

		if (m_bPremiumType == 0 && pTable->m_iSellPrice == SellTypeFullPrice)
			transactionPrice = ((uint32)pTable->m_iBuyPrice * count);
		else if (m_bPremiumType > 0 && pTable->m_iSellPrice == SellTypeFullPrice)
			transactionPrice = ((uint32)pTable->m_iBuyPrice * count);
		else if (m_bPremiumType == 0 && pTable->m_iSellPrice != SellTypeFullPrice)
			transactionPrice = (((uint32)pTable->m_iBuyPrice / 6) * count);
		else if (m_bPremiumType > 0 && pTable->m_iSellPrice != SellTypeFullPrice)
			transactionPrice = (((uint32)pTable->m_iBuyPrice / 4) * count);

		if (GetCoins() + transactionPrice > COIN_MAX)
		{
			errorCode = 3;
			goto fail_return;
		}

		GoldGain(transactionPrice, false);

		g_pMain->WriteItemTransactionLogFile(string_format("%d:%d:%d || ZoneID = %d(%d,%d),UserID=%s,Task=SellItem NPC,ItemName=%s,ItemID=%d,m_iBuyPrice=%d,count_array=%d\n",
		time.GetHour(),time.GetMinute(),time.GetSecond(),GetZoneID(),uint16(GetX()),uint16(GetZ()),GetName().c_str(),pTable->m_sName.c_str(),pTable->m_iNum,pTable->m_iBuyPrice, count));

		if (count >= pItem->sCount)
			memset(pItem, 0, sizeof(_ITEM_DATA));
		else
			pItem->sCount -= count;

		SetUserAbility(false);
		SendItemWeight();

		g_pMain->WriteItemTransactionLogFile(string_format("%d:%d:%d || ZoneID = %d(%d,%d),UserID=%s,Task=SellItem NPC,transactionPrice=%d\n",
		time.GetHour(),time.GetMinute(),time.GetSecond(),GetZoneID(),uint16(GetX()),uint16(GetZ()),GetName().c_str(),transactionPrice));

	}
	else if (type == 2 && pNpc->m_iSellingGroup == 249000)
	{
		_ITEM_DATA *pItem = &m_sItemArray[SLOT_MAX+pos];
		if (pItem->nNum != itemid
			|| pItem->isSealed() // need to check the error codes for these
			|| pItem->isRented())
		{
			errorCode = 2;
			goto fail_return;
		}

		if (pItem->sCount < count)
		{
			errorCode = 3;
			goto fail_return;
		}

		if (m_bPremiumType == 0 && pTable->m_iSellPrice == SellTypeFullPrice)
			transactionPrice = ((uint32)pTable->m_iNPBuyPrice * count);
		else if (m_bPremiumType > 0 && pTable->m_iSellPrice == SellTypeFullPrice)
			transactionPrice = ((uint32)pTable->m_iNPBuyPrice * count);
		else if (m_bPremiumType == 0 && pTable->m_iSellPrice != SellTypeFullPrice)
			transactionPrice = (((uint32)pTable->m_iNPBuyPrice / 6) * count);
		else if (m_bPremiumType > 0 && pTable->m_iSellPrice != SellTypeFullPrice)
			transactionPrice = (((uint32)pTable->m_iNPBuyPrice / 4) * count);

		if (GetLoyalty() + transactionPrice > LOYALTY_MAX)
		{
			errorCode = 3;
			goto fail_return;
		}

		SendLoyaltyChange(transactionPrice, false);

		g_pMain->WriteItemTransactionLogFile(string_format("%d:%d:%d || ZoneID = %d(%d,%d),UserID=%s,Task=SellItem NPC,ItemName=%s,ItemID=%d,m_iBuyPrice=%d,count_array=%d\n",
		time.GetHour(),time.GetMinute(),time.GetSecond(),GetZoneID(),uint16(GetX()),uint16(GetZ()),GetName().c_str(),pTable->m_sName.c_str(),pTable->m_iNum,pTable->m_iBuyPrice, count));

		if (count >= pItem->sCount)
			memset(pItem, 0, sizeof(_ITEM_DATA));
		else
			pItem->sCount -= count;

		SetUserAbility(false);
		SendItemWeight();

		g_pMain->WriteItemTransactionLogFile(string_format("%d:%d:%d || ZoneID = %d(%d,%d),UserID=%s,Task=SellItem NPC,transactionPrice=%d\n",
		time.GetHour(),time.GetMinute(),time.GetSecond(),GetZoneID(),uint16(GetX()),uint16(GetZ()),GetName().c_str(),transactionPrice));
	}
	bSuccess = true;

fail_return:
	result << bSuccess;
	if (!bSuccess)
		result << errorCode;
	else if (pNpc->m_iSellingGroup == 249000 && bSuccess)
		result << (uint8)pTable->m_bSellingGroup << m_iLoyalty << transactionPrice;
	else if (pNpc->m_iSellingGroup != 249000 && bSuccess)
		result << (uint8)pTable->m_bSellingGroup << m_iGold << transactionPrice; // price bought or sold for	
	Send(&result);
}

/**
* @brief	Handles the name change response packet
* 			containing the specified new name.
*
* @param	pkt	The packet.
*/
void CUser::HandleNameChange(Packet & pkt)
{
	uint8 opcode;
	pkt >> opcode;

	switch (opcode)
	{
	case NameChangePlayerRequest:
		HandlePlayerNameChange(pkt);
		break;
	case ClanNameChangePlayerRequest:
		HandlePlayerClanNameChange(pkt);
		break;
	}
}

/**
* @brief	Handles the character name change response packet
* 			containing the specified new character's name.
*
* @param	pkt	The packet.
*/
void CUser::HandlePlayerNameChange(Packet & pkt)
{
	NameChangeOpcode response = NameChangeSuccess;
	string strUserID;
	pkt >> strUserID;

	if (strUserID.empty() || strUserID.length() > MAX_ID_SIZE)
		response = NameChangeInvalidName;
	else if (isInClan())
		response = NameChangeInClan;
	else if (isKing())
		response = NameChangeKing; 

	if (response != NameChangeSuccess)
	{
		SendNameChange(response);
		return;
	}

	// Ensure we have the scroll before handling this request.
	if (!CheckExistItem(ITEM_SCROLL_OF_IDENTITY))
		return;

	Packet result(WIZ_NAME_CHANGE, uint8(NameChangePlayerRequest));
	result << strUserID;
	g_pMain->AddDatabaseRequest(result, this);
}

/**
* @brief	Handles the character clan name change response packet
* 			containing the specified new clan's name.
*
* @param	pkt	The packet.
*/
void CUser::HandlePlayerClanNameChange(Packet & pkt)
{
	ClanNameChangeOpcode response = ClanNameChangeSuccess;
	string strClanID;
	pkt >> strClanID;


	if (strClanID.empty() || strClanID.length() > MAX_ID_SIZE)
		response = ClanNameChangeInvalidName;
	else if (!isInClan() || !isClanLeader())
		response = ClanNameChangeInClan;
	else if (isInClan() && isClanLeader())
	{
		foreach_stlmap (itr, g_pMain->m_KnightsArray)
		{
			CKnights * pKnights = itr->second;
			if (pKnights == nullptr)
				continue;

			if (strClanID == pKnights->GetName())
				response = ClanNameChangeInvalidName;
		}
	}

	if (response != ClanNameChangeSuccess)
	{
		SendClanNameChange(response);
		return;
	}

	// Ensure we have the scroll before handling this request.
	if (!CheckExistItem(ITEM_CLAN_NAME_SCROLL))
		return;

	Packet result(WIZ_NAME_CHANGE, uint8(ClanNameChangePlayerRequest));
	result << strClanID;
	g_pMain->AddDatabaseRequest(result, this);
}

/**
* @brief	Sends a name change packet.
*
* @param	opcode	Name change packet opcode.
* 					NameChangeShowDialog shows the dialog where you can set your name.
* 					NameChangeSuccess confirms the name was changed.
* 					NameChangeInvalidName throws an error reporting the name is invalid.
* 					NameChangeInClan throws an error reporting the user's still in a clan (and needs to leave).
*					NameChangeIsKing if the user is king
*/
void CUser::SendNameChange(NameChangeOpcode opcode /*= NameChangeShowDialog*/)
{
	Packet result(WIZ_NAME_CHANGE, uint8(opcode));
	Send(&result);
}

/**
* @brief	Sends a clan name change packet.
*
* @param	opcode	Clan Name change packet opcode.
* 					ClanNameChangeShowDialog shows the dialog where you can set your name.
* 					ClanNameChangeSuccess confirms the name was changed.
* 					ClanNameChangeInvalidName throws an error reporting the name is invalid.
* 					ClanNameChangeInClan throws an error reporting the user's still in a clan (and needs to leave).
*					ClanNameChangeIsKing if the user is king
*/
void CUser::SendClanNameChange(ClanNameChangeOpcode opcode /*= ClanNameChangeShowDialog*/)
{
	CKnights * pKnights = nullptr;

	Packet result(WIZ_NAME_CHANGE, uint8(ClanNameChangePlayerRequest));

	if ((pKnights = g_pMain->GetClanPtr(GetClanID())) == nullptr)
	{
		result << uint8(ClanNameChangeInClan);
		goto fail_return;
	}

	result << uint8(opcode) << pKnights->GetName();

	if (opcode == ClanNameChangePlayerRequest)
		g_pMain->Send_KnightsMember(pKnights->GetID(), &result);
	else
		Send(&result);
	return;
fail_return:
	Send(&result);
}

void CUser::HandleCapeChange(Packet & pkt)
{
	Packet result(WIZ_CAPE);
	CKnights *pKnights = nullptr;
	CKnights *pMainKnights = nullptr;
	_KNIGHTS_ALLIANCE* pAlliance = nullptr;
	_KNIGHTS_CAPE *pCape = nullptr;
	uint32 nReqClanPoints = 0, nReqCoins = 0;
	int16 sErrorCode = 0, sCapeID;
	uint8 r, g, b;
	bool bApplyingPaint = false;

	pkt >> sCapeID >> r >> g >> b;

	// If we're not a clan leader, what are we doing changing the cape?
	if (!isClanLeader()
		|| isDead())
	{
		sErrorCode = -1;
		goto fail_return;
	}

	// Does the clan exist?
	if ((pKnights = g_pMain->GetClanPtr(GetClanID())) == nullptr)
	{
		sErrorCode = -2;
		goto fail_return;
	}

	// Make sure we're promoted
	if (!pKnights->isPromoted())
	{
		sErrorCode = -1;
		goto fail_return;
	}

	// and that if we're in an alliance, we're the primary clan in the alliance.
	if (pKnights->isInAlliance())
	{
		pMainKnights = g_pMain->GetClanPtr(pKnights->GetAllianceID());
		pAlliance = g_pMain->GetAlliancePtr(pKnights->GetAllianceID());
		if(pMainKnights == nullptr 
			|| pAlliance == nullptr
			|| (pAlliance->sMainAllianceKnights != pKnights->GetID() && pAlliance->sSubAllianceKnights != pKnights->GetID())
			|| (pAlliance->sSubAllianceKnights == pKnights->GetID() && sCapeID >= 0))
		{
			sErrorCode = -1;
			goto fail_return;
		}
	}

	if (sCapeID >= 0)
	{
		// Does this cape type exist?
		if ((pCape = g_pMain->m_KnightsCapeArray.GetData(sCapeID)) == nullptr)
		{
			sErrorCode = -5;
			goto fail_return;
		}

		// Is our clan allowed to use this cape?
		if ((pCape->byGrade && pKnights->m_byGrade > pCape->byGrade)
			// not sure if this should use another error, need to confirm
				|| pKnights->m_byFlag < pCape->byRanking)
		{
			sErrorCode = -6;
			goto fail_return;
		}

		// NOTE: Error code -8 is for nDuration
		// It applies if we do not have the required item ('nDuration', awful name).
		// Since no capes seem to use it, we'll ignore it...

		// Can we even afford this cape?
		if (!hasCoins(pCape->nReqCoins))
		{
			sErrorCode = -7;
			goto fail_return;
		}

		nReqCoins = pCape->nReqCoins;
		nReqClanPoints = pCape->nReqClanPoints;
	}

	// These are 0 when not used
	if (r != 0 || g != 0 || b != 0)
	{
		// To use paint, the clan needs to be accredited
		if (pKnights->m_byFlag < ClanTypeAccredited5)
		{
			sErrorCode = -1; // need to find the error code for this
			goto fail_return;
		}

		bApplyingPaint = true;
		nReqClanPoints += 1000; // does this need tweaking per clan rank?
	}

	// If this requires clan points, does our clan have enough?
	if (pKnights->m_nClanPointFund < nReqClanPoints
		|| GetCoins() < nReqCoins)
	{
		// this error may not be correct
		sErrorCode = -7;
		goto fail_return;
	}

	if (nReqCoins > 0)
		GoldLose(nReqCoins);

	if (nReqClanPoints)
	{
		pKnights->m_nClanPointFund -= nReqClanPoints;
		pKnights->UpdateClanFund();
	}

	// Are we changing the cape?
	if (sCapeID >= 0)
		pKnights->m_sCape = sCapeID;

	// Are we applying paint?
	if (bApplyingPaint)
	{
		pKnights->m_bCapeR = r;
		pKnights->m_bCapeG = g;
		pKnights->m_bCapeB = b;
	}

	if(!pKnights->isInAlliance())
	{
		result	<< uint16(1) // success
			<< pKnights->GetAllianceID()
			<< pKnights->GetID()
			<< pKnights->GetCapeID()
			<< pKnights->m_bCapeR << pKnights->m_bCapeG << pKnights->m_bCapeB
			<< uint8(0);
	}
	else if(pKnights->isInAlliance())
	{
		result	<< uint16(1) // success
			<< pKnights->GetAllianceID()
			<< pKnights->GetID()
			<< pMainKnights->GetCapeID()
			<< pKnights->m_bCapeR << pKnights->m_bCapeG << pKnights->m_bCapeB
			<< uint8(0);
	}

	Send(&result);

	// When we implement alliances, this should send to the alliance
	// if the clan is part of one. Also, their capes should be updated.
	if(!pKnights->isInAlliance() || (pKnights->isInAlliance() && !pKnights->isAllianceLeader()))
		pKnights->SendUpdate();
	else if(pKnights->isInAlliance() && pKnights->isAllianceLeader())
		pKnights->SendAllianceUpdate();

	// TODO: Send to other servers via UDP.

	// Now tell Database Agent to save (we don't particularly care whether it was able to do so or not).
	result.Initialize(WIZ_CAPE);
	result	<< pKnights->GetID() << pKnights->GetCapeID()
		<< pKnights->m_bCapeR << pKnights->m_bCapeG << pKnights->m_bCapeB;
	g_pMain->AddDatabaseRequest(result, this);
	return;

fail_return:
	result << sErrorCode;
	Send(&result);
}
