#pragma once
#include "../../../Utils/Feature/Feature.h"
#include "../../Definitions/Classes.h"
#include <unordered_map>

enum struct EGroupType
{
	GROUP_INVALID = -1,
	PLAYERS_ALL, PLAYERS_ENEMIES, PLAYERS_TEAMMATES,
	BUILDINGS_ALL, BUILDINGS_ENEMIES, BUILDINGS_TEAMMATES,
	PICKUPS_HEALTH, PICKUPS_AMMO, PICKUPS_MONEY, PICKUPS_POWERUP, PICKUPS_SPELLBOOK,
	WORLD_PROJECTILES, WORLD_OBJECTIVE, WORLD_NPC, WORLD_BOMBS, WORLD_GARGOYLE,
	MISC_LOCAL_STICKIES, MISC_LOCAL_FLARES, MISC_DOTS
};

struct DormantData
{
	Vec3 Location;
	float LastUpdate = 0.f;
};

struct VelFixRecord
{
	Vec3 m_vecOrigin;
	float m_flSimulationTime;
};

class CEntities
{
	CTFPlayer* m_pLocal = nullptr;
	CTFWeaponBase* m_pLocalWeapon = nullptr;
	CTFPlayerResource* m_pPlayerResource = nullptr;

	std::unordered_map<EGroupType, std::vector<CBaseEntity*>> m_mGroups = {};

	std::unordered_map<int, float> m_mSimTimes, m_mOldSimTimes, m_mDeltaTimes;
	std::unordered_map<int, int> m_mChokes, m_mSetTicks;
	std::unordered_map<int, std::pair<bool, matrix3x4[MAXSTUDIOBONES]>> m_mBones;
	std::unordered_map<int, Vec3> m_mOldAngles, m_mEyeAngles;
	std::unordered_map<int, bool> m_mLagCompensation;
	std::unordered_map<int, DormantData> m_mDormancy;
	std::unordered_map<int, Vec3> m_mAvgVelocities;
	std::unordered_map<int, uint32_t> m_mModels;
	std::unordered_map<int, std::deque<VelFixRecord>> m_mOrigins;

	std::unordered_map<int, int> m_mIPriorities;
	std::unordered_map<uint32_t, int> m_mUPriorities;
	std::unordered_map<int, bool> m_mIFriends;
	std::unordered_map<uint32_t, bool> m_mUFriends;
	std::unordered_map<int, uint64_t> m_mIParty;
	std::unordered_map<uint32_t, uint64_t> m_mUParty;
	std::unordered_map<int, bool> m_mIF2P;
	std::unordered_map<uint32_t, bool> m_mUF2P;
	std::unordered_map<int, int> m_mILevels;
	std::unordered_map<uint32_t, int> m_mULevels;
	uint32_t m_uFriendsID;

	bool m_bSettingUpBones = false;

public:
	void Store();
	void Clear(bool bShutdown = false);
	void ManualNetwork(const StartSoundParams_t& params);

	bool IsHealth(uint32_t uHash);
	bool IsAmmo(uint32_t uHash);
	bool IsPowerup(uint32_t uHash);
	bool IsSpellbook(uint32_t uHash);
	bool IsProjectile(CBaseEntity* pEntity);

	CTFPlayer* GetLocal();
	CTFWeaponBase* GetWeapon();
	CTFPlayerResource* GetPR();

	const std::vector<CBaseEntity*>& GetGroup(const EGroupType& Group);

	float GetSimTime(CBaseEntity* pEntity);
	float GetOldSimTime(CBaseEntity* pEntity);
	float GetDeltaTime(int iIndex);
	int GetChoke(int iIndex);
	matrix3x4* GetBones(int iIndex);
	Vec3 GetEyeAngles(int iIndex);
	Vec3 GetPingAngles(int iIndex);
	bool GetLagCompensation(int iIndex);
	void SetLagCompensation(int iIndex, bool bLagComp);
	bool GetDormancy(int iIndex);
	Vec3* GetAvgVelocity(int iIndex);
	void SetAvgVelocity(int iIndex, Vec3 vAvgVelocity);
	uint32_t GetModel(int iIndex);
	std::deque<VelFixRecord>* GetOrigins(int iIndex);

	int GetPriority(int iIndex);
	int GetPriority(uint32_t uFriendsID);
	bool IsFriend(int iIndex);
	bool IsFriend(uint32_t uFriendsID);
	bool InParty(int iIndex);
	bool InParty(uint32_t uFriendsID);
	bool IsF2P(int iIndex);
	bool IsF2P(uint32_t uFriendsID);
	int GetLevel(int iIndex);
	int GetLevel(uint32_t uFriendsID);
	uint64_t GetParty(int iIndex);
	uint64_t GetParty(uint32_t uFriendsID);

	bool IsSettingUpBones();
};

ADD_FEATURE_CUSTOM(CEntities, Entities, H);