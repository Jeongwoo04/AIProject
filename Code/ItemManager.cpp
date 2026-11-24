#include "pch.h"
#include "ItemManager.h"

ItemManager::ItemManager()
    : _acquisitionService(_idGenerator)
    , _dropSystem(_factory, _acquisitionService)
    , _rewardSystem(_factory, _acquisitionService)
    , _shopSystem(_factory, _acquisitionService)
    , _tradeSystem(_acquisitionService)
{
}

ItemManager::~ItemManager() = default;

bool ItemManager::Init(const ItemDefinitionDict& itemDict)
{
    if (itemDict.empty())
        return false;

    if (!_factory.Init(itemDict))
        return false;

    return true;
}