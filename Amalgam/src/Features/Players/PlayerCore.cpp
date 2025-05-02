#include "PlayerCore.h"

#include "PlayerUtils.h"
#include "../Configs/Configs.h"

void CPlayerlistCore::Run()
{
	static Timer tTimer = {};
	if (!tTimer.Run(1.f))
		return;

	LoadPlayerlist();
	SavePlayerlist();
}

void CPlayerlistCore::SavePlayerlist()
{
	if (!F::PlayerUtils.m_bSave || F::PlayerUtils.m_bLoad) // terrible if we end up saving while loading
		return;

	try
	{
		boost::property_tree::ptree writeTree;

		boost::property_tree::ptree configTree;
		for (auto it = F::PlayerUtils.m_vTags.begin(); it != F::PlayerUtils.m_vTags.end(); it++)
		{
			int iID = std::distance(F::PlayerUtils.m_vTags.begin(), it);
			auto& tTag = *it;

			boost::property_tree::ptree tagEntry;
			tagEntry.put("Name", tTag.m_sName);
			tagEntry.put_child("Color", F::Configs.ColorToTree(tTag.m_tColor));
			tagEntry.put("Priority", tTag.m_iPriority);
			tagEntry.put("Label", tTag.m_bLabel);

			configTree.put_child(std::to_string(F::PlayerUtils.IndexToTag(iID)), tagEntry);
		}
		writeTree.put_child("Config", configTree);

		boost::property_tree::ptree tagTree;
		for (auto& [uFriendsID, vTags] : F::PlayerUtils.m_mPlayerTags)
		{
			if (vTags.empty())
				continue;

			boost::property_tree::ptree tagList;
			for (auto& iID : vTags)
			{
				boost::property_tree::ptree child; child.put("", F::PlayerUtils.IndexToTag(iID));
				tagList.push_back(std::make_pair("", child));
			}

			tagTree.put_child(std::to_string(uFriendsID), tagList);
		}
		writeTree.put_child("Tags", tagTree);

		boost::property_tree::ptree aliasTree;
		for (auto& [uFriendsID, sAlias] : F::PlayerUtils.m_mPlayerAliases)
		{
			if (sAlias.empty())
				continue;

			aliasTree.put(std::to_string(uFriendsID), sAlias);
		}
		writeTree.put_child("Aliases", aliasTree);

		// Save the file
		write_json(F::Configs.m_sCorePath + "Players.json", writeTree);

		F::PlayerUtils.m_bSave = false;
		SDK::Output("Amalgam", "Saved playerlist", { 175, 150, 255 }, true, true, true);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Save playerlist failed", { 175, 150, 255, 127 }, true, true);
	}
}

