# Inventory_legacy_code 수정본

기존 Inventory 기능을 유지하면서 `ExtractItem`, `SplitItem` 및 임시 Gold 관리만 추가했다. ID 발급 책임은 AcquisitionService와 Inventory의 실제 신규 슬롯 삽입 시점 사이에 최종 통합이 필요하다.

## Inventory.h

```cpp
#pragma once
#include "Item.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

class Character;

class Inventory final
{
public:
    static constexpr std::size_t SlotCount = 40;
    enum class AddPolicy { AllOrNothing, Partial };

    struct AddResult
    {
        bool Success = false;
        int AddedCount = 0;
        int RemainingCount = 0;
    };

    Inventory() = default;
    Inventory(const Inventory&) = delete;
    Inventory& operator=(const Inventory&) = delete;

    Item* GetItem(int slotIndex);
    const Item* GetItem(int slotIndex) const;
    bool IsValidSlot(int slotIndex) const;
    int FindEmptySlot() const;
    AddResult TryAddItem(std::unique_ptr<Item>& item, AddPolicy policy);
    bool CanFit(const Item& item, int count) const;
    bool MoveItem(int fromSlot, int toSlot);
    bool RemoveCount(int slotIndex, int count, int& outRemovedCount);
    bool UseItem(int slotIndex, Character& target);
    void Clear();

    std::unique_ptr<Item> ExtractItem(int slotIndex);
    std::unique_ptr<Item> SplitItem(int slotIndex, int count);

    int64_t GetGold() const { return _gold; }
    bool TrySpendGold(int64_t amount);
    bool TryAddGold(int64_t amount);

private:
    bool IsSameDefinition(const Item& lhs, const Item& rhs) const;
    std::array<std::unique_ptr<Item>, SlotCount> _slots{};
    int64_t _gold = 0; // 임시. 추후 Wallet/Player 재화 구조로 이동 가능.
};
```

## Inventory.cpp

