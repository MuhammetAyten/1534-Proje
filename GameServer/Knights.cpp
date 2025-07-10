#include "stdafx.h"
#include "Knights.h"

using std::string;

CKnights::CKnights()
{
	m_sIndex = 0;
	m_byFlag = ClanTypeNone;
	m_byNation = 0;
	m_byGrade = 5;
	m_byRanking = 0;
	m_sMembers = 1;
	memset(&m_Image, 0, sizeof(m_Image));
	m_nMoney = 0;
	m_sDomination = 0;
	m_nPoints = 0;
	m_nClanPointFund = 0;
	m_sCape = -1;
	m_sAlliance = 0;
	m_sMarkLen = 0;
	m_sMarkVersion = 0;
	m_bCapeR = m_bCapeG = m_bCapeB = 0;
	m_sClanPointMethod = 0;
}

void CKnights::OnLogin(CUser *pUser)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_USER_ONLINE));
	result.SByte();

	// Set the active session for this user
	foreach_array (i, m_arKnightsUser)
	{
		_KNIGHTS_USER * p = &m_arKnightsUser[i];
		if (!p->byUsed
			|| STRCASECMP(p->strUserName.c_str(), pUser->GetName().c_str()) != 0)
			continue;

		p->pSession = pUser;
		p->strUserMemo = pUser->m_strMemo;
		p->m_sClass = pUser->GetClass();
		p->m_bLevel = pUser->GetLevel();
		pUser->m_pKnightsUser = p;
		break;
	}

	// Send login notice
	result << pUser->GetName();
	Send(&result);

	// Construct the clan notice packet to send to the logged in player
	if (!m_strClanNotice.empty())
	{
		ConstructClanNoticePacket(&result);
		pUser->Send(&result);
	}
}
void CKnights::OnLogoutAlliance(CUser *pUser)
{
	CKnights * pKnight = g_pMain->GetClanPtr(pUser->GetClanID());

	if(pKnight == nullptr)
		return;

	Packet result;

	// Unset the active session for this user
	if (pUser->m_pKnightsUser != nullptr)
	{
		pUser->m_pKnightsUser->pSession = nullptr;
		pUser->m_pKnightsUser = nullptr;
	}
	if (pKnight->m_strChief == pUser->GetName())
	{
		// TODO: Shift this to SERVER_RESOURCE
		std::string buffer = string_format("%s is offline.", pUser->GetName().c_str()); 
		ChatPacket::Construct(&result, ALLIANCE_CHAT, &buffer);
		g_pMain->Send_KnightsAlliance(pKnight->GetAllianceID(), &result);
	}
}
void CKnights::OnLoginAlliance(CUser *pUser)
{
	CKnights * pKnight = g_pMain->GetClanPtr(pUser->GetClanID());

	if (pKnight == nullptr)
		return;

	Packet result;

	// Set the active session for this user
	foreach_array (i, m_arKnightsUser)
	{
		_KNIGHTS_USER * p = &m_arKnightsUser[i];
		if (!p->byUsed
			|| STRCASECMP(p->strUserName.c_str(), pUser->GetName().c_str()) != 0)
			continue;

		p->pSession = pUser;
		pUser->m_pKnightsUser = p;
		break;
	}

	if (pKnight->m_strChief == pUser->GetName())
	{
		// Send login notice
		// TODO: Shift this to SERVER_RESOURCE
		std::string buffer = string_format("%s is online.", pUser->GetName().c_str()); 
		ChatPacket::Construct(&result, ALLIANCE_CHAT, &buffer);
		g_pMain->Send_KnightsAlliance(pKnight->GetAllianceID(), &result);
	}

	// Construct the clan notice packet to send to the logged in player
	if (!m_strClanNotice.empty())
	{
		ConstructClanNoticePacket(&result);
		pUser->Send(&result);
	}
}
void CKnights::ConstructClanNoticePacket(Packet *result)
{
	result->Initialize(WIZ_NOTICE);
	result->DByte();
	*result	<< uint8(4)			// type
		<< uint8(1)			// total blocks
		<< "Clan Notice"	// header
		<< m_strClanNotice;
}

