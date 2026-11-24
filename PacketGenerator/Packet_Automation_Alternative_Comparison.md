# 패킷 자동화 확장 설계 검토서

## 1. 목적

기존 `Client ↔ GameServer` 패킷 자동화에 `GameServer ↔ DBServer` 통신을 추가한다.

이 문서는 작업 목록만 나열하는 체크리스트가 아니다. 각 설계 항목에서 **현재 구조 → 선택 가능한 대안 → 대안별 차이 → 현재 결정 → 실제 확인 사항**을 비교해 구현 방향을 이해하고 결정하기 위한 문서다.

이번 작업의 기준은 기존 `PacketGenerator.py`, `ProtoParser.py`, `PacketHandler.h`, `GenPackets.bat`, `MakeExe.bat`의 흐름을 최대한 유지하면서 DB 통신을 추가하는 것이다.

---

## 2. 현재 구조 이해하기

현재 `GenPackets.bat`은 하나의 `Protocol.proto`를 컴파일하고, 같은 Proto를 두 번 Generator에 전달한다.

| 생성 대상 | recv | send | 의미 |
|---|---|---|---|
| `ClientPacketHandler` | `S_` | `C_` | GameServer가 받을 패킷은 `S_`, 보낼 패킷은 `C_` |
| `ServerPacketHandler` | `C_` | `S_` | Client가 받을 패킷은 `C_`, 보낼 패킷은 `S_` |

여기서 `recv`와 `send`는 **Generator를 실행하는 프로그램의 관점**이다.

현재 `ProtoParser.py`는 다음 순서로 동작한다.

1. `message` 선언을 한 줄씩 읽는다.
2. 이름이 `recv_prefix`로 시작하면 수신 패킷으로 분류한다.
3. 이름이 `send_prefix`로 시작하면 송신 패킷으로 분류한다.
4. 둘 중 하나에 해당하면 현재 ID를 부여하고 1 증가시킨다.
5. 기본 시작 ID는 `1000`이다.

현재 `PacketHandler.h`는 모든 패킷 처리에 전역 `GPacketHandler` 배열을 사용한다. 따라서 GameServer가 Client와 DB 양쪽 연결을 처리하게 되면, **두 통신 경계의 패킷 ID와 Handler 테이블을 어떻게 구분할지**가 핵심 설계 문제가 된다.

---

## 3. 설계 항목별 대안 비교

## 3.1 Proto 파일을 하나로 유지할까, 통신별로 분리할까?

### 현재 상황

현재는 Client와 GameServer 사이에서 사용하는 메시지가 하나의 `Protocol.proto`에 들어 있다.

DBServer 통신 메시지를 추가하면 Client용 메시지와 DB용 메시지가 한 파일에 함께 존재하게 된다.

### 대안 비교

| 구분 | A안: `Protocol.proto` 하나로 유지 | B안: 통신 경계별 Proto 분리 |
|---|---|---|
| 파일 예시 | `Protocol.proto` 하나에 `C_`, `S_`, `GS2DB_`, `DB2GS_` 메시지 모두 선언 | `ClientProtocol.proto`, `DBProtocol.proto` |
| 장점 | 파일과 컴파일 대상이 단순함 | 통신별 메시지와 ID 생성 범위가 명확함 |
| 단점 | 각 프로그램이 필요하지 않은 메시지까지 함께 보게 될 수 있음 | Proto 파일과 생성 명령이 늘어남 |
| GameServer | Client와 DB 메시지가 한 파일에 섞임 | 두 Proto를 모두 사용 |
| DBServer | DB 메시지만 필요해도 통합 Proto를 참조할 수 있음 | DB Proto만 사용 |

### 예시

**A안**에서는 하나의 파일에 다음 메시지가 함께 존재한다.

```proto
message C_Login { ... }
message S_Login { ... }

message GS2DB_LoadCharacter { ... }
message DB2GS_LoadCharacter { ... }
```

**B안**에서는 통신 경계에 따라 파일을 나눈다.

