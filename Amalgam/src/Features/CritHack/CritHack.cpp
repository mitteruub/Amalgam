#include "CritHack.h"

#include "../TickHandler/TickHandler.h"

#define WEAPON_RANDOM_RANGE				10000
#define TF_DAMAGE_CRIT_MULTIPLIER		3.0f
#define TF_DAMAGE_CRIT_CHANCE			0.02f
#define TF_DAMAGE_CRIT_CHANCE_RAPID		0.02f
#define TF_DAMAGE_CRIT_CHANCE_MELEE		0.15f
#define TF_DAMAGE_CRIT_DURATION_RAPID	2.0f

/*
	problems:
		flObservedCritChance in negative when more crit dmg than all damage
*/

void CCritHack::Fill(const CUserCmd* pCmd, int n)
{
	static int iStart = pCmd->command_number;

	for (auto& [iSlot, tStorage] : m_mStorage)
	{
		for (auto it = tStorage.m_vCritCommands.begin(); it != tStorage.m_vCritCommands.end();)
		{
			if (*it <= pCmd->command_number)
				it = tStorage.m_vCritCommands.erase(it);
			else
				++it;
		}
		for (auto it = tStorage.m_vSkipCommands.begin(); it != tStorage.m_vSkipCommands.end();)
		{
			if (*it <= pCmd->command_number)
				it = tStorage.m_vSkipCommands.erase(it);
			else
				++it;
		}

		for (int i = 0; i < n; i++)
		{
			if (tStorage.m_vCritCommands.size() >= unsigned(n))
				break;

			const int iCmdNum = iStart + i;
			if (IsCritCommand(iSlot, tStorage.m_iEntIndex, tStorage.m_flMultCritChance, iCmdNum))
				tStorage.m_vCritCommands.push_back(iCmdNum);
		}
		for (int i = 0; i < n; i++)
		{
			if (tStorage.m_vSkipCommands.size() >= unsigned(n))
				break;

			const int iCmdNum = iStart + i;
			if (IsCritCommand(iSlot, tStorage.m_iEntIndex, tStorage.m_flMultCritChance, iCmdNum, false))
				tStorage.m_vSkipCommands.push_back(iCmdNum);
		}
	}

	iStart += n;
}

bool CCritHack::IsCritCommand(int iSlot, int iIndex, float flMultCritChance, const i32 command_number, const bool bCrit, const bool bSafe)
{
	const auto uSeed = MD5_PseudoRandom(command_number) & 0x7FFFFFFF;
	SDK::RandomSeed(DecryptOrEncryptSeed(iSlot, iIndex, uSeed));
	const int iRandom = SDK::RandomInt(0, WEAPON_RANDOM_RANGE - 1);

	if (bSafe)
	{
		int iLower, iUpper;
		if (iSlot == EWeaponSlot::SLOT_MELEE)
			iLower = 1500, iUpper = 6000;
		else
			iLower = 100, iUpper = 800;
		iLower *= flMultCritChance, iUpper *= flMultCritChance;
		if (bCrit ? iLower >= 0 : iUpper < WEAPON_RANDOM_RANGE)
			return bCrit ? iRandom < iLower : !(iRandom < iUpper);
	}
	const int iRange = m_flCritChance * WEAPON_RANDOM_RANGE;
	return bCrit ? iRandom < iRange : !(iRandom < iRange);
}

u32 CCritHack::DecryptOrEncryptSeed(int iSlot, int iIndex, u32 uSeed)
{
	int iLeft = iSlot == SLOT_MELEE ? 8 : 0;
	unsigned int iMask = iIndex << (iLeft + 8) | I::EngineClient->GetLocalPlayer() << iLeft;
	return iMask ^ uSeed;
}

