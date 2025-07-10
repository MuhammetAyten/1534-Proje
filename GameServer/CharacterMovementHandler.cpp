#include "stdafx.h"
#include "Map.h"

void CUser::MoveProcess(Packet & pkt)
{
	ASSERT(GetMap() != nullptr);
	if (m_bWarp || isDead())
		return;

	uint16 will_x, will_z, will_y;
	int16 speed = 0;
	float real_x, real_z, real_y;
	uint8 echo, type;

	pkt >> will_x >> will_z >> will_y >> speed >> echo;
	real_x = will_x / 10.0f; real_z = will_z / 10.0f; real_y = will_y / 10.0f;

	m_sSpeed = speed;

	SpeedHackUser();

	if (!GetMap()->IsValidPosition(real_x, real_z, real_y))
		return;

	if (m_oldx != GetX() || m_oldz != GetZ())
	{
		m_oldx = GetX();
		m_oldy = GetY();
		m_oldz = GetZ();
	}

	// TODO: Ensure this is checked properly to prevent speedhacking
	SetPosition(real_x, real_y, real_z);

	if (RegisterRegion())
	{
		g_pMain->RegionNpcInfoForMe(this);
		g_pMain->RegionUserInOutForMe(this);
		g_pMain->MerchantUserInOutForMe(this);
	}

	if (m_bInvisibilityType == INVIS_DISPEL_ON_MOVE)
		CMagicProcess::RemoveStealth(this, INVIS_DISPEL_ON_MOVE);

	type = m_transformationType;

	Packet result(WIZ_MOVE);
	result << GetSocketID() << will_x << will_z << will_y << speed << echo;
	SendToRegion(&result, this, GetEventRoom());

	GetMap()->CheckEvent(real_x, real_z, this);
	EventTrapProcess(real_x, real_z, this);

	result.Initialize(AG_USER_MOVE);
	result << GetSocketID() << m_curx << m_curz << m_cury << speed << type;
	Send_AIServer(&result);
}

void CUser::AddToRegion(int16 new_region_x, int16 new_region_z)
{
	GetRegion()->Remove(this);
	SetRegion(new_region_x, new_region_z);
	GetRegion()->Add(this);
}

void CUser::GetInOut(Packet & result, uint8 bType)
{
	result.Initialize(WIZ_USER_INOUT);
	result << uint16(bType) << GetID();
	if (bType != INOUT_OUT)
		GetUserInfo(result);
	
}

void CUser::UserInOut(uint8 bType)
{
	if (GetRegion() == nullptr)
		return;

	Packet result;

	if (bType != INOUT_OUT)
		ResetGMVisibility();

	GetInOut(result, bType);

	if (bType == INOUT_OUT)
		GetRegion()->Remove(this);
	else
		GetRegion()->Add(this);

	SendToRegion(&result, this, GetEventRoom());

	if (bType == INOUT_OUT || !isBlinking())
	{
		result.Initialize(AG_USER_INOUT);
		result.SByte();
		result << bType << GetSocketID() << GetName() << m_curx << m_curz;
		Send_AIServer(&result);
	}
}

