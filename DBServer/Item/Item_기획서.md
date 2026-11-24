# Item 시스템 기획서

## 1. 목적
게임 내 아이템의 종류별 기본 정의와 공통 동작 구조를 마련한다. 아이템 정의 데이터는 JSON으로 관리하고, GameServer 시작 시 메모리에 적재하여 `ItemDefinitionID`로 조회한다.

## 2. 구현 범위
- `Item`을 공통 추상 기반 클래스로 구성한다.
- 소비형 아이템(`Consumable`)과 장비형 아이템(`Equipment`)을 파생 클래스로 구현한다.
- 아이템 종류별 공통 인터페이스를 제공하고, `Item::Use()`를 통해 사용 또는 착용을 시도한다.
- JSON 기반 Item Definition을 GameServer 시작 시 로딩하고 메모리에서 조회한다.
- 실제 소유 아이템은 Item Definition을 참조하는 Item Instance로 구분한다.

## 3. 데이터 개념 및 정책

### 3.1 Item Definition과 Item Instance
- **Item Definition**: 아이템 종류의 공통·기본 정보를 정의하는 데이터다. JSON 파일에 저장하며 서버 실행 중 변경하지 않는다.
- **Item Instance**: 유저가 실제로 소유하는 개별 아이템이다. 참조하는 `ItemDefinitionID`, 개별 식별자인 `ItemInstanceID`, 현재 수량인 `Count`를 가진다.
- 하나의 Item Definition을 기반으로 여러 Item Instance가 생성될 수 있다.
- Item Instance의 소유 및 수량 관리 방식은 Inventory 시스템에서 정의한다.

### 3.2 식별자와 수량
- `ItemDefinitionID`는 아이템 종류를 구분하며 중복될 수 없다.
- `ItemInstanceID`는 실제 소유 아이템 인스턴스를 구분한다.
- `Count`는 Item Instance의 현재 수량이다.
- 최대 중첩 수량은 `MaxStackCount`로 관리한다. 별도의 `Stackable` 필드는 사용하지 않는다.
- `MaxStackCount`의 기본값은 1이며, 아이템 정의에 따라 별도 값을 지정한다.

### 3.3 아이템 종류별 기본 정책
- 소비형 아이템의 기본 `MaxStackCount`는 99다.
- 장비형 아이템의 `MaxStackCount`는 1이다.
- 장비형 아이템은 `RequiredLevel`을 통해 착용 요구 레벨을 표현한다.
- `HP`, `MP`, `Attack`은 아이템이 제공하는 기본 능력치 증가량이다.
- 능력치 적용과 버프·기타 효과의 계산 공식은 Item 시스템이 아니라 Character 시스템에서 처리한다.

## 4. 시스템 책임

| 구성 요소 | 책임 |
|---|---|
| Item Definition | 아이템 종류별 기본 데이터 정의 |
| Item Instance | 실제 소유 아이템의 식별 및 수량 데이터 보유 |
| Item | 공통 인터페이스와 공통 데이터 제공 |
| ConsumableItem | 소비형 아이템의 사용 시도 |
| EquipmentItem | 장비형 아이템의 착용 시도 |
| DataManager | JSON에서 읽은 Item Definition을 메모리에 보관하고 ID로 조회 |
| Inventory | Item Instance의 소유, 수량 변경, 제거 및 장착과의 상호작용 관리 |
| Character | 능력치 적용 및 버프·효과 계산 |

`Item::Use()`는 사용 또는 착용을 시도하고 성공 여부를 반환한다. 사용 성공 후 수량 감소, 수량이 0이 된 아이템의 제거, Inventory 내 배치 변경 등의 후속 처리는 Inventory 시스템의 책임이다.

## 5. 기존 GameServer 연동 방향
- Player는 Inventory를 소유한다.
- Inventory는 Item Instance를 관리한다.
- Item Instance는 Item Definition을 참조한다.
- Item Definition은 기존 GameServer의 `DataManager` 로딩 흐름을 활용해 적재한다.
- Player 및 Inventory의 소유권, 포인터 사용 방식, Disconnect/Remove 시 저장 및 백업 정책은 관련 시스템의 기획 범위에서 정의한다.

## 6. 제외 범위
- 몬스터 아이템 드랍 및 확률 처리
- `ItemBox` 생성, 드랍 및 활성화
- 클라이언트 이미지 리소스 매핑용 JSON 관리
- Inventory 내 아이템 배치 및 보관 규칙
- 아이템 수량 관리, Inventory 내 아이템 제거 및 장착 상호작용
- 아이템 능력치 적용 및 버프·효과 계산 공식

## 7. 완료 기준
- JSON 파일로 Item Definition을 정의할 수 있다.
- GameServer 시작 시 JSON 데이터가 파싱되어 `ItemDict<int, ItemInfo>`에 적재된다.
- 유효한 `ItemDefinitionID`로 기본 아이템 정보를 조회할 수 있다.
- `Item` 추상 기반 클래스와 소비형·장비형 파생 클래스가 공통 인터페이스를 제공한다.
- `Item::Use()`가 아이템 사용 또는 착용을 시도하고 성공/실패를 반환하는 구조를 갖춘다.
