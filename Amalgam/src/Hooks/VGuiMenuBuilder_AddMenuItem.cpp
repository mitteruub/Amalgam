#include "../SDK/SDK.h"

#include "../Features/Players/PlayerUtils.h"
#include "../Features/Spectate/Spectate.h"
#include "../Features/Output/Output.h"

MAKE_SIGNATURE(CVoiceStatus_IsPlayerBlocked, "client.dll", "40 53 48 81 EC ? ? ? ? 48 8B D9 4C 8D 44 24", 0x0);
MAKE_SIGNATURE(VGuiMenuBuilder_AddMenuItem, "client.dll", "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B EA 49 8B F9 48 8B 51 ? 49 8B F0 48 8B D9 48 85 D2 74 ? 49 8B C9 E8 ? ? ? ? 85 C0 74 ? 48 8B 0B 48 8B 01 FF 90 ? ? ? ? 48 8B 0B 4C 8B C6 4C 8B 4B ? 48 8B D5 48 89 7B ? 48 C7 44 24 ? ? ? ? ? 48 8B 01 FF 90 ? ? ? ? 48 8B 13 3B 82 ? ? ? ? 73 ? 3B 82 ? ? ? ? 7F ? 48 8B 92 ? ? ? ? 8B C8 48 03 C9 39 44 CA ? 75 ? 39 44 CA ? 75 ? 48 8B 04 CA EB ? 33 C0 48 8B 5C 24 ? 48 8B 6C 24 ? 48 8B 74 24 ? 48 83 C4 ? 5F C3 CC CC CC CC 48 89 5C 24", 0x0);
MAKE_SIGNATURE(CTFClientScoreBoardDialog_OnCommand, "client.dll", "48 89 5C 24 ? 57 48 83 EC ? 48 8B DA 48 8B F9 48 8B CB 48 8D 15 ? ? ? ? E8 ? ? ? ? 48 85 C0 74 ? 48 8B 0D", 0x0);
MAKE_SIGNATURE(CTFClientScoreBoardDialog_OnScoreBoardMouseRightRelease_IsPlayerBlocked_Call, "client.dll", "84 C0 48 8D 0D ? ? ? ? 48 8D 15 ? ? ? ? 48 0F 45 D1 4C 8D 0D", 0x0);
MAKE_SIGNATURE(CTFClientScoreBoardDialog_OnScoreBoardMouseRightRelease_AddMenuItem_Call, "client.dll", "48 8B 0D ? ? ? ? 4C 8D 85 ? ? ? ? 48 8D 95 ? ? ? ? 48 8B 01 FF 90 ? ? ? ? 44 8B 85", 0x0);

static int PlayerIndex;
static uint32_t FriendsID;
static std::string PlayerName;

MAKE_HOOK(CVoiceStatus_IsPlayerBlocked, S::CVoiceStatus_IsPlayerBlocked(), bool,
    void* rcx, int playerIndex)
{
#ifdef DEBUG_HOOKS
    if (!Vars::Hooks::VGuiMenuBuilder_AddMenuItem[DEFAULT_BIND])
        return CALL_ORIGINAL(rcx, playerIndex);
#endif

    static const auto dwDesired = S::CTFClientScoreBoardDialog_OnScoreBoardMouseRightRelease_IsPlayerBlocked_Call();
    const auto dwRetAddr = uintptr_t(_ReturnAddress());

    if (Vars::Visuals::UI::ScoreboardUtility.Value && dwRetAddr == dwDesired)
        PlayerIndex = playerIndex;

    return CALL_ORIGINAL(rcx, playerIndex);
}

