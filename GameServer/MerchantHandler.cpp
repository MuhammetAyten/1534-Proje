#include "stdafx.h"
#include "../shared/DateTime.h"
using std::string;

#define ITEM_MENICIAS_LIST 810166000

enum MerchantOpenResponseCodes
{
	MERCHANT_OPEN_SUCCESS = 1,
	MERCHANT_OPEN_NO_SESSION = -1,
	MERCHANT_OPEN_DEAD = -2,
	MERCHANT_OPEN_TRADING = -3,
	MERCHANT_OPEN_MERCHANTING = -4,
	MERCHANT_OPEN_INVALID_ZONE = -5,
	MERCHANT_OPEN_SHOPPING = -6,
	MERCHANT_OPEN_UNDERLEVELED = 30
};

void CUser::MerchantProcess(Packet & pkt)
{
	if (!isInGame())
		return;

	uint8 opcode = pkt.read<uint8>();

	switch (opcode)
	{
		// Regular merchants
	case MERCHANT_OPEN: 
		MerchantOpen(); 
		break;

	case MERCHANT_CLOSE: 
		MerchantClose(); 
		break;

	case MERCHANT_ITEM_ADD: 
		MerchantItemAdd(pkt); 
		break;

	case MERCHANT_ITEM_CANCEL: 
		MerchantItemCancel(pkt); 
		break;

	case MERCHANT_ITEM_LIST:  // 6805F9FF
		MerchantItemList(pkt); 
		break;

	case MERCHANT_ITEM_BUY: 
		MerchantItemBuy(pkt); 
		break;

	case MERCHANT_INSERT: 
		MerchantInsert(pkt); 
		break;

	case MERCHANT_TRADE_CANCEL: 
		CancelMerchant(); 
		break;

		// Buying merchants
	case MERCHANT_BUY_OPEN: 
		BuyingMerchantOpen(pkt); 
		break;

	case MERCHANT_BUY_CLOSE: 
		BuyingMerchantClose(); 
		break;

	case MERCHANT_BUY_LIST: 
		BuyingMerchantList(pkt); 
		break;

	case MERCHANT_BUY_INSERT: 
		BuyingMerchantInsert(pkt); 
		break;

	case MERCHANT_BUY_BUY: // seeya!
		BuyingMerchantBuy(pkt); 
		break;

	case MERCHANT_MENISIA_LIST:
		MerchantOfficialList(pkt);
		break;
	}
}

void CUser::MerchantOpen()
{
	if(isBuyingMerchantingPreparing())
		return;

	int16 errorCode = 0;
	if (isDead())
		errorCode = MERCHANT_OPEN_DEAD;
	else if (isStoreOpen())
		errorCode = MERCHANT_OPEN_SHOPPING;
	else if (isTrading())
		errorCode = MERCHANT_OPEN_TRADING;
	else if (GetZoneID() > ZONE_MORADON)
		errorCode = MERCHANT_OPEN_INVALID_ZONE;
	else if (GetLevel() < 30)
		errorCode = MERCHANT_OPEN_UNDERLEVELED;
	else if (isMerchanting())
		errorCode = MERCHANT_OPEN_MERCHANTING;
	else 
		errorCode = MERCHANT_OPEN_SUCCESS;

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_OPEN));
	result << errorCode;
	Send(&result);

	// If we're already merchanting, user may be desynced
	// so we need to close our current merchant first.
	if (errorCode == MERCHANT_OPEN_MERCHANTING)
		MerchantClose();

	if (errorCode == MERCHANT_OPEN_SUCCESS)
		m_bSellingMerchantPreparing = true;

	DateTime time;
	g_pMain->WriteMerchantLogFile(string_format("MERCHANT SELLING-OPEN - %d:%d:%d || Seller=%s,Zone=%d,X=%d,Z=%d\n",time.GetHour(),time.GetMinute(),time.GetSecond(),GetName().c_str(),GetZoneID(),uint16(GetX()),uint16(GetZ())));
}