void CUser::GetUserInfo(Packet & pkt)
{
	pkt.SByte();
	pkt << GetName()
		<< GetNation() << GetClanID() << GetFame();

	CKnights * pKnights = g_pMain->GetClanPtr(GetClanID());
	if (pKnights == nullptr)
	{
		pkt << uint16(0) << uint8(0) << uint32(0) << uint16(-1) << uint16(0) << uint8(0) << uint16(0);
	}
	else
	{
		pkt << pKnights->GetAllianceID()
			<< pKnights->m_strName
			<< pKnights->m_byGrade << pKnights->m_byRanking
			<< uint16(pKnights->m_sMarkVersion); // symbol/mark version
		if (pKnights->GetAllianceID() > 0)
		{
			CKnights *pKnightsAlly = g_pMain->GetClanPtr(pKnights->GetAllianceID());

			if (pKnightsAlly == nullptr)
			{
				pkt << uint16(pKnights->m_sCape) // cape ID
					<< pKnights->m_bCapeR << pKnights->m_bCapeG << pKnights->m_bCapeB << uint8(0); // this is stored in 4 bytes after all.
			}
			else
			{
				pkt << uint16(pKnightsAlly->m_sCape) // cape ID
					<< pKnightsAlly->m_bCapeR << pKnightsAlly->m_bCapeG << pKnightsAlly->m_bCapeB << uint8(0); // this is stored in 4 bytes after all.
			}
		}
		else
		{
			pkt << uint16(pKnights->m_sCape) // cape ID
				<< pKnights->m_bCapeR << pKnights->m_bCapeG << pKnights->m_bCapeB << uint8(0); // this is stored in 4 bytes after all.
		}
		// not sure what this is, but it (just?) enables the clan symbol on the cape 
		// value in dump was 9, but everything tested seems to behave as equally well...
		// we'll probably have to implement logic to respect requirements.
		pkt << uint8(2);
	}

	// There are two event-driven invisibility states; dispel on attack, and dispel on move.
	// These are handled primarily server-side; from memory the client only cares about value 1 (which we class as 'dispel on move').
	// As this is the only place where this flag is actually sent to the client, we'll just convert 'dispel on attack' 
	// back to 'dispel on move' as the client expects.
	uint8 bInvisibilityType = m_bInvisibilityType;
	if (bInvisibilityType != INVIS_NONE)
		bInvisibilityType = INVIS_DISPEL_ON_MOVE;

	pkt << GetLevel() << m_bRace << m_sClass
		<< GetSPosX() << GetSPosZ() << GetSPosY()
		<< m_bFace << uint8(m_nHair)
		<< m_bResHpType << uint32(m_bAbnormalType)
		<< m_bNeedParty
		<< m_bAuthority
		<< m_bPartyLeader // is party leader (bool)
		<< bInvisibilityType // visibility state
		<< uint8(m_teamColour) // either this is correct and items are super buggy, or it causes baldness. You choose.
		<< m_sDirection // direction 
		<< m_bIsChicken // chicken/beginner flag
		<< m_bRank // king flag
		<< m_bKnightsRank << m_bPersonalRank; // NP ranks (total, monthly)

	uint8 equippedItems[] =
	{
		BREAST, LEG, HEAD, GLOVE, FOOT, SHOULDER, RIGHTHAND, LEFTHAND,
		CRIGHT, CWING, CHELMET, CLEFT
	};

	if (g_pMain->isWarOpen() && g_pMain->m_byBattleZone + ZONE_BATTLE_BASE != ZONE_BATTLE3)
	{
		if (isWarrior())
		{
			foreach_array(i, equippedItems)
			{
				_ITEM_DATA * pItem = GetItem(equippedItems[i]);

				if (pItem == nullptr)
					continue;

				if (i == RIGHTEAR)
					pkt << (uint32)WARRIOR_DRAGON_ARMOR_PAULDRON << pItem->sDuration << pItem->bFlag;
				else if (i == HEAD)
					pkt << (uint32)WARRIOR_DRAGON_ARMOR_PAD << pItem->sDuration << pItem->bFlag;
				else if (i == LEFTEAR)
					pkt << (uint32)WARRIOR_DRAGON_ARMOR_HELMET << pItem->sDuration << pItem->bFlag;
				else if (i == NECK)
					pkt << (uint32)WARRIOR_DRAGON_ARMOR_GAUNTLET << pItem->sDuration << pItem->bFlag;
				else if (i == BREAST)
					pkt << (uint32)WARRIOR_DRAGON_ARMOR_BOOTS << pItem->sDuration << pItem->bFlag;
				else
					pkt << pItem->nNum << pItem->sDuration << pItem->bFlag;
			}
		}
		else if (isRogue())
		{
			foreach_array(i, equippedItems)
			{
				_ITEM_DATA * pItem = GetItem(equippedItems[i]);

				if (pItem == nullptr)
					continue;

				if (i == RIGHTEAR)
					pkt << (uint32)ROGUE_DRAGON_ARMOR_PAULDRON << pItem->sDuration << pItem->bFlag;
				else if (i == HEAD)
					pkt << (uint32)ROGUE_DRAGON_ARMOR_PAD << pItem->sDuration << pItem->bFlag;
				else if (i == LEFTEAR)
					pkt << (uint32)ROGUE_DRAGON_ARMOR_HELMET << pItem->sDuration << pItem->bFlag;
				else if (i == NECK)
					pkt << (uint32)ROGUE_DRAGON_ARMOR_GAUNTLET << pItem->sDuration << pItem->bFlag;
				else if (i == BREAST)
					pkt << (uint32)ROGUE_DRAGON_ARMOR_BOOTS << pItem->sDuration << pItem->bFlag;
				else
					pkt << pItem->nNum << pItem->sDuration << pItem->bFlag;
			}
		}
		else if (isMage())
		{
			foreach_array(i, equippedItems)
			{
				_ITEM_DATA * pItem = GetItem(equippedItems[i]);

				if (pItem == nullptr)
					continue;

				if (i == RIGHTEAR)
					pkt << (uint32)MAGE_DRAGON_ARMOR_PAULDRON << pItem->sDuration << pItem->bFlag;
				else if (i == HEAD)
					pkt << (uint32)MAGE_DRAGON_ARMOR_PAD << pItem->sDuration << pItem->bFlag;
				else if (i == LEFTEAR)
					pkt << (uint32)MAGE_DRAGON_ARMOR_HELMET << pItem->sDuration << pItem->bFlag;
				else if (i == NECK)
					pkt << (uint32)MAGE_DRAGON_ARMOR_GAUNTLET << pItem->sDuration << pItem->bFlag;
				else if (i == BREAST)
					pkt << (uint32)MAGE_DRAGON_ARMOR_BOOTS << pItem->sDuration << pItem->bFlag;
				else
					pkt << pItem->nNum << pItem->sDuration << pItem->bFlag;
			}
		}
		else if (isPriest())
		{
			foreach_array(i, equippedItems)
			{
				_ITEM_DATA * pItem = GetItem(equippedItems[i]);

				if (pItem == nullptr)
					continue;

				if (i == RIGHTEAR)
					pkt << (uint32)PRIEST_DRAGON_ARMOR_PAULDRON << pItem->sDuration << pItem->bFlag;
				else if (i == HEAD)
					pkt << (uint32)PRIEST_DRAGON_ARMOR_PAD << pItem->sDuration << pItem->bFlag;
				else if (i == LEFTEAR)
					pkt << (uint32)PRIEST_DRAGON_ARMOR_HELMET << pItem->sDuration << pItem->bFlag;
				else if (i == NECK)
					pkt << (uint32)PRIEST_DRAGON_ARMOR_GAUNTLET << pItem->sDuration << pItem->bFlag;
				else if (i == BREAST)
					pkt << (uint32)PRIEST_DRAGON_ARMOR_BOOTS << pItem->sDuration << pItem->bFlag;
				else
					pkt << pItem->nNum << pItem->sDuration << pItem->bFlag;
			}
		}
	}
	else
	{
		foreach_array(i, equippedItems)
		{
			_ITEM_DATA * pItem = GetItem(equippedItems[i]);

			if (pItem == nullptr)
				continue;

			pkt << pItem->nNum << pItem->sDuration << pItem->bFlag;
		}
	}

	pkt << GetZoneID();
}

