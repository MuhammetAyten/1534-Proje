#include "stdafx.h"
#include "../shared/DateTime.h"
#include <cmath>

// Some item ID definitions
#define MIN_ITEM_ID 100000000
#define MAX_ITEM_ID 999999999

#define MAGE_EARRING 310310004
#define WARRIOR_EARRING 310310005
#define ROGUE_EARRING 310310006
#define PRIEST_EARRING 310310007

#define UPGRADE_RUBY_EARRING_MIN 310110005
#define UPGRADE_RUBY_EARRING_MAX 310110007

#define UPGRADE_PEARL_EARRING_MIN 310150005
#define UPGRADE_PEARL_EARRING_MAX 310150007

#define SHADOW_PIECE			700009000
#define ITEM_TRINA				700002000
#define ITEM_KARIVDIS		379258000
/**
* @brief	Packet handler for the assorted systems that
* 			were deemed to come under the 'upgrade' system.
*
* @param	pkt	The packet.
*/
void CUser::ItemUpgradeProcess(Packet & pkt)
{
	uint8 opcode = pkt.read<uint8>();
	
	switch (opcode)
	{
	case ITEM_UPGRADE:
		ItemUpgrade(pkt);
		break;

	case ITEM_ACCESSORIES:
		ItemUpgradeAccessories(pkt);
		break;

	case ITEM_BIFROST_EXCHANGE:
		BifrostPieceProcess(pkt);
		break;

	case ITEM_UPGRADE_REBIRTH:
		ItemUpgradeRebirth(pkt);
		break;

	case ITEM_SEAL:
		ItemSealProcess(pkt);
		break;

	case ITEM_CHARACTER_SEAL:
		CharacterSealProcess(pkt);
		break;

	case ITEM_SPECIAL_EXCHANGE:
		SpecialItemExchange(pkt);
		break;

	case ITEM_OLDMAN_EXCHANGE:
		SpecialOldManExchange(pkt);
		break;
	default:
		TRACE("Upgrade OpCode %u \n", opcode);
		break;
	}
}

/**
* @brief	Checks whether the specified ItemID exists in the map passed by.
*			And returns the number of instance on success.
*
* @return	Returns the number of instance found in the map.
*/
static uint16 IsExistInMap(UpgradeItemsMap &map, int32 ItemID)
{
	uint16 temp = 0;
	foreach(itr,map)
	{
		if(itr->second.nItemID == ItemID)
			temp++;
	}
	return temp;
}

static UpgradeScrollType GetScrollType(uint32 ScrollNum)
{
	// Low Class Scrolls
	if(ScrollNum == 379221000	
		|| ScrollNum == 379222000	
		|| ScrollNum == 379223000	
		|| ScrollNum == 379224000	
		|| ScrollNum == 379225000	
		|| ScrollNum == 379226000	
		|| ScrollNum == 379227000	
		|| ScrollNum == 379228000	
		|| ScrollNum == 379229000	
		|| ScrollNum == 379230000	
		|| ScrollNum == 379231000	
		|| ScrollNum == 379232000	
		|| ScrollNum == 379233000	
		|| ScrollNum == 379234000	
		|| ScrollNum == 379235000	
		|| ScrollNum == 379255000)
		return LowClassScroll;
	// Middle Class Scrolls
	else if(ScrollNum == 379205000	
		|| ScrollNum == 379206000	
		|| ScrollNum == 379208000	
		|| ScrollNum == 379209000	
		|| ScrollNum == 379210000	
		|| ScrollNum == 379211000	
		|| ScrollNum == 379212000	
		|| ScrollNum == 379213000	
		|| ScrollNum == 379214000	
		|| ScrollNum == 379215000	
		|| ScrollNum == 379216000	
		|| ScrollNum == 379217000	
		|| ScrollNum == 379218000	
		|| ScrollNum == 379219000	
		|| ScrollNum == 379220000)
		return MiddleClassScroll;
	// High Class Scrolls
	else if(ScrollNum == 379021000	
		|| ScrollNum == 379022000	
		|| ScrollNum == 379023000	
		|| ScrollNum == 379024000	
		|| ScrollNum == 379025000	
		|| ScrollNum == 379030000	
		|| ScrollNum == 379031000	
		|| ScrollNum == 379032000	
		|| ScrollNum == 379033000	
		|| ScrollNum == 379034000	
		|| ScrollNum == 379035000	
		|| ScrollNum == 379138000	
		|| ScrollNum == 379139000	
		|| ScrollNum == 379140000	
		|| ScrollNum == 379141000	
		|| ScrollNum == 379016000)
		return HighClassScroll;
	// High Class to Reverse Item Upgrade Scroll ( +7 to Rebirth +1)
	else if(ScrollNum == 379256000)
		return HighToRebirthScroll;
	// Rebirth Upgrade Scroll
	else if(ScrollNum == 379257000)
		return RebirthClassScroll;
	else if(ScrollNum == 379159000)
		return AccessoriesClassScroll;

	return InvalidScroll;
}