void CUser::MerchantClose()
{
	if(!isSellingMerchantingPreparing() || isBuyingMerchantingPreparing())
		return;

	GiveMerchantItems(); // Give back to the user that which hasn't been sold, if any.

	m_bMerchantOpen = false;
	m_bMerchantViewer = -1;
	m_bSellingMerchantPreparing = false;
	m_bMerchantState = MERCHANT_STATE_NONE;
	Packet result(WIZ_MERCHANT, uint8(MERCHANT_CLOSE));
	result << GetSocketID();
	SendToRegion(&result,nullptr,GetEventRoom());
	DateTime time;
	g_pMain->WriteMerchantLogFile(string_format("MERCHANT SELLING-CLOSE - %d:%d:%d || Seller=%s,Zone=%d,X=%d,Z=%d\n",time.GetHour(),time.GetMinute(),time.GetSecond(),GetName().c_str(),GetZoneID(),uint16(GetX()),uint16(GetZ())));
}

void CUser::MerchantItemAdd(Packet & pkt)
{
	if(!isSellingMerchantingPreparing() || isBuyingMerchantingPreparing())
		return;

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_ITEM_ADD));
	uint32 nGold, nItemID;
	uint16 sCount;
	uint8 bSrcPos, // It sends the "actual" inventory slot (SLOT_MAX -> INVENTORY_MAX-SLOT_MAX), so need to allow for it. 
		bDstPos, 
		bMode; // Might be a flag for normal / "premium" merchant mode, once skills are implemented take another look at this.

	pkt >> nItemID >> sCount >> nGold >> bSrcPos >> bDstPos >> bMode;

	// TODO: Implement the possible error codes for these various error cases.
	if (bSrcPos >= HAVE_MAX
		|| bDstPos >= MAX_MERCH_ITEMS)
		return;

	_ITEM_TABLE * pTable = g_pMain->GetItemPtr(nItemID);
	if (pTable == nullptr
		|| nItemID >= ITEM_NO_TRADE // Cannot be traded, sold or stored.
		|| pTable->m_bRace == RACE_UNTRADEABLE // Cannot be traded or sold.
		|| (pTable->m_bKind == 255 && sCount != 1)) // Cannot be traded or sold.
		goto fail_return;

	bSrcPos += SLOT_MAX;
	_ITEM_DATA *pSrcItem = GetItem(bSrcPos);
	if (pSrcItem == nullptr 
		|| pSrcItem->nNum != nItemID
		|| pSrcItem->sCount == 0 
		|| sCount == 0
		|| pSrcItem->sCount < sCount
		|| pSrcItem->isRented()
		|| pSrcItem->isSealed()
		|| pSrcItem->isBound()
		|| pSrcItem->isDuplicate())
		goto fail_return;

	_MERCH_DATA *pMerch = &m_arMerchantItems[bDstPos];

	if(pMerch == nullptr)
		return; 

	pMerch->nNum = nItemID;
	pMerch->nPrice = nGold;
	pMerch->sCount = sCount; // Selling Count
	if(pTable->m_bKind == 255)
		pMerch->bCount = sCount; // Original Count ( INVENTORY )
	else 
		pMerch->bCount = pSrcItem->sCount; // Original Count ( INVENTORY )

	pMerch->sDuration = pSrcItem->sDuration;
	pMerch->nSerialNum = pSrcItem->nSerialNum; // NOTE: Stackable items will have an issue with this.
	pMerch->nUserSeal = pSrcItem->nUserSeal;

	pMerch->bOriginalSlot = bSrcPos;
	pMerch->IsSoldOut = false;

	// Take the user's item.
	memset(pSrcItem, 0, sizeof(_ITEM_DATA));

	result	<< uint16(1)
		<< nItemID << sCount << pMerch->sDuration << nGold 
		<< bSrcPos << bDstPos;
	Send(&result);

fail_return:
	result  << uint16(0)
		<< nItemID << sCount << pMerch->sDuration << nGold 
		<< bSrcPos << bDstPos;
	Send(&result);
}

void CUser::MerchantItemCancel(Packet & pkt)
{
	if(!isSellingMerchantingPreparing() || isBuyingMerchantingPreparing())
		return;

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_ITEM_CANCEL));
	_MERCH_DATA *pMerch = nullptr;
	_ITEM_DATA *pItem = nullptr;

	uint8 bSrcPos = pkt.read<uint8>();

	// Invalid source position
	if (bSrcPos >= MAX_MERCH_ITEMS)
		result << int16(-2);
	// There's no item in that list..?
	else if ((pMerch = &m_arMerchantItems[bSrcPos])->nNum == 0)
		result << int16(-3);
	// Check to make sure we've got a valid stack
	else if (pMerch->sCount + (pItem = &m_sItemArray[pMerch->bOriginalSlot])->sCount > ITEMCOUNT_MAX) 
		result << int16(-3); // custom error
	else
	{
		pItem->nNum = pMerch->nNum;
		pItem->sCount = pMerch->sCount;
		pItem->sDuration = pMerch->sDuration;
		pItem->nSerialNum = pMerch->nSerialNum; // NOTE: Stackable items will have an issue with this.
		pItem->nUserSeal = pMerch->nUserSeal;

		memset(pMerch, 0, sizeof(_MERCH_DATA));
		result << int16(1) << bSrcPos;
	}

	Send(&result);
}