```text
Protocol/
├── Enum.proto
├── Struct.proto
├── ClientProtocol.proto
└── DBProtocol.proto
```

`ClientProtocol.proto`에는 Client와 GameServer 사이의 메시지를, `DBProtocol.proto`에는 GameServer와 DBServer 사이의 메시지를 선언한다. 공통 자료형인 `Enum.proto`, `Struct.proto`는 공유한다.

### 현재 결정

- [o] B안: Client 통신과 DB 통신의 Proto 파일을 분리한다.
- [o] `Enum.proto`, `Struct.proto`는 공통으로 사용한다.
- [o] 기존 Proto 경로와 생성 파일 배치 방식은 최대한 유지한다.

### 구현 전에 확인할 내용

- [o] 두 프로토콜이 공통 `Enum.proto`, `Struct.proto`를 import할 수 있도록 현재 `protoc` 실행 경로와 include 경로를 구성할 수 있는가?
- [o] GameServer에는 두 프로토콜의 생성 파일이 모두 필요하고, DBServer에는 DB 프로토콜 생성 파일만 있으면 되는가?
- [x] `PacketHandler.h`의 고정 include인 `Protocol.pb.h`를 프로토콜별 생성 파일명에 맞게 바꿀 필요가 있는가?

---

## 3.2 Prefix는 어떻게 정할까?

### 현재 상황

현재 Prefix는 메시지의 송신 방향을 이름으로 구분한다.

- Client가 보내는 메시지: `C_`
- GameServer가 보내는 메시지: `S_`

Generator의 `recv`와 `send` 설정은 실행 중인 프로그램 기준이므로 같은 Proto를 읽더라도 Client와 GameServer의 설정이 서로 반대다.

### 대안 비교

| 구분 | A안: 기존 Prefix 확장 | B안: 통신 경계와 무관한 공통 Prefix |
|---|---|---|
| 예시 | `GS2DB_`, `DB2GS_`처럼 송신자와 수신자를 이름에 표시 | `C_`, `S_` 같은 짧은 이름을 모든 통신에서 재사용 |
| 장점 | 메시지 이름만 봐도 어느 서버 사이의 통신인지 알기 쉬움 | 이름이 짧고 기존 규칙과 비슷함 |
| 단점 | Prefix 종류가 늘어남 | 서로 다른 통신 경계의 메시지 이름이 겹치거나 의미가 모호해질 수 있음 |

### 예시

DB 통신에서 GameServer가 요청을 보내고 DBServer가 응답한다면 다음과 같이 이름을 정한다.

```proto
message GS2DB_LoadCharacter { ... }
message DB2GS_LoadCharacter { ... }
```

GameServer에서 Generator를 실행할 때는 다음처럼 해석한다.

| Generator 설정 | GameServer 관점 |
|---|---|
| `recv=DB2GS_` | DBServer가 보내서 GameServer가 받는 패킷 |
| `send=GS2DB_` | GameServer가 DBServer로 보내는 패킷 |

DBServer에서는 반대로 설정한다.

| Generator 설정 | DBServer 관점 |
|---|---|
| `recv=GS2DB_` | GameServer가 보내서 DBServer가 받는 패킷 |
| `send=DB2GS_` | DBServer가 GameServer로 보내는 패킷 |

### 현재 결정

- [o] Client ↔ GameServer는 기존 `C_`, `S_` 규칙을 유지한다.
- [o] GameServer ↔ DBServer는 `GS2DB_`, `DB2GS_`를 사용한다.
- [o] Prefix는 송신 방향을 나타낸다.
- [o] `recv`와 `send`는 Generator 실행 프로그램의 관점으로 설정한다.

### 구현 전에 확인할 내용

- [o] 각 Generator 실행 명령에서 `recv`, `send` 값이 실행 대상 프로그램의 관점과 일치하는가?
- [o] Parser가 지정된 두 Prefix에 해당하는 메시지만 분류하는가? - 추후 분산 서버 추가시 확장될 수 있음

