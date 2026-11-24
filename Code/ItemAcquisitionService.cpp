#include "pch.h"
#include "ItemAcquisitionService.h"
#include "Item.h"
#include "ItemInstanceIDGenerator.h"

ItemAcquisitionService::ItemAcquisitionService(ItemInstanceIDGenerator& idGenerator)
    : _idGenerator(idGenerator)
{
}

Inventory::AddResult ItemAcquisitionService::Add(
    InventoryRef& inventory,
    std::unique_ptr<Item>& item,
    Inventory::AddPolicy policy)
{
    if (!item)
        return {};

    // TODO: ID 발급은 실제로 새 Item Instance가 Inventory 슬롯을 차지할 때만 수행하도록
    // Inventory의 신규 슬롯 배치 지점과 통합할 것. 기존 스택에 완전히 합쳐지는 경우 ID 불필요.
    if (item->GetItemInstanceID() == 0)
        item->SetItemInstanceID(_idGenerator.Generate());

    return inventory->TryAddItem(item, policy);
}