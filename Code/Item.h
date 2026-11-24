#pragma once

#include "ItemInfo.h"

#include <cstdint>

class Player;

class Item
{
public:
    Item(
        uint64_t itemInstanceID,
        const ItemInfo& itemInfo,
        int count = 1
    );

    virtual ~Item() = default;

    virtual bool Use(Player& target) = 0;

    uint64_t GetItemInstanceID() const { return _itemInstanceID; }
    int GetItemDefinitionID() const { return _itemInfo.ItemDefinitionID; }
    int GetCount() const { return _count; }
    int GetMaxStackCount() const { return _itemInfo.MaxStackCount; }
    const ItemInfo& GetItemInfo() { return _itemInfo; }

    bool SetItemInstanceID(uint64_t id)
    {
        if (id == 0 || _itemInstanceID != 0) return false;
        _itemInstanceID = id;
        return true;
    }

    bool TrySetCount(int count)
    {
        if (count <= 0 || count > _itemInfo.MaxStackCount)
            return false;

        _count = count;
        return true;
    }

protected:
    uint64_t _itemInstanceID = 0;

    // 기본 아이템 정의 데이터
    const ItemInfo& _itemInfo;

    // 실제 소유 아이템의 수량
    int _count = 1;
};

class ConsumableItem final : public Item
{
public:
    using Item::Item;

    bool Use(Player& target) override;
};

class EquipmentItem final : public Item
{
public:
    using Item::Item;

    bool Use(Player& target) override;
};