/**
* @brief	Updates this clan's notice with clanNotice
* 			and informs logged in clan members.
*
* @param	clanNotice	The clan notice.
*/
void CKnights::UpdateClanNotice(std::string & clanNotice)
{
	if (clanNotice.length() > MAX_CLAN_NOTICE_LENGTH)
		return;

	Packet result;

	// Update the stored clan notice
	m_strClanNotice = clanNotice;

	// Construct the update notice packet to inform players the clan notice has changed
	std::string updateNotice = string_format("%s updated the clan notice.", m_strChief.c_str()); 
	ChatPacket::Construct(&result, KNIGHTS_CHAT, &updateNotice);
	Send(&result);

	// Construct the new clan notice packet
	ConstructClanNoticePacket(&result);
	Send(&result);

	// Tell the database to update the clan notice.
	result.Initialize(WIZ_CHAT);
	result << uint8(CLAN_NOTICE) << GetID() << clanNotice;
	g_pMain->AddDatabaseRequest(result);
}

/**
* @brief	Sends a request to update the clan's fund in the database.
*/
void CKnights::UpdateClanFund()
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_UPDATE_FUND));
	result << GetID() << uint32(m_nClanPointFund);
	g_pMain->AddDatabaseRequest(result);
}

void CKnights::OnLogout(CUser *pUser)
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_USER_OFFLINE));
	result.SByte();

	// Unset the active session for this user
	if (pUser->m_pKnightsUser != nullptr)
	{
		pUser->m_pKnightsUser->pSession = nullptr;
		pUser->m_pKnightsUser = nullptr;
	}

	// Send Logout notice
	result << pUser->GetName();
	Send(&result);
}

bool CKnights::AddUser(std::string & strUserID)
{
	for (int i = 0; i < MAX_CLAN_USERS; i++)
	{
		_KNIGHTS_USER * p = &m_arKnightsUser[i];
		if (p->byUsed)
			continue;

		p->byUsed = 1;
		p->strUserName = strUserID;
		p->pSession = g_pMain->GetUserPtr(strUserID, TYPE_CHARACTER);
		if (p->pSession != nullptr)
			p->pSession->m_pKnightsUser = p;

		return true;
	}

	return false;
}

bool CKnights::AddUser(CUser *pUser)
{
	if (pUser == nullptr
		|| !AddUser(pUser->GetName()))
		return false;

	pUser->SetClanID(m_sIndex);
	pUser->m_bFame = TRAINEE;
	return true;
}

/**
* @brief	Removes the specified user from the clan array.
*
* @param	strUserID	Identifier for the user.
*
* @return	.
*/
bool CKnights::RemoveUser(std::string & strUserID)
{
	for (int i = 0; i < MAX_CLAN_USERS; i++)
	{
		_KNIGHTS_USER * p = &m_arKnightsUser[i];
		if (!p->byUsed)
			continue;

		if (STRCASECMP(p->strUserName.c_str(), strUserID.c_str()) == 0)
		{
			// If they're not logged in (note: logged in users being removed have their NP refunded in the other handler)
			// but they've donated NP, ensure they're refunded for the next time they login.
			if (p->nDonatedNP > 0)
				RefundDonatedNP(p->nDonatedNP, nullptr, p->strUserName.c_str());

			p->Initialise();
			return true;
		}
	}

	return false;
}

/**
* @brief	Removes the specified user from this clan.
*
* @param	pUser	The user.
*/
bool CKnights::RemoveUser(CUser *pUser)
{
	if (pUser == nullptr
		|| pUser->m_pKnightsUser == nullptr)
		return false;

	// Ensure users are refunded when they leave/are removed from a clan.
	// NOTE: If we bring back multiserver setups, this is going to cause synchronisation issues.
	uint32 nDonatedNP = pUser->m_pKnightsUser->nDonatedNP;
	if (nDonatedNP > 0)
		RefundDonatedNP(nDonatedNP, pUser);

	pUser->SetClanID(0);
	pUser->m_bFame = 0;

	pUser->m_pKnightsUser->Initialise();
	pUser->m_pKnightsUser = nullptr;

	if (!pUser->isClanLeader())
		pUser->SendClanUserStatusUpdate();

	return true;
}

