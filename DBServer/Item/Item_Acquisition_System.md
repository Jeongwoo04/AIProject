# Item_Acquisition_System_legacy_code

> 구현 뼈대. 기존 `Item` / `Inventory`를 재사용한다. 실제 Player/Room/Vector3/DataManager 선언은 제공된 Legacy Code에 없으므로 연결부는 TODO로 표시했다. 빌드·실행 검증 전 코드다.

## 1. 책임과 소유권

- `ItemFactory`: ItemDefinitionID로 ItemInfo를 조회해 새 Item을 생성.
- `ItemInstanceIDProvider`: ItemInstanceID 발급. PlayerID 등 다른 ID 종류와 숫자가 겹쳐도 무관하며, ItemInstanceID 도메인 안에서 충돌하지 않아야 한다.
- `ItemAcquisitionService`: 새 Item 생성은 하지 않고, Item을 Inventory에 전달. `Inventory::AddResult` 반환.
- `DropObject`: 하나의 월드 드랍 Item 스택과 DropID, 위치, 루팅 소유 제한을 보유.
- `DropSystem`: 몬스터 드랍 생성, 플레이어 드랍, 루팅, Drop 관리. Update 만료 처리는 TODO.
- `RewardSystem`: RewardID에 해당하는 여러 Item을 지급. 조건 검증은 현재 임시로 이 시스템에서 수행하고 QuestSystem 도입 후 이전.
- `ShopSystem`: ProductID 구매, Inventory Gold 차감.
- `TradeSystem`: 다수 Item 제안, 양쪽 확정 인터페이스. 상태 흐름 및 실제 교환은 TODO.

```text
몬스터 드랍: ItemFactory -> DropObject -> DropSystem
플레이어 드랍: Inventory::ExtractItem / SplitItem -> DropObject
루팅: DropObject -> ItemAcquisitionService -> Inventory
보상/구매: ItemFactory(호출 측에서 생성) -> ItemAcquisitionService -> Inventory
교환: Inventory -> Trade 임시 보관 -> 상대 Inventory
```

## 2. ItemFactory / ItemInstanceIDProvider

```cpp
// ItemFactory.h
#pragma once
#include "Item.h"
#include <memory>

class ItemFactory
{
public:
    std::unique_ptr<Item> Create(int itemDefinitionID, int count) const;
};

// ItemFactory.cpp
#include "ItemFactory.h"
#include "ConsumableItem.h"
#include "EquipmentItem.h"
#include "DataManager.h"
#include <exception>

std::unique_ptr<Item> ItemFactory::Create(int itemDefinitionID, int count) const
{
    // TODO: 실제 ItemDict<int, ItemInfo> 접근 API에 맞춘다.
    const ItemInfo* info = DataManager::Instance().GetItemInfo(itemDefinitionID);
    if (!info || count <= 0 || count > info->MaxStackCount)
        return nullptr;

    try
    {
        switch (info->Type)
        {
        case ItemType::Consumable:
            return std::make_unique<ConsumableItem>(0, *info, count);
        case ItemType::Equipment:
            return std::make_unique<EquipmentItem>(0, *info, count);
        default:
            return nullptr;
        }
    }
    catch (const std::exception&)
    {
        return nullptr;
    }
}
```

```cpp
// ItemInstanceIDProvider.h
#pragma once
#include <cstdint>

class ItemInstanceIDProvider
{
public:
    uint64_t Generate();

private:
    uint64_t _nextID = 1;
};

// ItemInstanceIDProvider.cpp
#include "ItemInstanceIDProvider.h"

uint64_t ItemInstanceIDProvider::Generate()
{
    // TODO: overflow 및 동시 접근 정책. DBServer/서버 재시작 이후 영속 ID 정책은 추후 확정.
    return _nextID++;
}
```

## 3. ItemAcquisitionService

