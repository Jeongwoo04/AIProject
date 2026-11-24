#include "pch.h"
#include "DropObject.h"
#include "Item.h"

DropObject::DropObject(uint64_t id, std::unique_ptr<Item> item,
    const Vector3& position, uint64_t ownerPlayerID)
    : _dropID(id), _item(std::move(item)),
    _position(position), _ownerPlayerID(ownerPlayerID) {
}

std::unique_ptr<Item> DropObject::ExtractItem()
{
    return std::move(_item);
}

bool DropObject::RestoreItem(std::unique_ptr<Item> item)
{
    if (_item || !item) return false;
    _item = std::move(item);
    return true;
}