void CUser::MerchantInsert(Packet & pkt)
{
	if(!isSellingMerchantingPreparing() || isBuyingMerchantingPreparing())
		return;

	DateTime time;
	string advertMessage; // check here maybe to make sure they're not using it otherwise?
	pkt >> advertMessage;
	if (advertMessage.size() > MAX_MERCH_MESSAGE)
		return;

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_INSERT));
	result << uint16(1) << advertMessage << GetSocketID()
		<< m_bPremiumMerchant; 

	uint16 totalMerchItems = 0;
	for (int i = 0; i < MAX_MERCH_ITEMS; i++)
	{
		if(m_arMerchantItems[i].nNum != 0 && 
			(m_arMerchantItems[i].bCount == 0 || m_arMerchantItems[i].bCount < m_arMerchantItems[i].sCount))
			return;

		result << m_arMerchantItems[i].nNum;

		if(m_arMerchantItems[i].nNum > 0)
		{
			totalMerchItems++;
			g_pMain->WriteMerchantLogFile(string_format("MERCHANT SELLING-ITEMS - %d:%d:%d || Seller=%s,ItemID=%d,Count=%d/%d,Price=%d,Zone=%d,X=%d,Z=%d\n",time.GetHour(),time.GetMinute(),time.GetSecond(),GetName().c_str(),m_arMerchantItems[i].nNum,m_arMerchantItems[i].bCount, m_arMerchantItems[i].sCount, m_arMerchantItems[i].nPrice,GetZoneID(),uint16(GetX()),uint16(GetZ())));
		}
	}

	if(totalMerchItems == 0)
	{
		Disconnect();
		g_pMain->WriteCheatLogFile(string_format("Merchant hack - %d:%d:%d || %s is disconnected.\n", time.GetHour(),time.GetMinute(),time.GetSecond(),GetName().c_str()));
		return;
	}

	m_bMerchantState = MERCHANT_STATE_SELLING;
	SendToRegion(&result,nullptr,GetEventRoom());
}

void CUser::MerchantItemList(Packet & pkt)
{
	uint16 uid = pkt.read<uint16>();

	CUser *pMerchant = g_pMain->GetUserPtr(uid);
	if (pMerchant == nullptr
		|| !pMerchant->isMerchanting()
		|| !pMerchant->isSellingMerchantingPreparing()
		|| pMerchant->isBuyingMerchantingPreparing())
		return;

	if(pMerchant->m_bMerchantOpen == true)
	{
		Packet result(WIZ_MERCHANT, uint8(MERCHANT_ITEM_LIST));
		result << uint8(249) << uint8(-1); // 6805F9FF
		Send(&result);
		return;
	}

	pMerchant->m_bMerchantOpen = true;
	pMerchant->m_bMerchantViewer = GetID();
	m_sMerchantsSocketID = uid;

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_ITEM_LIST));
	result << uint16(1) << uint16(uid);
	for (int i = 0; i < MAX_MERCH_ITEMS; i++)
	{
		_MERCH_DATA *pMerch = &pMerchant->m_arMerchantItems[i];
		result	<< pMerch->nNum
			<< pMerch->sCount
			<< pMerch->sDuration 
			<< pMerch->nPrice;

		if (pMerch->isUserSeal())
				SendUserSealItem(result, pMerch->nUserSeal);
			else 
				result << uint32(0); // Not sure what this one is
	}

	Send(&result);
}