```cpp
// ItemAcquisitionService.h
#pragma once
#include "Inventory.h"

class Item;
class ItemInstanceIDProvider;

class ItemAcquisitionService
{
public:
    explicit ItemAcquisitionService(ItemInstanceIDProvider& idProvider);

    // Item을 생성하지 않는다. 생성은 ItemFactory 담당.
    Inventory::AddResult Add(
        Inventory& inventory,
        std::unique_ptr<Item>& item,
        Inventory::AddPolicy policy);

private:
    ItemInstanceIDProvider& _idProvider;
};

// ItemAcquisitionService.cpp
#include "ItemAcquisitionService.h"
#include "Item.h"
#include "ItemInstanceIDProvider.h"

ItemAcquisitionService::ItemAcquisitionService(ItemInstanceIDProvider& idProvider)
    : _idProvider(idProvider)
{
}

Inventory::AddResult ItemAcquisitionService::Add(
    Inventory& inventory,
    std::unique_ptr<Item>& item,
    Inventory::AddPolicy policy)
{
    if (!item)
        return {};

    // TODO: ID 발급 정책 세분화 필요
    if (item->GetItemInstanceID() == 0)
        item->SetItemInstanceID(_idProvider.Generate());

    return inventory.TryAddItem(item, policy);
}
```

## 4. DropObject / DropSystem

```cpp
// DropObject.h
#pragma once
#include <cstdint>
#include <memory>

class Item;
struct Vector3;

class DropObject
{
public:
    DropObject(uint64_t id, std::unique_ptr<Item> item,
               const Vector3& position, uint64_t ownerPlayerID);

    uint64_t GetDropID() const { return _dropID; }
    const Vector3& GetPosition() const { return _position; }
    uint64_t GetOwnerPlayerID() const { return _ownerPlayerID; }
    Item* GetItem() const { return _item.get(); }

    std::unique_ptr<Item> ExtractItem();
    bool RestoreItem(std::unique_ptr<Item> item);

private:
    uint64_t _dropID = 0;
    std::unique_ptr<Item> _item;
    Vector3 _position;
    // 0이면 제한 없음. 몬스터 드랍은 처치자 PlayerID.
    uint64_t _ownerPlayerID = 0;
};
```

```cpp
// DropObject.cpp
#include "DropObject.h"
#include "Item.h"

DropObject::DropObject(uint64_t id, std::unique_ptr<Item> item,
                       const Vector3& position, uint64_t ownerPlayerID)
    : _dropID(id), _item(std::move(item)),
      _position(position), _ownerPlayerID(ownerPlayerID) {}

std::unique_ptr<Item> DropObject::ExtractItem()
{
    return std::move(_item);
}

bool DropObject::RestoreItem(std::unique_ptr<Item> item)
{
    if (_item || !item) return false;
    _item = std::move(item);
    return true;
}
```

```cpp
// DropSystem.h
#pragma once
#include "DropObject.h"
#include "Inventory.h"
#include <memory>
#include <unordered_map>

class ItemFactory;
class ItemAcquisitionService;
class Player;
class Item;

class DropSystem
{
public:
    DropSystem(ItemFactory& factory, ItemAcquisitionService& acquisition);

    uint64_t CreateDrop(int itemDefinitionID, int count,
                        const Vector3& position, uint64_t ownerPlayerID = 0);
    uint64_t DropFromPlayer(Player& player, int slotIndex, int count,
                            const Vector3& position);
    Inventory::AddResult Loot(Player& player, uint64_t dropID);
    void Update(); // TODO: 만료된 DropObject 제거

private:
    uint64_t Register(std::unique_ptr<Item> item,
                      const Vector3& position, uint64_t ownerPlayerID);
    bool IsLootAllowed(const DropObject& drop, const Player& player) const;

    ItemFactory& _factory;
    ItemAcquisitionService& _acquisition;
    uint64_t _nextDropID = 1;
    std::unordered_map<uint64_t, std::unique_ptr<DropObject>> _drops;
};
```

