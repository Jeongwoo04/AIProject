# Item_legacy_code 수정본

기존 구조를 유지하면서 ItemInstanceID가 Inventory에 귀속되기 전에는 0일 수 있도록 최소 수정했다.

## ItemInfo.h

```cpp
#pragma once
#include <optional>

enum class ItemType { Consumable, Equipment };

struct ItemInfo
{
    int ItemDefinitionID = 0;
    ItemType Type = ItemType::Consumable;
    int MaxStackCount = 1;
    std::optional<int> HP;
    std::optional<int> MP;
    std::optional<int> Attack;
    std::optional<int> RequiredLevel;
};
```

## Item.h

```cpp
#pragma once
#include "ItemInfo.h"
#include <cstdint>

class Character;

class Item
{
public:
    Item(uint64_t itemInstanceID, const ItemInfo& itemInfo, int count = 1);
    virtual ~Item() = default;

    virtual bool Use(Character& target) = 0;

    uint64_t GetItemInstanceID() const { return _itemInstanceID; }
    int GetItemDefinitionID() const { return _itemInfo.ItemDefinitionID; }
    int GetCount() const { return _count; }
    int GetMaxStackCount() const { return _itemInfo.MaxStackCount; }
    const ItemInfo& GetItemInfo() const { return _itemInfo; }

    // ID가 아직 없는 Item(0)에 한 번만 발급할 수 있다.
    bool SetItemInstanceID(uint64_t id)
    {
        if (id == 0 || _itemInstanceID != 0) return false;
        _itemInstanceID = id;
        return true;
    }

    bool TrySetCount(int count)
    {
        if (count <= 0 || count > _itemInfo.MaxStackCount) return false;
        _count = count;
        return true;
    }

protected:
    uint64_t _itemInstanceID = 0;
    ItemInfo _itemInfo;
    int _count = 1;
};
```

## Item.cpp

```cpp
#include "Item.h"
#include <stdexcept>

Item::Item(uint64_t itemInstanceID, const ItemInfo& itemInfo, int count)
    : _itemInstanceID(itemInstanceID), _itemInfo(itemInfo), _count(count)
{
    if (_itemInfo.ItemDefinitionID <= 0)
        throw std::invalid_argument("Invalid ItemDefinitionID");
    if (_itemInfo.MaxStackCount <= 0)
        throw std::invalid_argument("Invalid MaxStackCount");
    if (_count <= 0 || _count > _itemInfo.MaxStackCount)
        throw std::invalid_argument("Invalid Item Count");
    // itemInstanceID == 0은 Inventory 귀속 전 상태.
}
```

## 파생 Item

```cpp
#pragma once
#include "Item.h"

class ConsumableItem final : public Item
{
public:
    using Item::Item;
    bool Use(Character& target) override;
};

class EquipmentItem final : public Item
{
public:
    using Item::Item;
    bool Use(Character& target) override;
};
```

```cpp
#include "ConsumableItem.h"
bool ConsumableItem::Use(Character& target)
{
    // TODO: Character 시스템에 소비형 아이템 사용 시도.
    return false;
}
```

```cpp
#include "EquipmentItem.h"
bool EquipmentItem::Use(Character& target)
{
    // TODO: RequiredLevel 등 착용 조건 검사 및 Character/Equipment 시스템에 요청.
    return false;
}
```

### 변경 사항

- ItemInstanceID 0 허용: 아직 Inventory에 귀속되지 않은 월드 Drop Item을 표현.
- `SetItemInstanceID()` 추가: 미발급 상태에서 한 번만 ID 설정.
- 기존 `TrySetCount()`가 이미 존재하면 중복 추가하지 않고 기존 함수를 유지.
