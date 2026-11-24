#include "pch.h"
#include "Inventory.h"

#include <algorithm>
#include <limits>

Item* Inventory::GetItem(int slotIndex)
{
    if (!IsValidSlot(slotIndex))
        return nullptr;
    return _slots[static_cast<std::size_t>(slotIndex)].get();
}

const Item* Inventory::GetItem(int slotIndex) const
{
    if (!IsValidSlot(slotIndex))
        return nullptr;
    return _slots[static_cast<std::size_t>(slotIndex)].get();
}

bool Inventory::IsValidSlot(int slotIndex) const
{
    return slotIndex >= 0 && static_cast<std::size_t>(slotIndex) < SlotCount;
}

int Inventory::FindEmptySlot() const
{
    for (std::size_t i = 0; i < SlotCount; ++i)
    {
        if (!_slots[i])
            return static_cast<int>(i);
    }
    return -1;
}

bool Inventory::IsSameDefinition(const Item& lhs, const Item& rhs) const
{
    // Inventory 기획: 중첩 후보는 ItemDefinitionID가 동일한 Instance.
    return lhs.GetItemDefinitionID() == rhs.GetItemDefinitionID();
}

bool Inventory::CanFit(const Item& item, int count) const
{
    if (count <= 0 || item.GetMaxStackCount() <= 0 || item.GetCount() <= 0)
        return false;

    int64_t capacity = 0;
    const int maxStack = item.GetMaxStackCount();

    // 기존 동일 Definition 스택의 여유 수량을 먼저 계산한다.
    for (const auto& slot : _slots)
    {
        if (slot && IsSameDefinition(*slot, item))
        {
            const int stackMax = std::min(maxStack, slot->GetMaxStackCount());
            capacity += std::max(0, stackMax - slot->GetCount());
        }
    }

    // 빈 슬롯은 Item의 MaxStackCount만큼 수용 가능하다.
    for (const auto& slot : _slots)
    {
        if (!slot)
            capacity += maxStack;
    }

    return capacity >= static_cast<int64_t>(count);
}

Inventory::AddResult Inventory::TryAddItem(
    std::unique_ptr<Item>& item,
    AddPolicy policy)
{
    AddResult result{};

    if (!item)
        return result;

    const int incomingCount = item->GetCount();
    const int maxStack = item->GetMaxStackCount();

    if (incomingCount <= 0 || maxStack <= 0 || incomingCount > maxStack)
        return result;

    // AllOrNothing은 적재 실패 시 슬롯 변경을 허용하지 않는다.
    if (policy == AddPolicy::AllOrNothing &&
        !CanFit(*item, incomingCount))
    {
        result.RemainingCount = incomingCount;
        return result;
    }

    int remaining = incomingCount;

    // 1. 기존 동일 아이템 슬롯부터 채운다.
    for (int i = 0; i < static_cast<int>(_slots.size()); i++)
    {
        auto& slot = _slots[i];

        if (!slot || !IsSameDefinition(*slot, *item))
            continue;

        const int stackMax = std::min(maxStack, slot->GetMaxStackCount());
        const int room = stackMax - slot->GetCount();

        if (room <= 0)
            continue;

        const int transfer = std::min(room, remaining);

        if (transfer <= 0)
            continue;

        const int newCount = slot->GetCount() + transfer;

        if (!slot->TrySetCount(newCount))
        {
            result.RemainingCount = incomingCount;
            return result;
        }

        result.SlotDeltas.push_back({ i, item->GetItemDefinitionID(), transfer });

        remaining -= transfer;
        result.AddedCount += transfer;

        if (remaining == 0)
            break;
    }

    // 2. 남은 수량이 있다면 빈 슬롯에 배치한다.
    if (remaining > 0)
    {
        for (int i = 0; i < static_cast<int>(_slots.size()); ++i)
        {
            auto& slot = _slots[i];

            if (slot)
                continue;

            const int transfer = std::min(maxStack, remaining);

            if (transfer <= 0)
                continue;

            if (!item->TrySetCount(transfer))
            {
                result.RemainingCount = incomingCount;
                return result;
            }

            int32 itemId = item->GetItemDefinitionID();
            _slots[i] = std::move(item);

            result.SlotDeltas.push_back({ i, itemId, transfer });

            result.AddedCount += transfer;
            remaining -= transfer;

            if (remaining == 0)
                break;

            // 다음 빈 슬롯에 넣기 위해 Item Instance를 다시 생성해야 한다.
            // 현재 Item Instance는 위에서 슬롯으로 이동했으므로
            // 이후 적재에 사용할 새로운 Instance가 필요하다.
        }
    }

    // 3. 최종 결과를 정리한다.
    result.RemainingCount = remaining;

    result.Success = (remaining == 0) ||
        (policy == AddPolicy::Partial && result.AddedCount > 0);

    // 완전히 적재했다면 전달받은 Item Instance는 소멸한다.
    if (remaining == 0)
        item.reset();
    else if (item)
        item->TrySetCount(remaining);

    return result;
}

