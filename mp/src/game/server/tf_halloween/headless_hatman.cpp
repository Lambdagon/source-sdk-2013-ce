//========= Copyright Valve Corporation, All rights reserved. ============//
// headless_hatman.cpp
// An NPC that spawns in the Halloween map and wreaks havok
// Michael Booth, October 2010

#include "cbase.h"
#include "headless_hatman.h"
#include "NextBot/Path/NextBotChasePath.h"
#include "team_control_point_master.h"
#include "particle_parse.h"
#include "dod_shareddefs.h"
#include "dod_gamerules.h"
#include "props_shared.h"

#include "halloween_behavior/headless_hatman_emerge.h"
#include "halloween_behavior/headless_hatman_dying.h"


ConVar tf_halloween_bot_health_base( "tf_halloween_bot_health_base", "3000", FCVAR_CHEAT );
ConVar tf_halloween_bot_health_per_player( "tf_halloween_bot_health_per_player", "200", FCVAR_CHEAT );
ConVar tf_halloween_bot_min_player_count( "tf_halloween_bot_min_player_count", "10", FCVAR_CHEAT );

ConVar tf_halloween_bot_speed( "tf_halloween_bot_speed", "400", FCVAR_CHEAT );
ConVar tf_halloween_bot_attack_range( "tf_halloween_bot_attack_range", "200", FCVAR_CHEAT );
ConVar tf_halloween_bot_speed_recovery_rate( "tf_halloween_bot_speed_recovery_rate", "100", FCVAR_CHEAT, "Movement units/second" );
ConVar tf_halloween_bot_chase_duration( "tf_halloween_bot_chase_duration", "30", FCVAR_CHEAT );
ConVar tf_halloween_bot_terrify_radius( "tf_halloween_bot_terrify_radius", "500", FCVAR_CHEAT );
ConVar tf_halloween_bot_chase_range( "tf_halloween_bot_chase_range", "1500", FCVAR_CHEAT );
ConVar tf_halloween_bot_quit_range( "tf_halloween_bot_quit_range", "2000", FCVAR_CHEAT );


//-----------------------------------------------------------------------------------------------------
// The Horseless Headless Horseman
//-----------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( headless_hatman, CHeadlessHatman );

IMPLEMENT_SERVERCLASS_ST( CHeadlessHatman, DT_HeadlessHatman )
END_SEND_TABLE()


//-----------------------------------------------------------------------------------------------------
CHeadlessHatman::CHeadlessHatman()
{
	m_intention = new CHeadlessHatmanIntention( this );
	m_locomotor = new CHeadlessHatmanLocomotion( this );
	m_body = new CHeadlessHatmanBody( this );
}


//-----------------------------------------------------------------------------------------------------
CHeadlessHatman::~CHeadlessHatman()
{
	if ( m_intention )
		delete m_intention;

	if ( m_locomotor )
		delete m_locomotor;

	if ( m_body )
		delete m_body;
}


void CHeadlessHatman::PrecacheHeadlessHatman()
{
	int model = PrecacheModel( "models/bots/headless_hatman.mdl" );
	PrecacheGibsForModel( model );

	PrecacheModel( "models/weapons/c_models/c_bigaxe/c_bigaxe.mdl" );

	PrecacheScriptSound( "Halloween.HeadlessBossSpawn" );
	PrecacheScriptSound( "Halloween.HeadlessBossSpawnRumble" );
	PrecacheScriptSound( "Halloween.HeadlessBossAttack" );
	PrecacheScriptSound( "Halloween.HeadlessBossAlert" );
	PrecacheScriptSound( "Halloween.HeadlessBossBoo" );
	PrecacheScriptSound( "Halloween.HeadlessBossPain" );
	PrecacheScriptSound( "Halloween.HeadlessBossLaugh" );
	PrecacheScriptSound( "Halloween.HeadlessBossDying" );
	PrecacheScriptSound( "Halloween.HeadlessBossDeath" );
	PrecacheScriptSound( "Halloween.HeadlessBossAxeHitFlesh" );
	PrecacheScriptSound( "Halloween.HeadlessBossAxeHitWorld" );
	PrecacheScriptSound( "Halloween.HeadlessBossFootfalls" );
	PrecacheScriptSound( "Player.IsNowIt" );
	PrecacheScriptSound( "Player.YouAreIt" );
	PrecacheScriptSound( "Player.TaggedOtherIt" );

	PrecacheParticleSystem( "halloween_boss_summon" );
	PrecacheParticleSystem( "halloween_boss_axe_hit_world" );
	PrecacheParticleSystem( "halloween_boss_injured" );
	PrecacheParticleSystem( "halloween_boss_death" );
	PrecacheParticleSystem( "halloween_boss_foot_impact" );
	PrecacheParticleSystem( "halloween_boss_eye_glow" );
}

