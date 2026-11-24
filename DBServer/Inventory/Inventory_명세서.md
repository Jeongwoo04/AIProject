# Inventory 시스템 명세서

## 1. 상수 및 자료구조

### 1.1 Inventory

```cpp
class Inventory final
{
public:
    static constexpr std::size_t SlotCount = 40;

    enum class AddPolicy
    {
        AllOrNothing,
        Partial
    };

    struct AddResult
    {
        bool Success = false;
        int AddedCount = 0;
        int RemainingCount = 0;
    };

private:
    std::array<std::unique_ptr<Item>, SlotCount> _slots{};
};
```

- 슬롯은 40개 고정 배열이다.
- 각 슬롯은 `std::unique_ptr<Item>`로 Item Instance의 소유권을 관리한다.
- `AllOrNothing`은 전체 적재 가능할 때만 반영한다.
- `Partial`은 가능한 수량만 적재하고 잔여 Instance를 호출자에게 남긴다.
- 복사 생성자와 복사 대입 연산자는 삭제한다.

## 2. 공개 인터페이스

| 함수 | 역할 |
|---|---|
| `GetItem(int slotIndex)` | 슬롯의 Item 포인터 조회. 잘못된 인덱스면 `nullptr` |
| `IsValidSlot(int slotIndex)` | 슬롯 인덱스 범위 검사 |
| `FindEmptySlot()` | 첫 번째 빈 슬롯 반환. 없으면 `-1` |
| `CanFit(const Item&, int count)` | 지정 수량의 사전 적재 가능 여부 확인 |
| `TryAddItem(std::unique_ptr<Item>&, AddPolicy)` | 정책에 따라 Item Instance 적재 |
| `MoveItem(int fromSlot, int toSlot)` | 이동, 동일 Definition 중첩, 서로 다른 Item 교환 |
| `RemoveCount(int slotIndex, int count, int& outRemovedCount)` | 지정 슬롯에서 수량 제거 |
| `UseItem(int slotIndex, Player& target)` | Item 사용 성공 시 수량 1 감소 |
| `Clear()` | 모든 슬롯의 Item Instance 정리 |

## 3. 슬롯 조회 명세

- `IsValidSlot()`은 `0 <= slotIndex < SlotCount`를 만족할 때 `true`를 반환한다.
- `GetItem()`은 인덱스가 유효하지 않으면 `nullptr`, 유효하면 해당 슬롯의 Item 포인터를 반환한다.
- `FindEmptySlot()`은 인덱스 0부터 순회해 첫 빈 슬롯을 반환하며, 빈 슬롯이 없으면 `-1`을 반환한다.

## 4. 적재 가능 여부 명세

`CanFit(item, count)`:
1. `count`, Item의 `MaxStackCount`, 현재 `Count`가 양수인지 검사한다.
2. 동일 `ItemDefinitionID`를 가진 기존 슬롯의 남은 중첩 공간을 계산한다.
3. 빈 슬롯마다 대상 Item의 `MaxStackCount`만큼 수용 가능 공간을 더한다.
4. 총 수용량이 요청 수량 이상이면 `true`, 아니면 `false`를 반환한다.
5. 누적 수용량은 `int64_t`로 계산한다.

기존 슬롯의 중첩 한도는 대상 Item과 기존 슬롯 Item의 `MaxStackCount` 중 작은 값으로 계산한다.

## 5. 아이템 적재 명세

`TryAddItem(item, policy)` 처리:
1. 입력 `unique_ptr`가 비어 있으면 실패한다.
2. 입력 Item의 `Count`, `MaxStackCount`가 양수이고 `Count <= MaxStackCount`인지 확인한다.
3. `AllOrNothing`이면 `CanFit()`으로 전체 적재 가능 여부를 먼저 검사한다. 공간이 부족하면 슬롯을 변경하지 않고 실패 결과를 반환한다.
4. 기존 동일 Definition 슬롯을 순회하며 여유 수량부터 채운다.
5. 남은 수량이 있으면 첫 빈 슬롯에 입력 Item Instance를 배치한다.
6. 전체 적재 완료 시 입력 포인터를 비우고 Inventory가 소유권을 갖는다.
7. `Partial`에서 일부만 적재하면 잔여 Count를 입력 Item에 유지해 호출자가 계속 소유한다.

