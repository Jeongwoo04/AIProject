# Item 시스템 검증 보고서

## 1. 검증 범위
본 보고서는 Item 기획서와 Legacy Code를 바탕으로 이전 작업에서 실제로 검토·확정한 설계 및 코드 수준의 확인 사항만 기록한다. 별도로 실행하지 않은 빌드·단위 테스트·GameServer 통합 테스트는 검증 완료로 기재하지 않는다.

## 2. 확인한 사항

### 2.1 Item 데이터 및 정책 검토

| 확인 항목 | 확인 내용 | 결과 |
|---|---|---|
| 아이템 데이터 구분 | Item Definition과 Item Instance를 구분 | 확정 |
| 수량 필드 | `Stackable` 대신 `MaxStackCount`를 사용하고 현재 수량은 `Count`로 관리 | 확정 |
| 아이템 분류 | `Consumable`, `Equipment`를 구현 대상으로 정의 | 확정 |
| 능력치 필드 | `HP`, `MP`, `Attack`은 기본 능력치 증가량으로 정의 | 확정 |
| 장비 레벨 조건 | 장비 착용 요구 레벨 필드를 `RequiredLevel`로 정의 | 확정 |
| 책임 분리 | 능력치·버프 효과 계산은 Character, 수량·제거·장착 상호작용은 Inventory 책임으로 구분 | 확정 |

### 2.2 Legacy Code 수준 검토

| 확인 항목 | 코드에서 확인한 내용 | 결과 |
|---|---|---|
| Item 생성자 검증 | `ItemDefinitionID > 0`, `MaxStackCount > 0`, `0 < Count <= MaxStackCount` 조건 검사 및 위반 시 `std::invalid_argument` 발생 | 코드 반영 확인 |
| 선택 필드 | `HP`, `MP`, `Attack`, `RequiredLevel`을 `std::optional<int>`로 표현 | 코드 반영 확인 |
| Item 추상화 | `Item::Use(Character&)`를 순수 가상 함수로 정의 | 코드 반영 확인 |
| 타입별 생성 | `CreateItem()`에서 `ItemType`에 따라 `ConsumableItem` 또는 `EquipmentItem` 생성 | 코드 반영 확인 |
| 정의 데이터 조회 실패 | `GetItemInfo()` 결과가 `nullptr`이면 팩토리가 `nullptr` 반환 | 코드 반영 확인 |
| 알 수 없는 타입 | `switch`의 기본 분기에서 `nullptr` 반환 | 코드 반영 확인 |

## 3. 미검증 및 미구현 항목
- Item 코드 자체의 실제 빌드 성공 여부는 확인하지 않았다.
- Item 생성자 유효성 검사에 대한 별도 실행 테스트 결과는 없다.
- `CreateItem()`의 정상 생성, 정의 ID 조회 실패, 알 수 없는 타입 분기를 실행한 테스트 결과는 없다.
- JSON 파일 파싱 및 `DataManager` 적재·조회가 실제 GameServer에서 동작하는지 통합 확인하지 않았다.
- `ConsumableItem::Use()`와 `EquipmentItem::Use()`는 Legacy Code에서 TODO 상태이며 모두 `false`를 반환한다.
- 실제 소비 효과, 장비 착용, `RequiredLevel` 검사 및 Inventory와의 후속 연동은 검증하지 않았다.

## 4. 종합 결과
현재 확인 가능한 결과는 **기획 정책의 검토·확정과 Legacy Code에 반영된 구조 및 조건문의 코드 검토**까지다. Item 코드의 빌드·실행 테스트를 완료한 것은 아니다.

특히 `Use()`의 실제 동작과 GameServer의 `DataManager`/JSON 로딩 흐름에 대한 통합 검증은 남아 있다. 따라서 본 보고서는 실행 테스트 통과 보고서가 아니라, 현재까지 확인한 설계 및 코드 반영 사항과 검증 범위를 구분해 기록한 문서다.