void CUser::MerchantItemBuy(Packet & pkt)
{
	uint32 itemid, req_gold;
	uint16 item_count, leftover_count;
	uint8 item_slot, dest_slot;

	CUser *pMerchant = g_pMain->GetUserPtr(m_sMerchantsSocketID);
	if (pMerchant == nullptr  
		|| pMerchant->GetMerchantState() != MERCHANT_STATE_SELLING
		|| !pMerchant->isSellingMerchantingPreparing() 
		|| pMerchant->isBuyingMerchantingPreparing())
		return;

	pkt >> itemid >> item_count >> item_slot >> dest_slot;

	// Make sure the slots are correct and that we're actually buying at least 1 item.
	if (item_slot >= MAX_MERCH_ITEMS
		|| dest_slot >= HAVE_MAX
		|| item_count == 0)
		return;

	// Grab pointers to the items.
	_MERCH_DATA *pMerch = &pMerchant->m_arMerchantItems[item_slot];
	_ITEM_DATA *pItem = GetItem(SLOT_MAX + dest_slot);
	if(pItem == nullptr)
		return;

	// Make sure the merchant actually has that item in that slot
	// and that they have enough
	if (pMerch->nNum != itemid
		|| pMerch->sCount < item_count
		|| pMerch->bCount < item_count)
		return;

	// If it's not stackable, and we're specifying something other than 1
	// we really don't care to handle this request...
	_ITEM_TABLE *proto = g_pMain->GetItemPtr(itemid);
	if (proto == nullptr
		|| !proto->m_bCountable && item_count != 1)
		return;

	// Do we have enough coins?
	req_gold = pMerch->nPrice * item_count;
	if (m_iGold < req_gold)
		return;

	// If the slot's not empty
	if (pItem->nNum != 0 
		// and we already have an item that isn't the same item
			// or it's the same item but the item's not stackable...
				&& (pItem->nNum != itemid || !proto->m_bCountable))
				return;

	if(pMerchant->m_iGold + req_gold > COIN_MAX)
		return;

	if(!GoldLose(req_gold))
		return;
	pMerchant->GoldGain(req_gold);

	leftover_count = pMerch->sCount - item_count;

	pItem->nNum = pMerch->nNum;
	pItem->sCount += item_count;
	pItem->sDuration = pMerch->sDuration;
	pItem->nSerialNum = pMerch->nSerialNum;
	pItem->nUserSeal = pMerch->nUserSeal;

	pMerch->sCount -= item_count;
	pMerch->bCount -= item_count;

	SendStackChange(itemid, pItem->sCount, pItem->sDuration, dest_slot, 
		(pItem->sCount == item_count)); // is it a new item?

	pMerchant->SendStackChange(pMerch->nNum, pMerch->bCount, pMerch->sDuration,
		pMerch->bOriginalSlot- SLOT_MAX);

	if (pMerch->sCount == 0 && pMerch->bCount == 0)
		memset(pMerch, 0, sizeof(_MERCH_DATA));
	else if (pMerch->sCount == 0 && pMerch->bCount != 0) // Countable item protect.
		pMerch->IsSoldOut = true;

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_ITEM_PURCHASED));
	result << itemid << GetName();
	pMerchant->Send(&result);

	result.clear();

	result	<< uint8(MERCHANT_ITEM_BUY) << uint16(1)
		<< itemid << leftover_count
		<< item_slot << dest_slot;
	Send(&result);

	if (item_slot < 4 && leftover_count == 0)
	{
		result.Initialize(WIZ_MERCHANT_INOUT);
		result << uint8(2) << m_sMerchantsSocketID << uint8(1) << uint8(0) << item_slot;
		pMerchant->SendToRegion(&result,nullptr,GetEventRoom());
	}

	int nItemsRemaining = 0;
	for (int i = 0; i < MAX_MERCH_ITEMS; i++)
	{
		if (pMerchant->m_arMerchantItems[i].nNum != 0 && !pMerchant->m_arMerchantItems[i].IsSoldOut)
			nItemsRemaining++;
	}

	if (nItemsRemaining == 0)
		pMerchant->MerchantClose();
	DateTime time;
	g_pMain->WriteMerchantLogFile(string_format("MERCHANT SELLING-BUY - %d:%d:%d || Seller=%s,Buyer=%s,ItemID=%d,Count=%d,Price=%d,Zone=%d,X=%d,Z=%d\n",time.GetHour(),time.GetMinute(),time.GetSecond(),pMerchant->GetName().c_str(),GetName().c_str(),itemid,item_count,req_gold,GetZoneID(),uint16(GetX()),uint16(GetZ())));
}			