---

## 3.3 패킷 ID를 통합할까, 통신별로 따로 발급할까?

### 현재 상황

현재 Parser는 메시지 선언 순서대로 ID를 발급한다. 기본 시작 ID는 `1000`이며, 설정한 recv/send Prefix에 해당하는 메시지만 ID를 소비한다.

예를 들어 다음과 같은 Proto를 읽고 `recv=S_`, `send=C_`로 실행하면, 두 Prefix에 해당하는 메시지에 선언 순서대로 ID가 부여된다.

### 대안 비교

| 구분 | A안: 모든 통신이 하나의 ID 번호 체계 공유 | B안: 통신별 독립 ID 발급 |
|---|---|---|
| ID 예시 | Client 패킷이 1000~1099, DB 패킷이 1100~1199 등 하나의 범위에서 관리 | Client와 DB 프로토콜 모두 1000부터 시작 가능 |
| 장점 | 전체 시스템에서 ID 숫자만으로 구분할 여지가 있음 | 각 통신 경계에서 ID를 독립적으로 관리할 수 있음 |
| 단점 | 통신이 늘어날수록 ID 범위와 충돌 방식을 함께 관리해야 함 | ID 숫자만으로 통신 경계를 구분할 수 없음 |
| Handler | 통합 ID 정책에 맞춰 테이블을 관리해야 함 | 통신별 Handler가 자기 ID 테이블만 조회하면 됨 |

### 예시

**A안**이라면 Client와 DB가 같은 ID 공간을 사용하므로 ID 범위를 나누거나 전체 메시지 목록을 기준으로 번호를 관리해야 한다.

**B안**이라면 아래처럼 서로 다른 통신에서 같은 숫자를 사용해도 된다.

```text
Client 통신
  C_Login  = 1000
  S_Login  = 1001

DB 통신
  GS2DB_LoadCharacter = 1000
  DB2GS_LoadCharacter = 1001
```

단, 이 경우 GameServer는 Client 패킷을 Client Handler로, DB 패킷을 DB Handler로 전달해야 한다. ID 숫자만 보고 통신 경계를 판단해서는 안 된다.

### 현재 결정

- [o] B안: Client 통신과 DB 통신의 ID를 독립적으로 발급한다.
- [o] 각 Proto의 메시지 선언 순서대로 ID를 발급하는 현재 방식을 유지한다.
- [o] 수동 ID 지정이나 별도 ID 관리 파일은 도입하지 않는다.
- [o] ID 중복 검사는 각 통신 프로토콜 내부에서 수행한다.

### 구현 전에 확인할 내용

- [o] `ClientProtocol.proto`를 읽을 때와 `DBProtocol.proto`를 읽을 때 Parser가 각각 시작 ID `1000`부터 발급하는가?
- [o] GameServer와 DBServer가 동일한 `DBProtocol.proto`를 같은 선언 순서로 읽어 동일한 DB 패킷 ID를 생성하는가?
- [o] Proto 분리 후에도 Client 통신의 기존 패킷 ID가 기존과 동일하게 생성되는가?

---

## 3.4 GameServer의 Handler를 하나로 둘까, 통신별로 분리할까?

### 현재 상황

현재 `PacketHandler.h`는 다음 전역 배열을 사용한다.

```cpp
extern PacketHandlerFunc GPacketHandler[UINT16_MAX];
```

그리고 생성된 Handler 클래스의 `Init()`이 이 배열을 초기화하고, `HandlePacket()`이 패킷 ID로 배열을 조회한다.

DB 통신을 추가하면 GameServer는 Client 연결과 DB 연결에서 모두 패킷을 받는다. 두 통신의 ID가 독립적이므로 같은 배열을 공유하면 초기화 또는 등록 과정에서 서로의 항목을 덮어쓸 수 있다.

### 대안 비교

