#pragma once
#include "Item.h"
#include "DataManager.h"
#include <memory>

using ItemDefinitionDict = unordered_map<int32, ItemInfo>;

class ItemFactory
{
public:
    bool Init(const ItemDefinitionDict& itemDict);

    std::unique_ptr<Item> Create(int itemDefinitionID, int count) const;

private:
    const ItemDefinitionDict* _itemDict = nullptr;
};