MAKE_HOOK(VGuiMenuBuilder_AddMenuItem, S::VGuiMenuBuilder_AddMenuItem(), void*,
    void* rcx, const char* pszButtonText, const char* pszCommand, const char* pszCategoryName)
{
#ifdef DEBUG_HOOKS
    if (!Vars::Hooks::VGuiMenuBuilder_AddMenuItem[DEFAULT_BIND])
        return CALL_ORIGINAL(rcx, pszButtonText, pszCommand, pszCategoryName);
#endif

    static const auto dwDesired = S::CTFClientScoreBoardDialog_OnScoreBoardMouseRightRelease_AddMenuItem_Call();
    const auto dwRetAddr = uintptr_t(_ReturnAddress());

    if (Vars::Visuals::UI::ScoreboardUtility.Value && dwRetAddr == dwDesired && PlayerIndex != -1)
    {
        auto ret = CALL_ORIGINAL(rcx, pszButtonText, pszCommand, pszCategoryName);

        PlayerInfo_t pi{};
        if (I::EngineClient->GetPlayerInfo(PlayerIndex, &pi) && !pi.fakeplayer)
        {
            FriendsID = pi.friendsID;
            PlayerName = pi.name;

            CALL_ORIGINAL(rcx, "History", "history", "profile");
            CALL_ORIGINAL(rcx, I::EngineClient->GetPlayerForUserID(F::Spectate.m_iIntendedTarget) == PlayerIndex ? "Unspectate" : "Spectate", "spectate", "profile");

            CALL_ORIGINAL(rcx, std::format("Tags for {}", PlayerName).c_str(), "listtags", "tags");
            for (auto it = F::PlayerUtils.m_vTags.begin(); it != F::PlayerUtils.m_vTags.end(); it++)
            {
                int iID = std::distance(F::PlayerUtils.m_vTags.begin(), it);
                auto& tTag = *it;
                if (!tTag.Assignable)
                    continue;

                bool bHasTag = F::PlayerUtils.HasTag(FriendsID, iID);
                CALL_ORIGINAL(rcx, std::format("{} {}", bHasTag ? "Remove" : "Add", tTag.Name).c_str(), std::format("modifytag{}", iID).c_str(), "tags");
            }
        }

        return ret;
    }

    return CALL_ORIGINAL(rcx, pszButtonText, pszCommand, pszCategoryName);
}

MAKE_HOOK(CTFClientScoreBoardDialog_OnCommand, S::CTFClientScoreBoardDialog_OnCommand(), void,
    void* rcx, const char* command)
{
#ifdef DEBUG_HOOKS
    if (!Vars::Hooks::VGuiMenuBuilder_AddMenuItem[DEFAULT_BIND])
        return CALL_ORIGINAL(rcx, command);
#endif

    if (!Vars::Visuals::UI::ScoreboardUtility.Value && !command)
        return CALL_ORIGINAL(rcx, command);

    auto uHash = FNV1A::Hash32(command);
    switch (uHash)
    {
    case FNV1A::Hash32Const("history"):
        I::SteamFriends->ActivateGameOverlayToWebPage(std::format("https://steamhistory.net/id/{}", CSteamID(FriendsID, k_EUniversePublic, k_EAccountTypeIndividual).ConvertToUint64()).c_str());
        break;
    case FNV1A::Hash32Const("spectate"):
        if (auto pResource = H::Entities.GetPR())
            F::Spectate.SetTarget(pResource->m_iUserID(PlayerIndex));
        break;
    case FNV1A::Hash32Const("listtags"):
        F::Output.TagsOnJoin(PlayerName, FriendsID);
        break;
    default:
        if (strstr(command, "modifytag"))
        {
            try
            {
                std::string sTag = command;
                sTag = sTag.replace(0, strlen("modifytag"), "");
                int iID = std::stoi(sTag);
                if (!F::PlayerUtils.HasTag(FriendsID, iID))
                    F::PlayerUtils.AddTag(FriendsID, iID, true, PlayerName);
                else
                    F::PlayerUtils.RemoveTag(FriendsID, iID, true, PlayerName);
            }
            catch (...) {}
        }
        CALL_ORIGINAL(rcx, command);
    }
}