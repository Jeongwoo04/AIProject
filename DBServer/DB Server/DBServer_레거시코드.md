# DBServer_레거시코드

> 실제 구현 코드에서 핵심 구조와 설계 의도를 설명한다. 전체 소스 나열보다 구조를 이해하는 데 필요한 부분을 발췌한다.

## 1. DBJob 비동기 처리

### 1.1 DBJob — 작업 단위 구성

DBJob은 `DBConnection`을 인자로 받아 실행하는 콜백을 보관하는 작업 단위다. `Execute()`에서 전달받은 Connection을 콜백에 넘겨 실제 DB 처리 로직을 실행한다.

```cpp
class DBJob
{
public:
    using CallbackType = std::function<void(DBConnection*)>;

    DBJob(CallbackType&& callback)
        : _callback(std::move(callback)) {}

    virtual ~DBJob() {}

    virtual void Execute(DBConnection* dbConn)
    {
        _callback(dbConn);
    }

private:
    CallbackType _callback;
};
```

### 1.2 DoDBAsync — 멤버 함수와 인자를 Job으로 구성

`JobQueue::DoDBAsync()`는 멤버 함수와 호출 인자를 묶어 DBJob 콜백으로 구성한다. 인자는 Tuple로 보관하고, 실행 시 `std::apply`를 통해 풀어 멤버 함수를 호출한다. 이 과정에서 호출 대상 객체의 `shared_ptr`를 캡처하여 실행 시점까지 객체의 수명을 유지한다.

```cpp
template<typename T, typename Ret, typename... Args>
void DoDBAsync(Ret(T::* memFunc)(DBConnection*, Args...), Args... args)
{
    shared_ptr<T> owner =
        static_pointer_cast<T>(shared_from_this());

    using TupleType = std::tuple<std::decay_t<Args>...>;
    TupleType tup = std::make_tuple(std::move(args)...);

    auto callback = [owner, memFunc, tup = std::move(tup)]
        (DBConnection* dbConn) mutable
    {
        std::apply([&](auto&&... unpackedArgs) {
            (owner.get()->*memFunc)(
                dbConn,
                std::forward<decltype(unpackedArgs)>(unpackedArgs)...);
        }, tup);
    };

    PushDBJob(ObjectPool<DBJob>::MakeShared(std::move(callback)));
}
```

### 1.3 JobQueue — 등록 및 실행

Job 등록 시 `_jobCount`를 증가시키고, 기존 작업이 없으며 현재 Queue 실행 흐름 밖인 경우 Global DB Queue에 JobQueue를 등록한다.

```cpp
void JobQueue::PushDBJob(DBJobRef dbJob)
{
    const int32 prevCount = _jobCount.fetch_add(1);
    _dbJobs.Push(dbJob);

    if (prevCount == 0 && LCurrentJobQueue == nullptr)
        GGlobalDBQueue->Push(shared_from_this());
}
```

`ExecuteDBJob()`은 Queue에 모인 Job을 실행하고 처리 수를 갱신한다. 작업이 남아 있으면 TimeSlice를 확인하고, 제한 시간에 도달한 경우 JobQueue를 Global Queue에 다시 등록한다.

```cpp
void JobQueue::ExecuteDBJob(DBConnection* dbConn)
{
    if (LCurrentJobQueue != nullptr)
        return;

    LCurrentJobQueue = this;

    while (true)
    {
        vector<DBJobRef> jobs;
        _dbJobs.PopAll(OUT jobs);

        const int32 jobCount = static_cast<int32>(jobs.size());
        for (int32 i = 0; i < jobCount; i++)
            jobs[i]->Execute(dbConn);

        if (_jobCount.fetch_sub(jobCount) == jobCount)
        {
            LCurrentJobQueue = nullptr;
            return;
        }

        const uint64 now = ::GetTickCount64();
        if (now >= LEndTickCount)
        {
            LCurrentJobQueue = nullptr;
            GGlobalDBQueue->Push(shared_from_this());
            break;
        }
    }
}
```

### 1.4 DBWorker — Connection 확보 및 TimeSlice 처리

DBWorker는 작업 시작 시 Connection Pool에서 Connection을 가져온다. TimeSlice가 종료되거나 Global Queue가 비어 있을 때까지 JobQueue를 처리한 뒤, Connection을 Pool에 반환한다.

