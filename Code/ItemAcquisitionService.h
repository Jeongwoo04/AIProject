#pragma once
#include "Inventory.h"

class Item;
class ItemInstanceIDGenerator;
using InventoryRef = shared_ptr<Inventory>;

class ItemAcquisitionService
{
public:
    explicit ItemAcquisitionService(ItemInstanceIDGenerator& idGenerator);

    // Item을 생성하지 않는다. 생성은 ItemFactory 담당.
    Inventory::AddResult Add(
        InventoryRef& inventory,
        std::unique_ptr<Item>& item,
        Inventory::AddPolicy policy);

private:
    ItemInstanceIDGenerator& _idGenerator;
};