bool Inventory::MoveItem(int fromSlot, int toSlot)
{
    if (!IsValidSlot(fromSlot) || !IsValidSlot(toSlot) || fromSlot == toSlot)
        return false;

    auto& source = _slots[static_cast<std::size_t>(fromSlot)];
    auto& destination = _slots[static_cast<std::size_t>(toSlot)];
    if (!source)
        return false;

    // 빈 슬롯이면 Instance 소유권만 이전한다.
    if (!destination)
    {
        destination = std::move(source);
        return true;
    }

    if (IsSameDefinition(*source, *destination))
    {
        // 같은 Definition이면 destination 최대 스택까지 합친다.
        const int stackMax = std::min(
            source->GetMaxStackCount(), destination->GetMaxStackCount());
        const int room = stackMax - destination->GetCount();
        if (room <= 0)
            return false;

        const int transfer = std::min(room, source->GetCount());
        if (transfer <= 0)
            return false;

        const int destinationNewCount = destination->GetCount() + transfer;
        const int sourceRemaining = source->GetCount() - transfer;

        // 양쪽 Count를 먼저 검증하고, 성공 가능한 값만 반영한다.
        if (!destination->TrySetCount(destinationNewCount))
            return false;

        if (sourceRemaining == 0)
            source.reset();
        else if (!source->TrySetCount(sourceRemaining))
        {
            // 이 지점은 사전 계산상 발생하지 않아야 함. 방어적 원복.
            destination->TrySetCount(destination->GetCount() - transfer);
            return false;
        }
        return true;
    }

    // 서로 다른 Definition이면 두 슬롯의 Instance를 교환한다.
    std::swap(source, destination);
    return true;
}

bool Inventory::RemoveCount(int slotIndex, int count, int& outRemovedCount)
{
    outRemovedCount = 0;
    if (!IsValidSlot(slotIndex) || count <= 0)
        return false;

    auto& slot = _slots[static_cast<std::size_t>(slotIndex)];
    if (!slot || count > slot->GetCount())
        return false;

    const int current = slot->GetCount();
    if (count == current)
    {
        slot.reset();
        outRemovedCount = count;
        return true;
    }

    if (!slot->TrySetCount(current - count))
        return false;

    outRemovedCount = count;
    return true;
}

bool Inventory::UseItem(int slotIndex, Player& target)
{
    if (!IsValidSlot(slotIndex))
        return false;

    auto& slot = _slots[static_cast<std::size_t>(slotIndex)];
    if (!slot)
        return false;

    const int count = slot->GetCount();
    if (count <= 0)
        return false; // Item 생성자 계약 위반/손상 상태 방어

    // Item Legacy: Use(Player&)가 성공 여부를 반환한다. 실패 시 Inventory Count 유지.
    if (!slot->Use(target))
        return false;

    if (count == 1)
    {
        // 성공한 사용의 수량 1 소진: Inventory가 Instance를 슬롯에서 제거한다.
        slot.reset();
        return true;
    }

    // 성공한 사용의 수량 1 차감. Item Legacy에 추가한 TrySetCount 사용.
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
    // Player 제거 시 Inventory 소유 Item Instance를 전부 정리한다.
    for (auto& slot : _slots)
        slot.reset();
}