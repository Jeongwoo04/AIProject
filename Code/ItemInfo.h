#pragma once

#include <optional>

enum class ItemType
{
	Consumable,
	Equipment
};

struct ItemInfo
{
	int ItemDefinitionID = 0;
	ItemType Type = ItemType::Consumable;
	string ItemName = "";
	int MaxStackCount = 1;

	std::optional<int> HP;
	std::optional<int> MP;

	std::optional<int> Attack;
	std::optional<int> RequiredLevel;
};