/**
* @brief	Packet handler for the standard item upgrade system.
*
* @param	pkt	The packet.
*/
void CUser::ItemUpgrade(Packet & pkt, uint8 nUpgradeType)
{
	Packet result(WIZ_ITEM_UPGRADE, nUpgradeType);
	_ITEM_DATA  * pOriginItem;
	_ITEM_TABLE * proto;
	CNpc* pNpc;
	DateTime time;
	UpgradeScrollType m_ScrollType; // the Scroll type that shall be used for this upgrade.
	UpgradeItemsMap nItems; // the items sent by the client 
	UpgradeIterator OriginItem; 
	uint16 sNpcID;
	int32 nItemID; int8 bPos;
	UpgradeScrollType pUserScroll = InvalidScroll;

	int8 bType = UpgradeType::UpgradeTypeNormal, bResult = UpgradeErrorCodes::UpgradeNoMatch, ItemClass = 0;
	bool trina = false, karivdis = false, Accessories = false;

	if (isTrading() || isMerchanting() || isMining())
	{
		bResult = UpgradeTrading;
		goto fail_return;
	}

	pkt >> bType; // either preview or upgrade, need to allow for these types
	pkt >> sNpcID;

	pNpc = g_pMain->GetNpcPtr(sNpcID);

	if(pNpc == nullptr 
		|| pNpc->GetType() != NPC_ANVIL)
		goto fail_return;


	int counter = 0;
	for (int i = 0; i < 10;i++) // client sends 10 items in a row, starting with the item to be upgraded and latter the required items
	{
		pkt >> nItemID >> bPos;

		// Invalid slot ID
		if (bPos != -1 && bPos >= HAVE_MAX)
			goto fail_return;

		// We do not care where the user put the scroll in his screen. We do only care
		// is that whether the received Item IDs and POSs are valid.
		if(nItemID > 0 && bPos >= 0 && bPos < HAVE_MAX)
		{
			ITEM_UPGRADE_ITEM pUpgradeItem;
			pUpgradeItem.nItemID = nItemID;
			pUpgradeItem.bPos = bPos;
			nItems.insert(std::make_pair(counter++, pUpgradeItem));
		}
	}

	if(nItems.empty()) // if no items are available, then it should fail.
		goto fail_return;

	foreach (itr, nItems) // we shall check if the items sent by the client do really exist on the user.
	{
		_ITEM_DATA  * pItem = GetItem(SLOT_MAX + itr->second.bPos);
		_ITEM_TABLE * pTable =  g_pMain->GetItemPtr(itr->second.nItemID);
		// Here we check and iterate through the users inventory and check
		// whether the item ids and positions are valid in the users inventory.
		// The reason why we do that is that we never trust a player.
		if(pItem == nullptr
			|| pTable == nullptr
			|| pTable->m_bRace == 20 // untradable items
			|| pItem->nNum != itr->second.nItemID)
			goto fail_return;
		// We shall allow no user to upgrade items with protected types like bounded, sealed etc. 
		// Even worse trying to upgrade a duplicate item cant be accepted.
		// The official behavious is to check the origin item only but c'mon
		// a user shouldnt upgrade an item by using a sealed scroll too.
		else if(pItem->isBound()
			|| pItem->isDuplicate()
			|| pItem->isRented()
			|| pItem->isSealed())
		{
			bResult = UpgradeErrorCodes::UpgradeRental;
			goto fail_return;
		}
	}


	// Now we are about to put the data on our pointers, iterators that
	// we are going to use for the rest of upgrade process.
	OriginItem = nItems.begin(); // this is our first item in the map which is going to be upgraded.
	if(OriginItem == nItems.end())
		goto fail_return;

	proto = g_pMain->GetItemPtr(OriginItem->second.nItemID);
	pOriginItem = GetItem(SLOT_MAX + OriginItem->second.bPos);
	if(proto == nullptr || pOriginItem == nullptr) 
		goto fail_return;

	foreach(itr, nItems)
	{
		pUserScroll = GetScrollType(itr->second.nItemID);

		if(pUserScroll != InvalidScroll)
			break;
	}

	if (pUserScroll == InvalidScroll)
		goto fail_return;
	
	//printf("Item Upgrade sOriginItem: %u bOriginType: %u \n",OriginItem->second.nItemID % 100000, proto->ItemExt);

	int nReqOriginItem = OriginItem->second.nItemID % 100000;
	{
		_ITEM_UPGRADE * pUpgrade = nullptr;
		foreach_stlmap (itr, g_pMain->m_ItemUpgradeArray)
		{
			pUpgrade = itr->second;

			if(pUpgrade == nullptr)
				continue;

			if (pUpgrade->sOriginItem != nReqOriginItem)
				continue;

			if ((OriginItem->second.nItemID / MIN_ITEM_ID) != pUpgrade->nIndex / 100000
				&& pUpgrade->nIndex < 300000) 
				continue;

			if(IsExistInMap(nItems, ITEM_TRINA)) // Trina Piece 
				trina = true;

			if(IsExistInMap(nItems, ITEM_KARIVDIS)) // Karvis Rebirth Trina
				karivdis = true;

			if((trina && karivdis) 
				|| IsExistInMap(nItems, ITEM_TRINA) > 1 
				|| IsExistInMap(nItems, ITEM_KARIVDIS) > 1)
				goto fail_return;

			// Up to this point, we have checked basic requirements and whether
			// the user is trying to fool us with dummy packets. Yet, it still
			// needs to be checked whether the items are valid for this kind
			// of upgrade.

			if (pUpgrade->bOriginType != -1 
				&& pUpgrade->nIndex < 200000 && pUpgrade->nIndex >= 100000)
			{
				switch (pUpgrade->bOriginType)
				{
				case 0:
					if (!proto->isDagger()) 
						continue;
					break;

				case 1:
					if (proto->m_bKind != 21)
						continue;
					break;

				case 2:
					if (proto->m_bKind != 22)
						continue;
					break;

				case 3:
					if (proto->m_bKind != 31) 
						continue;
					break;

				case 4:
					if (proto->m_bKind != 32) 
						continue;
					break;

				case 5:
					if (proto->m_bKind != 41) 
						continue;
					break;

				case 6:
					if (proto->m_bKind != 42) 
						continue;
					break;

				case 7:
					if (proto->m_bKind != 51) 
						continue;
					break;

				case 8:
					if (proto->m_bKind != 52) 
						continue;
					break;

				case 9:
					if (proto->m_bKind != 70 && proto->m_bKind != 71) 
						continue;
					break;

				case 10:
					if (proto->m_bKind != 110) 
						continue;
					break;

				case 11:
					if ((OriginItem->second.nItemID / 10000000) != 19) 
						continue;
					break;

				case 12:
					if (proto->m_bKind != 60) 
						continue;
					break;

				case 13:
					if (proto->m_bKind != 210 && proto->m_bKind != 220 && proto->m_bKind != 230 && proto->m_bKind != 240) 
						continue;
					break;

				case 14:
					if (proto->m_bKind != 11)
						continue;
					break;
				}
			}

			if ((OriginItem->second.nItemID / MIN_ITEM_ID) != (pUpgrade->nIndex / 100000) 
				&& ((pUpgrade->nIndex / 100000) == 1 
				|| (pUpgrade->nIndex / 100000) == 2))
				continue;


			bool isValidMatch = false;
			// Does our upgrade attempt match the requirements for this upgrade entry?
			for (int x = 1; x < MAX_ITEMS_REQ_FOR_UPGRADE; x++)
			{
				if (pUpgrade->nReqItem[x-1] == 0)
					continue;

				if(IsExistInMap(nItems, pUpgrade->nReqItem[x-1]))
				{
					isValidMatch = true;
					break;
				}
			}

			// Not a valid match, try another row.
			if (!isValidMatch)
				continue;

			m_ScrollType = UpgradeScrollType::InvalidScroll;
			if(proto->ItemClass == 1) // low class scroll
				m_ScrollType = UpgradeScrollType::LowClassScroll;
			else if (proto->ItemClass == 2) // middle class scroll
				m_ScrollType = UpgradeScrollType::MiddleClassScroll;
			else if (proto->ItemClass == 3) // high class scroll
				m_ScrollType = UpgradeScrollType::HighClassScroll;
			else if (proto->ItemClass == 4) // rebirth scroll
				m_ScrollType = UpgradeScrollType::RebirthClassScroll;
			else if (proto->ItemClass == 8) // accessories class scroll
				m_ScrollType = UpgradeScrollType::AccessoriesClassScroll;


			if(m_ScrollType == LowClassScroll && 
				(pUserScroll != LowClassScroll 
				&& pUserScroll != MiddleClassScroll
				&& pUserScroll != HighClassScroll))
				goto fail_return;
			// Middle Class Scroll Match
			else if(m_ScrollType == MiddleClassScroll && 
				(pUserScroll != MiddleClassScroll
				&& pUserScroll != HighClassScroll))
				goto fail_return;
			// High Class Scroll Match && High to Rebirth Upgrade Class Scroll Match
			else if(m_ScrollType == HighClassScroll && 
				(pUserScroll != HighClassScroll
				&& pUserScroll != HighToRebirthScroll))
				goto fail_return;
			// Rebirth Upgrade Class Scroll Match
			else if(m_ScrollType == RebirthClassScroll 
				&& pUserScroll != RebirthClassScroll) // rebirth + basma
				goto fail_return;
			// Accessories Upgrade Class Scroll Match
			else if(m_ScrollType == AccessoriesClassScroll 
				&& pUserScroll != AccessoriesClassScroll) // Accessories Scroll
				goto fail_return;

			if (!hasCoins(pUpgrade->nReqNoah))
			{
				bResult = UpgradeNeedCoins;
				goto fail_return;
			}

			bResult = UpgradeSucceeded;
			break;
		}

		// If we ran out of upgrades to search through, it failed.
		if (bResult != UpgradeSucceeded
			|| pUpgrade == nullptr)
			goto fail_return;

		// Generate a random number, test if the item burned.
		int rand = myrand(1, 10000), GenRate;
		if(trina || karivdis)
		{
			if (trina)
				GenRate = (pUpgrade->sGenRate + (pUpgrade->sGenRate * g_pMain->m_upgrade1) / 100);
			else
				GenRate = (pUpgrade->sGenRate + (pUpgrade->sGenRate * g_pMain->m_upgrade2) / 100);

			if(GenRate > 10000)
				GenRate = 10000;
		}
		else 
			GenRate = pUpgrade->sGenRate;

		if (bType == UpgradeTypeNormal
			&& GenRate < rand)
		{
			bResult = UpgradeFailed;
			memset(pOriginItem, 0, sizeof(_ITEM_DATA));

			// Send upgrade notice.
			ItemUpgradeNotice(proto, UpgradeFailed);
			GoldLose(pUpgrade->nReqNoah,true); 
		}
		else
		{
			// Generate the new item ID
			int nNewItemID = pOriginItem->nNum + pUpgrade->nGiveItem;

			int nNewItemLevel = nNewItemID % 10;

			// Does this new item exist?
			_ITEM_TABLE * newProto = g_pMain->GetItemPtr(nNewItemID);
			if (newProto == nullptr)
			{ // if not, just say it doesn't match. No point removing the item anyway (like official :/).
				bResult = UpgradeNoMatch;
				goto fail_return;
			}

			if (bType != UpgradeTypePreview)
			{
				// Update the user's item in their inventory with the new item
				pOriginItem->nNum = nNewItemID;

				// Reset the durability also, to the new cap.
				pOriginItem->sDuration = newProto->m_sDuration;

				// Send upgrade notice.
				ItemUpgradeNotice(newProto, UpgradeSucceeded);

				// Rob gold upgrade noah
				GoldLose(pUpgrade->nReqNoah,true); 
			}

			// Replace the item ID in the list for the packet
			OriginItem->second.nItemID = nNewItemID;
		}

		// Remove the source item 
		if (bType != UpgradeTypePreview)
		{
			// Remove all required items, if applicable.
			for (int i = 1; i < MAX_ITEMS_REQ_FOR_UPGRADE; i++)
			{
				auto xitr = nItems.find(i);
				if(xitr == nItems.end())
					continue;

				_ITEM_DATA * pItem = GetItem(SLOT_MAX + xitr->second.bPos);
				if (pItem == nullptr
					|| pItem->nNum == 0 
					|| pItem->sCount == 0)
					continue;

				pItem->sCount--;
				if (pItem->sCount == 0 && pItem->nNum == xitr->second.nItemID)
					memset(pItem, 0, sizeof(pItem));
			}
		}
	}
	result << bType;

	result << bResult;
	foreach(i, nItems)
		result << i->second.nItemID <<  i->second.bPos;
	Send(&result);

	// Send the result to everyone in the area
	// (i.e. make the anvil do its burned/upgraded indicator thing)
	result.Initialize(WIZ_OBJECT_EVENT);
	result << uint8(OBJECT_ANVIL) << bResult << sNpcID;
	SendToRegion(&result,nullptr,GetEventRoom());

	return;
fail_return:
	result  << bType << bResult;

	foreach(i, nItems)
		result << i->second.nItemID <<  i->second.bPos;

	Send(&result);
}