void CUser::Rotate(Packet & pkt)
{
	if (isDead())
		return;

	Packet result(WIZ_ROTATE);
	pkt >> m_sDirection;
	result << GetSocketID() << m_sDirection;
	SendToRegion(&result, this, GetEventRoom());
}

bool CUser::CanChangeZone(C3DMap * pTargetMap, WarpListResponse & errorReason)
{
	// While unofficial, game masters should be allowed to teleport anywhere.
	if (isGM())
		return true;

	// Generic error reason; this should only be checked when the method returns false.
	errorReason = WarpListGenericError;

	if (GetLevel() < pTargetMap->GetMinLevelReq())
	{
		errorReason = WarpListMinLevel;
		return false;
	}

	if (GetLevel() > pTargetMap->GetMaxLevelReq()
		|| !CanLevelQualify(pTargetMap->GetMaxLevelReq()))
	{
		errorReason = WarpListDoNotQualify;
		return false;
	}

	switch (pTargetMap->GetID())
	{
	case ZONE_KARUS:
	case ZONE_KARUS2:
	case ZONE_KARUS3:
	case ZONE_KARUS4:
		// Users may enter Luferson (1)/El Morad (2) if they are that nation, 
		if (GetNation() == KARUS && pTargetMap->GetID() >= ZONE_KARUS && pTargetMap->GetID() <= ZONE_KARUS4)
			return true;

		// Users may also enter if there's a war invasion happening in that zone.
		if (GetNation() == ELMORAD)
			return g_pMain->m_byKarusOpenFlag;
		else
			return g_pMain->m_byElmoradOpenFlag;
	case ZONE_ELMORAD:
	case ZONE_ELMORAD2:
	case ZONE_ELMORAD3:
	case ZONE_ELMORAD4:
		// Users may enter Luferson (1)/El Morad (2) if they are that nation, 
		if (GetNation() == ELMORAD && pTargetMap->GetID() >= ZONE_ELMORAD && pTargetMap->GetID() <= ZONE_ELMORAD4)
			return true;

		// Users may also enter if there's a war invasion happening in that zone.
		if (GetNation() == KARUS)
			return g_pMain->m_byElmoradOpenFlag;
		else
			return g_pMain->m_byKarusOpenFlag;
	case ZONE_KARUS_ESLANT:
	case ZONE_KARUS_ESLANT2:
	case ZONE_KARUS_ESLANT3:
		if (GetNation() == KARUS && pTargetMap->GetID() >= ZONE_KARUS_ESLANT && pTargetMap->GetID() <= ZONE_KARUS_ESLANT3)
			return true;
	case ZONE_ELMORAD_ESLANT:
	case ZONE_ELMORAD_ESLANT2:
	case ZONE_ELMORAD_ESLANT3:
		if (GetNation() == ELMORAD && pTargetMap->GetID() >= ZONE_ELMORAD_ESLANT && pTargetMap->GetID() <= ZONE_ELMORAD_ESLANT3)
			return true;
	case ZONE_DELOS: // TODO: implement CSW logic.
		if (g_pMain->m_byBattleOpen == SIEGE_BATTLE && !g_pMain->m_byBattleSiegeWarTeleport ||
			g_pMain->m_byBattleOpen == SIEGE_BATTLE && !isInClan())
		{
			errorReason = WarpListNotDuringCSW;
			return false;
		}

		if (GetLoyalty() <= 0)
		{
			errorReason = WarpListNeedNP;
			return false;
		}
		return true;
	case ZONE_BIFROST:
		return true;
	case ZONE_ARDREAM:
		if (g_pMain->isWarOpen())
		{
			errorReason = WarpListNotDuringWar;
			return false;
		}

		if (GetLoyalty() <= 0)
		{
			errorReason = WarpListNeedNP;
			return false;
		}

		return true;
	case ZONE_RONARK_LAND_BASE:
		if (g_pMain->isWarOpen() && g_pMain->m_byBattleZoneType != ZONE_ARDREAM)
		{
			errorReason = WarpListNotDuringWar;
			return false;
		}

		if (GetLoyalty() <= 0)
		{
			errorReason = WarpListNeedNP;
			return false;
		}

		return true;
	case ZONE_RONARK_LAND:
		if (g_pMain->isWarOpen() && g_pMain->m_byBattleZoneType != ZONE_ARDREAM)
		{
			errorReason = WarpListNotDuringWar;
			return false;
		}

		if (GetLoyalty() <= 0)
		{
			errorReason = WarpListNeedNP;
			return false;
		}

		return true;
	default:
		// War zones may only be entered if that war zone is active.
		if (pTargetMap->isWarZone())
		{
			if (pTargetMap->GetID() == ZONE_SNOW_BATTLE)
			{
				if ((pTargetMap->GetID() - ZONE_SNOW_BATTLE) != g_pMain->m_byBattleZone)
					return false;
			}
			else if ((pTargetMap->GetID() - ZONE_BATTLE_BASE) != g_pMain->m_byBattleZone)
				return false;
			else if ((GetNation() == ELMORAD && g_pMain->m_byElmoradOpenFlag)
				|| (GetNation() == KARUS && g_pMain->m_byKarusOpenFlag))
				return false;
		}
	}

	return true;
}

