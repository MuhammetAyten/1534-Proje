#include "stdafx.h"
#include "DBAgent.h"

using std::string;

#define ITEM_SEAL_PRICE 1000000

enum
{
	SEAL_TYPE_SEAL		= 1,
	SEAL_TYPE_UNSEAL	= 2,
	SEAL_TYPE_KROWAZ	= 3
};

enum SealErrorCodes
{
	SealErrorNone			= 0, // no error, success!
	SealNoError				= 1,
	SealErrorFailed			= 2, // "Seal Failed."
	SealErrorNeedCoins		= 3, // "Not enough coins."
	SealErrorInvalidCode	= 4, // "Invalid Citizen Registry Number" (i.e. invalid code/password)
	SealErrorPremiumOnly	= 5, // "Only available to premium users"
	SealErrorFailed2		= 6, // "Seal Failed."
	SealErrorTooSoon		= 7, // "Please try again. You may not repeat this function instantly."
};

/**
* @brief	Packet handler for the item sealing system.
*
* @param	pkt	The packet.
*/
void CUser::ItemSealProcess(Packet & pkt)
{
	// Seal type
	uint8 opcode = pkt.read<uint8>();

	Packet result(WIZ_ITEM_UPGRADE, uint8(ITEM_SEAL));
	result << opcode;

	switch (opcode)
	{
		// Used when sealing an item.
	case SEAL_TYPE_SEAL:
		{
			string strPasswd;
			uint32 nItemID; 
			int16 unk0; // set to -1 in this case
			uint8 bSrcPos, bResponse = SealErrorNone;
			pkt >> unk0 >> nItemID >> bSrcPos >> strPasswd;

			/* 
			Most of these checks are handled client-side, so we shouldn't need to provide error messages.
			Also, item sealing requires certain premium types (gold, platinum, etc) - need to double-check 
			these before implementing this check.
			*/

			// is this a valid position? (need to check if it can be taken from new slots)
			if (bSrcPos >= HAVE_MAX 
				// does the item exist where the client says it does?
					|| GetItem(SLOT_MAX + bSrcPos)->nNum != nItemID
					// i ain't be allowin' no stealth items to be sealed!
					|| GetItem(SLOT_MAX + bSrcPos)->nSerialNum == 0)
					bResponse = SealErrorFailed;
			// is the password valid by client limits?
			else if (strPasswd.empty() || strPasswd.length() > 8)
				bResponse = SealErrorInvalidCode;
			// do we have enough coins?
			else if (!hasCoins(ITEM_SEAL_PRICE))
				bResponse = SealErrorNeedCoins;

			_ITEM_TABLE* pItem = g_pMain->m_ItemtableArray.GetData(nItemID);

			if(pItem == nullptr)
				return;

			// If no error, pass it along to the database.
			if (bResponse == SealErrorNone)
			{
				result << nItemID << bSrcPos << strPasswd << bResponse;
				g_pMain->AddDatabaseRequest(result, this);
			}
			// If there's an error, tell the client.
			// From memory though, there was no need -- it handled all of these conditions itself
			// so there was no need to differentiate (just ignore the packet). Need to check this.
			else 
			{
				result << bResponse;
				Send(&result);
			}
		} break;

		// Used when unsealing an item.
	case SEAL_TYPE_UNSEAL:
		{
			string strPasswd;
			uint32 nItemID; 
			int16 unk0; // set to -1 in this case
			uint8 bSrcPos, bResponse = SealErrorNone;
			pkt >> unk0 >> nItemID >> bSrcPos >> strPasswd;

			if (bSrcPos >= HAVE_MAX
				|| GetItem(SLOT_MAX+bSrcPos)->bFlag != ITEM_FLAG_SEALED
				|| GetItem(SLOT_MAX+bSrcPos)->nNum != nItemID)
				bResponse = SealErrorFailed;
			else if (strPasswd.empty() || strPasswd.length() > 8)
				bResponse = SealErrorInvalidCode;

			// If no error, pass it along to the database.
			if (bResponse == SealErrorNone)
			{
				result << nItemID << bSrcPos << strPasswd << bResponse;
				g_pMain->AddDatabaseRequest(result, this);
			}
			// If there's an error, tell the client.
			// From memory though, there was no need -- it handled all of these conditions itself
			// so there was no need to differentiate (just ignore the packet). Need to check this.
			else
			{
				result << bResponse;
				Send(&result);
			}
		} break;

		// Used when binding a Krowaz item (used to take it from not bound -> bound)
	case SEAL_TYPE_KROWAZ:
		{
			string strPasswd = "0"; //Dummy, not actually used.
			uint32 nItemID;
			uint8 bSrcPos = 0 , unk3, bResponse = SealErrorNone;
			uint16 unk1, unk2;
			pkt >> unk1 >> nItemID >> bSrcPos >> unk3 >> unk2;

			if (bSrcPos >= HAVE_MAX
				|| GetItem(SLOT_MAX+bSrcPos)->bFlag != ITEM_FLAG_NONE
				|| GetItem(SLOT_MAX+bSrcPos)->nNum != nItemID)
				bResponse = SealErrorFailed;

			if (bResponse == SealErrorNone)
			{
				result << nItemID << bSrcPos << strPasswd << bResponse;
				g_pMain->AddDatabaseRequest(result, this);
			}
		} break;
	}
}

