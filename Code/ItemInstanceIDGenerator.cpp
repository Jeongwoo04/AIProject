#include "pch.h"
#include "ItemInstanceIDGenerator.h"

uint64_t ItemInstanceIDGenerator::Generate()
{
    // TODO: overflow 및 동시 접근 정책. DBServer/서버 재시작 이후 영속 ID 정책은 추후 확정.
    return _nextID.fetch_add(1);
}