/**
* @brief	Refunds 30% of the user's donated NP.
* 			If the user has the item "CONT Recovery", refund ALL of the user's 
* 			donated NP.
*
* @param	nDonatedNP	The donated NP.
* @param	pUser	  	The user's session, when refunding the user in-game.
* 						Set to nullptr to indicate the use of the character's name
* 						and consequently a database update instead.
* @param	strUserID 	Logged out character's name. 
* 						Used to refund logged out characters' national points 
* 						when pUser is set to nullptr.
*/
void CKnights::RefundDonatedNP(uint32 nDonatedNP, CUser * pUser, const char * strUserID)
{
	uint32 m_totalDonationChange = nDonatedNP;

	if(m_byFlag >= ClanTypeAccredited5)
	{
		int counter = 0;
		while(m_totalDonationChange != 0)
		{
			if(m_nClanPointFund < m_totalDonationChange)
			{
				switch (m_byFlag)
				{
				case ClanTypeAccredited5:
					m_nClanPointFund = 0;
					m_totalDonationChange = 0;
					break;
				case ClanTypeAccredited4: // 7000 * 36 = 252000
					m_byFlag = ClanTypeFlag::ClanTypeAccredited5;
					m_nClanPointFund += 252000;  
					break;
				case ClanTypeAccredited3: // 10000 * 36 = 360000
					m_byFlag = ClanTypeFlag::ClanTypeAccredited4;
					m_nClanPointFund += 360000;   
					break;
				case ClanTypeAccredited2: // 15000 * 36 = 540000
					m_byFlag = ClanTypeFlag::ClanTypeAccredited3;
					m_nClanPointFund += 540000; 
					break;
				case ClanTypeAccredited1: // 20000 * 36 = 720000
					m_byFlag = ClanTypeFlag::ClanTypeAccredited2;
					m_nClanPointFund += 720000; 
					break;
				case ClanTypeRoyal5: // 25000 * 36 = 900000
					m_byFlag = ClanTypeFlag::ClanTypeAccredited1;
					m_nClanPointFund += 900000; 
					break;
				case ClanTypeRoyal4: // 30000 * 36 = 1080000
					m_byFlag = ClanTypeFlag::ClanTypeRoyal5;
					m_nClanPointFund += 1080000; 
					break;
				case ClanTypeRoyal3: // 35000 * 36 = 1260000
					m_byFlag = ClanTypeFlag::ClanTypeRoyal4;
					m_nClanPointFund += 1260000; 
					break;
				case ClanTypeRoyal2: // 40000 * 36 = 1440000
					m_byFlag = ClanTypeFlag::ClanTypeRoyal3;
					m_nClanPointFund += 1440000; 
					break;
				case ClanTypeRoyal1: // 45000 * 36 = 1620000
					m_byFlag = ClanTypeFlag::ClanTypeRoyal2;
					m_nClanPointFund += 1620000; 
					break;
				default:
//					TRACE("A clan with unknow clantype IDNum = %d ClanPoints = %d", GetID(), m_nClanPointFund);
					break;
				}

				if(m_nClanPointFund < m_totalDonationChange)
				{
					m_totalDonationChange -= m_nClanPointFund;
					m_nClanPointFund = 0;
				}
				else
				{
					m_nClanPointFund -= m_totalDonationChange;
					m_totalDonationChange = 0;
					break;
				}
			}
			else
			{
				m_nClanPointFund -= m_totalDonationChange;
				m_totalDonationChange = 0;
				break;
			}
		}
	}
	else
	{
		if(m_nClanPointFund < nDonatedNP)
			m_nClanPointFund = 0;
		else
			m_nClanPointFund -= nDonatedNP;
	}

	_KNIGHTS_CAPE *pCape = g_pMain->m_KnightsCapeArray.GetData(m_sCape);
	if (pCape != nullptr)
	{
		if ((pCape->byGrade && m_byGrade > pCape->byGrade)
			// not sure if this should use another error, need to confirm
				|| m_byFlag < pCape->byRanking)
		{
			m_sCape = 0; m_bCapeR = 0;  m_bCapeG = 0; m_bCapeB = 0;

			// Now save (we don't particularly care whether it was able to do so or not).
			Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_UPDATE_CAPE));
			result	<< GetID() << GetCapeID()
				<< m_bCapeR << m_bCapeG << m_bCapeB;
			g_pMain->AddDatabaseRequest(result);
			SendUpdate();
		}
	}

	// If the player's logged in, just adjust their national points in-game.
	if (pUser != nullptr)
	{
		// Refund 30% of NP unless the user has the item "CONT Recovery".
		// In this case, ALL of the donated NP will be refunded.
		if (!pUser->RobItem(ITEM_CONT_RECOVERY)
			&& !pUser->RobItem(ITEM_CONT_RESTORE_CERT))
		{
			nDonatedNP = (nDonatedNP * 30) / 100;
		}

		if (pUser->m_iLoyalty + nDonatedNP > LOYALTY_MAX)
			pUser->m_iLoyalty = LOYALTY_MAX;
		else
			pUser->m_iLoyalty += nDonatedNP;

		Packet result(WIZ_LOYALTY_CHANGE, uint8(LOYALTY_NATIONAL_POINTS));
		result << pUser->m_iLoyalty << pUser->m_iLoyaltyMonthly
			<< uint32(0) // Clan donations(? Donations made by this user? For the clan overall?)
			<< uint32(0); // Premium NP(? Additional NP gained?)

		pUser->Send(&result);
		return;
	}

	nDonatedNP = (nDonatedNP * 30) / 100;

	// For logged out players, we must update the player's national points in the database.
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_REFUND_POINTS));
	result << strUserID << nDonatedNP;
	g_pMain->AddDatabaseRequest(result);
}