bool CUser::CanLevelQualify(uint8 sLevel)
{
	//temporary cancel the logic
	return true;

	int16 nStatTotal = 300 + (sLevel - 1) * 3;
	uint8 nSkillTotal = (sLevel - 9) * 2;

	if (sLevel > 60)
		nStatTotal += 2 * (sLevel - 60);

	if ((m_sPoints + GetStatTotal()) > nStatTotal || GetTotalSkillPoints() > nSkillTotal)
		return false;

	return true;
}

void CUser::ZoneChange(uint16 sNewZone, float x, float z)
{
	C3DMap * pMap = g_pMain->GetZoneByID(sNewZone);
	_KNIGHTS_SIEGE_WARFARE *pKnightSiege = g_pMain->GetSiegeMasterKnightsPtr(Aktive);
	CKnights *pKnightsMaster = g_pMain->GetClanPtr(pKnightSiege->sMasterKnights);

	if (pMap == nullptr)
		return;

	WarpListResponse errorReason;

	if (!CanChangeZone(pMap, errorReason))
	{
		Packet result(WIZ_WARP_LIST, uint8(2));

		result << uint8(errorReason);

		if (errorReason == WarpListMinLevel)
			result << pMap->GetMinLevelReq();

		Send(&result);
		return;
	}

	if (x == 0.0f && z == 0.0f)
	{
		_START_POSITION * pStartPosition = g_pMain->GetStartPosition(sNewZone);
		if (pStartPosition != nullptr)
		{
			x = (float)(GetNation() == KARUS ? pStartPosition->sKarusX : pStartPosition->sElmoradX + myrand(0, pStartPosition->bRangeX));
			z = (float)(GetNation() == KARUS ? pStartPosition->sKarusZ : pStartPosition->sElmoradZ + myrand(0, pStartPosition->bRangeZ));
		}
	}

	m_bWarp = true;
	m_bZoneChangeFlag = true;

	// Random respawn position...
	if (sNewZone == ZONE_CHAOS_DUNGEON)
	{
		short sx, sz;
		GetStartPositionRandom(sx, sz, (uint8)sNewZone);
		x = (float)sx;
		z = (float)sz;
	}

	m_LastX = x;
	m_LastZ = z;

	if (sNewZone == ZONE_DELOS)
	{
		if (pKnightSiege->sMasterKnights == GetClanID() && GetClanID())
		{
			if (GetNation() == KARUS)
			{
				x = (float)(455 + myrand(0, 5));
				z = (float)(790 + myrand(0, 5));
			}
			else
			{
				x = (float)(555 + myrand(0, 5));
				z = (float)(790 + myrand(0, 5));
			}
		}
	}

	if (isInTempleEventZone((uint8)sNewZone)
		&& !isGM())
	{
		if (!isEventUser())
			g_pMain->AddEventUser(this);

		g_pMain->SetEventUser(this);
	}

	if (isInTempleQuestEventZone((uint8)sNewZone)
		&& isQuestEventUser() && !isGM())
		g_pMain->SetQuestEventUser(this);

	if (GetZoneID() != sNewZone)
	{
		UserInOut(INOUT_OUT);

		m_bZoneChangeSameZone = false;
		// Reset the user's anger gauge when leaving the zone
		// Unknown if this is official behaviour, but it's logical.
		if (GetAngerGauge() > 0)
			UpdateAngerGauge(0);

		/*
		Here we also send a clan packet with subopcode 0x16 (with a byte flag of 2) if war zone/Moradon
		or subopcode 0x17 (with nWarEnemyID) for all else
		*/
#if 0
		if (isInClan())
		{
			CKnights * pKnights = g_pMain->GetClanPtr(GetClanID());
			if (pKnights != nullptr && pKnights->bKnightsWarStarted)
			{
				Packet clanPacket(WIZ_KNIGHTS_PROCESS);
				if (pMap->isWarZone() || sNewZone == ZONE_MORADON)
					clanPacket << uint8(0x17) << uint8(2);
				else
					clanPacket << uint16(0x16) << uint16(0 /*nWarEnemyID*/);

				Send(&clanPacket);
	}
}
#endif

		if (isInParty() && isPartyLeader() && !m_bZoneChangeSameZone)
		{
			_PARTY_GROUP * pParty = g_pMain->GetPartyPtr(GetPartyID());

			if (pParty == nullptr)
				return;

			PartyPromote(pParty->uid[1]);
		}

		if (isInParty() && !isPartyLeader() && !m_bZoneChangeSameZone)
			PartyRemove(GetSocketID());

		if (hasRival())
			RemoveRival();

		ResetWindows();
	}
	else
	{
		m_bWarp = false;
		Warp(uint16(x * 10), uint16(z * 10));
		return;
	}

	if ((isEventUser() && isInTempleEventZone((uint8)sNewZone)) ||
		(isQuestEventUser() && isInTempleQuestEventZone((uint8)sNewZone)))
		g_pMain->EventRemainingTime(this, (uint8)sNewZone);

	if (sNewZone != ZONE_SNOW_BATTLE && GetZoneID() == ZONE_SNOW_BATTLE)
		SetMaxHp(1);
	else if (isEventUser() && (sNewZone != ZONE_CHAOS_DUNGEON && GetZoneID() == ZONE_CHAOS_DUNGEON
		|| sNewZone != ZONE_BORDER_DEFENSE_WAR && GetZoneID() == ZONE_BORDER_DEFENSE_WAR
		|| sNewZone != ZONE_JURAID_MOUNTAIN && GetZoneID() == ZONE_JURAID_MOUNTAIN))
	{
		if (sNewZone != ZONE_CHAOS_DUNGEON && GetZoneID() == ZONE_CHAOS_DUNGEON)
		{
			SetMaxHp(1);
			RobChaosSkillItems();
		}

		g_pMain->UpdateEventUser(this, 0);
	}
	else if (isQuestEventUser() && (sNewZone != ZONE_STONE1 && GetZoneID() == ZONE_STONE1
		|| sNewZone != ZONE_STONE2 && GetZoneID() == ZONE_STONE2
		|| sNewZone != ZONE_STONE3 && GetZoneID() == ZONE_STONE3))
		MonsterStoneProcess(TEMPLE_EVENT_DISBAND);

	else if (sNewZone == ZONE_FORGOTTEN_TEMPLE)
		g_pMain->m_nForgettenTempleUsers.push_back(GetSocketID());
	else if (sNewZone != ZONE_FORGOTTEN_TEMPLE && GetZoneID() == ZONE_FORGOTTEN_TEMPLE)
		g_pMain->m_nForgettenTempleUsers.erase(std::remove(g_pMain->m_nForgettenTempleUsers.begin(), g_pMain->m_nForgettenTempleUsers.end(), GetSocketID()), g_pMain->m_nForgettenTempleUsers.end());

	m_bZone = (uint8)sNewZone; // this is 2 bytes to support the warp data loaded from SMDs. It should not go above a byte, however.
	SetPosition(x, 0.0f, z);
	m_pMap = pMap;

	if (g_pMain->m_nServerNo != pMap->m_nServerNo)
	{
		_ZONE_SERVERINFO *pInfo = g_pMain->m_ServerArray.GetData(pMap->m_nServerNo);
		if (pInfo == nullptr)
			return;

		UserDataSaveToAgent();

		m_bLogout = 2;	// server change flag
		SendServerChange(pInfo->strServerIP, 2);
		return;
	}

	SetRegion(GetNewRegionX(), GetNewRegionZ());

	Packet result(WIZ_ZONE_CHANGE, uint8(ZoneChangeTeleport));
	result << uint16(GetZoneID()) << GetSPosX() << GetSPosZ() << GetSPosY() << g_pMain->m_byOldVictory;
	Send(&result);

	if (!m_bZoneChangeSameZone)
	{
		m_sWhoKilledMe = -1;
		m_iLostExp = 0;
		m_bRegeneType = 0;
		m_tLastRegeneTime = 0;
		m_sBind = -1;
		InitType3();
		InitType4();
		CMagicProcess::CheckExpiredType9Skills(this, true);
		SetUserAbility();
	}

	m_bZoneChangeFlag = false;
	g_pMain->TempleEventSendActiveEventTime(this);

	result.Initialize(AG_ZONE_CHANGE);
	result << GetSocketID() << GetZoneID() << GetEventRoom();
	Send_AIServer(&result);
}

