#pragma once

#include "Vars.h"
#include "Globals.h"
#include "Helpers/Entities/Entities.h"
#include "Helpers/Draw/Draw.h"
#include "Helpers/Color/Color.h"
#include "Helpers/TraceFilters/TraceFilters.h"
#include "Helpers/Particles/Particles.h"
#include "Definitions/Types.h"
#include "Definitions/Interfaces.h"
#include "Definitions/Classes.h"
#include "../Utils/Signatures/Signatures.h"
#include "../Utils/Interfaces/Interfaces.h"
#include "../Utils/Hooks/Hooks.h"
#include "../Utils/Memory/Memory.h"
#include "../Utils/ConVars/ConVars.h"
#include "../Utils/KeyHandler/KeyHandler.h"
#include "../Utils/Hash/FNV1A.h"
#include "../Utils/Timer/Timer.h"
#include "../Utils/Feature/Feature.h"
#include <intrin.h>

#define VK_0              0x30
#define VK_1              0x31
#define VK_2              0x32
#define VK_3              0x33
#define VK_4              0x34
#define VK_5              0x35
#define VK_6              0x36
#define VK_7              0x37
#define VK_8              0x38
#define VK_9              0x39
#define VK_A              0x41
#define VK_B              0x42
#define VK_C              0x43
#define VK_D              0x44
#define VK_E              0x45
#define VK_F              0x46
#define VK_G              0x47
#define VK_H              0x48
#define VK_I              0x49
#define VK_J              0x4A
#define VK_K              0x4B
#define VK_L              0x4C
#define VK_M              0x4D
#define VK_N              0x4E
#define VK_O              0x4F
#define VK_P              0x50
#define VK_Q              0x51
#define VK_R              0x52
#define VK_S              0x53
#define VK_T              0x54
#define VK_U              0x55
#define VK_V              0x56
#define VK_W              0x57
#define VK_X              0x58
#define VK_Y              0x59
#define VK_Z              0x5A

template <typename T> int sign(T val)
{
	return (val > T(0)) - (val < T(0));
}

namespace SDK
{
	void Output(const char* cFunction, const char* cLog = nullptr, Color_t tColor = { 255, 255, 255, 255 },
		bool bConsole = true, bool bDebug = false, bool bToast = false, bool bMenu = false, bool bChat = false, bool bParty = false, int iMessageBox = -1,
		const char* sLeft = "[", const char* sRight = "]");

	void SetClipboard(std::string sString);
	std::string GetClipboard();

	HWND GetTeamFortressWindow();
	bool IsGameWindowInFocus();

	std::wstring ConvertUtf8ToWide(const std::string& source);
	std::string ConvertWideToUTF8(const std::wstring& source);

	double PlatFloatTime();
	int StdRandomInt(int min, int max);
	float StdRandomFloat(float min, float max);

	int SeedFileLineHash(int seedvalue, const char* sharedname, int additionalSeed);
	int SharedRandomInt(unsigned iseed, const char* sharedname, int iMinVal, int iMaxVal, int additionalSeed);
	void RandomSeed(int iSeed);
	int RandomInt(int iMinVal = 0, int iMaxVal = 0x7FFF);
	float RandomFloat(float flMinVal = 0.f, float flMaxVal = 1.f);
	
	int HandleToIDX(unsigned int pHandle);

	bool W2S(const Vec3& vOrigin, Vec3& vScreen, bool bAlways = false);
	bool IsOnScreen(CBaseEntity* pEntity, const matrix3x4& transform, float* pLeft = nullptr, float* pRight = nullptr, float* pTop = nullptr, float* pBottom = nullptr);
	bool IsOnScreen(CBaseEntity* pEntity, Vec3 vOrigin);
	bool IsOnScreen(CBaseEntity* pEntity, bool bShouldGetOwner = true);

	void Trace(const Vec3& vecStart, const Vec3& vecEnd, unsigned int nMask, ITraceFilter* pFilter, CGameTrace* pTrace);
	void TraceHull(const Vec3& vecStart, const Vec3& vecEnd, const Vec3& vecHullMin, const Vec3& vecHullMax, unsigned int nMask, ITraceFilter* pFilter, CGameTrace* pTrace);

	bool VisPos(CBaseEntity* pSkip, const CBaseEntity* pEntity, const Vec3& from, const Vec3& to, unsigned int nMask = MASK_SHOT | CONTENTS_GRATE);
	bool VisPosProjectile(CBaseEntity* pSkip, const CBaseEntity* pEntity, const Vec3& from, const Vec3& to, unsigned int nMask = MASK_SHOT | CONTENTS_GRATE);
	bool VisPosWorld(CBaseEntity* pSkip, const CBaseEntity* pEntity, const Vec3& from, const Vec3& to, unsigned int nMask = MASK_SHOT | CONTENTS_GRATE);

	int GetRoundState();
	int GetWinningTeam();
	EWeaponType GetWeaponType(CTFWeaponBase* pWeapon, EWeaponType* pSecondaryType = nullptr);
	const char* GetClassByIndex(const int nClass);

	int IsAttacking(CTFPlayer* pLocal, CTFWeaponBase* pWeapon, const CUserCmd* pCmd, bool bTickBase = false);
	float MaxSpeed(CTFPlayer* pPlayer, bool bIncludeCrouch = false, bool bIgnoreSpecialAbility = false);

	void FixMovement(CUserCmd* pCmd, const Vec3& vCurAngle, const Vec3& vTargetAngle);
	void FixMovement(CUserCmd* pCmd, const Vec3& vTargetAngle);
	bool StopMovement(CTFPlayer* pLocal, CUserCmd* pCmd);

	Vec3 ComputeMove(const CUserCmd* pCmd, CTFPlayer* pLocal, Vec3& vFrom, Vec3& vTo);
	void WalkTo(CUserCmd* pCmd, CTFPlayer* pLocal, Vec3& vFrom, Vec3& vTo, float flScale = 1.f);
	void WalkTo(CUserCmd* pCmd, CTFPlayer* pLocal, Vec3& vTo, float flScale = 1.f);

	void GetProjectileFireSetup(CTFPlayer* pPlayer, const Vec3& vAngIn, Vec3 vOffset, Vec3& vPosOut, Vec3& vAngOut, bool bPipes = false, bool bInterp = false, bool bAllowFlip = true);
	void GetProjectileFireSetupAirblast(CTFPlayer* pPlayer, const Vec3& vAngIn, Vec3 vPosIn, Vec3& vAngOut, bool bInterp = false);
}