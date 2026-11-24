#pragma once
#include <cstdint>
#include <memory>
#include "Utils.h"

class Item;
struct Vector3;

class DropObject
{
public:
    DropObject(uint64_t id, std::unique_ptr<Item> item,
        const Vector3& position, uint64_t ownerPlayerID);

    uint64_t GetDropID() const { return _dropID; }
    const Vector3& GetPosition() const { return _position; }
    uint64_t GetOwnerPlayerID() const { return _ownerPlayerID; }
    Item* GetItem() const { return _item.get(); }

    std::unique_ptr<Item> ExtractItem();
    bool RestoreItem(std::unique_ptr<Item> item);

private:
    uint64_t _dropID = 0;
    std::unique_ptr<Item> _item;
    Vector3 _position;
    // 0이면 제한 없음. 몬스터 드랍은 처치자 PlayerID.
    uint64_t _ownerPlayerID = 0;
};