void CUser::CheckWaiting(uint8 sNewZone, uint16 Time)
{
	uint16 m_RemainEventZone = Time;

	Packet hata(WIZ_EVENT);
	hata << uint8(1); // Inventory Hatasý.
	Send(&hata);

	if (sNewZone == ZONE_STONE1 || sNewZone == ZONE_STONE2 || sNewZone == ZONE_STONE3)
	{
		Packet stone(WIZ_SELECT_MSG);
		stone << uint16(0x00) << uint8(0x09) << uint64(0x00) << uint8(0x09) << uint32(0x00) << uint8(0x10) << Time << uint16(0x00);
		Send(&stone);

		Packet stonetime(WIZ_BIFROST, uint8(MONSTER_SQUARD));
		stonetime << uint16(m_RemainEventZone);
		Send(&stonetime);
	}
}

void CUser::PlayerRankingProcess(uint16 ZoneID, bool RemoveInZone)
{
	if (m_bZoneChangeSameZone)
		return;

	if (isInPKZone() || isInEventZone())
	{
		if (RemoveInZone)
			RemovePlayerRank();
		else
		{
			RemovePlayerRank();
			AddPlayerRank(ZoneID);
		}
	}
	else
		RemovePlayerRank();
}

void CUser::AddPlayerRank(uint16 ZoneID)
{
	m_iLoyaltyDaily = 0;
	m_iLoyaltyPremiumBonus = 0;
	m_KillCount = 0;
	m_DeathCount = 0;

	_USER_RANKING * pData = new _USER_RANKING;

	pData->m_socketID = GetSocketID();
	pData->m_bEventRoom = GetEventRoom();
	pData->m_bZone = ZoneID;
	pData->m_bNation = GetNation();
	pData->m_iLoyaltyDaily = m_iLoyaltyDaily;
	pData->m_iLoyaltyPremiumBonus = m_iLoyaltyPremiumBonus;
	pData->m_KillCount = 0;
	pData->m_DeathCount = 0;

	if (!g_pMain->m_UserRankingArray[GetNation() - 1].PutData(pData->m_socketID, pData))
		delete pData;
}