void CKnights::Disband(CUser *pLeader /*= nullptr*/)
{
	string clanNotice;
	g_pMain->GetServerResource(m_byFlag == ClanTypeTraining ? IDS_CLAN_DESTROY : IDS_KNIGHTS_DESTROY, 
		&clanNotice, m_strName.c_str());
	SendChat(clanNotice.c_str());

	foreach_array (i, m_arKnightsUser)
	{
		_KNIGHTS_USER *p = &m_arKnightsUser[i];
		if (!p->byUsed)
			continue;

		// If the user's logged in, handle the player data removal in-game.
		// It will be saved to the database when they log out.
		if (p->pSession != nullptr)
			RemoveUser(p->pSession);
		else
			RemoveUser(p->strUserName);

	}
	g_pMain->m_KnightsArray.DeleteData(m_sIndex);
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_DESTROY));
	result << uint8(1) << GetID();
	pLeader->Send(&result);
	pLeader->UserDataSaveToAgent();
}

void CKnights::SendUpdate()
{
	Packet result(WIZ_KNIGHTS_PROCESS, uint8(KNIGHTS_UPDATE));
	if(isInAlliance())
	{
		CKnights * pMainClan = g_pMain->GetClanPtr(GetAllianceID());
		_KNIGHTS_ALLIANCE* pAlliance = g_pMain->GetAlliancePtr(GetAllianceID());

		if(pMainClan  == nullptr || pAlliance == nullptr)
			return;

		if(pAlliance->sMainAllianceKnights == GetID())
		{
			result << GetID() << m_byFlag << pMainClan->GetCapeID()
				<< pMainClan->m_bCapeR << pMainClan->m_bCapeG << pMainClan->m_bCapeB << uint8(0);
		}
		else if(pAlliance->sSubAllianceKnights == GetID())
		{
			result << GetID() << m_byFlag << pMainClan->GetCapeID() // sub alliance can have symbol and their own cape
				<< m_bCapeR << m_bCapeG << m_bCapeB << uint8(0);
		}
		else if(pAlliance->sMercenaryClan_1 == GetID() 
			|| pAlliance->sMercenaryClan_2 == GetID())
		{
			result << GetID() << m_byFlag << pMainClan->GetCapeID()  << uint32(0); // only the cape will be present
		}

		Send(&result);
	}
	else
	{
		result << GetID() << m_byFlag << GetCapeID()
			<< m_bCapeR << m_bCapeG << m_bCapeB << uint8(0);
		Send(&result);
	}
}

#pragma region CKnights::SendAllianceUpdate()

void CKnights::SendAllianceUpdate()
{
	_KNIGHTS_ALLIANCE * pAlliance = g_pMain->GetAlliancePtr(GetAllianceID());
	CKnights * pKnights1 = g_pMain->GetClanPtr(pAlliance->sMercenaryClan_1),
		* pKnights2 = g_pMain->GetClanPtr(pAlliance->sMercenaryClan_2),
		* pKnights3 = g_pMain->GetClanPtr(pAlliance->sSubAllianceKnights);
	if(pKnights1 != nullptr)
		pKnights1->SendUpdate();
	if(pKnights2 != nullptr)
		pKnights2->SendUpdate();
	if(pKnights3 != nullptr)
		pKnights3->SendUpdate();
}


#pragma endregion 

void CKnights::SendChat(const char * format, ...)
{
	char buffer[128];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buffer, 128, format, ap);
	va_end(ap);

	Packet result;
	ChatPacket::Construct(&result, KNIGHTS_CHAT, buffer);
	Send(&result);
}

void CKnights::SendChatAlliance(const char * format, ...)
{
	char buffer[128];
	va_list ap;
	va_start(ap, format);
	vsnprintf(buffer, 128, format, ap);
	va_end(ap);

	Packet result;
	ChatPacket::Construct(&result, ALLIANCE_CHAT, buffer);
	Send(&result);
}

void CKnights::Send(Packet *pkt)
{
	foreach_array (i, m_arKnightsUser)
	{
		_KNIGHTS_USER *p = &m_arKnightsUser[i];
		if (p->byUsed && p->pSession != nullptr)
			p->pSession->Send(pkt);
	}
}

CKnights::~CKnights()
{
}