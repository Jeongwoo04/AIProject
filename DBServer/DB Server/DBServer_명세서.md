# DBServer 명세서

## 1. 개요

DBServer는 GameServer의 데이터 처리 요청을 독립적으로 수행하기 위해 구성한 C++ IOCP 기반 DB 서버이다.

GameServer로부터 전달된 요청을 DB 작업 단위로 구성하고, 작업 큐와 Worker를 통해 비동기 처리한 뒤 결과를 다시 GameServer로 전달한다. 데이터 접근은 DBConnection과 DBBind를 통해 관리하며, MSSQL Stored Procedure를 중심으로 데이터 조회 및 변경 작업을 수행한다.

---

## 2. 시스템 구성

### 2.1 구성 요소 및 역할

| 구성 요소 | 역할 |
|---|---|
| DBServer | DB 요청 처리 서버 및 전체 처리 흐름 관리 |
| DBSession | 연결된 클라이언트와의 네트워크 세션 관리 |
| DBJob | 개별 DB 처리 작업 단위 |
| DBJobQueue | DB 작업을 등록하고 Worker가 가져갈 수 있도록 관리 |
| DBWorker | 큐에 등록된 DB 작업을 비동기 실행 |
| DBProcessor | 요청 유형에 따른 실제 DB 처리 로직 수행 |
| DBConnection | DB 연결 및 쿼리 실행 관리 |
| DBConnectionPool | Pool에서 꺼내온 커넥션을 자동으로 DBJob에 바인딩 |
| DBBind | 쿼리 파라미터 및 결과 컬럼 바인딩 |
| MSSQL | 게임 데이터 저장 및 조회 |

### 2.2 처리 구조

```text
GameServer
    │
    ▼
DBServer Network / Session
    │
    ▼
Request Handling
    │
    ▼
DBJob
    │
    ▼
DBJobQueue
    │
    ▼
DBWorker
    │
    ▼
DBProcessor
    │
    ▼
DBConnection / DBBind
    │
    ▼
MSSQL Stored Procedure
    │
    ▼
Result / Response
    │
    ▼
GameServer
```

---

## 3. 비동기 DB 작업 처리

### 3.1 DBJob

DBJob은 DBServer에서 처리할 개별 작업을 표현한다.

네트워크 요청으로 전달된 데이터를 DB 처리 단계에서 실행할 작업으로 구성하고, 작업 큐를 통해 실행 흐름에 전달한다. 이를 통해 요청 처리와 실제 DB 작업을 분리한다.

### 3.2 DBJobQueue

DBJobQueue는 생성된 DBJob을 등록하고, Worker가 처리할 작업을 가져갈 수 있도록 관리한다.

DB 작업을 큐 기반으로 전달하여 요청 처리 단계와 DB 실행 단계가 직접 연결되지 않도록 구성한다.

### 3.3 DBWorker

DBWorker는 DBJobQueue에 등록된 작업을 가져와 DB 처리를 수행하는 실행 주체이다.

Worker는 작업을 전달받아 DBProcessor의 처리 로직으로 연결하며, DB 작업이 네트워크 처리 흐름과 분리된 실행 단계에서 수행되도록 한다.

### 3.4 DBProcessor

DBProcessor는 요청 유형에 따른 실제 DB 처리 로직을 담당한다.

각 요청에 필요한 파라미터를 구성하고 Stored Procedure를 실행하며, 처리 결과에 따라 응답 데이터를 생성하고 전달하는 흐름을 담당한다.

---

## 4. DB 접근 구조

### 4.1 DBConnection

DBConnection은 MSSQL과의 연결 및 DB 쿼리 실행을 관리한다.

DB 처리 로직이 직접 연결 관리 세부 사항에 의존하지 않도록 DB 접근 기능을 별도 구성 요소로 분리한다.

주요 책임:

- DB 연결 관리
- 쿼리 실행
- 트랜잭션 시작
- Commit 및 Rollback
- DB 작업 수행에 필요한 실행 환경 제공

### 4.2 DBBind

DBBind는 쿼리 실행에 필요한 파라미터와 조회 결과 컬럼을 바인딩하는 기능을 제공한다.

이를 통해 Stored Procedure 호출 시 입력값과 결과 데이터를 지정된 타입 및 변수에 연결하고, 각 DB 처리 로직에서 반복되는 바인딩 작업을 공통 구조로 관리한다.

주요 사용 흐름:

1. DBConnection을 이용해 DBBind 생성
2. 쿼리 또는 Stored Procedure 지정
3. 입력 파라미터 바인딩
4. 결과 컬럼 바인딩
5. 실행 및 결과 데이터 처리

---

## 5. 데이터 처리 방식

### 5.1 Stored Procedure 기반 처리

DBServer의 데이터 처리 로직은 MSSQL Stored Procedure를 중심으로 구성한다.

서버에서 요청에 필요한 입력값을 전달하고, DB에서 데이터 조회 및 변경을 수행한 뒤 처리 결과를 반환한다.

