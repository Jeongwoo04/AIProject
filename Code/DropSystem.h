#pragma once
#include "DropObject.h"
#include "Inventory.h"
#include <memory>
#include <unordered_map>

class ItemFactory;
class ItemAcquisitionService;
class Player;
class Item;

using DropLists = unordered_map<uint64_t, std::unique_ptr<DropObject>>;
using InventoryRef = shared_ptr<Inventory>;

class DropSystem
{
public:
    DropSystem(ItemFactory& factory, ItemAcquisitionService& acquisition);

    DropObject* FindDrop(DropLists& drops, uint64_t dropID);
    uint64_t CreateDrop(DropLists& drops, int itemDefinitionID, int count, const Vector3& pos, uint64_t ownerID);
    uint64_t DropFromInventory(DropLists& drops, InventoryRef inventory, int slotIndex, int count,
    const Vector3& position);
    Inventory::AddResult Loot(DropLists& drops, InventoryRef inventory, uint64_t dropID);
    void Update(); // TODO: 만료된 DropObject 제거

private:
    uint64_t Register(DropLists& drops, std::unique_ptr<DropObject> item, uint64_t dropID);
    bool IsLootAllowed(const DropObject& drop, Player& player) const;

    ItemFactory& _factory;
    ItemAcquisitionService& _acquisition;
    atomic<uint64_t> _nextDropID = 1;
    //std::unordered_map<uint64_t, std::unique_ptr<DropObject>> _drops;
};