void CUser::RemovePlayerRank()
{
	if (g_pMain->m_UserRankingArray[GetNation() - 1].GetData(GetSocketID()) != nullptr)
		g_pMain->m_UserRankingArray[GetNation() - 1].DeleteData(GetSocketID());
}

void CUser::UpdatePlayerRank()
{
	if (isGM())
		return;

	_USER_RANKING * pRankInfo = g_pMain->m_UserRankingArray[GetNation() - 1].GetData(GetSocketID());
	if (pRankInfo == nullptr)
		return;

	pRankInfo->m_iLoyaltyDaily = m_iLoyaltyDaily;
	pRankInfo->m_iLoyaltyPremiumBonus = m_iLoyaltyPremiumBonus;
	pRankInfo->m_KillCount = m_KillCount;
	pRankInfo->m_DeathCount = m_DeathCount;
}

/**
* @brief	Changes the zone of all party members within the user's zone.
* 			If the user is not in a party, they should still be teleported.
*
* @param	sNewZone	ID of the new zone.
* @param	x			The x coordinate.
* @param	z			The z coordinate.
*/
void CUser::ZoneChangeParty(uint16 sNewZone, float x, float z)
{
	_PARTY_GROUP * pParty = g_pMain->GetPartyPtr(GetPartyID());
	if (pParty == nullptr)
		return ZoneChange(sNewZone, x, z);

	short partyUsers[MAX_PARTY_USERS];

	for (int i = 0; i < MAX_PARTY_USERS; i++)
		partyUsers[i] = pParty->uid[i];

	for (int i = 0; i < MAX_PARTY_USERS; i++)
	{
		CUser * pUser = g_pMain->GetUserPtr(partyUsers[i]);
		if (pUser != nullptr)
			pUser->ZoneChange(sNewZone, x, z);
	}
}

