#include "pch.h"
#include "Item.h"

#include <stdexcept>

Item::Item(
    uint64_t itemInstanceID,
    const ItemInfo& itemInfo,
    int count
)
    : _itemInstanceID(itemInstanceID)
    , _itemInfo(itemInfo)
    , _count(count)
{
    if (_itemInfo.ItemDefinitionID <= 0)
        throw std::invalid_argument("Invalid ItemDefinitionID");

    if (_itemInfo.MaxStackCount <= 0)
        throw std::invalid_argument("Invalid MaxStackCount");

    if (_count <= 0 || _count > _itemInfo.MaxStackCount)
        throw std::invalid_argument("Invalid Item Count");
}

bool ConsumableItem::Use(Player& target)
{
    // TODO:
    // Player 시스템에 소비형 아이템 사용 시도 요청
    // 성공 여부 반환

    return true;
}

bool EquipmentItem::Use(Player& target)
{
    // TODO:
    // RequiredLevel 등 착용 조건 검사
    // Player / Equipment 시스템에 착용 시도 요청
    // 성공 여부 반환

    return true;
}