반환값:
- `Success`: 전체 적재 완료 또는 `Partial` 정책에서 1개 이상 적재 성공
- `AddedCount`: 실제 적재한 수량
- `RemainingCount`: 적재되지 않은 수량

## 6. 슬롯 이동 명세

`MoveItem(fromSlot, toSlot)`:
1. 두 인덱스가 유효하고 서로 달라야 한다.
2. 출발 슬롯에 Item이 존재해야 한다.
3. 도착 슬롯이 비어 있으면 `unique_ptr` 소유권을 이동한다.
4. 두 Item의 `ItemDefinitionID`가 같으면 도착 슬롯의 최대 중첩 한도까지 합친다.
5. Definition이 다르면 두 슬롯의 Item Instance를 교환한다.
6. 중첩 가능한 수량이 없거나 사전 검증에 실패하면 `false`를 반환하고 기존 상태를 유지한다.
7. 중첩 결과 출발 수량이 0이면 출발 Instance를 제거한다. 일부가 남으면 출발 Instance를 유지한다.

## 7. 수량 제거 명세

`RemoveCount(slotIndex, count, outRemovedCount)`:
- 호출 시 `outRemovedCount`를 0으로 초기화한다.
- 슬롯 인덱스가 유효하고 `count > 0`이어야 한다.
- 슬롯에 Item이 존재하고 제거 수량이 현재 Count를 초과하지 않아야 한다.
- 전체 수량 제거면 슬롯의 Item Instance를 제거한다.
- 일부 수량 제거면 `TrySetCount(current - count)`를 호출한다.
- 성공하면 제거 수량을 `outRemovedCount`에 기록한다.

## 8. 아이템 사용 명세

`UseItem(slotIndex, target)`:
1. 슬롯 인덱스가 유효한지 확인한다.
2. 슬롯에 Item Instance가 존재하는지 확인한다.
3. `Item::Use(target)`를 호출한다.
4. 사용이 실패하면 `false`를 반환하고 Inventory Count를 변경하지 않는다.
5. 사용 성공 후 Count가 1이면 Item Instance를 슬롯에서 제거한다.
6. Count가 2 이상이면 `TrySetCount(count - 1)`로 1개 차감한다.

## 9. 정리 명세

`Clear()`는 모든 슬롯의 `unique_ptr`를 reset하여 Inventory가 소유한 Item Instance를 정리한다. Player 제거 시 호출하는 것을 전제로 한다.

## 10. 코드에서 확인되는 연동 조건 및 미완성 사항

- `Item`에는 `TrySetCount(int)` 함수가 필요하다. 현재 Inventory Legacy Code가 적재·중첩·부분 제거·사용 후 수량 변경에 이를 호출한다.
- `Item::Use()`는 Inventory 코드에서 `Player&`를 전달받는 형태로 사용한다. Item Legacy Code의 실제 선언과 시그니처가 일치해야 한다.
- 월드 드랍 생성, 상점 구매, 교환, 보상 및 재화 차감은 Inventory 외부의 호출 흐름에서 조건을 검증하고 결과를 연계해야 한다.
- 장착/해제의 실제 Equipment 소유권 이전 로직은 현재 Inventory Legacy Code에 구현되어 있지 않다.
- Player 생성 시 Inventory 초기화, Player 제거 시 `Clear()` 호출 및 DB 저장 절차의 GameServer 통합 여부는 별도 확인이 필요하다.
- 동시성 직렬화는 Inventory 클래스 자체에서 제공하지 않는다. GameServer의 Player 처리 흐름에서 보장해야 한다.