```cpp
#include "Inventory.h"
#include "ConsumableItem.h"
#include "EquipmentItem.h"
#include <algorithm>
#include <limits>

Item* Inventory::GetItem(int slotIndex)
{
    if (!IsValidSlot(slotIndex)) return nullptr;
    return _slots[static_cast<std::size_t>(slotIndex)].get();
}

const Item* Inventory::GetItem(int slotIndex) const
{
    if (!IsValidSlot(slotIndex)) return nullptr;
    return _slots[static_cast<std::size_t>(slotIndex)].get();
}

bool Inventory::IsValidSlot(int slotIndex) const
{
    return slotIndex >= 0 && static_cast<std::size_t>(slotIndex) < SlotCount;
}

int Inventory::FindEmptySlot() const
{
    for (std::size_t i = 0; i < SlotCount; ++i)
        if (!_slots[i]) return static_cast<int>(i);
    return -1;
}

bool Inventory::IsSameDefinition(const Item& a, const Item& b) const
{
    return a.GetItemDefinitionID() == b.GetItemDefinitionID();
}

bool Inventory::CanFit(const Item& item, int count) const
{
    if (count <= 0 || item.GetMaxStackCount() <= 0 || item.GetCount() <= 0)
        return false;

    int64_t capacity = 0;
    const int maxStack = item.GetMaxStackCount();

    for (const auto& slot : _slots)
        if (slot && IsSameDefinition(*slot, item))
        {
            const int stackMax = std::min(maxStack, slot->GetMaxStackCount());
            capacity += std::max(0, stackMax - slot->GetCount());
        }

    for (const auto& slot : _slots)
        if (!slot) capacity += maxStack;

    return capacity >= static_cast<int64_t>(count);
}

Inventory::AddResult Inventory::TryAddItem(
    std::unique_ptr<Item>& item, AddPolicy policy)
{
    AddResult result{};
    if (!item) return result;

    const int incoming = item->GetCount();
    const int maxStack = item->GetMaxStackCount();
    if (incoming <= 0 || maxStack <= 0 || incoming > maxStack) return result;

    if (policy == AddPolicy::AllOrNothing && !CanFit(*item, incoming))
    {
        result.RemainingCount = incoming;
        return result;
    }

    int remaining = incoming;
    for (auto& slot : _slots)
    {
        if (!slot || !IsSameDefinition(*slot, *item)) continue;
        const int stackMax = std::min(maxStack, slot->GetMaxStackCount());
        const int room = stackMax - slot->GetCount();
        if (room <= 0) continue;

        const int transfer = std::min(room, remaining);
        if (!slot->TrySetCount(slot->GetCount() + transfer))
        {
            result.RemainingCount = incoming;
            return result;
        }
        remaining -= transfer;
        result.AddedCount += transfer;
        if (remaining == 0) break;
    }

    if (remaining > 0)
    {
        const int empty = FindEmptySlot();
        if (empty >= 0)
        {
            if (!item->TrySetCount(remaining))
            {
                result.RemainingCount = incoming;
                return result;
            }
            // TODO: 신규 슬롯에 실제 귀속되는 시점에만 ItemInstanceIDProvider에서 ID 발급.
            _slots[static_cast<std::size_t>(empty)] = std::move(item);
            result.AddedCount += remaining;
            remaining = 0;
        }
    }

    result.RemainingCount = remaining;
    result.Success = remaining == 0 ||
        (policy == AddPolicy::Partial && result.AddedCount > 0);

    if (remaining == 0) item.reset();
    else if (item) item->TrySetCount(remaining);
    return result;
}

bool Inventory::MoveItem(int fromSlot, int toSlot)
{
    if (!IsValidSlot(fromSlot) || !IsValidSlot(toSlot) || fromSlot == toSlot)
        return false;

    auto& source = _slots[static_cast<std::size_t>(fromSlot)];
    auto& destination = _slots[static_cast<std::size_t>(toSlot)];
    if (!source) return false;

    if (!destination)
    {
        destination = std::move(source);
        return true;
    }

    if (IsSameDefinition(*source, *destination))
    {
        const int maxStack = std::min(
            source->GetMaxStackCount(), destination->GetMaxStackCount());
        const int room = maxStack - destination->GetCount();
        if (room <= 0) return false;

        const int transfer = std::min(room, source->GetCount());
        const int destNew = destination->GetCount() + transfer;
        const int sourceNew = source->GetCount() - transfer;

        if (!destination->TrySetCount(destNew)) return false;
        if (sourceNew == 0) source.reset();
        else if (!source->TrySetCount(sourceNew))
        {
            destination->TrySetCount(destination->GetCount() - transfer);
            return false;
        }
        return true;
    }

    std::swap(source, destination);
    return true;
}

bool Inventory::RemoveCount(int slotIndex, int count, int& outRemovedCount)
{
    outRemovedCount = 0;
    if (!IsValidSlot(slotIndex) || count <= 0) return false;
    auto& slot = _slots[static_cast<std::size_t>(slotIndex)];
    if (!slot || count > slot->GetCount()) return false;

    if (count == slot->GetCount())
    {
        slot.reset();
        outRemovedCount = count;
        return true;
    }

    if (!slot->TrySetCount(slot->GetCount() - count)) return false;
    outRemovedCount = count;
    return true;
}

bool Inventory::UseItem(int slotIndex, Character& target)
{
    if (!IsValidSlot(slotIndex)) return false;
    auto& slot = _slots[static_cast<std::size_t>(slotIndex)];
    if (!slot || !slot->Use(target)) return false;

    const int count = slot->GetCount();
    if (count <= 0) return false;
    if (count == 1)
    {
        slot.reset();
        return true;
    }
    return slot->TrySetCount(count - 1);
}

std::unique_ptr<Item> Inventory::ExtractItem(int slotIndex)
{
    if (!IsValidSlot(slotIndex)) return nullptr;
    auto& slot = _slots[static_cast<std::size_t>(slotIndex)];
    if (!slot) return nullptr;
    return std::move(slot);
}

std::unique_ptr<Item> Inventory::SplitItem(int slotIndex, int count)
{
    if (!IsValidSlot(slotIndex) || count <= 0) return nullptr;
    auto& source = _slots[static_cast<std::size_t>(slotIndex)];
    if (!source || count >= source->GetCount()) return nullptr;

    const ItemInfo& info = source->GetItemInfo();
    if (info.Type != ItemType::Consumable) return nullptr;

    std::unique_ptr<Item> split;
    try
    {
        split = std::make_unique<ConsumableItem>(0, info, count);
    }
    catch (...)
    {
        return nullptr;
    }

    // 새 Item 생성 성공 후 원본 수량 감소. 실패하면 원본은 유지.
    if (!source->TrySetCount(source->GetCount() - count)) return nullptr;
    return split;
}

bool Inventory::TrySpendGold(int64_t amount)
{
    if (amount < 0 || amount > _gold) return false;
    _gold -= amount;
    return true;
}

bool Inventory::TryAddGold(int64_t amount)
{
    if (amount < 0 || amount > std::numeric_limits<int64_t>::max() - _gold)
        return false;
    _gold += amount;
    return true;
}

void Inventory::Clear()
{
    for (auto& slot : _slots) slot.reset();
}
```

### 변경 사항

- `ExtractItem`: 슬롯 전체 Item의 `unique_ptr` 소유권 이전.
- `SplitItem`: 일부 Count를 새 ConsumableItem으로 분리. 생성 성공 후에만 원본 Count 감소. Equipment는 MaxStackCount=1 전제로 분리 불가.
- Gold 임시 필드와 안전한 증감 함수 추가.
- `TryAddItem`의 새 슬롯 삽입 지점에 ItemInstanceID 발급 TODO 표시.