void CCritHack::Resync(CTFPlayer* pLocal)
{
	CTFPlayerResource* pResource = H::Entities.GetPR();
	if (!pResource)
		return;

	if (pResource->GetDamage(pLocal->entindex()) <= 0)
	{
		/*	m_iCritDamage = 0;
			m_iAllDamage = 0;
			m_iMeleeDamage = 0;
			m_iBoostedDamage = 0;*/
		return;
	}

	int iProperDamage = pResource->GetDamage(pLocal->entindex()) - m_iCritDamage - m_iBoostedDamage - m_iMeleeDamage;
	if (m_iAllDamage < iProperDamage)
	{
		if (Vars::Debug::Logging.Value)
			SDK::Output("Crithack", std::format("player resource reports {} damage but current m_iAllDamage is {}", iProperDamage, m_iAllDamage).c_str());
		
		m_iDamageDiff = iProperDamage - m_iAllDamage;
		m_flResyncTime = I::GlobalVars->curtime + 1.0f;
		m_iAllDamage = iProperDamage;
	}
}

void CCritHack::GetTotalCrits(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	int iSlot = pWeapon->GetSlot();
	if (!m_mStorage.contains(iSlot))
		return;

	auto& tStorage = m_mStorage[iSlot];

	static float flOldBucket = 0.f; static int iOldID = 0, iOldCritChecks = 0, iOldCritSeedRequests = 0;
	const float flBucket = pWeapon->m_flCritTokenBucket(); const int iID = pWeapon->GetWeaponID(), iCritChecks = pWeapon->m_nCritChecks(), iCritSeedRequests = pWeapon->m_nCritSeedRequests();
	const bool bShouldUpdate = !tStorage.m_flDamage || flOldBucket != flBucket || iOldID != iID || iOldCritChecks != iCritChecks || iOldCritSeedRequests != iCritSeedRequests;
	flOldBucket = flBucket; iOldID = iID, iOldCritChecks = iCritChecks, iOldCritSeedRequests = iCritSeedRequests;
	if (!bShouldUpdate)
		return;

	static auto tf_weapon_criticals_bucket_cap = U::ConVars.FindVar("tf_weapon_criticals_bucket_cap");
	const float flBucketCap = tf_weapon_criticals_bucket_cap ? tf_weapon_criticals_bucket_cap->GetFloat() : 1000.f;
	bool bRapidFire = pWeapon->IsRapidFire();
	float flFireRate = pWeapon->GetFireRate();

	float flDamage = pWeapon->GetDamage();
	int nProjectilesPerShot = pWeapon->GetBulletsPerShot(false);
	if (iSlot != SLOT_MELEE && nProjectilesPerShot > 0)
		nProjectilesPerShot = SDK::AttribHookValue(nProjectilesPerShot, "mult_bullets_per_shot", pWeapon);
	else
		nProjectilesPerShot = 1;
	float flBaseDamage = flDamage *= nProjectilesPerShot;
	if (bRapidFire)
	{
		flDamage *= TF_DAMAGE_CRIT_DURATION_RAPID / flFireRate;
		if (flDamage * TF_DAMAGE_CRIT_MULTIPLIER > flBucketCap)
			flDamage = flBucketCap / TF_DAMAGE_CRIT_MULTIPLIER;
	}

	float flMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapVal(float(iCritSeedRequests + 1) / (iCritChecks + 1), 0.1f, 1.f, 1.f, 3.f);
	float flCost = flDamage * TF_DAMAGE_CRIT_MULTIPLIER;

	int iPotentialCrits = (flBucketCap - flBaseDamage) / (TF_DAMAGE_CRIT_MULTIPLIER * flDamage / (iSlot == SLOT_MELEE ? 2 : 1) - flBaseDamage);
	int iAvailableCrits = 0;
	{
		int iTestShots = iCritChecks, iTestCrits = iCritSeedRequests;
		float flTestBucket = flBucket;
		int iAttempts = std::min(iPotentialCrits, 1000);
		for (int i = 0; i < iAttempts; i++)
		{
			iTestShots++; iTestCrits++;

			float flTestMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapVal(float(iTestCrits) / iTestShots, 0.1f, 1.f, 1.f, 3.f);
			flTestBucket = std::min(flTestBucket + flBaseDamage, flBucketCap) - flCost * flTestMult;
			if (flTestBucket < 0.f)
				break;

			iAvailableCrits++;
		}
	}

	int iNextCrit = 0;
	if (iAvailableCrits != iPotentialCrits)
	{
		int iTestShots = iCritChecks, iTestCrits = iCritSeedRequests;
		float flTestBucket = flBucket;
		float flTickBase = TICKS_TO_TIME(pLocal->m_nTickBase());
		float flLastRapidFireCritCheckTime = pWeapon->m_flLastRapidFireCritCheckTime();
		int iAttempts = std::min(iPotentialCrits, 1000);
		for (int i = 0; i < 1000; i++)
		{
			int iCrits = 0;
			{
				int iTestShots2 = iTestShots, iTestCrits2 = iTestCrits;
				float flTestBucket2 = flTestBucket;
				for (int j = 0; j < iAttempts; j++)
				{
					iTestShots2++; iTestCrits2++;

					float flTestMult = iSlot == SLOT_MELEE ? 0.5f : Math::RemapVal(float(iTestCrits2) / iTestShots2, 0.1f, 1.f, 1.f, 3.f);
					flTestBucket2 = std::min(flTestBucket2 + flBaseDamage, flBucketCap) - flCost * flTestMult;
					if (flTestBucket2 < 0.f)
						break;

					iCrits++;
				}
			}
			if (iAvailableCrits < iCrits)
				break;

			if (!bRapidFire)
				iTestShots++;
			else
			{
				flTickBase += std::ceilf(flFireRate / TICK_INTERVAL) * TICK_INTERVAL;
				if (flTickBase >= flLastRapidFireCritCheckTime + 1.f || !i && flTestBucket == flBucketCap)
				{
					iTestShots++;
					flLastRapidFireCritCheckTime = flTickBase;
				}
			}

			flTestBucket = std::min(flTestBucket + flBaseDamage, flBucketCap);

			iNextCrit++;
		}
	}

	tStorage.m_flDamage = flBaseDamage;
	tStorage.m_flCost = flCost * flMult;
	tStorage.m_iPotentialCrits = iPotentialCrits;
	tStorage.m_iAvailableCrits = iAvailableCrits;
	tStorage.m_iNextCrit = iNextCrit;
}