```cpp
void ThreadManager::DoDBWork()
{
    DBConnection* dbConn = GDBConnectionPool->Pop();

    while (true)
    {
        uint64 now = ::GetTickCount64();
        if (now > LEndTickCount)
            break;

        JobQueueRef jobQueue = GGlobalDBQueue->Pop();
        if (jobQueue == nullptr)
            break;

        jobQueue->ExecuteDBJob(dbConn);
    }

    GDBConnectionPool->Push(dbConn);
}
```

**핵심**
- DBJob을 개별 실행 큐에 등록하는 대신 JobQueue 단위로 관리한다.
- Worker는 작업 구간 동안 Connection을 확보하고 여러 JobQueue를 연속 처리한다.
- JobQueue 내부에서 처리할 작업이 남아 있고 TimeSlice가 끝나면 Global Queue에 다시 등록한다.
- Worker의 작업 루프가 끝나면 Connection을 Pool에 반환한다.

---

## 2. DBConnection — DB 접근 및 실행 Wrapper

DBConnection은 ODBC 핸들을 관리하고 쿼리 실행, 결과 Fetch, 바인딩 및 트랜잭션 API를 제공한다.

### 2.1 Query 실행

`Execute()`는 `SQLExecDirectW`를 통해 쿼리를 실행한다. 실행에 성공하면 true, 실패하면 ODBC 진단 정보를 처리한 뒤 false를 반환한다.

```cpp
bool DBConnection::Execute(const WCHAR* query)
{
    if (_statement == SQL_NULL_HANDLE || query == nullptr)
        return false;

    SQLRETURN ret = ::SQLExecDirectW(
        _statement,
        reinterpret_cast<SQLWCHAR*>(const_cast<WCHAR*>(query)),
        SQL_NTS);

    if (IsOdbcSuccess(ret))
        return true;

    HandleError(SQL_HANDLE_STMT, _statement);
    return false;
}
```

### 2.2 Fetch / FetchResult

`Fetch()`는 `SQLFetch` 반환값을 `FetchResult`로 구분해 호출부가 조회 결과를 처리하도록 한다.

```cpp
FetchResult DBConnection::Fetch()
{
    if (_statement == SQL_NULL_HANDLE)
        return FetchResult::Error;

    SQLRETURN ret = ::SQLFetch(_statement);
    switch (ret)
    {
    case SQL_SUCCESS:
        return FetchResult::Success;
    case SQL_SUCCESS_WITH_INFO:
        HandleError(SQL_HANDLE_STMT, _statement);
        return FetchResult::Success;
    case SQL_NO_DATA:
        return FetchResult::NoData;
    default:
        HandleError(SQL_HANDLE_STMT, _statement);
        _isBroken = true;
        return FetchResult::Error;
    }
}
```

| 상태 | 의미 |
|---|---|
| `Success` | Fetch 성공. `SQL_SUCCESS_WITH_INFO`도 성공으로 처리하며 진단 메시지를 확인한다. |
| `NoData` | 추가로 읽을 데이터가 없음 |
| `Error` | Fetch 실패. 오류 진단 후 Connection을 Broken 상태로 표시한다. |

### 2.3 Unbind — Statement 정리 및 재사용

`Unbind()`는 Statement의 컬럼 바인딩, 파라미터 및 실행 결과를 정리하여 다음 DB 작업에서 재사용할 수 있도록 한다.

```cpp
void DBConnection::Unbind()
{
    if (_statement == SQL_NULL_HANDLE)
        return;

    ::SQLFreeStmt(_statement, SQL_UNBIND);
    ::SQLFreeStmt(_statement, SQL_RESET_PARAMS);
    ::SQLFreeStmt(_statement, SQL_CLOSE);
}
```

### 2.4 주요 Wrapper

| API | 역할 |
|---|---|
| `Connect` / `Clear` | ODBC 연결 및 핸들 정리 |
| `Execute` | 쿼리 실행 |
| `Fetch` | 결과 행 읽기 및 상태 반환 |
| `GetRowCount` | 실행 결과의 영향 행 수 조회. SELECT 전체 행 수를 보장하지 않음 |
| `Unbind` | 컬럼 바인딩, 파라미터, Statement 결과 정리 |
| `BindParam` / `BindParamEx` / `BindParamNull` | 입력 및 확장 파라미터 바인딩 |
| `BindCol` | 결과 컬럼을 타입별 버퍼에 연결 |
| `BeginTransaction` / `Commit` / `Rollback` | Connection 단위 트랜잭션 제어 |

