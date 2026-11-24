#include "pch.h"
#include "DropSystem.h"
#include "ItemFactory.h"
#include "ItemAcquisitionService.h"
#include "Player.h"
#include "Item.h"

DropSystem::DropSystem(ItemFactory& factory, ItemAcquisitionService& acquisition)
    : _factory(factory), _acquisition(acquisition) {
}

uint64_t DropSystem::Register(DropLists& drops, std::unique_ptr<DropObject> item, uint64_t dropID)
{
    if (!item)
        return 0;

    drops.emplace(dropID, std::move(item));

    return dropID;
}

uint64_t DropSystem::CreateDrop(DropLists& drops, int definitionID, int count, const Vector3& pos, uint64_t ownerID)
{
    auto item = _factory.Create(definitionID, count);
    if (!item)
        return 0;

    uint64 dropID = _nextDropID.fetch_add(1);

    auto drop = std::make_unique<DropObject>(
        dropID,
        std::move(item),
        pos,
        ownerID
    );
    cout << "DropSystem::CreateDrop()\n";
    cout << "DropId: " << dropID << ", ItemId: " << drop->GetItem()->GetItemDefinitionID() << ", ItemCount: " << drop->GetItem()->GetCount() << ", OwnerId: " << drop->GetOwnerPlayerID() << endl;

    if (!Register(drops, std::move(drop), dropID))
        return 0;

    return dropID;
}

DropObject* DropSystem::FindDrop(
    DropLists& drops,
    uint64_t dropID)
{
    auto it = drops.find(dropID);
    if (it == drops.end())
        return nullptr;

    return it->second.get();
}

uint64_t DropSystem::DropFromInventory(DropLists& drops, InventoryRef inventory, int slotIndex, int count,
    const Vector3& pos)
{
    if (count <= 0 || slotIndex < 0 || slotIndex >= Inventory::SlotCount)
        return 0;

    const Item* source = inventory->GetItem(slotIndex);
    if (!source || count > source->GetCount())
        return 0;

    std::unique_ptr<Item> item;
    if (count == source->GetCount())
        item = inventory->ExtractItem(slotIndex);
    else
        item = inventory->SplitItem(slotIndex, count);

    if (!item)
        return 0;

    uint64 dropID = _nextDropID.fetch_add(1);

    auto drop = std::make_unique<DropObject>(
        dropID,
        std::move(item),
        pos,
        inventory->GetOwnerID()
    );

    // 플레이어가 직접 버린 아이템은 소유 제한 없음.
    // TODO: Register 실패 시 Inventory 원복 경로를 보장하도록 등록 순서 개선.
    return Register(drops, std::move(drop), dropID);
}

bool DropSystem::IsLootAllowed(const DropObject& drop, Player& player) const
{
    if (drop.GetOwnerPlayerID() == player.GetId()) return true;
    // TODO: Player의 실제 PlayerID getter 확인.
    return drop.GetOwnerPlayerID() == player.GetId();
}

Inventory::AddResult DropSystem::Loot(DropLists& drops, InventoryRef inventory, uint64_t dropID)
{
    DropObject& drop = *FindDrop(drops,dropID);

    auto item = drop.ExtractItem();

    if (!item)
        return {};

    auto result = _acquisition.Add(
        inventory,
        item,
        Inventory::AddPolicy::Partial);

    // 인벤토리에 들어가지 못한 수량은 DropObject가 다시 소유한다.
    if (item)
        drop.RestoreItem(std::move(item));

    // 남은 아이템이 없다면 월드에서 제거한다.
    if (result.RemainingCount == 0)
        drops.erase(dropID);

    return result;

    //auto it = _drops.find(dropID);
    //if (it == _drops.end()) return {};
    //DropObject& drop = *it->second;

    //if (!IsLootAllowed(drop, player)) return {};

    //// TODO: 요청 위치 주변의 Drop 탐색 및 Player- Drop 거리 검사.
    //auto inventory = player.GetInventory();
    //if (!inventory) return {};

    //auto item = drop.ExtractItem();
    //if (!item) return {};

    //auto result = _acquisition.Add(
    //    *inventory, item, Inventory::AddPolicy::Partial);

    //if (item)
    //    drop.RestoreItem(std::move(item));

    //if (result.RemainingCount == 0)
    //    _drops.erase(it);

    //return result;
}

void DropSystem::Update()
{
    // TODO: Drop 만료 시각/타이머 정책은 현재 미구현. 만료 Drop을 여기서 제거.
}