void CCritHack::CanFireCritical(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	m_bCritBanned = false;
	m_flDamageTilFlip = 0;

	if (pWeapon->GetSlot() == SLOT_MELEE)
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE_MELEE * pLocal->GetCritMult();
	else if (pWeapon->IsRapidFire())
	{
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE_RAPID * pLocal->GetCritMult();
		float flNonCritDuration = (TF_DAMAGE_CRIT_DURATION_RAPID / m_flCritChance) - TF_DAMAGE_CRIT_DURATION_RAPID;
		m_flCritChance = 1.f / flNonCritDuration;
	}
	else
		m_flCritChance = TF_DAMAGE_CRIT_CHANCE * pLocal->GetCritMult();
	m_flCritChance = SDK::AttribHookValue(m_flCritChance, "mult_crit_chance", pWeapon);

	const float flNormalizedDamage = m_iCritDamage / TF_DAMAGE_CRIT_MULTIPLIER;
	float flCritChance = m_flCritChance + 0.1f;
	if (m_iAllDamage && m_iCritDamage)
	{
		const float flObservedCritChance = flNormalizedDamage / (flNormalizedDamage + m_iAllDamage - m_iCritDamage);
		m_bCritBanned = flObservedCritChance > flCritChance;
	}

	if (m_bCritBanned)
		m_flDamageTilFlip = flNormalizedDamage / flCritChance + flNormalizedDamage * 2 - m_iAllDamage;
	else
<<<<<<< HEAD
		m_flDamageTilFlip = ((flCritChance * flNormalizedDamage - flCritChance * m_iCritDamage + flCritChance * m_iAllDamage - flNormalizedDamage) / flCritChance) / TF_DAMAGE_CRIT_MULTIPLIER;
=======
		m_flDamageTilFlip = ( ( flCritChance * flNormalizedDamage - flCritChance * m_iCritDamage + flCritChance * nTotalDamage - flNormalizedDamage ) / flCritChance ) / TF_DAMAGE_CRIT_MULTIPLIER;
>>>>>>> 14178772bf9bc5e3139db67b629ac54d7afb2007
}