/**
* @brief	Upgrade notice.
*
* @param	pItem	The item.
*/
void CUser::ItemUpgradeNotice(_ITEM_TABLE * pItem, uint8 UpgradeResult)
{
	bool bSendUpgradeNotice = false;
	std::string sUpgradeNotice;

	// Notice is only rebirth upgrade a Offical stuff.
	if (pItem->m_ItemType == 11 || pItem->m_ItemType == 12) 
		bSendUpgradeNotice = true;

	if (bSendUpgradeNotice)
	{
		if (UpgradeResult == 0)
			sUpgradeNotice = string_format("%s has failed to upgrade %s.",GetName().c_str(),pItem->m_sName.c_str());
		else if (UpgradeResult == 1)
			sUpgradeNotice = string_format("%s has succeeded to upgrade %s.",GetName().c_str(),pItem->m_sName.c_str());

		g_pMain->SendNotice(sUpgradeNotice.c_str(), Nation::ALL);
	}
}
/**
* @brief	Packet handler for the accessory upgrade system.
*
* @param	pkt	The packet.
*/
void CUser::ItemUpgradeAccessories(Packet & pkt)
{
	ItemUpgrade(pkt, ITEM_ACCESSORIES);
}

/**
* @brief	Packet handler for the Chaotic Generator system
* 			which is used to exchange Bifrost pieces/fragments.
*
* @param	pkt	The packet.
*/
void CUser::BifrostPieceProcess(Packet & pkt)
{
	enum ResultOpCodes
	{
		Failed	= 0,
		Success = 1
	};

	enum ResultMessages
	{
		EffectNone	= 0, // No effect
		EffectRed	= 1, // There will be better days.
		EffectGreen	= 2, // Don't be too disappointed. You're luck isn't that bad.
		EffectWhite	= 3 // It must be your lucky day.
	};

	uint16 nObjectID = 0;
	uint32 nExchangeItemID = 0;

	pkt >> nObjectID >> nExchangeItemID;

	std::vector<uint32> ExchangeIndexList;
	ResultOpCodes resultOpCode = Success;
	ResultMessages resultMessage = EffectNone;
	uint32 nItemID = 0;
	uint8 sItemSlot = 0;
	uint8 sExchangeItemSlot = 0;

	sExchangeItemSlot = FindSlotForItem(nExchangeItemID, 1) - SLOT_MAX;

	if (g_pMain->m_ItemExchangeArray.GetSize() > 0)
	{
		foreach_stlmap (itr, g_pMain->m_ItemExchangeArray)
		{
			if (itr->second->nOriginItemNum[0] == nExchangeItemID)
			{
				if (std::find(ExchangeIndexList.begin(),ExchangeIndexList.end(),itr->second->nIndex) == ExchangeIndexList.end())
					ExchangeIndexList.push_back(itr->second->nIndex);
			}
			else
				continue;
		}
	}

	if (ExchangeIndexList.size() > 0)
	{
		uint32 randIndex = myrand(0, ((int32) ExchangeIndexList.size() - 1));
		uint32 nExchangeID = ExchangeIndexList[randIndex];

		_ITEM_EXCHANGE * pExchange = g_pMain->m_ItemExchangeArray.GetData(nExchangeID);

		if (pExchange == nullptr
			|| !CheckExchange(nExchangeID)
			|| pExchange->bRandomFlag > 101
			|| !CheckExistItemAnd(pExchange->nOriginItemNum[0], pExchange->sOriginItemCount[0], 0, 0, 0, 0, 0, 0, 0, 0)) 
			resultOpCode = Failed;
		
		if (isTrading() || isMerchanting() || isSellingMerchant() || isBuyingMerchant() || isStoreOpen() || isMining())
			resultOpCode = Failed;

		if (resultOpCode == Success)
		{
			uint32 nTotalPercent = 0;
			for (int i = 0; i < ITEMS_IN_EXCHANGE_GROUP; i++)
				nTotalPercent += pExchange->sExchangeItemCount[i];

			if (nTotalPercent > 10000)
				resultOpCode = Failed;

			if (resultOpCode == Success)
			{
				uint8 bRandArray[10000];
				memset(&bRandArray, 0, sizeof(bRandArray)); 
				uint32 sExchangeCount[ITEMS_IN_EXCHANGE_GROUP];
				memcpy(&sExchangeCount, &pExchange->sExchangeItemCount, sizeof(pExchange->sExchangeItemCount));

				int offset = 0;
				for (int n = 0, i = 0; n < ITEMS_IN_EXCHANGE_GROUP; n++)
				{
					if (sExchangeCount[n] > 0)
					{
						memset(&bRandArray[offset], n, sExchangeCount[n]);
						offset += sExchangeCount[n];
					}
				}

				uint8 bRandSlot = bRandArray[myrand(0, 9999)];
				nItemID = pExchange->nExchangeItemNum[bRandSlot];

				sItemSlot = GetEmptySlot() - SLOT_MAX;
				RobItem(pExchange->nOriginItemNum[0], 1);
				GiveItem(nItemID, 1);

				_ITEM_TABLE *pItem = g_pMain->m_ItemtableArray.GetData(nItemID);

				if (pItem != nullptr)
				{
					if (pItem->m_ItemType == 4)
						resultMessage = EffectWhite;
					else if (pItem->m_ItemType == 5 
						|| pItem->m_ItemType == 11 
						|| pItem->m_ItemType == 12)
						resultMessage = EffectGreen;
					else
						resultMessage = EffectRed;
				}
			}
		}
	} 

	Packet result(WIZ_ITEM_UPGRADE);
	result << (uint8)ITEM_BIFROST_EXCHANGE << (uint8)resultOpCode << nItemID << sItemSlot << nExchangeItemID << sExchangeItemSlot << (uint8)resultMessage;
	Send(&result);

	result.clear();
	result.SetOpcode(WIZ_OBJECT_EVENT);
	result << (uint8)OBJECT_ARTIFACT << (uint8)resultMessage << nObjectID;

	if (resultOpCode != Failed)
		SendToRegion(&result);
}

