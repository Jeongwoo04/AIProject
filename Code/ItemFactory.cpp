#include "pch.h"
#include "ItemFactory.h"
#include "Item.h"
#include "DataManager.h"
#include <exception>
#include "ItemManager.h"

bool ItemFactory::Init(const ItemDefinitionDict& itemDict)
{
    if (itemDict.empty())
        return false;

    _itemDict = &itemDict;
    return true;
}

std::unique_ptr<Item> ItemFactory::Create(int itemDefinitionID, int count) const
{
    // TODO: 실제 ItemDict<int, ItemInfo> 접근 API에 맞춘다.
    if (!_itemDict || itemDefinitionID <= 0 || count <= 0)
        return nullptr;

    auto it = _itemDict->find(itemDefinitionID);

    if (it == _itemDict->end())
        return nullptr;

    const ItemInfo& info = it->second;

    if (count > info.MaxStackCount)
        return nullptr;

    try
    {
        switch (info.Type)
        {
        case ItemType::Consumable:
            return std::make_unique<ConsumableItem>(0, info, count);
        case ItemType::Equipment:
            return std::make_unique<EquipmentItem>(0, info, count);
        default:
            return nullptr;
        }
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}