/**
* @brief	Changes the zone of all clan members in home/neutral zones (including Eslant).
* 			If the user is not in a clan, they should not be teleported.
*
* @param	sNewZone	ID of the new zone.
* @param	x			The x coordinate.
* @param	z			The z coordinate.
*/
void CUser::ZoneChangeClan(uint16 sNewZone, float x, float z)
{
	CKnights * pKnights = g_pMain->GetClanPtr(GetClanID());
	if (pKnights == nullptr)
		return;

	for (int i = 0; i < MAX_CLAN_USERS; i++)
	{
		_KNIGHTS_USER * p = &pKnights->m_arKnightsUser[i];
		CUser * pUser = p->pSession;
		if (p->byUsed && pUser != nullptr
			&& pUser->GetZoneID() < ZONE_DELOS)
			pUser->ZoneChange(sNewZone, x, z);
	}
}

void CUser::Warp(uint16 sPosX, uint16 sPosZ)
{
	ASSERT(GetMap() != nullptr);
	if (m_bWarp)
		return;

	float real_x = sPosX / 10.0f, real_z = sPosZ / 10.0f;
	if (!GetMap()->IsValidPosition(real_x, real_z, 0.0f))
	{
		TRACE("Invalid position %f,%f\n", real_x, real_z);
		return;
	}

	m_LastX = real_x;
	m_LastZ = real_z;

	Packet result(WIZ_WARP);
	result << sPosX << sPosZ;
	Send(&result);

	UserInOut(INOUT_OUT);

	m_curx = real_x;
	m_curz = real_z;

	SetRegion(GetNewRegionX(), GetNewRegionZ());

	UserInOut(INOUT_WARP);
	g_pMain->UserInOutForMe(this);
	g_pMain->RegionNpcInfoForMe(this);
	g_pMain->MerchantUserInOutForMe(this);

	ResetWindows();
}