/**
* @brief	Packet handler for the Special exchange system
* 			which is used to exchange Krowaz meterials.
*
* @param	pkt	The packet.
*/

void CUser::SpecialItemExchange(Packet & pkt)
{
	enum ResultOpCodes
	{
		WrongMaterial = 0,
		Success = 1,
		Failed = 2
	};

	ResultOpCodes resultOpCode = WrongMaterial;

	uint16 sNpcID;
	uint32 nShadowPiece;
	uint8 nShadowPieceSlot;
	uint8 nMaterialCount;
	uint8 nItemSlot[10];
	uint8 nDownFlag;
	uint32 nItemID[10]; 
	uint8 nItemCount[10];

	uint32 nItemNumber = 0;
	uint8 sItemSlot = 0;

	pkt >> sNpcID >> nShadowPiece >> nShadowPieceSlot >> nMaterialCount;

	for (int i = 0; i < 10; i++)
	{
		nItemID[i] = 0;
		nItemCount[i] = 0;
	}

	for (int i = 0; i < nMaterialCount; i++)
		pkt >> nItemSlot[i];

	pkt >> nDownFlag;

	for (int i = 0; i < nMaterialCount; i++)
	{
		uint8 nReadByte;
		int nDigit = 100000000;
		nItemID[i] = 0;
		for( int x = 0; x < 9; x++ ) 
		{
			pkt >> nReadByte;
			nItemID[i] += (nReadByte - 48) * nDigit;
			nDigit = nDigit / 10;
		}

		uint8 nCount[3] = { 0, 0, 0};
		pkt >> nCount[0];
		pkt >> nCount[1];
		pkt >> nCount[2];
		int nCountFinish = 0;
		nCountFinish += (nCount[0] - 48) * 100;
		nCountFinish += (nCount[1] - 48) * 10;
		nCountFinish += (nCount[2] - 48) * 1;
		nItemCount[i] = nCountFinish;
	}

	std::vector<uint32> ExchangeIndexList;

	if (nMaterialCount >= 2) // Minimum Required : 2 Material
	{
		if (g_pMain->m_ItemExchangeArray.GetSize() > 0)
		{
			foreach_stlmap(itr, g_pMain->m_ItemExchangeArray)
			{
				if (itr->second->bRandomFlag == 102) // Special Item Exchange
				{
					if (nShadowPiece == 0 && itr->second->nOriginItemNum[0] == SHADOW_PIECE) // If Need Shadow Piece Please Set is nOriginItem1 Column... 
						continue;
					else
					{
						uint8 nOriginItemCount = 0;
						uint8 nMatchCount = 0;
						bool bAddArray = false;

						if (nMaterialCount == 2)
							nMatchCount = (nShadowPiece == 0 ? 2 : 3);
						else if (nMaterialCount == 3)
							nMatchCount = (nShadowPiece == 0 ? 3 : 4);
						else if (nMaterialCount == 4)
							nMatchCount = (nShadowPiece == 0 ? 4 : 5);
						else if (nMaterialCount == 5)
							nMatchCount = (nShadowPiece == 0 ? 5 : 6);
						else if (nMaterialCount == 6)
							nMatchCount = (nShadowPiece == 0 ? 6 : 7);
						else if (nMaterialCount == 7)
							nMatchCount = (nShadowPiece == 0 ? 7 : 8);
						else if (nMaterialCount == 8)
							nMatchCount = (nShadowPiece == 0 ? 8 : 9);
						else if (nMaterialCount == 9)
							nMatchCount = (nShadowPiece == 0 ? 9 : 10);

						for (int i = 0; i < nMaterialCount; i++)
						{
							if (nItemID[i] != 0)
							{
								for (int x = 0; x < ITEMS_IN_ORIGIN_GROUP; x++)
								{
									if (itr->second->nOriginItemNum[x] != 0
										&& nItemID[i] == itr->second->nOriginItemNum[x])
									{
										nOriginItemCount++;
										break;
									}
								}
							}
						}

						if (nOriginItemCount == nMatchCount)
							bAddArray = true;
						else if (nOriginItemCount == nMatchCount)
							bAddArray = true;

						if (bAddArray && std::find(ExchangeIndexList.begin(),ExchangeIndexList.end(),itr->second->nIndex) == ExchangeIndexList.end())
							ExchangeIndexList.push_back(itr->second->nIndex);
					}
				}
				else
					continue;
			}
		}
	}

	if (ExchangeIndexList.size() > 0)
	{
		uint32 randIndex = myrand(0, ((int32) ExchangeIndexList.size() - 1));
		uint32 nExchangeID = ExchangeIndexList[randIndex];

		_ITEM_EXCHANGE * pExchange = g_pMain->m_ItemExchangeArray.GetData(nExchangeID);

		if (pExchange == nullptr
			|| !CheckExchange(nExchangeID)
			|| pExchange->bRandomFlag > 102
			|| !CheckExistItemAnd(
			pExchange->nOriginItemNum[0], pExchange->sOriginItemCount[0], 
			pExchange->nOriginItemNum[1], pExchange->sOriginItemCount[1], 
			pExchange->nOriginItemNum[2], pExchange->sOriginItemCount[2], 
			pExchange->nOriginItemNum[3], pExchange->sOriginItemCount[3], 
			pExchange->nOriginItemNum[4], pExchange->sOriginItemCount[4]))
		{
			resultOpCode = WrongMaterial;
		}
		else
		{
			bool bContinueExchange = true;

			for (int i = 0; i < nMaterialCount; i++)
			{
				if (!bContinueExchange)
					break;

				if (nItemID[i] != 0)
				{
					for (int x = 0; x < ITEMS_IN_ORIGIN_GROUP; x++)
					{
						if (pExchange->nOriginItemNum[x] != 0
							&& nItemID[i] == pExchange->nOriginItemNum[x]
						&& nItemCount[i] != pExchange->sOriginItemCount[x])
						{
							bContinueExchange = false;
							break;
						}
					}
				}
			}

			if (isTrading() || isMerchanting() || isSellingMerchant() || isBuyingMerchant() || isStoreOpen() || isMining())
				bContinueExchange = false;

			if (!bContinueExchange)
				resultOpCode = WrongMaterial;
			else
			{
				uint32 nTotalPercent = 0;
				for (int i = 0; i < ITEMS_IN_EXCHANGE_GROUP; i++)
					nTotalPercent += pExchange->sExchangeItemCount[i];

				if (nTotalPercent > 10000)
					resultOpCode = WrongMaterial;
				else
				{
					uint8 bRandArray[10000];
					memset(&bRandArray, 0, sizeof(bRandArray)); 
					uint16 sExchangeCount[ITEMS_IN_EXCHANGE_GROUP];
					memcpy(&sExchangeCount, &pExchange->sExchangeItemCount, sizeof(pExchange->sExchangeItemCount));

					int offset = 0;
					for (int n = 0, i = 0; n < ITEMS_IN_EXCHANGE_GROUP; n++)
					{
						if (sExchangeCount[n] > 0)
						{
							memset(&bRandArray[offset], n, sExchangeCount[n]);
							offset += sExchangeCount[n];
						}
					}

					uint8 bRandSlot = bRandArray[myrand(0, 9999)];				
					nItemNumber = pExchange->nExchangeItemNum[bRandSlot];
					uint16 nItemRate = pExchange->sExchangeItemCount[bRandSlot];
					int rand = myrand(0, myrand(9000, 10000));

					if (nItemRate <= rand)
						resultOpCode = Failed;
					else
					{
						sItemSlot = GetEmptySlot() - SLOT_MAX;
						GiveItem(nItemNumber, 1);
						resultOpCode = Success;
					}

					for (int i = 0; i < ITEMS_IN_ORIGIN_GROUP; i++)
					{
						if (pExchange->nOriginItemNum[i] != 0)
							RobItem(pExchange->nOriginItemNum[i], pExchange->sOriginItemCount[i]);
					}
				}
			}
		}
	}

	Packet result(WIZ_ITEM_UPGRADE);
	result << (uint8)ITEM_SPECIAL_EXCHANGE << (uint8)resultOpCode << sNpcID;

	if (resultOpCode == Success)
		result << nItemNumber << sItemSlot;

	Send(&result);

	if (resultOpCode == Success)
		ShowNpcEffect(31033, true);
	else if (resultOpCode == Failed)
		ShowNpcEffect(31034, true);
}