DBConnection은 ODBC 호출과 상위 DB 처리 로직 사이에 공통 접근 인터페이스를 제공한다.

---

## 3. DBBind — 타입 기반 Param / Col 바인딩

DBBind는 템플릿으로 파라미터 수와 컬럼 수를 지정하고, 바인딩 성공 여부를 Flag로 관리한다. `Execute()` 전에 `Validate()`를 통해 Query 유효성과 모든 Param/Col 바인딩이 완료되었는지 확인한다.

### 3.1 Bind 상태 검증

```cpp
bool Validate() const
{
    return _validQuery &&
        (_paramFlag == MakeFullBits(ParamCount)) &&
        (_columnFlag == MakeFullBits(ColumnCount));
}

bool Execute()
{
    if (!Validate())
        return false;

    return _dbConnection.Execute(_query.c_str());
}
```

각 Param과 Col은 0-based 인덱스를 사용하지만 DBConnection으로 전달할 때 ODBC 바인딩 인덱스에 맞춰 `idx + 1`로 변환한다. 바인딩 성공 여부에 따라 해당 인덱스의 Flag를 설정하여 누락되거나 실패한 바인딩을 실행 전에 확인한다.

### 3.2 Param / Col 바인딩 예시

```cpp
template<typename T>
typename std::enable_if<
    !std::is_pointer<T>::value && !std::is_array<T>::value,
    bool>::type
BindParam(int32 idx, T& value)
{
    if (!IsValidParamIndex(idx))
        return false;

    const bool ok = _dbConnection.BindParam(
        idx + 1, &value, &_paramIndex[idx]);

    SetFlag(_paramFlag, idx, ok);
    return ok;
}
```

```cpp
template<typename T>
typename std::enable_if<
    !std::is_pointer<T>::value && !std::is_array<T>::value,
    bool>::type
BindCol(int32 idx, T& value)
{
    if (!IsValidColumnIndex(idx))
        return false;

    const bool ok = _dbConnection.BindCol(
        idx + 1, &value, &_columnIndex[idx]);

    SetFlag(_columnFlag, idx, ok);
    return ok;
}
```

### 3.3 지원 바인딩

- 기본 타입 Param / Col 바인딩
- WCHAR 문자열 및 배열 바인딩
- Binary 데이터 바인딩
- NULL Param 지정
- OUTPUT / INPUT_OUTPUT 파라미터를 위한 `BindParamEx`
- 바인딩별 Indicator 관리

Param과 Col의 인덱스 범위는 각각 0~64로 제한한다. DBBind는 DBConnection의 타입별 바인딩 함수를 호출하는 상위 wrapper이며, 바인딩 누락과 인덱스 범위를 실행 전에 확인한다.

---

## 4. Transaction 처리

트랜잭션 제어는 기능에 따라 C++ DBConnection 또는 Stored Procedure에서 수행한다. Inventory 획득은 C++에서 여러 DB 작업을 묶고, Trade는 Procedure 내부에서 양측 데이터 변경을 묶는다.

### 4.1 C++ 트랜잭션 — DB_AddInventoryItem

`DB_AddInventoryItem`은 요청에 포함된 여러 Inventory Delta를 순회하며 `dbo.AddInventoryItem`을 실행한다. Delta가 없으면 처리하지 않고, 먼저 트랜잭션을 시작한다.

```cpp
if (pkt.delta_size() <= 0)
    return;

if (dbConn->BeginTransaction() == false)
    return;
```

각 Delta마다 Stored Procedure를 실행하고 Execute, Fetch 및 ResultCode를 확인한다. 실행 실패, Fetch 실패 또는 ResultCode가 성공이 아니면 Rollback 후 종료한다.

```cpp
for (const Protocol::InventoryItem& delta : pkt.delta())
{
    DBBind<4, 1> dbBind(
        *dbConn, L"{CALL dbo.AddInventoryItem(?, ?, ?, ?)}");

    int32 resultCode = -1;

    dbBind.BindParam(0, playerId);
    dbBind.BindParam(1, delta.slot());
    dbBind.BindParam(2, delta.itemid());
    dbBind.BindParam(3, delta.itemcount());
    dbBind.BindCol(0, resultCode);

    // Execute / Fetch 및 ResultCode 확인
    // 실패 시 Rollback
}
```

모든 Delta 처리가 성공하면 Commit한다.