void CCritHack::IsAllowedToWithdrawFromCritBucketHandler(float flDamage) { m_iAllDamage = flDamage; }

bool CCritHack::WeaponCanCrit(CTFWeaponBase* pWeapon, bool bWeaponOnly)
{
	if (!bWeaponOnly && !pWeapon->AreRandomCritsEnabled() || SDK::AttribHookValue(1.f, "mult_crit_chance", pWeapon) <= 0.f)
		return false;

	switch (pWeapon->GetWeaponID())
	{
	case TF_WEAPON_PDA:
	case TF_WEAPON_PDA_ENGINEER_BUILD:
	case TF_WEAPON_PDA_ENGINEER_DESTROY:
	case TF_WEAPON_PDA_SPY:
	case TF_WEAPON_PDA_SPY_BUILD:
	case TF_WEAPON_BUILDER:
	case TF_WEAPON_INVIS:
	case TF_WEAPON_JAR_MILK:
	case TF_WEAPON_LUNCHBOX:
	case TF_WEAPON_BUFF_ITEM:
	case TF_WEAPON_FLAME_BALL:
	case TF_WEAPON_ROCKETPACK:
	case TF_WEAPON_JAR_GAS:
	case TF_WEAPON_LASER_POINTER:
	case TF_WEAPON_MEDIGUN:
	case TF_WEAPON_SNIPERRIFLE:
	case TF_WEAPON_SNIPERRIFLE_DECAP:
	case TF_WEAPON_SNIPERRIFLE_CLASSIC:
	case TF_WEAPON_COMPOUND_BOW:
	case TF_WEAPON_JAR:
	case TF_WEAPON_KNIFE:
		return false;
	}

	return true;
}

void CCritHack::ResetWeapons(CTFPlayer* pLocal)
{
	if (!pLocal->m_hMyWeapons())
		return;

	std::unordered_map<int, bool> mWeapons = {};

	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		auto pWeapon = pLocal->GetWeaponFromSlot(i);
		if (!pWeapon || !WeaponCanCrit(pWeapon))
			continue;

		int iSlot = pWeapon->GetSlot();
		auto& tStorage = m_mStorage[iSlot];
		mWeapons[iSlot] = true;

		int iEntIndex = pWeapon->entindex();
		int iDefIndex = pWeapon->m_iItemDefinitionIndex();
		float flMultCritChance = SDK::AttribHookValue(1.f, "mult_crit_chance", pWeapon);

		if (tStorage.m_iEntIndex != iEntIndex || tStorage.m_iDefIndex != iDefIndex || tStorage.m_flMultCritChance != flMultCritChance)
		{
			tStorage = { iEntIndex, iDefIndex, flMultCritChance };
			SDK::Output("Crithack", std::format("Resetting weapon {}", iDefIndex).c_str(), { 0, 255, 255, 255 }, Vars::Debug::Logging.Value);
		}
	}

	for (auto& [iSlot, _] : m_mStorage)
	{
		if (!mWeapons.contains(iSlot))
			m_mStorage.erase(iSlot);
	}
}

void CCritHack::Reset()
{
	m_mStorage = {};
	m_mHealthStorage = {};

	m_iBoostedDamage = 0;
	m_iMeleeDamage = 0;
	m_iCritDamage = 0;
	m_iAllDamage = 0;
	m_iDamageDiff = 0;
	m_flResyncTime = 0.f;

	m_bCritBanned = false;
	m_flDamageTilFlip = 0;
	m_flCritChance = 0.f;
	m_flObservedCritChance = 0.f;
	m_iWishRandomSeed = 0;

	SDK::Output("Crithack", "Resetting all", { 0, 255, 255, 255 }, Vars::Debug::Logging.Value);
}