```cpp
// DropSystem.cpp
#include "DropSystem.h"
#include "ItemFactory.h"
#include "ItemAcquisitionService.h"
#include "Player.h"
#include "Item.h"

DropSystem::DropSystem(ItemFactory& factory, ItemAcquisitionService& acquisition)
    : _factory(factory), _acquisition(acquisition) {}

uint64_t DropSystem::Register(std::unique_ptr<Item> item,
                              const Vector3& position, uint64_t ownerPlayerID)
{
    if (!item) return 0;
    const uint64_t id = _nextDropID++;
    auto drop = std::make_unique<DropObject>(
        id, std::move(item), position, ownerPlayerID);
    _drops.emplace(id, std::move(drop));
    return id;
}

uint64_t DropSystem::CreateDrop(int definitionID, int count,
                                const Vector3& position, uint64_t ownerPlayerID)
{
    auto item = _factory.Create(definitionID, count);
    return item ? Register(std::move(item), position, ownerPlayerID) : 0;
}

uint64_t DropSystem::DropFromPlayer(Player& player, int slotIndex, int count,
                                    const Vector3& position)
{
    auto inventory = player.Inventory(); // Player는 shared_ptr<Inventory> 보유
    if (!inventory || count <= 0) return 0;

    const Item* source = inventory->GetItem(slotIndex);
    if (!source || count > source->GetCount()) return 0;

    std::unique_ptr<Item> item;
    if (count == source->GetCount())
        item = inventory->ExtractItem(slotIndex);
    else
        item = inventory->SplitItem(slotIndex, count);

    if (!item) return 0;

    // 플레이어가 직접 버린 아이템은 소유 제한 없음.
    // TODO: Register 실패 시 Inventory 원복 경로를 보장하도록 등록 순서 개선.
    return Register(std::move(item), position, 0);
}

bool DropSystem::IsLootAllowed(const DropObject& drop, const Player& player) const
{
    if (drop.GetOwnerPlayerID() == 0) return true;
    // TODO: Player의 실제 PlayerID getter 확인.
    return drop.GetOwnerPlayerID() == player.GetPlayerID();
}

Inventory::AddResult DropSystem::Loot(Player& player, uint64_t dropID)
{
    auto it = _drops.find(dropID);
    if (it == _drops.end()) return {};
    DropObject& drop = *it->second;

    if (!IsLootAllowed(drop, player)) return {};

    // TODO: 요청 위치 주변의 Drop 탐색 및 Player- Drop 거리 검사.
    auto inventory = player.Inventory();
    if (!inventory) return {};

    auto item = drop.ExtractItem();
    if (!item) return {};

    auto result = _acquisition.Add(
        *inventory, item, Inventory::AddPolicy::Partial);

    if (item)
        drop.RestoreItem(std::move(item));

    if (result.RemainingCount == 0)
        _drops.erase(it);

    return result;
}

void DropSystem::Update()
{
    // TODO: Drop 만료 시각/타이머 정책은 현재 미구현. 만료 Drop을 여기서 제거.
}
```

## 5. RewardSystem

```cpp
// RewardSystem.h
#pragma once
#include <vector>

class Player;
class ItemFactory;
class ItemAcquisitionService;

struct RewardItem
{
    int ItemDefinitionID = 0;
    int Count = 0;
};

class RewardSystem
{
public:
    RewardSystem(ItemFactory& factory, ItemAcquisitionService& acquisition);
    bool GiveReward(Player& player, int rewardID);

private:
    ItemFactory& _factory;
    ItemAcquisitionService& _acquisition;
    // TODO: RewardID -> vector<RewardItem> 데이터 테이블 연결.
};
```