void CUser::SealItem(uint8 bSealType, uint8 bSrcPos)
{
	_ITEM_DATA * pItem = GetItem(SLOT_MAX + bSrcPos);
	if (pItem == nullptr)
		return;

	switch (bSealType)
	{
	case SEAL_TYPE_SEAL:
		pItem->bFlag = ITEM_FLAG_SEALED;
		GoldLose(ITEM_SEAL_PRICE);
		break;

	case SEAL_TYPE_UNSEAL:
		pItem->bFlag = 0;
		break;

	case SEAL_TYPE_KROWAZ:
		pItem->bFlag = ITEM_FLAG_BOUND;
		break;
	}
}

/**
* @brief	Packet handler for the character sealing system.
*
* @param	pkt	The packet.
*/
void CUser::CharacterSealProcess(Packet & pkt)
{
	if (isDead())
		return;

	enum CharacterSealOpcode
	{
		SealFailed			= 0,
		Characterlistopen	= 1,
		SealSuccess			= 2,
		Sealhasbeenlifted	= 3, 
		InventoryShow		= 4
	};

	Packet result(WIZ_ITEM_UPGRADE, uint8(ITEM_CHARACTER_SEAL));
	string strCharID1, strCharID2, strCharID3, strCharID4;
	uint8 bResponse = SealSuccess;
	uint8 opcode;
	uint32 nItemID = CYPHER_RING;
	_ITEM_DATA * pDstItem = nullptr;

	pkt >> opcode;


	switch (opcode)
	{
		case Characterlistopen:
		{
			result << opcode << uint8(1);
			g_DBAgent.GetAllCharID(m_strAccountID, strCharID1, strCharID2, strCharID3);
			g_DBAgent.LoadCharInfo(strCharID1, result);
			g_DBAgent.LoadCharInfo(strCharID2, result);
			g_DBAgent.LoadCharInfo(strCharID3, result);

			Send(&result);
			return;
		}
		break;
		case SealSuccess:
		{
			string RecvCharID, RecvPass, SealPass;
			uint8 RecvSlot, blevel = 0, bRace = 0, nStr, nSta, nDex, nInt, nCha;
			uint16 Unkw, bJob = 0;
			uint32 RecvItemID, nGold;
			uint16 bExp = 0;
			ByteBuffer strSkill, strItem;

			pkt >> Unkw >> RecvSlot >> RecvItemID >> RecvCharID >> RecvPass;

			g_DBAgent.LoadSealPasswd(m_strAccountID, SealPass); 

			if (RecvSlot >= INVENTORY_COSP)
				bResponse = SealFailed;
			if (RecvPass.empty() || RecvPass.length() > 8 || RecvPass != SealPass)
				bResponse = SealErrorInvalidCode;
			if (!g_DBAgent.LoadSealInfo(RecvCharID, blevel, bExp, bRace, bJob, nStr, nSta, nDex, nInt, nCha, nGold, strSkill, strItem))
				bResponse = SealFailed;

			if (bResponse == SealSuccess)
			{
				uint64 iSerial = g_pMain->GenerateItemSerial(); // This get Serial Number
				uint32 sSerial = (uint32) g_pMain->GenerateItemSerial();
				
				bool tSuccess = g_DBAgent.CharacterSealSave(RecvCharID, iSerial, sSerial, bRace, bJob, blevel, bExp, nStr, nSta, nDex, nInt, nCha, nGold, strSkill, strItem);

				if (!tSuccess)
				{
					result << uint8(SealSuccess) << uint8(SealFailed);
					Send(&result);
					return;
				}

				pDstItem = GetItem(SLOT_MAX + RecvSlot);

				memset(pDstItem, 0, sizeof(_ITEM_DATA));

				pDstItem->nNum = CYPHER_RING;
				pDstItem->sDuration = 1;
				pDstItem->sCount = 1;
				pDstItem->bFlag = ITEM_FLAG_CHAR_SEAL;
				pDstItem->sRemainingRentalTime = 0;
				pDstItem->nExpirationTime = 0;
				pDstItem->nSerialNum = iSerial;
				pDstItem->nUserSeal = sSerial;

				if (blevel >= 83) 
					bExp = 10000;
				else
					bExp = 5000;

				g_DBAgent.UpdateAccountChar(m_strAccountID, RecvCharID, Sealhasbeenlifted);

				SetUserAbility(true);
				SendItemWeight();

				result << uint8(SealSuccess) << uint8(SealNoError)
					<< RecvSlot
					<< pDstItem->nNum
					<< sSerial
					<< RecvCharID
					<< uint8(bJob)
					<< uint8(blevel)
					<< uint16(bExp)
					<< uint16(bRace)
					<< uint32(0);

				Send(&result);
				return;
			}
			else
				result << uint8(SealSuccess) << uint8(SealFailed);
			Send(&result);
		}
		break;
		case Sealhasbeenlifted: // To unlock
		{
			uint8 InvSlot, AccSlot;
			uint16 Unkw;
			uint32 RecvItemID;

			pkt >> Unkw >> InvSlot >> RecvItemID >> AccSlot;

			pDstItem = GetItem(SLOT_MAX + InvSlot);

			_USER_SEAL * pUserSeal = g_pMain->m_UserSealItemArray.GetData(pDstItem->nUserSeal);

			if(pUserSeal == nullptr)
			{
				result << uint8(0);
				Send(&result);
				return;
			}

			if (GetNation() == KARUS && pUserSeal->bRaceSatis > 10)
			{
				result << uint8(SealSuccess) << uint8(SealFailed);
				Send(&result);
				return;
			}
			else if (GetNation() == ELMORAD && pUserSeal->bRaceSatis < 10)
			{
				result << uint8(SealSuccess) << uint8(SealFailed);
				Send(&result);
				return;
			}

			g_DBAgent.UpdateAccountChar(m_strAccountID, pUserSeal->sCharID, AccSlot);
			g_DBAgent.CharacterSealDelete(pUserSeal->sCharID);

			if (g_pMain->m_UserSealItemArray.GetData(pUserSeal->SealSerial) != nullptr)
				g_pMain->m_UserSealItemArray.DeleteData(pUserSeal->SealSerial);

			memset(pDstItem, 0, sizeof(_ITEM_DATA));

			result << opcode << uint8(1)
				<< InvSlot;

			Send(&result);
		}
		break;
		case InventoryShow:
		{
			uint32 sSerial;

			pkt >> sSerial;

			_USER_SEAL * pUserSeal = g_pMain->m_UserSealItemArray.GetData(sSerial);

			if(pUserSeal == nullptr)
			{
				result << uint8(0);
				Send(&result);
				return;
			}

			result.DByte();
			result << uint8(InventoryShow) 
				<< uint8(1)
				<< pUserSeal->sCharID 
				<< pUserSeal->bRaceSatis
				<< pUserSeal->sClass
				<< pUserSeal->bLevel
				<< pUserSeal->Stat_STR
				<< pUserSeal->Stat_STA
				<< pUserSeal->Stat_DEX
				<< pUserSeal->Stat_INT
				<< pUserSeal->Stat_CHA
				<< pUserSeal->sGold;

			for (int i = 0; i < 9; i++)
				result << pUserSeal->Skill_Points[i];

			for (int i = 0; i < INVENTORY_COSP; i++)
			{
				if (pUserSeal->Item_Num[i] == 0)
					continue;

				result << pUserSeal->Item_Num[i] << pUserSeal->Item_Duration[i] << pUserSeal->Item_Count[i]
					<< pUserSeal->Item_Flag[i];
			}

			Send(&result);
		}
		break;
	}
}