void CCritHack::Run(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!pLocal || !pWeapon || !pLocal->IsAlive() || !I::EngineClient->IsInGame())
		return;

	int iSlot = pWeapon->GetSlot();
	ResetWeapons(pLocal);
	Resync(pLocal);
	Fill(pCmd, 128);
	GetTotalCrits(pLocal, pWeapon);
	CanFireCritical(pLocal, pWeapon);
	if (pLocal->IsCritBoosted() || pWeapon->m_flCritTime() > I::GlobalVars->curtime || !m_mStorage.contains(iSlot))
		return;

	// try not to crit at stickies?
	// and maybe crit arrows at teammates when healing

	auto& tStorage = m_mStorage[iSlot];

	if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN && pCmd->buttons & IN_ATTACK)
		pCmd->buttons &= ~IN_ATTACK2;

	bool bAttacking = G::Attacking /*== 1*/ || F::Ticks.m_bDoubletap || F::Ticks.m_bSpeedhack;
	if (G::PrimaryWeaponType == EWeaponType::MELEE)
	{
		bAttacking = G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK;
		if (!bAttacking && pWeapon->GetWeaponID() == TF_WEAPON_FISTS)
			bAttacking = G::CanPrimaryAttack && pCmd->buttons & IN_ATTACK2;
	}
	else if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN && !(G::LastUserCmd->buttons & IN_ATTACK))
		bAttacking = false;

	bool bRapidFire = pWeapon->IsRapidFire();
	bool bStreamWait = bRapidFire && TICKS_TO_TIME(pLocal->m_nTickBase()) < pWeapon->m_flLastRapidFireCritCheckTime() + 1.f;

	int iClosestCrit = !tStorage.m_vCritCommands.empty() ? tStorage.m_vCritCommands.front() : 0;
	int iClosestSkip = !tStorage.m_vSkipCommands.empty() ? tStorage.m_vSkipCommands.front() : 0;

	if (bAttacking)
	{
		const bool bCanCrit = tStorage.m_iAvailableCrits > 0 && (!m_bCritBanned || iSlot == SLOT_MELEE) && !bStreamWait;
		const bool bPressed = Vars::CritHack::ForceCrits.Value || iSlot == SLOT_MELEE && Vars::CritHack::AlwaysMeleeCrit.Value && (Vars::Aimbot::General::AutoShoot.Value ? pCmd->buttons & IN_ATTACK && !(G::OriginalMove.m_iButtons & IN_ATTACK) : Vars::Aimbot::General::AimType.Value);
		if (!Vars::Misc::Game::AntiCheatCompatibility.Value)
		{
			if (bCanCrit && bPressed && iClosestCrit)
				pCmd->command_number = iClosestCrit;
			else if (Vars::CritHack::AvoidRandom.Value && iClosestSkip)
				pCmd->command_number = iClosestSkip;
		}
		else if (Vars::Misc::Game::AntiCheatCritHack.Value)
		{
			bool bShouldAvoid = false;

			bool bCritCommand = IsCritCommand(iSlot, tStorage.m_iEntIndex, tStorage.m_flMultCritChance, pCmd->command_number, true, false);
			if (bCanCrit && bPressed && !bCritCommand
				|| Vars::CritHack::AvoidRandom.Value && !bPressed && bCritCommand)
				bShouldAvoid = true;

			if (bShouldAvoid)
			{
				pCmd->buttons &= ~IN_ATTACK;
				pCmd->viewangles = G::OriginalMove.m_vView;
				G::PSilentAngles = false;
			}
		}
	}
	//else if (Vars::CritHack::AvoidRandom.Value && closestSkip)
	//	pCmd->command_number = closestSkip;

	//if (pCmd->command_number == closestCrit || pCmd->command_number == closestSkip)
	m_iWishRandomSeed = MD5_PseudoRandom(pCmd->command_number) & std::numeric_limits<int>::max();

	if (pCmd->command_number == iClosestCrit)
		tStorage.m_vCritCommands.pop_front();
	else if (pCmd->command_number == iClosestSkip)
		tStorage.m_vSkipCommands.pop_front();
}

