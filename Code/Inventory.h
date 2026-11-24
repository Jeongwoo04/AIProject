#pragma once

#include "Item.h"

#include <array>
#include <cstddef>
#include <memory>

class Player;

class Inventory final
{
public:
    static constexpr std::size_t SlotCount = 40;

    enum class AddPolicy
    {
        AllOrNothing, // 보상/구매/교환: 전체 적재 가능해야 성공
        Partial       // 월드 루팅: 가능한 수량만 적재
    };

    struct SlotAddDelta
    {
        int32 Slot = -1;
        int32 ItemId = 0;
        int32 AddedCount = 0;
    };

    struct AddResult
    {
        bool Success = false;
        int AddedCount = 0;
        int RemainingCount = 0;

        vector<SlotAddDelta> SlotDeltas;
    };

    Inventory() = default;
    Inventory(const Inventory&) = delete;
    Inventory& operator=(const Inventory&) = delete;

    Item* GetItem(int slotIndex);
    const Item* GetItem(int slotIndex) const;
    bool IsValidSlot(int slotIndex) const;
    int FindEmptySlot() const;

    // item은 호출자가 소유한다. 성공/부분 성공분만 Inventory로 이동한다.
    // Partial에서 일부만 적재되면 item은 남은 Count로 호출자에게 유지된다.
    AddResult TryAddItem(std::unique_ptr<Item>& item, AddPolicy policy);

    // 정확한 적재량을 미리 확인한다. count는 양수여야 한다.
    bool CanFit(const Item& item, int count) const;

    // 슬롯 이동: 빈 슬롯 이동, 동일 Definition 중첩, 서로 다른 Item 교환.
    bool MoveItem(int fromSlot, int toSlot);

    // 서버가 사전 검증한 제거/판매/교환 등에 사용할 슬롯 수량 제거.
    // 제거한 수량은 outRemovedCount로 반환. 0 이하 수량은 거부한다.
    bool RemoveCount(int slotIndex, int count, int& outRemovedCount);

    // Item::Use 성공 후에만 수량을 차감한다.
    bool UseItem(int slotIndex, Player& target);

    // Player 제거 시 Inventory가 소유한 Item Instance를 정리한다.
    void Clear();

    std::unique_ptr<Item> ExtractItem(int slotIndex);
    std::unique_ptr<Item> SplitItem(int slotIndex, int count);

    uint64_t GetGold() { return _gold; }
    bool TrySpendGold(int64_t amount);
    bool TryAddGold(int64_t amount);

    uint64_t GetOwnerID() { return _ownerID; }
    void SetOwnerID(uint64_t owner) { _ownerID = owner; }

private:
    bool IsSameDefinition(const Item& lhs, const Item& rhs) const;

    std::array<std::unique_ptr<Item>, SlotCount> _slots{};
    uint64_t _ownerID = 0;
    uint64_t _gold = 0;
};