/**
* @brief	Packet handler for the upgrading of 'rebirthed' items.
*
* @param	pkt	The packet.
*/
void CUser::ItemUpgradeRebirth(Packet & pkt)
{
	ItemUpgrade(pkt, ITEM_UPGRADE_REBIRTH);
}

/**
* @brief	Packet handler for the Special Old Man Exchange
* 			which is used to exchange ItemExchangeCrash.
*
* @param	pkt	The packet.
*/

void CUser::SpecialOldManExchange(Packet & pkt)
{
#if 0
	enum ResultOpCodes
	{
		Failed	= 0,
		Success = 1
	};

	enum ResultMessages
	{
		nExchangeIndexID1 = 1,
		nExchangeIndexID2 = 2,
		nExchangeIndexID3 = 3
	};
	uint8 nPos;
	uint8 nItemCount = 1;
	ResultOpCodes resultOpCode = Success;
	uint16 nObjectID = 0;
	uint32 nExchangeItemID = 0;
	std::vector<uint32> ExchangeIndexCrashList;
	pkt >> nExchangeItemID >> nPos >> nObjectID;
	Packet result(WIZ_ITEM_UPGRADE);

	_ITEM_DATA* pItemData = GetItem(nPos + SLOT_MAX);

	if (pItemData == nullptr)
		return;

	if (g_pMain->m_ItemExchangeCrashArray.GetSize() > 0)
	{
		foreach_stlmap(itr, g_pMain->m_ItemExchangeCrashArray) 
		{
			if (std::find(ExchangeIndexCrashList.begin(),ExchangeIndexCrashList.end(),itr->second->nIndex) == ExchangeIndexCrashList.end())
				ExchangeIndexCrashList.push_back(itr->second->nIndex);
		}
	}

	if (ExchangeIndexCrashList.size() > 0)
	{
		_ITEM_TABLE* pItem = g_pMain->GetItemPtr(nExchangeItemID);

		uint32 randIndex[nExchangeIndexID3];
		uint32 nExchangeID[nExchangeIndexID3];

		if (pItem == nullptr)
			return;

		if (pItem->m_ItemType >= 0 && pItem->m_ItemType <= 1 )
			nItemCount = nExchangeIndexID1;
		else if (pItem->m_ItemType == 5 || pItem->m_ItemType == 11)
			nItemCount = nExchangeIndexID2;
		else if (pItem->m_ItemType == 4	|| pItem->m_ItemType == 12 || pItem->m_ItemType == 13) 
			nItemCount = nExchangeIndexID3;
		else 
			return;

		for (int i = 0; i < nItemCount; i++)
		{
			randIndex[i] = myrand(0, (ExchangeIndexCrashList.size() - 1));
			nExchangeID[i] = ExchangeIndexCrashList[randIndex[i]];
		}

		bool CheckSlot = CheckGiveSlot(nItemCount);

		if(!CheckSlot 
			|| !RobItem(nExchangeItemID, 1))
			return;

		RobItem(nExchangeItemID, 1);
		uint8 emptySlot = GetEmptySlot();
		result << (uint8)ITEM_OLDMAN_EXCHANGE << (uint16)resultOpCode << nExchangeItemID << nPos << (uint16)nItemCount;

		if (emptySlot >= nItemCount)
		{
			for (int i = 0; i < nItemCount; i++)
			{
				_ITEM_EXCHANGE_CRASH * pExchangeCrashItem = g_pMain->m_ItemExchangeCrashArray.GetData(nExchangeID[i]);
				if (pExchangeCrashItem == nullptr) 
					continue;

				int8 pos;
				if ((pos = FindSlotForItem(pExchangeCrashItem->nItemID, pExchangeCrashItem->nCount)) < 0)
					continue;

				GiveItem(pExchangeCrashItem->nItemID, pExchangeCrashItem->nCount, true);
				result << pExchangeCrashItem->nItemID << uint8(pos - SLOT_MAX) << (uint16)pExchangeCrashItem->nCount;
			}
		}
	}
	Send(&result);
#endif
}