int CCritHack::PredictCmdNum(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, CUserCmd* pCmd)
{
	if (!pLocal || !pWeapon || !pLocal->IsAlive() || !I::EngineClient->IsInGame())
		return pCmd->command_number;

	int iSlot = pWeapon->GetSlot();
	if (pLocal->IsCritBoosted() || pWeapon->m_flCritTime() > I::GlobalVars->curtime || !m_mStorage.contains(iSlot))
		return pCmd->command_number;

	auto& tStorage = m_mStorage[iSlot];
	bool bRapidFire = pWeapon->IsRapidFire();
	bool bStreamWait = bRapidFire && TICKS_TO_TIME(pLocal->m_nTickBase()) < pWeapon->m_flLastRapidFireCritCheckTime() + 1.f;

	int closestCrit = !tStorage.m_vCritCommands.empty() ? tStorage.m_vCritCommands.front() : 0;
	int closestSkip = !tStorage.m_vSkipCommands.empty() ? tStorage.m_vSkipCommands.front() : 0;

	const bool bCanCrit = tStorage.m_iAvailableCrits > 0 && (!m_bCritBanned || pWeapon->GetSlot() == SLOT_MELEE) && !bStreamWait;
	const bool bPressed = Vars::CritHack::ForceCrits.Value || pWeapon->GetSlot() == SLOT_MELEE && Vars::CritHack::AlwaysMeleeCrit.Value && (Vars::Aimbot::General::AutoShoot.Value ? pCmd->buttons & IN_ATTACK && !(G::OriginalMove.m_iButtons & IN_ATTACK) : Vars::Aimbot::General::AimType.Value);
	if (bCanCrit && bPressed && closestCrit)
		return closestCrit;
	else if (Vars::CritHack::AvoidRandom.Value && closestSkip)
		return closestSkip;

	return pCmd->command_number;
}

bool CCritHack::CalcIsAttackCriticalHandler(CTFPlayer* pLocal, CTFWeaponBase* pWeapon)
{
	if (!I::Prediction->m_bFirstTimePredicted || !pLocal || !pWeapon)
		return false;

	if (pWeapon->GetWeaponID() == TF_WEAPON_MINIGUN || pWeapon->GetWeaponID() == TF_WEAPON_FLAMETHROWER)
	{
		static int iStaticAmmo = pLocal->GetAmmoCount(pWeapon->m_iPrimaryAmmoType());
		int iOldAmmo = iStaticAmmo;
		int iNewAmmo = iStaticAmmo = pLocal->GetAmmoCount(pWeapon->m_iPrimaryAmmoType());
		if (iOldAmmo == iNewAmmo)
			return false;
	}

	if (m_iWishRandomSeed)
	{
		*G::RandomSeed() = m_iWishRandomSeed;
		m_iWishRandomSeed = 0;
	}

	return true;
}