| 구분 | A안: 하나의 전역 Handler 테이블 공유 | B안: Handler별 독립 테이블 |
|---|---|---|
| 구조 | `GPacketHandler` 하나를 Client/DB가 함께 사용 | Client Handler와 DB Handler가 각자 테이블 보유 |
| 장점 | 기존 전역 배열 구조를 그대로 활용할 수 있음 | 같은 ID가 있어도 통신별로 독립 처리 가능 |
| 단점 | ID 충돌을 피하도록 통합 관리가 필요하고 초기화 충돌 가능 | 테이블이 두 개로 나뉘며 연결별 Handler 선택이 필요 |
| GameServer | 두 연결의 패킷을 하나의 테이블에서 처리 | 연결 종류에 따라 서로 다른 Handler 호출 |
| DBServer | DB 패킷 테이블을 별도로 구성해야 함 | DB Handler만 사용 |

### 예시

**B안**에서는 GameServer가 패킷을 받는 연결에 따라 Handler를 선택한다.

```cpp
// Client와 연결된 세션
ClientPacketHandler::HandlePacket(session, buffer, len);

// DBServer와 연결된 세션
DBPacketHandler::HandlePacket(session, buffer, len);
```

위 코드는 구조를 설명하기 위한 예시다. 실제 함수 호출 위치와 세션 타입은 기존 연결 처리 흐름에 맞춰 결정한다.

### 현재 결정

- [o] B안: Client 통신용과 DB 통신용 Handler를 분리한다.
- [o] 생성 파일은 `ClientPacketHandler.h`, `DBPacketHandler.h`로 구분한다. - Client-GameServer간 통신 시. 요청하는 입장(Client)와 받는 입장(GameServer)로 구분해서. Client에서 ClientPacketHandler를, GameServer에서 ServerPacketHandler를 두었다. GameServer-DBServer에선 GameServer가 요청하는 입장(Client)이 되고 DBServer가 받는 입장(Server) 가 된다. 이를 유의해서 각각의 프로젝트에 보내줄 PacketHandler 네이밍을 적절히 사용한다. 기존 Client - GameServer간에 정의된 PacketHandler.h 네이밍은 유지한다.
- [o] 전역 `GPacketHandler`도 통신별 독립 테이블로 분리한다.
- [o] `Init()`, `HandlePacket()`, `MakeSendBuffer()`의 기존 역할은 최대한 유지한다.

### 구현 전에 확인할 내용

- [o] 각 Handler가 자기 테이블만 초기화하고 조회하도록 템플릿을 수정했는가? - 해야 됨.
- [o] GameServer의 Client 연결 처리부는 Client Handler를, DB 연결 처리부는 DB Handler를 호출하는가? - 해야 됨.
- [o] DBServer는 DB Handler만 초기화하고 사용하는가? - 해야됨
- [o] 두 Handler에 동일한 ID가 등록되어도 서로의 등록 내용에 영향을 주지 않는가? - 해야 됨
- [o] `MakeSendBuffer()`가 각 Handler의 패킷 ID를 사용해 올바른 패킷을 생성하는가? - 해야 됨

---

## 3.5 Generator를 서버별로 따로 만들까, 하나를 설정만 바꿔 재사용할까?

### 현재 상황

현재 `PacketGenerator.py`는 실행 인자로 다음 값을 받는다.

- `--path`: 읽을 Proto 경로
- `--output`: 생성할 Handler 파일명
- `--recv`: 수신 Prefix
- `--send`: 송신 Prefix

따라서 같은 Generator에 서로 다른 설정을 전달해 여러 Handler를 생성할 수 있는 구조다.

### 대안 비교

| 구분 | A안: 서버별 Generator 복제 | B안: 기존 Generator 재사용 |
|---|---|---|
| 구조 | Client용, DB용 Generator를 별도 파일로 관리 | 하나의 `GenPackets.exe`에 실행 인자만 다르게 전달 |
| 장점 | 서버별 동작을 별도로 수정하기 쉬움 | 중복 코드와 유지보수 대상이 늘어나지 않음 |
| 단점 | Parser/Generator 수정 사항을 여러 파일에 반영해야 함 | 실행 인자와 생성 명령을 정확히 구성해야 함 |
| 현재 코드와의 관계 | 현재 구조를 복제해야 함 | 기존 `--path`, `--output`, `--recv`, `--send`를 활용 가능 |