//-----------------------------------------------------------------------------------------------------
void CHeadlessHatman::Precache()
{
	BaseClass::Precache();

	// always allow late precaching, so we don't pay the cost of the
	// Halloween Boss for the entire year

	bool bAllowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	PrecacheHeadlessHatman();
	
	CBaseEntity::SetAllowPrecache( bAllowPrecache );
}


//-----------------------------------------------------------------------------------------------------
void CHeadlessHatman::Spawn( void )
{
	Precache();

	BaseClass::Spawn();

	SetModel( "models/bots/headless_hatman.mdl" );

	m_axe = (CBaseAnimating *)CreateEntityByName( "prop_dynamic" );
	if ( m_axe )
	{
		m_axe->SetModel( GetWeaponModel() );

		// bonemerge the axe into our model
		m_axe->FollowEntity( this, true );
	}

	// scale the boss' health with the player count
	int totalPlayers = GetGlobalTeam( TEAM_AXIS )->GetNumPlayers() + GetGlobalTeam( TEAM_ALLIES )->GetNumPlayers();

	int health = tf_halloween_bot_health_base.GetInt();
	if ( totalPlayers > tf_halloween_bot_min_player_count.GetInt() )
	{
		health += ( totalPlayers - tf_halloween_bot_min_player_count.GetInt() ) * tf_halloween_bot_health_per_player.GetInt();
	}

	SetHealth( health );
	SetMaxHealth( health );

	m_homePos = GetAbsOrigin();

	m_damagePoseParameter = -1;

	SetBloodColor( DONT_BLEED );
}


//-----------------------------------------------------------------------------------------------------
int CHeadlessHatman::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	DispatchParticleEffect( "halloween_boss_injured", info.GetDamagePosition(), GetAbsAngles() );

	return BaseClass::OnTakeDamage_Alive( info );
}


//---------------------------------------------------------------------------------------------
void CHeadlessHatman::Update( void )
{
	BaseClass::Update();

	if ( m_damagePoseParameter < 0 )
	{
		m_damagePoseParameter = LookupPoseParameter( "damage" );
	}

	if ( m_damagePoseParameter >= 0 )
	{
		SetPoseParameter( m_damagePoseParameter, 1.0f - ( (float)GetHealth() / (float)GetMaxHealth() ) );
	}
}


//---------------------------------------------------------------------------------------------
const char *CHeadlessHatman::GetWeaponModel() const
{
	return "models/weapons/c_models/c_bigaxe/c_bigaxe.mdl";
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
class CHeadlessHatmanBehavior : public Action< CHeadlessHatman >
{
public:
	virtual Action< CHeadlessHatman > *InitialContainedAction( CHeadlessHatman *me )	
	{
		return new CHeadlessHatmanEmerge;
	}

	virtual ActionResult< CHeadlessHatman >	Update( CHeadlessHatman *me, float interval )
	{
		if ( !me->IsAlive() )
		{
			// nobody is IT any longer
			DODGameRules()->SetIT( NULL );

			return ChangeTo( new CHeadlessHatmanDying, "I am dead!" );
		}

		return Continue();
	}

	virtual const char *GetName( void ) const	{ return "Attack"; }		// return name of this action
};


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
CHeadlessHatmanIntention::CHeadlessHatmanIntention( CHeadlessHatman *me ) : IIntention( me )
{ 
	m_behavior = new Behavior< CHeadlessHatman >( new CHeadlessHatmanBehavior ); 
}

CHeadlessHatmanIntention::~CHeadlessHatmanIntention()
{
	delete m_behavior;
}

void CHeadlessHatmanIntention::Reset( void )
{ 
	delete m_behavior; 
	m_behavior = new Behavior< CHeadlessHatman >( new CHeadlessHatmanBehavior );
}

void CHeadlessHatmanIntention::Update( void )
{
	m_behavior->Update( static_cast< CHeadlessHatman * >( GetBot() ), GetUpdateInterval() ); 
}

// is this a place we can be?
QueryResultType CHeadlessHatmanIntention::IsPositionAllowed( const INextBot *meBot, const Vector &pos ) const
{
	return ANSWER_YES;
}



//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
float CHeadlessHatmanLocomotion::GetRunSpeed( void ) const
{
	return tf_halloween_bot_speed.GetFloat();
}


//---------------------------------------------------------------------------------------------
// if delta Z is greater than this, we have to jump to get up
float CHeadlessHatmanLocomotion::GetStepHeight( void ) const
{
	return 18.0f;
}


//---------------------------------------------------------------------------------------------
// return maximum height of a jump
float CHeadlessHatmanLocomotion::GetMaxJumpHeight( void ) const
{
	return 18.0f;
}


//---------------------------------------------------------------------------------------------
// Return max rate of yaw rotation
float CHeadlessHatmanLocomotion::GetMaxYawRate( void ) const
{
	return 200.0f;
}


//---------------------------------------------------------------------------------------------
// Should we collide with this entity?
bool CHeadlessHatmanLocomotion::ShouldCollideWith( const CBaseEntity *object ) const
{
	return BaseClass::ShouldCollideWith( object );
}
