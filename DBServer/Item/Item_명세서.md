# Item 시스템 명세서

## 1. 데이터 명세

### 1.1 Item Definition
Item Definition은 JSON 파일에 저장되는 아이템 종류별 정적 데이터다. GameServer 시작 시 로딩하며 런타임 중 변경하지 않는다.

| 필드 | 자료형(개념) | 설명 | 적용 대상 |
|---|---|---|---|
| `ItemDefinitionID` | 정수 | 아이템 종류의 고유 ID | 공통 |
| `ItemType` | 문자열/열거형 | `Consumable` 또는 `Equipment` | 공통 |
| `MaxStackCount` | 정수 | 최대 중첩 수량 | 공통 |
| `HP` | 정수형 선택 값 | 기본 HP 능력치 증가량 | 소비형, 장비형 |
| `MP` | 정수형 선택 값 | 기본 MP 능력치 증가량 | 소비형, 장비형 |
| `Attack` | 정수형 선택 값 | 기본 공격력 증가량 | 장비형 |
| `RequiredLevel` | 정수형 선택 값 | 장비 착용 요구 레벨 | 장비형 |

- `ItemDefinitionID`는 중복될 수 없다.
- `MaxStackCount`는 기본값 1이다.
- 소비형 아이템의 기본 `MaxStackCount`는 99, 장비형 아이템은 1이다.
- `HP`, `MP`, `Attack`, `RequiredLevel`은 아이템 종류에 따라 존재하지 않을 수 있다.
- `Stackable` 필드는 사용하지 않는다.

### 1.2 JSON 예시

장비형:
```json
{
  "ItemDefinitionID": 1001,
  "ItemType": "Equipment",
  "MaxStackCount": 1,
  "HP": 50,
  "MP": 20,
  "Attack": 10,
  "RequiredLevel": 10
}
```

소비형:
```json
{
  "ItemDefinitionID": 2001,
  "ItemType": "Consumable",
  "MaxStackCount": 99,
  "HP": 50,
  "MP": 20
}
```

### 1.3 Item Instance

| 필드 | 자료형(개념) | 설명 |
|---|---|---|
| `ItemInstanceID` | 64비트 정수 | 개별 아이템 인스턴스 식별자 |
| `ItemDefinitionID` | 정수 | 참조하는 Item Definition ID |
| `Count` | 정수 | 현재 수량 |

`ItemInstanceID` 생성·관리, Item Instance의 소유 및 수량 변경은 Inventory 시스템에서 정의한다.

## 2. C++ 자료구조 및 클래스 명세

### 2.1 ItemType
```cpp
enum class ItemType
{
    Consumable,
    Equipment
};
```

### 2.2 ItemInfo
```cpp
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
- JSON에서 읽은 Item Definition을 보관한다.
- `HP`, `MP`, `Attack`, `RequiredLevel`은 선택 필드로 표현한다.
- `Type`은 JSON의 `ItemType`에 대응한다.

### 2.3 Item 추상 기반 클래스
```cpp
class Item
{
public:
    Item(uint64_t itemInstanceID, const ItemInfo& itemInfo, int count = 1);
    virtual ~Item() = default;

    virtual bool Use(Character& target) = 0;

    uint64_t GetItemInstanceID() const;
    int GetItemDefinitionID() const;
    int GetCount() const;
    int GetMaxStackCount() const;
    const ItemInfo& GetItemInfo() const;

protected:
    uint64_t _itemInstanceID = 0;
    ItemInfo _itemInfo;
    int _count = 1;
};
```
- `Item`은 직접 인스턴스화할 수 없는 추상 클래스다.
- Item Instance ID, Item Definition 데이터, 현재 수량을 보유한다.
- `Use()`는 파생 클래스가 구현한다.
- Getter를 통해 식별자, 수량, 최대 중첩 수량, 기본 정의 정보를 조회한다.

### 2.4 파생 클래스
```cpp
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
- `ConsumableItem::Use()`는 Character 시스템에 소비형 아이템 사용을 요청하고 성공 여부를 반환하는 역할이다.
- `EquipmentItem::Use()`는 착용 조건을 확인하고 Character/Equipment 시스템에 착용을 요청한 뒤 성공 여부를 반환하는 역할이다.
- 현재 Legacy Code에서는 두 함수 모두 TODO 상태이며 `false`를 반환한다. 실제 효과·착용 동작은 구현되어 있지 않다.