### 예시

현재 방식의 확장이라면 다음처럼 같은 실행 파일을 여러 번 호출할 수 있다.

```bat
GenPackets.exe --path=./ClientProtocol.proto --output=ClientPacketHandler --recv=S_ --send=C_

GenPackets.exe --path=./DBProtocol.proto --output=DBPacketHandler --recv=DB2GS_ --send=GS2DB_
```

위 명령은 GameServer용 Handler를 생성하는 설정 예시다. DBServer용 Handler는 DB Proto를 대상으로 recv/send Prefix를 반대로 설정해 생성한다.

### 현재 결정

- [o] B안: 기존 Generator를 재사용한다.
- [o] 통신 경계마다 Proto 경로, 출력 파일명, recv/send Prefix를 다르게 전달한다.
- [o] Parser와 Generator를 서버별로 복제하지 않는다.

### 구현 전에 확인할 내용

- [o] `GenPackets.bat`에 Client용과 DB용 생성 명령을 각각 추가했는가? - 해야 됨
- [ ] GameServer용 DB Handler와 DBServer용 DB Handler가 같은 DB Proto를 사용하고 Prefix만 반대인가? - 위에 설명과 같이. 요청하는 입장이 Client를 붙여서 네이밍 하도록. GameServer와 DBServer간 Handler는 GameServer엔 DBClientPacketHandler.h 로 DBServer에선 DBServerPacketHandler.h 로 네이밍하도록 수정이 필요하다.
- [ ] 출력 파일명이 서로 덮어쓰지 않도록 구분되어 있는가? - 위에 맞게끔 네이밍하자.

---

## 3.6 BAT와 생성 파일 배포를 어떻게 바꿀까?

### 현재 상황

현재 역할은 다음과 같다.

- `MakeExe.bat`: PyInstaller로 `GenPackets.exe` 생성
- `GenPackets.bat`: Protobuf 컴파일, Handler 생성, 프로젝트별 파일 복사
- Client와 GameServer의 Pre-Build Event: `GenPackets.bat` 실행

### 대안 비교

| 구분 | A안: 빌드 자동화 전체 재설계 | B안: 기존 흐름에 DB 생성·복사만 추가 |
|---|---|---|
| 변경 범위 | BAT 실행 방식, 오류 처리, 복사 구조 등을 함께 변경 | 기존 BAT 흐름과 프로젝트별 복사 방식을 유지 |
| 장점 | 전체 구조를 한 번에 정리할 수 있음 | 이번 DB 통신 추가에 필요한 변경만 수행 |
| 단점 | 기존 빌드 동작에 영향을 줄 가능성이 큼 | 기존 BAT의 불편한 부분은 그대로 남음 |
| 작업 위험 | 변경 범위가 큼 | 변경 범위가 상대적으로 작음 |

### 예시

기존처럼 `MakeExe.bat`은 실행 파일 생성만 담당하고, `GenPackets.bat`에 아래 작업을 추가하는 방식이다.

1. 공통 Enum/Struct와 각 Protocol을 컴파일한다.
2. Client Handler와 DB Handler를 각각 생성한다.
3. GameServer에는 Client/DB 관련 생성 파일을 복사한다.
4. DBServer에는 DB 관련 생성 파일을 복사한다.
5. 기존 Client, LoginServer, DummyClient, Unreal Client의 복사 대상은 새 Proto 구성에 맞게 조정한다.

### 현재 결정

- [o] B안: 기존 자동화 흐름을 유지하고 필요한 명령과 복사 대상만 변경한다.
- [o] `MakeExe.bat`은 기존처럼 `GenPackets.exe` 생성 역할을 유지한다.
- [o] `GenPackets.bat`에서 통신별 Handler 생성과 프로젝트별 파일 복사를 관리한다.
- [o] 기존 BAT의 오류 처리, 일시 정지, 파일 삭제, 작업 디렉터리 동작은 이번 범위에서 개선하지 않는다.