void CPlayerlistCore::LoadPlayerlist()
{
	if (!F::PlayerUtils.m_bLoad)
		return;

	try
	{
		if (std::filesystem::exists(F::Configs.m_sCorePath + "Players.json"))
		{
			boost::property_tree::ptree readTree;
			read_json(F::Configs.m_sCorePath + "Players.json", readTree);

			F::PlayerUtils.m_mPlayerTags.clear();
			F::PlayerUtils.m_mPlayerAliases.clear();
			F::PlayerUtils.m_vTags = {
				{ "Default", { 200, 200, 200, 255 }, 0, false, false, true },
				{ "Ignored", { 200, 200, 200, 255 }, -1, false, true, true },
				{ "Cheater", { 255, 100, 100, 255 }, 1, false, true, true },
				{ "Friend", { 100, 255, 100, 255 }, 0, true, false, true },
				{ "Party", { 100, 50, 255, 255 }, 0, true, false, true },
				{ "F2P", { 255, 255, 255, 255 }, 0, true, false, true }
			};

			int iTagsVersion = readTree.get_child_optional("NewTags") ? 1 : 0; // support for old tag savings
			if (auto configTree = readTree.get_child_optional("Config"))
			{
				iTagsVersion = 2;

				for (auto& it : *configTree)
				{
					PriorityLabel_t tTag = {};
					if (auto getValue = it.second.get_optional<std::string>("Name")) { tTag.m_sName = *getValue; }
					if (const auto getChild = it.second.get_child_optional("Color")) { F::Configs.TreeToColor(*getChild, tTag.m_tColor); }
					if (auto getValue = it.second.get_optional<int>("Priority")) { tTag.m_iPriority = *getValue; }
					if (auto getValue = it.second.get_optional<bool>("Label")) { tTag.m_bLabel = *getValue; }

					int iID = -1;
					try
					{	// new id based indexing
						iID = std::stoi(it.first);
						iID = F::PlayerUtils.TagToIndex(iID);
					}
					catch (...) {}

					if (iID > -1 && iID < F::PlayerUtils.m_vTags.size())
					{
						F::PlayerUtils.m_vTags[iID].m_sName = tTag.m_sName;
						F::PlayerUtils.m_vTags[iID].m_tColor = tTag.m_tColor;
						F::PlayerUtils.m_vTags[iID].m_iPriority = tTag.m_iPriority;
						F::PlayerUtils.m_vTags[iID].m_bLabel = tTag.m_bLabel;
					}
					else
						F::PlayerUtils.m_vTags.push_back(tTag);
				}
			}
			else if (iTagsVersion < 2 && std::filesystem::exists(F::Configs.m_sCorePath + "Tags.json"))
			{	// support legacy file
				boost::property_tree::ptree readTree2;
				read_json(F::Configs.m_sCorePath + "Tags.json", readTree2);

				bool bNewTags = bool(readTree2.get_child_optional("NewTags")); // newer system to support adding default tags better

				auto tagTree = readTree2.get_child_optional("Tags");
				if (!tagTree)
					tagTree = readTree2; // support format w/o tag tree

				for (auto& it : *tagTree)
				{
					PriorityLabel_t tTag = {};
					if (auto getValue = it.second.get_optional<std::string>("Name")) { tTag.m_sName = *getValue; }
					if (const auto getChild = it.second.get_child_optional("Color")) { F::Configs.TreeToColor(*getChild, tTag.m_tColor); }
					if (auto getValue = it.second.get_optional<int>("Priority")) { tTag.m_iPriority = *getValue; }
					if (auto getValue = it.second.get_optional<bool>("Label")) { tTag.m_bLabel = *getValue; }

					int iID = -1;
					try
					{	// new id based indexing
						iID = std::stoi(it.first);
						if (bNewTags)
							iID = F::PlayerUtils.TagToIndex(iID);
						else if (iID > 3)
							iID += TAG_COUNT - 3;
					}
					catch (...)
					{	// old string based indexing
						tTag.m_sName = it.first;
						iID = F::PlayerUtils.GetTag(it.first);
					}

					if (iID > -1 && iID < F::PlayerUtils.m_vTags.size())
					{
						F::PlayerUtils.m_vTags[iID].m_sName = tTag.m_sName;
						F::PlayerUtils.m_vTags[iID].m_tColor = tTag.m_tColor;
						F::PlayerUtils.m_vTags[iID].m_iPriority = tTag.m_iPriority;
						F::PlayerUtils.m_vTags[iID].m_bLabel = tTag.m_bLabel;
					}
					else
						F::PlayerUtils.m_vTags.push_back(tTag);
				}
			}

			auto tagTree = readTree.get_child_optional("Tags");
			if (!tagTree)
				tagTree = readTree; // support format w/o tag tree
			
			for (auto& player : *tagTree)
			{
				uint32_t uFriendsID = std::stoi(player.first);

				for (auto& tag : player.second)
				{
					std::string sTag = std::string(tag.first.data()).empty() ? tag.second.data() : tag.first.data(); // account for dumb old format

					int iID = -1;
					try
					{	// new id based indexing
						iID = std::stoi(sTag);
						if (iTagsVersion != 0)
							iID = F::PlayerUtils.TagToIndex(iID);
						else if (iID > 3)
							iID += TAG_COUNT - 3;
					}
					catch (...)
					{	// old string based indexing
						iID = F::PlayerUtils.GetTag(sTag);
					}

					if (iID == -1)
						continue;

					auto pTag = F::PlayerUtils.GetTag(iID);
					if (!pTag || !pTag->m_bAssignable)
						continue;

					if (!F::PlayerUtils.HasTag(uFriendsID, iID))
						F::PlayerUtils.AddTag(uFriendsID, iID, false);
				}
			}

			if (auto aliasTree = readTree.get_child_optional("Aliases"))
			{
				for (auto& player : *aliasTree)
				{
					uint32_t uFriendsID = std::stoi(player.first);
					std::string sAlias = player.second.data();

					if (!sAlias.empty())
						F::PlayerUtils.m_mPlayerAliases[uFriendsID] = player.second.data();
				}
			}
		}

		F::PlayerUtils.m_bLoad = false;
		SDK::Output("Amalgam", "Loaded playerlist", { 175, 150, 255 }, true, true, true);
	}
	catch (...)
	{
		SDK::Output("Amalgam", "Load playerlist failed", { 175, 150, 255, 127 }, true, true);
	}
}