```cpp
// RewardSystem.cpp
#include "RewardSystem.h"
#include "ItemFactory.h"
#include "ItemAcquisitionService.h"
#include "Player.h"
#include "Inventory.h"

RewardSystem::RewardSystem(ItemFactory& factory, ItemAcquisitionService& acquisition)
    : _factory(factory), _acquisition(acquisition) {}

bool RewardSystem::GiveReward(Player& player, int rewardID)
{
    auto inventory = player.Inventory();
    if (!inventory || rewardID <= 0) return false;

    // TODO: QuestSystem 도입 전 RewardSystem에서 임시로 수령 조건/중복 수령 검증.
    // TODO: RewardID에 대응하는 보상 목록 조회.
    std::vector<RewardItem> rewards;
    if (rewards.empty()) return false;

    // 모든 Item을 먼저 생성하고 Inventory 공간을 사전 확인해야 전체 실패 정책을 보장할 수 있다.
    // TODO: Inventory에 여러 Item을 원자적으로 추가하는 최소 Batch API 또는 rollback 구현.
    std::vector<std::unique_ptr<Item>> items;
    for (const auto& reward : rewards)
    {
        auto item = _factory.Create(reward.ItemDefinitionID, reward.Count);
        if (!item) return false;
        items.push_back(std::move(item));
    }

    // TODO: 전체 공간 사전 확인 후 지급. 현재 개별 Add 반복은 다중 보상 원자성을 보장하지 않으므로
    // Batch 지급 구현 전까지 실제 적용 금지.
    // TODO(DBServer): PlayerID, RewardID, 지급 ItemDefinitionID/Count 목록, RequestID 기록 요청.
    return false;
}
```

## 6. ShopSystem

```cpp
// ShopSystem.h
#pragma once
#include <unordered_map>

class Player;
class ItemFactory;
class ItemAcquisitionService;

struct ShopProduct
{
    int ProductID = 0;
    int ItemDefinitionID = 0;
    int Count = 0; // 1회 구매 지급량
    int Price = 0; // 1회 구매 가격
};

class ShopSystem
{
public:
    ShopSystem(ItemFactory& factory, ItemAcquisitionService& acquisition);
    bool Purchase(Player& player, int productID, int purchaseCount);

private:
    ItemFactory& _factory;
    ItemAcquisitionService& _acquisition;
    std::unordered_map<int, ShopProduct> _products;
};
```

```cpp
// ShopSystem.cpp
#include "ShopSystem.h"
#include "ItemFactory.h"
#include "ItemAcquisitionService.h"
#include "Player.h"
#include "Inventory.h"
#include <limits>

ShopSystem::ShopSystem(ItemFactory& factory, ItemAcquisitionService& acquisition)
    : _factory(factory), _acquisition(acquisition)
{
    // TODO: 상품 데이터 로드. 재고 무한, 구매 횟수 제한 없음.
}

bool ShopSystem::Purchase(Player& player, int productID, int purchaseCount)
{
    auto inventory = player.Inventory();
    auto it = _products.find(productID);
    if (!inventory || it == _products.end() || purchaseCount <= 0) return false;

    const auto& p = it->second;
    const int64_t count64 = static_cast<int64_t>(p.Count) * purchaseCount;
    const int64_t price64 = static_cast<int64_t>(p.Price) * purchaseCount;
    if (p.Count <= 0 || p.Price < 0 ||
        count64 > std::numeric_limits<int>::max() ||
        price64 > std::numeric_limits<int>::max()) return false;

    if (inventory->GetGold() < price64) return false;

    // TODO: count가 MaxStackCount를 초과할 경우 여러 Item 스택으로 분할 생성.
    // TODO: Inventory 전체 공간 사전 확인 및 지급/Gold 차감 원자성 보장.
    auto item = _factory.Create(p.ItemDefinitionID, static_cast<int>(count64));
    if (!item) return false;

    auto result = _acquisition.Add(
        *inventory, item, Inventory::AddPolicy::AllOrNothing);
    if (!result.Success) return false;

    if (!inventory->TrySpendGold(price64))
    {
        // TODO: 지급 Item rollback. 사전 검증/직렬화 전제로도 실패 경로는 명시적으로 처리.
        return false;
    }

    // TODO(DBServer): PlayerID, ProductID, purchaseCount, totalPrice,
    // ItemDefinitionID, grantedCount, RequestID 저장 요청.
    return true;
}
```

## 7. TradeSystem