void CCritHack::Event(IGameEvent* pEvent, uint32_t uHash, CTFPlayer* pLocal)
{
	switch (uHash)
	{
	case FNV1A::Hash32Const("player_hurt"):
	{
		auto pWeapon = H::Entities.GetWeapon();
		if (!pLocal || !pWeapon)
			return;

		const int iVictim = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("userid"));
		CTFPlayer* pVictim = I::ClientEntityList->GetClientEntity(iVictim)->As<CTFPlayer>();
		const int iAttacker = I::EngineClient->GetPlayerForUserID(pEvent->GetInt("attacker"));
		const bool bCrit = pEvent->GetBool("crit") || pEvent->GetBool("minicrit");
		const auto iWeaponID = pEvent->GetInt("weaponid");

		int iDamage = m_mHealthStorage.contains(iVictim) ? std::min(pEvent->GetInt("damageamount"), m_mHealthStorage[iVictim]) : pEvent->GetInt("damageamount");

		if (iVictim == iAttacker || iAttacker != pLocal->entindex() /*|| iWeaponID != pWeapon->GetWeaponID()*/ || pWeapon->GetSlot() == SLOT_MELEE) // weapon id stuff is dumb simplification
		{
			if (pWeapon->GetSlot() == SLOT_MELEE)
				m_iMeleeDamage += iDamage;
			return;
		}


		// m_iAllDamage += iDamage;
		if (bCrit)
		{
			if (pLocal->IsCritBoosted())
				m_iBoostedDamage += iDamage;
			else
				m_iCritDamage += iDamage;
		}

		return;
	}
	case FNV1A::Hash32Const("teamplay_round_start"):
		m_iAllDamage = m_iCritDamage = m_iMeleeDamage = m_iBoostedDamage = 0;
		return;
	case FNV1A::Hash32Const("client_beginconnect"):
	case FNV1A::Hash32Const("client_disconnect"):
	case FNV1A::Hash32Const("game_newmap"):
		Reset();
	}
}

void CCritHack::Store()
{
	auto pResource = H::Entities.GetPR();
	if (!pResource)
		return;

	for (auto it = m_mHealthStorage.begin(); it != m_mHealthStorage.end();)
	{
		if (I::ClientEntityList->GetClientEntity(it->first))
			++it;
		else
			it = m_mHealthStorage.erase(it);
	}
	for (auto& pEntity : H::Entities.GetGroup(EGroupType::PLAYERS_ALL))
	{
		auto pPlayer = pEntity->As<CTFPlayer>();
		if (pPlayer->IsAlive() && !pPlayer->IsAGhost())
			m_mHealthStorage[pPlayer->entindex()] = pPlayer->m_iHealth();
	}
}