void CUser::RecvWarp(Packet & pkt)
{
	uint16 warp_x, warp_z;
	pkt >> warp_x >> warp_z;
	Warp(warp_x, warp_z);
}

void CUser::RecvZoneChange(Packet & pkt)
{
	uint8 opcode = pkt.read<uint8>();
	if (opcode == ZoneChangeLoading)
	{
		g_pMain->RegionUserInOutForMe(this);
		g_pMain->RegionNpcInfoForMe(this);
		g_pMain->MerchantUserInOutForMe(this);

		Packet result(WIZ_ZONE_CHANGE, uint8(ZoneChangeLoaded)); // finalise the zone change
		Send(&result);
	}
	else if (opcode == ZoneChangeLoaded)
	{
		UserInOut(INOUT_RESPAWN);

		// TODO: Fix all this up (it's too messy/confusing)
		if (!m_bZoneChangeSameZone)
			BlinkStart();

		if (GetZoneID() != ZONE_CHAOS_DUNGEON)
		{
			InitType4();
			RecastSavedMagic();
		}

		if (isDead())
			SendDeathAnimation();

		SetZoneAbilityChange((uint16)GetZoneID());

		m_bZoneChangeFlag = false;
		m_bWarp = false;
	}
	else if (opcode == ZoneMilitaryCamp)
	{
		uint16 ZoneID = pkt.read<uint8>();
		ZoneChange(ZoneID, GetX(), GetZ());
	}
}