이를 통해 데이터 처리 쿼리와 서버 측 요청 처리 로직을 분리하고, DB 작업별 실행 흐름을 명확하게 관리한다.

### 5.2 트랜잭션

여러 데이터 변경 작업이 하나의 논리적 처리에 포함되는 경우 트랜잭션을 적용한다.

```text
BeginTransaction
      │
      ▼
  DB 작업 수행
      │
      ├── 정상 처리 ──► Commit
      │
      └── 실패 ──────► Rollback
```

트랜잭션을 통해 복수의 데이터 변경 작업이 부분적으로 적용되는 상황을 방지하고, 처리 성공 또는 실패에 따른 데이터 정합성을 유지한다.

주요 적용 대상:

- 여러 데이터 변경이 함께 이루어지는 작업
- Gold 및 Inventory 변경
- 양측 데이터를 함께 변경하는 Trade 처리

---

## 6. GameServer와 DBServer 통신

### 6.1 요청 및 응답

GameServer는 게임 로직에서 필요한 데이터 처리를 DBServer에 요청한다.

DBServer는 전달받은 요청을 DB 작업으로 구성하여 처리하고, 완료된 결과를 응답으로 전달한다. GameServer는 응답 결과를 바탕으로 게임 로직의 후속 처리를 진행한다.

### 6.2 처리 흐름

1. GameServer에서 DB 요청 생성
2. DBServer로 요청 패킷 전달
3. GameServer에서 후속 게임 로직 수행
4. 요청에 맞는 DB 작업 구성
5. DBJobQueue 등록
6. DBWorker가 작업 실행
7. DBProcessor에서 DB 처리
8. 처리 결과에 따른 응답 생성
9. GameServer로 결과 전달

---

## 7. 주요 구현 사례

### 7.1 Inventory

Inventory 데이터 조회 및 변경을 통해 플레이어의 슬롯, ItemID, 아이템 수량을 관리한다.

인벤토리 데이터의 조회와 갱신을 DBServer 요청 흐름에 연결하고, 서버 처리 결과를 기반으로 데이터가 정상적으로 반영되는지 검증한다.

### 7.2 Gold

Gold 데이터의 조회 및 증감 처리를 구현한다.

보유량 검증과 변경 요청을 DB 처리 흐름에 연결하고, 정상 처리 및 실패 상황에서 데이터 결과를 확인한다.

### 7.3 Trade

Trade는 양측 플레이어의 Gold 및 Inventory 변경을 함께 처리하는 복합 데이터 처리 사례이다.

거래 조건을 검증하고 양측 데이터 변경을 하나의 트랜잭션으로 처리하여, 거래 실패 시 데이터가 부분적으로 변경되지 않도록 구성한다.

주요 검증 항목:

- 정상 거래
- Gold 부족
- Inventory 공간 부족
- 거래 대상 및 아이템 유효성
- 거래 실패 시 양측 데이터 유지
- 거래 성공 시 양측 데이터 변경 결과 확인

---

## 8. 패킷 핸들러 자동화 도구

프로젝트 개발 과정에서 반복적으로 작성되는 패킷 핸들러 관련 작업을 줄이기 위해 자동화 도구를 활용한다.

Client - GameServer / GameServer - DBServer 양쪽의 패킷 핸들러 자동화.

자동화 대상과 생성 결과는 기존 패킷 및 핸들러 구조에 맞추며, 생성된 코드가 프로젝트의 규칙과 동작 흐름에 맞는지 확인한 후 적용한다.

자동화 도구는 반복 작업을 줄이는 보조 수단이며, 생성 결과의 적용 및 검증은 개발 과정에서 수행한다.

---

## 9. 검증

DBServer의 기능 및 데이터 처리 결과는 실제 서버 실행, 요청·응답 확인, MSSQL 데이터 조회 등을 통해 검증한다.

### 주요 검증 항목

- GameServer와 DBServer 간 요청 및 응답
- DBJob 생성과 큐 등록 및 실행 흐름
- DB 작업의 비동기 처리
- Stored Procedure 실행 및 반환 결과
- Inventory 및 Gold 데이터 조회·변경
- Trade 성공 및 실패 처리
- 트랜잭션 Commit 및 Rollback
- 실패 상황에서 데이터 정합성 유지

---

## 10. 개선 및 확장 고려 사항

현재 구현 구조를 기반으로 추후 다음 영역을 검토할 수 있다.

- DB 작업 처리량 및 Worker 운영 방식 개선
- DB 작업 실패 및 예외 처리 강화
- 요청과 응답의 추적 및 로깅 개선
- 반복적인 DB 테스트 및 데이터 초기화 자동화
- 처리량, 지연 시간 및 동시 요청에 대한 성능 측정

위 항목은 현재 구현 범위와 구분되는 향후 개선 방향이며, 구현 완료 사항으로 간주하지 않는다.