void CUser::GiveMerchantItems()
{
	if(m_bMerchantState == MERCHANT_STATE_SELLING || m_bSellingMerchantPreparing)
	{
		for (int i = 0; i < MAX_MERCH_ITEMS; i++)
		{
			_MERCH_DATA *pMerch = &m_arMerchantItems[i];
			if (pMerch->nNum == 0) continue;
			_ITEM_DATA *pItem = &m_sItemArray[pMerch->bOriginalSlot];

			pItem->nNum = pMerch->nNum;
			pItem->nSerialNum = pMerch->nSerialNum;
			pItem->nUserSeal = pMerch->nUserSeal;
			pItem->sCount = pMerch->bCount;
			pItem->sDuration = pMerch->sDuration;

			// NOTE: Don't need to update the client, the client doesn't see any change.
		}
	}
	// remove the items from the array now that they've been restored to the user
	memset(&m_arMerchantItems, 0, sizeof(m_arMerchantItems));
}

void CUser::CancelMerchant()
{
	if (m_sMerchantsSocketID < 0)
		return;

	RemoveFromMerchantLookers();
	Packet result(WIZ_MERCHANT, uint8(MERCHANT_TRADE_CANCEL));
	result << uint16(1);
	Send(&result);
}

void CUser::BuyingMerchantOpen(Packet & pkt)
{
	if(isSellingMerchantingPreparing())
		return;

	int16 errorCode = 0;
	if (isDead())
		errorCode = MERCHANT_OPEN_DEAD;
	else if (isStoreOpen())
		errorCode = MERCHANT_OPEN_SHOPPING;
	else if (isTrading())
		errorCode = MERCHANT_OPEN_TRADING;
	else if (GetZoneID() > ZONE_MORADON || GetZoneID() <= ELMORAD)
		errorCode = MERCHANT_OPEN_INVALID_ZONE;
	else if (GetLevel() < 30)
		errorCode = MERCHANT_OPEN_UNDERLEVELED;
	else if (isMerchanting())
		errorCode = MERCHANT_OPEN_MERCHANTING;
	else 
		errorCode = MERCHANT_OPEN_SUCCESS;

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_BUY_OPEN));
	result << errorCode;
	Send(&result);

	if (errorCode == MERCHANT_OPEN_MERCHANTING)
		BuyingMerchantClose();

	if (errorCode == MERCHANT_OPEN_SUCCESS)
		m_bBuyingMerchantPreparing = true;

	memset(&m_arMerchantItems, 0, sizeof(m_arMerchantItems));
	DateTime time;
	g_pMain->WriteMerchantLogFile(string_format("MERCHANT BUYING-OPEN - %d:%d:%d || Buyer=%s,Zone=%d,X=%d,Z=%d\n",time.GetHour(),time.GetMinute(),time.GetSecond(),GetName().c_str(),GetZoneID(),uint16(GetX()),uint16(GetZ())));
}

void CUser::BuyingMerchantClose()
{
	if (isMerchanting() && isBuyingMerchantingPreparing())
	{
		if(m_bMerchantViewer >= 0)
		{
			CUser* pUser = g_pMain->GetUserPtr(m_bMerchantViewer);
			if(pUser != nullptr)
			{
				Packet result2(WIZ_MERCHANT, uint8(29));
				result2 << uint16(1);
				pUser->Send(&result2);
			}
		}

		m_bMerchantViewer = -1;
		m_bBuyingMerchantPreparing = false;
		m_bMerchantOpen = false;
		m_bMerchantState = MERCHANT_STATE_NONE;
		memset(m_arMerchantItems, 0, sizeof(m_arMerchantItems));

		Packet result(WIZ_MERCHANT, uint8(MERCHANT_BUY_CLOSE));
		result << GetSocketID();
		SendToRegion(&result,nullptr,GetEventRoom());
		DateTime time;
		g_pMain->WriteMerchantLogFile(string_format("MERCHANT BUYING-CLOSE - %d:%d:%d || Buyer=%s,Zone=%d,X=%d,Z=%d\n",time.GetHour(),time.GetMinute(),time.GetSecond(),GetName().c_str(),GetZoneID(),uint16(GetX()),uint16(GetZ())));
	}
	else if (m_sMerchantsSocketID >= 0)
	{
		RemoveFromMerchantLookers();
	}
}

