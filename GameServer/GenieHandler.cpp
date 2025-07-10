#include "stdafx.h"

void CUser::HandleGenie(Packet & pkt)
{
	uint8 command = pkt.read<uint8>();
	
	switch (command)
	{
	case 0x01:
		GenieNonAttackProgress(pkt);
		break;
	case 0x02:
		GenieAttackProgress(pkt);
		break;
	default:
//		printf("[%s] Genie Non Attack Unknow Handle %d Packet Len [%d]\r\n",GetName().c_str(),command,pkt.size());
		break;
	}
}

void CUser::GenieNonAttackProgress(Packet & pkt)
{

	enum GenieNonAttackType
	{
		GenieUseSpiringPotion = 1,
		GenieLoadOptions = 0x02,
		GenielSaveOptions = 0x03,
		GenieStartHandle = 0x04,
		GenieStopHandle  = 0x05
	};

	uint8 command = pkt.read<uint8>();

	switch (command)
	{
	case GenieUseSpiringPotion:
		GenieUseGenieSpirint();
		break;
	case GenieStartHandle:
		GenieStart();
		break;
	case GenielSaveOptions:
		for(int i=0; i<sizeof(m_GenieOptions); i++)
		{
			*(uint8 *)(m_GenieOptions + i ) = pkt.read<uint8>();
		}
		break;
	case GenieLoadOptions:
		{
			Packet result(WIZ_GENIE);	
			result << uint8(0x01) << uint8(2);

			for(int i=0; i<sizeof(m_GenieOptions); i++)
				result << uint8(*(uint8 *)(m_GenieOptions + i));

			Send(&result);
		}
		break;
	case GenieStopHandle:
		GenieStop();
		break;
	default:
		TRACE("[%s] Genie Non Attack Unknow Handle %d Packet Len [%d]\r\n",GetName().c_str(),command,pkt.size());
		break;
	}

}

void CUser::GenieAttackProgress(Packet & pkt)
{
	enum GenieAttackHandle
	{
		GenieMove = 1,
		GenieSkillSave = 2,
		GenieMainAttack = 3,
		GenieMagic = 4
	};

	uint8 command = pkt.read<uint8>();

	if(m_GenieTime == 0)
		GenieStop();
	
	switch (command)
	{
		case GenieMove:
			MoveProcess(pkt);
			break;
		case GenieMagic:
			CMagicProcess::MagicPacket(pkt, this);			
			break;
		case GenieMainAttack:
			Attack(pkt);
			break;
		default:
			TRACE("[%s] Genie Unknow Attack Handle %d Packet Len[%d]\r\n",GetName().c_str(),command,pkt.size());
			break;
	}
}

void CUser::GenieStart()
{	
	Packet result(WIZ_GENIE ,uint8(1));
	result << uint8(4) << uint16(1) << GetGenieTime();
	m_bGenieStatus = true;
	UserInOut(INOUT_IN);

	Send(&result);
}

void CUser::GenieStop()
{
	Packet result(WIZ_GENIE, uint8(1));
	result << uint8(5) << uint16(1) << GetGenieTime();
	m_bGenieStatus = false;
	UserInOut(INOUT_IN);

	Send(&result);
}

void CUser::UpdateGenieTime(uint16 m_sTime)
{
	Packet result(WIZ_GENIE, uint8(1));
	result << uint8(6) << m_sTime;

	Send(&result);
}

void CUser::GenieUseGenieSpirint()
{	
	Packet result(WIZ_GENIE, uint8(1));
	
	// item check and rob
	uint16 CountA,CountB;

	CountA = GetItemCount(810305000);
	CountB = GetItemCount(810306000);

	if(CountA < 1 && CountB < 1)
		return;
	else if(CountA > 0 )
		RobItem(810305000);
	else if(CountB > 0)
		RobItem(810306000);

	m_GenieTime = 120;

	result << uint8(1) << GetGenieTime();
	Send(&result);
}

void CUser::HandleMovingTower(Packet & pkt)
{
	uint8 command = pkt.read<uint8>();
	Packet result(WIZ_MOVING_TOWER);
	switch (command)
	{
	case 1:
		{
			CUser * pTUser = g_pMain->GetUserPtr(GetTargetID());

			if(pTUser == nullptr)
				return;

			Warp(uint16(pTUser->GetX() * 10),uint16(pTUser->GetZ() * 10));

			result << (uint8) command << (uint16) command;
			Send(&result);
		}
		break;
	case 2:
		{
			uint16 x,z;
			pkt >> x >> z;

			Warp(x,z);

			result << (uint8) command << uint16(1);
			Send(&result);
		}
		break;
	case 16:
		{
			CNpc *pNpc = g_pMain->GetNpcPtr(GetTargetID());

			if (pNpc == nullptr)
				return;
			
			StateChangeServerDirect(3,450018);

			result << (uint8) command << uint8(1) << GetSocketID() << pNpc->GetID() 
				<< (uint16) pNpc->GetSPosX() << (uint16) pNpc->GetSPosZ() << (uint16) pNpc->GetSPosZ();
			Send(&result);
		}
		break;
	case 17:
		{
			StateChangeServerDirect(3,ABNORMAL_NORMAL);

			result << (uint8) command << uint16(1);
			Send(&result);
		}
		break;
	default:
//		printf("[%s] Moving Tower %d Packet Len [%d]\r\n",GetName().c_str(),command,pkt.size());
		break;
	}
}