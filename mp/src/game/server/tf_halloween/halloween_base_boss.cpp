//========= Copyright Valve Corporation, All rights reserved. ============//
// halloween_boss_base.cpp
// Shared code for the Halloween Bosses
// Michael Booth, October 2011

#include "cbase.h"
#include "halloween_base_boss.h"
#include "dod_player.h"
#include "dod_gamerules.h"
#include "dod_shareddefs.h"


//-----------------------------------------------------------------------------------------------------
CHalloweenBaseBoss::CHalloweenBaseBoss()
{
	m_wasSpawnedByCheats = false;
}


//-----------------------------------------------------------------------------------------------------
CHalloweenBaseBoss::~CHalloweenBaseBoss()
{
}


//-----------------------------------------------------------------------------------------------------
void CHalloweenBaseBoss::Spawn( void )
{
	BaseClass::Spawn();

	ConVarRef sv_cheats( "sv_cheats" );
	if ( sv_cheats.IsValid() && sv_cheats.GetBool() )
	{
		// remember we spawned with a cheat command
		m_wasSpawnedByCheats = true;
	}

	m_damagePerSecond = 0.0f;
	m_maxDamagePerSecond = 0.0f;

	if ( DODGameRules() )
	{
		DODGameRules()->AddActiveBoss( this );
	}

	// track how many players were playing when boss spawned
	CUtlVector< CDODPlayer * > playerVector;
	CollectPlayers( &playerVector, TEAM_ALLIES );
	CollectPlayers( &playerVector, TEAM_AXIS, false, APPEND_PLAYERS );
}


//-----------------------------------------------------------------------------------------------------
void CHalloweenBaseBoss::Update( void )
{
	BaseClass::Update();

	UpdateDamagePerSecond();
}