```cpp
// TradeSystem.h
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

class Player;
class ItemAcquisitionService;

struct TradeItemOffer
{
    int SlotIndex = -1;
    int Count = 0;
};

struct TradeSession
{
    uint64_t TradeID = 0;
    std::shared_ptr<Player> PlayerA;
    std::shared_ptr<Player> PlayerB;
    std::vector<TradeItemOffer> OfferA;
    std::vector<TradeItemOffer> OfferB;
    bool ConfirmedA = false;
    bool ConfirmedB = false;
};

class TradeSystem
{
public:
    explicit TradeSystem(ItemAcquisitionService& acquisition);

    uint64_t RequestTrade(std::shared_ptr<Player> a, std::shared_ptr<Player> b);
    bool AddOffer(uint64_t tradeID, bool isPlayerA, int slotIndex, int count);
    bool Confirm(uint64_t tradeID, bool isPlayerA);
    bool Cancel(uint64_t tradeID);

private:
    bool ExecuteTrade(TradeSession& session);
    ItemAcquisitionService& _acquisition;
    // TODO: TradeID -> TradeSession 컨테이너 및 ID 발급.
};
```

```cpp
// TradeSystem.cpp
#include "TradeSystem.h"
#include "ItemAcquisitionService.h"
#include "Player.h"
#include "Inventory.h"

TradeSystem::TradeSystem(ItemAcquisitionService& acquisition)
    : _acquisition(acquisition) {}

uint64_t TradeSystem::RequestTrade(std::shared_ptr<Player> a,
                                   std::shared_ptr<Player> b)
{
    if (!a || !b || a == b) return 0;
    // TODO: TradeSession 등록 및 TradeID 반환.
    return 0;
}

bool TradeSystem::AddOffer(uint64_t tradeID, bool isPlayerA,
                           int slotIndex, int count)
{
    // TODO: 세션 조회, 슬롯/수량 검증, 다수 아이템 제안 저장.
    // 양쪽 확정 전까지 Item은 원래 Inventory가 소유한다.
    return false;
}

bool TradeSystem::Confirm(uint64_t tradeID, bool isPlayerA)
{
    // TODO: 확정 상태 반영. 양쪽 확정 시 ExecuteTrade.
    return false;
}

bool TradeSystem::Cancel(uint64_t tradeID)
{
    // TODO: 세션 취소/삭제. 아직 Item 소유권을 이동하지 않으므로 반환 처리 불필요.
    return false;
}

bool TradeSystem::ExecuteTrade(TradeSession& session)
{
    auto invA = session.PlayerA->Inventory();
    auto invB = session.PlayerB->Inventory();
    if (!invA || !invB) return false;

    // TODO: 양쪽 Item/Count 재검증, 수용 공간 사전 계산.
    // TODO: ExtractItem/SplitItem으로 소유권 분리 후 상대 Inventory로 이전.
    // TODO: 양쪽 중 하나라도 실패하면 양쪽 모두 원상복구.
    // TODO(DBServer): TradeID, 양쪽 PlayerID, ItemInstanceID/DefinitionID/Count,
    // RequestID를 포함한 거래 기록 및 소유권 변경 요청.
    return false;
}
```

## 8. DBServer 연동 시 남길 필드

- Reward: `PlayerID`, `RewardID`, 지급 Item 목록(`ItemDefinitionID`, `Count`), `RequestID`.
- Shop: `PlayerID`, `ProductID`, 구매 횟수, 총 가격, 지급 Item 정보, `RequestID`.
- Player Drop: `PlayerID`, `DropID`, Item 정의 ID, Count, 위치/Room 식별 정보.
- Loot: `PlayerID`, `DropID`, 획득 Count, 남은 Count, `RequestID`.
- Trade: `TradeID`, 양쪽 `PlayerID`, 양쪽 Item 식별 정보와 Count, `RequestID`.

> 실제 Protobuf 필드와 DB 트랜잭션/중복 요청 처리는 DBServer 단계에서 확정한다.