void CUser::BuyingMerchantInsert(Packet & pkt)
{
	if(isSellingMerchantingPreparing() || !isBuyingMerchantingPreparing())
		return;

	uint8 amount_of_items;
	uint32 itemid, buying_price;
	uint32 totalamount = 0;
	uint16 item_count;
	_ITEM_TABLE *pItem = nullptr;

	pkt >> amount_of_items;

	for (int i = 0; i < amount_of_items; i++)
	{
		pkt >> itemid >> item_count >> buying_price;
		pItem = g_pMain->m_ItemtableArray.GetData(itemid);

		if (pItem == nullptr
			|| itemid >= ITEM_NO_TRADE // Cannot be traded, sold or stored.
			|| pItem->m_bRace == RACE_UNTRADEABLE // Cannot be traded or sold.
			|| (pItem->m_bKind == 255 && item_count != 1)) // Cannot be traded or sold.
			return;

		m_arMerchantItems[i].nNum = itemid;
		m_arMerchantItems[i].sCount = item_count;
		m_arMerchantItems[i].nPrice = buying_price;
		m_arMerchantItems[i].sDuration = pItem->m_sDuration;
		totalamount += buying_price;
	}

	if (!hasCoins(totalamount))
		return;

	m_bMerchantState = MERCHANT_STATE_BUYING;
	Packet result(WIZ_MERCHANT, uint8(MERCHANT_BUY_INSERT));
	result << uint8(1);
	Send(&result);

	BuyingMerchantInsertRegion();
}

void CUser::BuyingMerchantInsertRegion()
{
	Packet result(WIZ_MERCHANT, uint8(MERCHANT_BUY_REGION_INSERT));
	result << GetSocketID();
	DateTime time;
	for (int i = 0; i < 4; i++)
	{
		result << m_arMerchantItems[i].nNum;
		if(m_arMerchantItems[i].nNum > 0)
			g_pMain->WriteMerchantLogFile(string_format("MERCHANT BUYING-ITEMS - %d:%d:%d || Buyer=%s,ItemID=%d,Count=%d/%d,Price=%d,Zone=%d,X=%d,Z=%d\n",time.GetHour(),time.GetMinute(),time.GetSecond(),GetName().c_str(),m_arMerchantItems[i].nNum,m_arMerchantItems[i].bCount, m_arMerchantItems[i].sCount,m_arMerchantItems[i].nPrice,GetZoneID(),uint16(GetX()),uint16(GetZ())));
	}

	SendToRegion(&result,nullptr,GetEventRoom());
}

void CUser::BuyingMerchantList(Packet & pkt)
{
	if (m_sMerchantsSocketID >= 0)
		RemoveFromMerchantLookers(); //This check should never be hit...

	uint16 uid = pkt.read<uint16>();

	CUser *pMerchant = g_pMain->GetUserPtr(uid);
	if (pMerchant == nullptr
		|| !pMerchant->isMerchanting()
		|| pMerchant->isSellingMerchantingPreparing()
		|| !pMerchant->isBuyingMerchantingPreparing())
		return;

	if(pMerchant->m_bMerchantOpen == true)
	{
		Packet result(WIZ_MERCHANT, uint8(MERCHANT_BUY_LIST));
		result << uint8(7);
		Send(&result);
		return;
	}

	pMerchant->m_bMerchantOpen = true;
	pMerchant->m_bMerchantViewer = GetID();

	m_sMerchantsSocketID = uid;

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_BUY_LIST));
	result << uint8(1) << uint16(uid);
	for (int i = 0; i < MAX_MERCH_ITEMS; i++)
	{
		_MERCH_DATA *pMerch = &pMerchant->m_arMerchantItems[i];
		result	<< pMerch->nNum << pMerch->sCount
			<< pMerch->sDuration << pMerch->nPrice;
	}
	Send(&result);
}