//-----------------------------------------------------------------------------------------------------
void CHalloweenBaseBoss::UpdateOnRemove()
{
	if ( DODGameRules() )
	{
		DODGameRules()->RemoveActiveBoss( this );
	}

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------------------------------
int CHalloweenBaseBoss::OnTakeDamage( const CTakeDamageInfo &info )
{
	return BaseClass::OnTakeDamage( info );
}

//-----------------------------------------------------------------------------------------------------
int CHalloweenBaseBoss::OnTakeDamage_Alive( const CTakeDamageInfo &rawInfo )
{
	CTakeDamageInfo info = rawInfo;

	if ( info.GetAttacker() && info.GetAttacker()->GetTeamNumber() == GetTeamNumber() )
	{
		return 0;
	}

	// keep a list of everyone who hurt me, and when
	CDODPlayer *playerAttacker = ToDODPlayer( info.GetAttacker() );
	if ( playerAttacker )
	{
		CWeaponDODBase *attackerWeapon = assert_cast<CWeaponDODBase* >( info.GetWeapon() );
		bool isMeleeAttack = attackerWeapon && attackerWeapon->IsMeleeWeapon();

		RememberAttacker( playerAttacker, isMeleeAttack, info.GetDamage() );
	}

	// fire event for client combat text, beep, etc.
	IGameEvent *event = gameeventmanager->CreateEvent( "npc_hurt" );
	if ( event )
	{
		event->SetInt( "entindex", entindex() );
		event->SetInt( "health", MAX( 0, GetHealth() ) );
		event->SetInt( "damageamount", info.GetDamage() );
		event->SetInt( "boss", GetBossType() );

		CDODPlayer *attackerPlayer = ToDODPlayer( info.GetAttacker() );
		if ( attackerPlayer )
		{
			event->SetInt( "attacker_player", attackerPlayer->GetUserID() );

			if ( attackerPlayer->GetActiveDODWeapon() )
			{
				event->SetInt( "weaponid", attackerPlayer->GetActiveDODWeapon()->GetWeaponID() );
			}
			else
			{
				event->SetInt( "weaponid", 0 );
			}
		}
		else
		{
			// hurt by world
			event->SetInt( "attacker_player", 0 );
			event->SetInt( "weaponid", 0 );
		}

		gameeventmanager->FireEvent( event );
	}

	return BaseClass::OnTakeDamage_Alive( info );
}

void CHalloweenBaseBoss::Event_Killed( const CTakeDamageInfo &info )
{
	const CUtlVector< AttackerInfo > &attackerVector = GetAttackerVector();
	for( int i=0; i<attackerVector.Count(); ++i )
	{
		if ( attackerVector[i].m_attacker != NULL )
		{
			IGameEvent *pEvent = gameeventmanager->CreateEvent( "halloween_boss_killed" );
			if ( pEvent )
			{
				pEvent->SetInt( "boss", GetBossType() );
				pEvent->SetInt( "killer", attackerVector[i].m_attacker->GetUserID() );
				gameeventmanager->FireEvent( pEvent, true );
			}
		}
	}

	BaseClass::Event_Killed( info );
}

//---------------------------------------------------------------------------------------------
void CHalloweenBaseBoss::RememberAttacker( CDODPlayer *playerAttacker, bool wasMeleeHit, float damage )
{
	// record the damage for dps calculations
	DamageRateInfo info;
	info.m_damage = damage;
	info.m_timestamp = gpGlobals->curtime;
	m_damageVector.AddToTail( info );

	int i;

	// has this player hurt me before
	for( i=0; i<m_attackerVector.Count(); ++i )
	{
		if ( m_attackerVector[i].m_attacker && m_attackerVector[i].m_attacker->entindex() == playerAttacker->entindex() )
		{
			// this player is hurting me again
			m_attackerVector[i].m_timestamp = gpGlobals->curtime;
			m_attackerVector[i].m_wasLastHitFromMeleeWeapon = wasMeleeHit;
			return;
		}
	}

	// new attacker
	AttackerInfo attackerInfo;

	attackerInfo.m_attacker = playerAttacker;
	attackerInfo.m_timestamp = gpGlobals->curtime;
	attackerInfo.m_wasLastHitFromMeleeWeapon = wasMeleeHit;

	m_attackerVector.AddToTail( attackerInfo );
}


//---------------------------------------------------------------------------------------------
void CHalloweenBaseBoss::UpdateDamagePerSecond( void )
{
	const float windowDuration = 5.0f;
	int i;

	m_damagePerSecond = 0.0f;

	for( i=0; i<m_damageVector.Count(); ++i )
	{
		float age = gpGlobals->curtime - m_damageVector[i].m_timestamp;

		if ( age > windowDuration )
		{
			// too old
			continue;
		}

		m_damagePerSecond += m_damageVector[i].m_damage;
	}

	m_damagePerSecond /= windowDuration;

	if ( m_damagePerSecond > m_maxDamagePerSecond )
	{
		m_maxDamagePerSecond = m_damagePerSecond;
	}

// 	if ( m_damagePerSecond > 0.0001f )
// 	{
// 		DevMsg( "%3.2f: dps = %3.2f\n", gpGlobals->curtime, m_damagePerSecond );
// 	}
}


//---------------------------------------------------------------------------------------------
void CHalloweenBaseBoss::Break( void )
{
	CPVSFilter filter( GetAbsOrigin() );
	UserMessageBegin( filter, "BreakModel" );
		WRITE_SHORT( GetModelIndex() );
		WRITE_VEC3COORD( GetAbsOrigin() );
		WRITE_ANGLES( GetAbsAngles() );
		WRITE_SHORT( m_nSkin );
	MessageEnd();
}


//---------------------------------------------------------------------------------------------
/*static*/ CHalloweenBaseBoss* CHalloweenBaseBoss::SpawnBossAtPos( HalloweenBossType bossType, const Vector& vSpawnPos, int nTeam /*= TF_TEAM_HALLOWEEN*/, CBaseEntity* pOwner /*= NULL*/ )
{
	const char* pszBossType = NULL;
	switch ( bossType )
	{
	case HALLOWEEN_BOSS_HHH:
		pszBossType = "headless_hatman";
		break;
	case HALLOWEEN_BOSS_MONOCULUS:
		pszBossType = "eyeball_boss";
		break;
	case HALLOWEEN_BOSS_MERASMUS:
		pszBossType = "merasmus";
		break;
	default:
		AssertMsg( 0, "Invalid Halloween Boss Type" );
	}

	CHalloweenBaseBoss *pBoss = NULL;
	if ( pszBossType )
	{
		pBoss = dynamic_cast< CHalloweenBaseBoss * >( CreateEntityByName( pszBossType ) );
		if ( pBoss )
		{
			pBoss->SetAbsOrigin( vSpawnPos + Vector( 0, 0, 10.0f ) );
			pBoss->ChangeTeam( nTeam );
			pBoss->SetOwnerEntity( pOwner );

			DispatchSpawn( pBoss );
		}
	}

	return pBoss;
}