## 3. 생성 및 유효성 검사 명세

### 3.1 Item 생성자
생성자는 다음 조건을 검사한다.

| 검사 대상 | 유효 조건 | 실패 처리 |
|---|---|---|
| `ItemDefinitionID` | 0보다 커야 함 | `std::invalid_argument` 발생 |
| `MaxStackCount` | 0보다 커야 함 | `std::invalid_argument` 발생 |
| `Count` | 0보다 크고 `MaxStackCount` 이하여야 함 | `std::invalid_argument` 발생 |

현재 코드:
```cpp
if (_itemInfo.ItemDefinitionID <= 0)
    throw std::invalid_argument("Invalid ItemDefinitionID");

if (_itemInfo.MaxStackCount <= 0)
    throw std::invalid_argument("Invalid MaxStackCount");

if (_count <= 0 || _count > _itemInfo.MaxStackCount)
    throw std::invalid_argument("Invalid Item Count");
```

## 4. 데이터 로딩 및 조회 명세

### 4.1 로딩 흐름
1. GameServer 시작 시 기존 `DataManager`의 데이터 로딩 흐름을 호출한다.
2. JSON Item Definition 데이터를 파싱한다.
3. 각 데이터를 `ItemInfo`로 구성한다.
4. `ItemDefinitionID`를 Key로 `ItemDict<int, ItemInfo>`에 적재한다.

### 4.2 조회
- `ItemDefinitionID`를 Key로 `DataManager`의 `ItemDict<int, ItemInfo>`에서 조회한다.
- Legacy Code의 `CreateItem()`은 `DataManager::Instance().GetItemInfo(itemDefinitionID)`를 호출한다.
- 조회 결과가 `nullptr`이면 아이템 생성을 중단하고 `nullptr`를 반환한다.

## 5. Item 생성 팩토리 명세

함수 형태:
```cpp
std::unique_ptr<Item> CreateItem(
    uint64_t itemInstanceID,
    int itemDefinitionID,
    int count
);
```

처리 순서:
1. `itemDefinitionID`로 ItemInfo를 조회한다.
2. 조회 결과가 없으면 `nullptr`를 반환한다.
3. `ItemType`에 따라 파생 클래스를 생성한다.
   - `Consumable` → `ConsumableItem`
   - `Equipment` → `EquipmentItem`
4. 정의되지 않은 타입이면 `nullptr`를 반환한다.
5. 생성된 파생 객체를 `std::unique_ptr<Item>`로 반환한다.

생성자에서 수량 및 정의 ID 등의 유효성 검사를 수행하므로, 전달된 값이 유효하지 않으면 생성자에서 예외가 발생할 수 있다.

## 6. 책임 경계 및 연동
- Item은 기본 정의와 개별 인스턴스 데이터, 종류별 사용 인터페이스를 제공한다.
- Inventory는 Item Instance의 소유, 수량 변경, 제거 및 장착 상호작용을 담당한다.
- Character 시스템은 HP/MP/Attack 적용과 버프·효과 계산을 담당한다.
- Item Definition은 서버 시작 시 로딩되는 불변 데이터다.
- `ItemInstanceID` 생성 및 관리 방식은 Inventory 명세에 따른다.

## 7. 미구현 및 확인 필요 사항
- `ConsumableItem::Use()`의 실제 효과 적용 및 성공 조건
- `EquipmentItem::Use()`의 `RequiredLevel` 검사와 실제 장착 처리
- Inventory와 `Use()`의 성공 결과 연동 및 수량 감소 처리
- 실제 GameServer의 `DataManager`, JSON 파서 및 헤더/네임스페이스와의 통합 확인
- JSON 필드 누락·중복 ID·잘못된 ItemType 등에 대한 로딩 단계의 상세 검증 정책

위 항목은 현재 제공된 Legacy Code에서 완성되었거나 통합 검증된 것으로 확인되지 않는다.
