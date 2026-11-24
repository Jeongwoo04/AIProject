#pragma once
#include <cstdint>

class ItemInstanceIDGenerator
{
public:
    uint64_t Generate();

private:
    atomic<uint64_t> _nextID = 1;
};