```cpp
if (dbConn->Commit() == false)
{
    dbConn->Rollback();
    return;
}
```

**핵심**
- 여러 Inventory Delta를 하나의 C++ 트랜잭션으로 묶는다.
- 개별 Stored Procedure 결과를 확인하여 하나라도 실패하면 전체 변경을 Rollback한다.
- 모든 Delta가 성공한 경우에만 Commit한다.

이를 통해 여러 슬롯에 걸친 아이템 획득을 하나의 작업 단위로 처리한다.

### 4.2 Stored Procedure 트랜잭션 — Trade

Trade는 C++에서 별도의 BeginTransaction / Commit을 호출하지 않고, `dbo.Trade` Stored Procedure 내부에서 트랜잭션을 관리한다.

```sql
SET XACT_ABORT ON;

BEGIN TRY
    BEGIN TRANSACTION;

    -- 양측 Player 잠금 및 검증
    -- 양측 Item / Gold 검증
    -- Inventory 차감 및 지급
    -- Gold 변경

    COMMIT TRANSACTION;
END TRY
BEGIN CATCH
    IF XACT_STATE() <> 0
        ROLLBACK TRANSACTION;

    SET @ResultCode = 9;
END CATCH
```

Trade는 두 플레이어의 Inventory와 Gold 변경을 하나의 트랜잭션으로 묶는다. 검증 결과에 따라 Commit 또는 Rollback하고, 예외 발생 시 Catch에서 트랜잭션을 정리한다.

---

## 5. Trade — 복합 데이터 처리

Trade는 DBProcessor와 Stored Procedure의 역할을 나누어 구현했다.

### 5.1 C++ DBProcessor

`DB_Trade`는 8개 Parameter를 DBBind로 전달한다.

- Player A / Player B
- Player A가 제공하는 Item ID, 수량, Gold
- Player B가 제공하는 Item ID, 수량, Gold

Stored Procedure를 실행하고 Fetch한 ResultCode를 확인한다. C++에서는 요청 전달과 결과 수신을 담당하며, 거래 검증과 데이터 변경은 Procedure에서 수행한다.

### 5.2 Stored Procedure 처리

`dbo.Trade`는 다음 순서로 거래를 처리한다.

1. 요청값 검증 및 비정상 거래 조건 확인
2. PlayerID 오름차순으로 Player 잠금 획득
3. 양측 Item 보유량 검증
4. 양측 Gold 보유량 검증
5. 거래 후 Gold 범위 검증
6. 양측 Item 차감
7. Player A의 Item 수령 및 Inventory Slot 처리
8. Player B의 Item 수령 및 Inventory Slot 처리
9. 양측 Gold 갱신
10. 결과에 따른 Commit / Rollback

Player 잠금 순서를 고정하여 잠금 순서가 서로 달라지는 상황을 줄이고, Inventory와 Gold 변경을 동일 트랜잭션 안에서 처리한다. 아이템을 받을 때 기존 동일 Item 슬롯이 있으면 수량을 합산하고, 없으면 사용 가능한 Slot을 찾아 신규 행을 삽입한다. Slot 부족 또는 수량 범위 초과 등의 실패 조건에서는 거래를 취소한다.

Trade는 양측 Inventory와 Gold를 동시에 변경하는 복합 작업이므로, Stored Procedure 내부에서 검증부터 데이터 변경 및 트랜잭션 완료까지 처리한다.

---

## 6. Inventory / Gold 시연

Inventory 및 Gold의 개별 기능은 상세 코드 설명을 반복하기보다 시연 영상으로 실제 클라이언트와 서버, DB 간 처리 결과를 보여준다.

- 아이템 획득 및 Inventory 반영
- Inventory Slot 및 아이템 수량 확인
- Gold 변경
- 실제 DB 결과 확인

---

## 7. Packet Handler Automation

패킷 핸들러 자동화 도구는 반복적인 Handler 코드 생성 과정을 줄이기 위해 사용한다.

---

## 8. 정리

DBServer의 주요 구현은 작업 단위 구성과 실행 관리, DB 접근 wrapper, 타입 기반 바인딩, 트랜잭션 제어를 통해 데이터 처리 구조를 구성하는 데 초점을 두었다.

각 구성 요소를 독립적으로 설명하는 데 그치지 않고, 실제 게임 데이터 처리와 통합 검증 사례를 연결하여 설계 의도와 구현 결과를 보여준다.