void CCritHack::Draw(CTFPlayer* pLocal)
{
	if (!(Vars::Menu::Indicators.Value & (1 << Vars::Menu::IndicatorsEnum::CritHack)) || !I::EngineClient->IsInGame())
		return;

	auto pWeapon = H::Entities.GetWeapon();
	if (!pWeapon || !pLocal->IsAlive() || pLocal->IsAGhost())
		return;

	int x = Vars::Menu::CritsDisplay.Value.x;
	int y = Vars::Menu::CritsDisplay.Value.y + 8;
	const auto& fFont = H::Fonts.GetFont(FONT_INDICATORS);
	const int nTall = fFont.m_nTall + H::Draw.Scale(1);

	EAlign align = ALIGN_TOP;
	if (x <= 100 + H::Draw.Scale(50, Scale_Round))
	{
		x -= H::Draw.Scale(42, Scale_Round);
		align = ALIGN_TOPLEFT;
	}
	else if (x >= H::Draw.m_nScreenW - 100 - H::Draw.Scale(50, Scale_Round))
	{
		x += H::Draw.Scale(42, Scale_Round);
		align = ALIGN_TOPRIGHT;
	}

	auto iSlot = pWeapon->GetSlot();
	if (!m_mStorage.contains(iSlot))
	{
		if (WeaponCanCrit(pWeapon, true))
			H::Draw.String(fFont, x, y, Color_t(255, 150, 150, 255), align, "Random crits disabled");
	}
	else
	{
		auto& tStorage = m_mStorage[iSlot];
		auto bRapidFire = pWeapon->IsRapidFire();
		float flTickBase = TICKS_TO_TIME(pLocal->m_nTickBase());

		if (tStorage.m_flDamage > 0)
		{
			if (pLocal->IsCritBoosted())
				H::Draw.String(fFont, x, y, Color_t(100, 255, 255, 255), align, "Crit Boosted");
			else if (pWeapon->m_flCritTime() > flTickBase)
			{
				float flTime = pWeapon->m_flCritTime() - flTickBase;
				H::Draw.String(fFont, x, y, Color_t(100, 255, 255, 255), align, std::format("Streaming crits {:.1f}s", flTime).c_str());
			}
			else if (!m_bCritBanned)
			{
				if (tStorage.m_iAvailableCrits > 0)
				{
					if (bRapidFire && flTickBase < pWeapon->m_flLastRapidFireCritCheckTime() + 1.f)
					{
						float flTime = pWeapon->m_flLastRapidFireCritCheckTime() + 1.f - flTickBase;
						H::Draw.String(fFont, x, y, Color_t(255, 255, 255, 255), align, std::format("Wait {:.1f}s", flTime).c_str());
					}
					else
						H::Draw.String(fFont, x, y, Color_t(150, 255, 150, 255), align, "Crit Ready");
				}
				else
				{
					int iShots = tStorage.m_iNextCrit;
					H::Draw.String(fFont, x, y, Color_t(255, 150, 150, 255), align, std::format("Crit in {}{} shot{}", iShots, iShots == 1000 ? "+" : "", iShots == 1 ? "" : "s").c_str());
				}
			}
			else
				H::Draw.String(fFont, x, y += nTall, Color_t(255, 150, 150, 255), align, std::format("Deal {} damage", ceilf(m_flDamageTilFlip)).c_str());

			int iCrits = tStorage.m_iAvailableCrits;
			H::Draw.String(fFont, x, y += nTall, Color_t(255, 255, 255, 255), align, std::format("{}{} / {} potential crits", iCrits, iCrits == 1000 ? "+" : "", tStorage.m_iPotentialCrits).c_str());

			if (iCrits && tStorage.m_iNextCrit)
			{
				int iShots = tStorage.m_iNextCrit;
				H::Draw.String(fFont, x, y += nTall, Color_t(255, 255, 255, 255), align, std::format("Next in {}{} shot{}", iShots, iShots == 1000 ? "+" : "", iShots == 1 ? "" : "s").c_str());
			}

			if (!m_bCritBanned && iSlot != SLOT_MELEE && m_flDamageTilFlip)
				H::Draw.String(fFont, x, y += nTall, Color_t(150, 255, 150, 255), align, std::format("{} damage", floor(m_flDamageTilFlip)).c_str());

			if (m_flResyncTime >= I::GlobalVars->curtime)
			{
				H::Draw.String(fFont, x, y += nTall, Color_t(255, 150, 150, 255), align, std::format("Damage desync +{}", m_iDamageDiff).c_str());
			}
		}
		else
			H::Draw.String(fFont, x, y, Color_t(255, 255, 255, 255), align, "Calculating");

		if (Vars::Debug::Info.Value)
		{
			H::Draw.String(fFont, x, y += nTall * 2, Color_t(255, 255, 255, 255), align, std::format("AllDamage: {}, CritDamage: {}", m_iAllDamage, m_iCritDamage).c_str());
			H::Draw.String(fFont, x, y += nTall, Color_t(255, 255, 255, 255), align, std::format("Bucket: {}, Shots: {}, Crits: {}", pWeapon->m_flCritTokenBucket(), pWeapon->m_nCritChecks(), pWeapon->m_nCritSeedRequests()).c_str());
			H::Draw.String(fFont, x, y += nTall, Color_t(255, 255, 255, 255), align, std::format("Damage: {}, Cost: {}", tStorage.m_flDamage, tStorage.m_flCost).c_str());
			H::Draw.String(fFont, x, y += nTall, Color_t(255, 255, 255, 255), align, std::format("CritChance: {:.2f} ({:f}), ObservedCritChance: {:f}", m_flCritChance, m_flCritChance + 0.1f, m_flObservedCritChance).c_str());
			H::Draw.String(fFont, x, y += nTall, Color_t(255, 255, 255, 255), align, std::format("Force: {}, Skip: {}", tStorage.m_vCritCommands.size(), tStorage.m_vSkipCommands.size()).c_str());
		}
	}
}