void CUser::BuyingMerchantBuy(Packet & pkt)
{
	DateTime time;
	uint32 nPrice;
	uint16 sStackSize, sRemainingStackSize;
	uint8 bSellerSrcSlot, bMerchantListSlot;

	CUser *pMerchant = g_pMain->GetUserPtr(m_sMerchantsSocketID);
	if (pMerchant == nullptr 
		|| pMerchant->GetMerchantState() != MERCHANT_STATE_BUYING
		|| !pMerchant->isBuyingMerchantingPreparing()
		|| pMerchant->isSellingMerchantingPreparing())
		return;

	pkt >> bSellerSrcSlot >> bMerchantListSlot >> sStackSize;

	if (bSellerSrcSlot >= HAVE_MAX
		|| bSellerSrcSlot < SLOT_MAX
		|| bMerchantListSlot >= MAX_MERCH_ITEMS)
		return;

	_MERCH_DATA *pWantedItem = &pMerchant->m_arMerchantItems[bMerchantListSlot];
	_ITEM_DATA *pSellerItem = GetItem(SLOT_MAX + bSellerSrcSlot);

	// Make sure the merchant actually has that item in that slot
	// and that they want enough, and the selling user has enough
	if (pWantedItem == nullptr 
		|| pSellerItem == nullptr
		|| pWantedItem->nNum != pSellerItem->nNum
		|| pWantedItem->sCount < sStackSize
		|| pSellerItem->sCount < sStackSize
		// For scrolls, this will ensure you can only sell a full stack of scrolls.
		// For everything else, this will ensure you cannot sell items that need repair.
		|| pSellerItem->sDuration != pWantedItem->sDuration
		|| pSellerItem->isDuplicate()
		|| pSellerItem->isSealed()
		|| pSellerItem->isBound()
		|| pSellerItem->isRented())
		return;

	// If it's not stackable, and we're specifying something other than 1
	// we really don't care to handle this request...
	_ITEM_TABLE *proto = g_pMain->GetItemPtr(pWantedItem->nNum);
	if (proto == nullptr
		|| !proto->m_bCountable && sStackSize != 1)
		return;

	// Do they have enough coins?
	nPrice = pWantedItem->nPrice * sStackSize;
	if (!pMerchant->hasCoins(nPrice))
		return;

	// Now find the buyer a home for their item
	int8 bDstPos = pMerchant->FindSlotForItem(pWantedItem->nNum, sStackSize);
	if (bDstPos < 0)
		return;

	_ITEM_DATA *pMerchantItem = pMerchant->GetItem(bDstPos);
	if(pMerchantItem == nullptr)
		return;

	// Take coins off the buying merchant
	if (!pMerchant->GoldLose(nPrice))
		return;

	// and give them all to me, me, me!
	GoldGain(nPrice);

	// Get the remaining stack size after purchase.
	sRemainingStackSize = pSellerItem->sCount - sStackSize;

	// Now we give the buying merchant their wares.
	pMerchantItem->nNum = pSellerItem->nNum;
	pMerchantItem->sDuration = pSellerItem->sDuration;
	pSellerItem->sCount -= sStackSize;
	pMerchantItem->sCount += sStackSize;

	// Update how many items the buyer still needs.
	pWantedItem->sCount -= sStackSize;

	// If the buyer needs no more, remove this item from the wanted list.
	if (pWantedItem->sCount == 0)
		memset(pWantedItem, 0, sizeof(_MERCH_DATA));

	// If the seller's all out, remove their item.
	if (pSellerItem->sCount == 0)
		memset(pSellerItem, 0, sizeof(_ITEM_DATA));

	// TODO : Proper checks for the removal of the items in the array, we're now assuming everything gets bought

	// Update players
	SendStackChange(pSellerItem->nNum, pSellerItem->sCount, pSellerItem->sDuration, bSellerSrcSlot);
	pMerchant->SendStackChange(pMerchantItem->nNum, pMerchantItem->sCount, pMerchantItem->sDuration, bDstPos - SLOT_MAX,
		pMerchantItem->sCount == sStackSize); 	// if the buying merchant only has what they wanted, it's a new item.
	// (otherwise it was a stackable item that was merged into an existing slot)

	Packet result(WIZ_MERCHANT, uint8(MERCHANT_BUY_BOUGHT));
	result << bMerchantListSlot << uint16(0) << GetName();
	pMerchant->Send(&result);

	result.clear();
	result << uint8(MERCHANT_BUY_SOLD) << uint8(1) << bMerchantListSlot << pWantedItem->sCount << bSellerSrcSlot << pSellerItem->sCount;
	Send(&result);

	result.clear();
	result << uint8(MERCHANT_BUY_BUY) << uint8(1);
	Send(&result);

	if (bMerchantListSlot < 4 && pWantedItem->sCount == 0)
	{
		result.Initialize(WIZ_MERCHANT_INOUT);
		result << uint8(2) << m_sMerchantsSocketID << uint8(1) << uint8(0) << bMerchantListSlot;
		pMerchant->SendToRegion(&result,nullptr,GetEventRoom());
	}		

	int nItemsRemaining = 0;
	for (int i = 0; i < MAX_MERCH_ITEMS; i++)
	{
		if (pMerchant->m_arMerchantItems[i].nNum != 0)
			nItemsRemaining++;
	}

	if (nItemsRemaining == 0)
		pMerchant->BuyingMerchantClose();

	g_pMain->WriteMerchantLogFile(string_format("MERCHANT BUYING-BUY - %d:%d:%d || Seller=%s,Buyer=%d,ItemID=%d,Count=%d/%d,Price=%d,Zone=%d,X=%d,Z=%d\n",time.GetHour(),time.GetMinute(),time.GetSecond(),GetName().c_str(),pMerchant->GetName().c_str(),pWantedItem->nNum, pWantedItem->bCount,pWantedItem->sCount,nPrice,GetZoneID(),uint16(GetX()),uint16(GetZ())));
}

