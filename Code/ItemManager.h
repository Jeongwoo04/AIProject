#pragma once

#include "ItemFactory.h"
#include "ItemInstanceIDGenerator.h"
#include "ItemAcquisitionService.h"
#include "DropSystem.h"
#include "RewardSystem.h"
#include "ShopSystem.h"
#include "TradeSystem.h"

struct ItemInfo;
using ItemDefinitionDict = unordered_map<int32, ItemInfo>;

class ItemManager
{
public:
	static ItemManager& Instance()
	{
		static ItemManager instance;
		return instance;
	}

	bool Init(const ItemDefinitionDict& itemDict);
    ItemFactory& GetFactory() { return _factory; }
    ItemInstanceIDGenerator& GetIDProvider() { return _idGenerator; }
    ItemAcquisitionService& GetAcquisitionService() { return _acquisitionService; }
    DropSystem& GetDropSystem() { return _dropSystem; }
    RewardSystem& GetRewardSystem() { return _rewardSystem; }
    ShopSystem& GetShopSystem() { return _shopSystem; }
    TradeSystem& GetTradeSystem() { return _tradeSystem; }

private:
    ItemManager();
    ~ItemManager();

    ItemManager(const ItemManager&) = delete;
    ItemManager& operator=(const ItemManager&) = delete;

private:
    bool _initialized = false;

    ItemFactory _factory;
    ItemInstanceIDGenerator _idGenerator;
    ItemAcquisitionService _acquisitionService;
    DropSystem _dropSystem;
    RewardSystem _rewardSystem;
    ShopSystem _shopSystem;
    TradeSystem _tradeSystem;
};