### 구현 전에 확인할 내용

- [ ] `GenPackets.bat`이 두 Proto를 각각 컴파일하고 있는가? - 해야 됨
- [ ] GameServer에 Client/DB Handler와 필요한 Protobuf 생성 파일이 모두 복사되는가? - 해야 됨
- [ ] DBServer에는 DB Handler와 필요한 Protobuf 생성 파일이 복사되는가? - 해야 됨
- [ ] 기존 Client 계열 프로젝트에는 Client 통신에 필요한 파일만 전달되는가? - 현상태 유지
- [ ] 각 프로젝트의 기존 Pre-Build Event가 변경된 생성·복사 흐름과 호환되는가? - 현상태는 무조건 호출하게 되어있음. 이것도 현상태 유지

---

## 4. 이번 작업에서 제외할 사항

아래 항목은 현재 결정한 구현 방향에 포함하지 않는다.

- [o] Parser 또는 Generator를 서버별로 복제하지 않는다.
- [o] 모든 서버 종류를 관리하는 별도 프레임워크를 만들지 않는다.
- [o] 전체 서버가 공유하는 전역 패킷 ID 체계를 새로 만들지 않는다.
- [o] 수동 ID 관리 파일을 도입하지 않는다.
- [o] 동적 Proto 로딩을 추가하지 않는다.
- [o] 서버 자동 탐색·등록 기능을 추가하지 않는다.
- [o] 기존 BAT 오류 처리, pause, 파일 삭제, 작업 디렉터리 동작을 개선하지 않는다.
- [o] 기존 `Init()`, `HandlePacket()`, `MakeSendBuffer()`의 역할을 불필요하게 재설계하지 않는다.

---

## 5. 구현 완료 판단 기준

아래 항목을 실제 빌드와 패킷 처리로 확인하면 이번 작업의 핵심 검증은 완료된다.

- [ ] Client ↔ GameServer의 기존 패킷 ID와 송수신 분류가 유지된다.
- [ ] GameServer와 DBServer가 동일한 DB Proto를 기준으로 동일한 DB 패킷 ID를 생성한다.
- [ ] GameServer의 Client Handler와 DB Handler가 각각 독립적으로 초기화된다.
- [ ] GameServer가 Client 연결 패킷은 Client Handler로, DB 연결 패킷은 DB Handler로 전달한다.
- [ ] DBServer가 DB Handler로 GameServer의 요청을 처리하고 DBServer 응답을 생성한다.
- [ ] 같은 숫자의 패킷 ID가 서로 다른 Handler에 존재해도 충돌하지 않는다.
- [ ] 각 프로젝트에 필요한 Protobuf 생성 파일과 Handler만 배포된다.

---

## 6. 권장 구현 순서

1. `ClientProtocol.proto`, `DBProtocol.proto`로 메시지를 분리하고 공통 Proto import를 확인한다.
2. Parser가 입력받은 Proto 하나만 기준으로 ID를 발급하는지 확인한다.
3. Generator 인자를 이용해 Client/DB Handler를 각각 생성한다.
4. Handler 템플릿의 전역 테이블을 Handler별 독립 테이블로 변경한다.
5. GameServer의 연결 종류별 패킷 처리 경로에서 올바른 Handler를 선택한다.
6. `GenPackets.bat`의 컴파일, 생성, 복사 대상을 변경한다.
7. 구현 완료 판단 기준에 따라 기존 Client 통신과 신규 DB 통신을 검증한다.

**최종 설계 방향:** Proto 파일, 패킷 ID, Handler 테이블은 통신 경계별로 분리한다. Prefix는 송신 방향을 표현하며, 기존 Generator를 실행 인자만 바꿔 재사용한다. 빌드 자동화는 기존 흐름을 유지하고 DB 통신에 필요한 부분만 확장한다.