void CUser::RemoveFromMerchantLookers()
{
	CUser *pPreviousMerchantUser = g_pMain->GetUserPtr(m_sMerchantsSocketID);
	if (pPreviousMerchantUser == nullptr)
		return;

	pPreviousMerchantUser->m_bMerchantOpen = false;
	pPreviousMerchantUser->m_bMerchantViewer = -1;
	m_sMerchantsSocketID = -1;
}

void CUser::MerchantOfficialList(Packet & pkt)
{
	bool bIsNeutralZone = (GetZoneID() >= ZONE_MORADON && GetZoneID() <= ZONE_MORADON5);  

	if (!bIsNeutralZone)
		goto fail_return;

	uint8 opcode = pkt.read<uint8>();

	switch (opcode)
	{
	case 1:
		MerchantListSend(pkt);
		break;

	case 2:
		MerchantListMoveProcess(pkt);
		break;
	}	

fail_return:
	Packet result(WIZ_MERCHANT, uint8(MERCHANT_MENISIA_LIST));
	result << uint8(1) << uint8(0) << uint8(3);
	Send(&result);
}

void CUser::MerchantListSend(Packet & pkt)
{
	uint32 RecvItemID = pkt.read<uint32>();
	Packet result(WIZ_MERCHANT,uint8(MERCHANT_MENISIA_LIST));
	SessionMap sessMap = g_pMain->m_socketMgr.GetActiveSessionMap();
	std::vector<uint32> MerchantOfficalListen;
	int8 nCount = 0;

	if (RecvItemID != ITEM_MENICIAS_LIST)
		return;

	if(!CheckExistItem(ITEM_MENICIAS_LIST))
		return;

	foreach (itr, sessMap)
	{
		CUser * pUser = TO_USER(itr->second);

		if (!pUser->isInGame() || !pUser->isMerchanting())
			continue;

		MerchantOfficalListen.push_back(pUser->GetSocketID());
	}

	if(MerchantOfficalListen.size() > 0)
	{
		int16 nSize = (int16) MerchantOfficalListen.size();
		while (nSize > 0)
		{
			nCount++;
			result << uint8(1) << uint8(2) << nCount;

			if(nSize > 50)
				result << uint8(50);
			else
				result << uint8(nSize);

			result.SByte();
			foreach(itr, MerchantOfficalListen)
			{
				CUser * pUser = g_pMain->GetUserPtr(*itr);

				if (!pUser->isInGame() || !pUser->isMerchanting())
					continue;

				result << pUser->GetID() << pUser->GetName() << pUser->GetMerchantState();

				for (int i = 0; i < MAX_MERCH_ITEMS; i++)
				{
					if(pUser->m_arMerchantItems[i].nNum == 3452816845)
						result << uint32(0) << uint32(0);
					else
						result << pUser->m_arMerchantItems[i].nNum << pUser->m_arMerchantItems[i].nPrice;
				}
			}
			Send(&result);
			nSize = nSize - 50;
		}
	}
}

void CUser::MerchantListMoveProcess(Packet & pkt)
{
	if (isDead() || isTrading() || isMerchanting() || isStoreOpen() || isMining())
		goto fail_return;
	
	uint16 TargetID = pkt.read<uint16>();
	CUser *pUser = g_pMain->GetUserPtr(TargetID);

	if (pUser == nullptr)
		return;

	if (!pUser->isInGame() || !pUser->isMerchanting())
		return;

	ZoneChange(pUser->GetZoneID(), pUser->GetX(), pUser->GetZ());

fail_return:
	Packet result(WIZ_MERCHANT, uint8(MERCHANT_MENISIA_LIST));
	result << uint8(1) << uint8(0) << uint8(3);
	Send(&result);
}