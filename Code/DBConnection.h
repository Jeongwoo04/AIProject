#pragma once
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

enum
{
    WVARCHAR_MAX = 4000, // SQL Server NVARCHAR(n)의 최대 길이(문자 수)
    BINARY_MAX = 8000
};

enum class FetchResult
{
    Success,
    NoData,
    Error
};

class DBConnection
{
public:
    bool Connect(SQLHENV henv, const WCHAR* connectionString);
    void Clear();

    bool Execute(const WCHAR* query);
    FetchResult Fetch();
    // TODO: SQLMoreResults 지원은 보류. 복수 결과 집합 정책 확정 후 추가한다.
    int32 GetRowCount();
    void Unbind();

    // 트랜잭션은 하나의 DBConnection에서 실행되는 작업만 묶는다.
    bool BeginTransaction();
    bool Commit();
    bool Rollback();

    // 오염된 커넥션(트랜잭션 열려있거나 끊어진 커넥션)
    bool IsBroken() const { return _isBroken; }

public:
    bool BindParam(int32 paramIndex, bool* value, SQLLEN* indicator);
    bool BindParam(int32 paramIndex, float* value, SQLLEN* indicator);
    bool BindParam(int32 paramIndex, double* value, SQLLEN* indicator);
    // int8는 signed C 타입으로 SQL_SMALLINT에 매핑한다. SQL Server TINYINT(0~255)
    // 전용 uint8 바인딩은 실제 공용 타입 정의와 스키마를 확인한 뒤 추가한다.
    bool BindParam(int32 paramIndex, int8* value, SQLLEN* indicator);
    bool BindParam(int32 paramIndex, int16* value, SQLLEN* indicator);
    bool BindParam(int32 paramIndex, int32* value, SQLLEN* indicator);
    bool BindParam(int32 paramIndex, int64* value, SQLLEN* indicator);
    bool BindParam(int32 paramIndex, TIMESTAMP_STRUCT* value, SQLLEN* indicator);
    bool BindParam(int32 paramIndex, const WCHAR* str, SQLLEN* indicator);
    bool BindParam(int32 paramIndex, const BYTE* bin, int32 size, SQLLEN* indicator);

    // OUTPUT / INPUT_OUTPUT 및 특수 SQL 타입을 위한 확장 인터페이스.
    // TODO: OUTPUT 값의 최종 유효 시점은 모든 결과 집합을 소비한 뒤인지 확인 필요.
    // bufferLength는 C 버퍼의 바이트 크기, columnSize는 SQL 타입의 크기/정밀도다.
    bool BindParamEx(int32 paramIndex, SQLSMALLINT direction,
        SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN columnSize,
        SQLLEN bufferLength, SQLPOINTER value, SQLLEN* indicator,
        SQLLEN initialIndicator = 0);

    // NULL은 SQL 타입 정보가 필요하므로 호출 측에서 타입을 명시한다.
    bool BindParamNull(int32 paramIndex, SQLSMALLINT cType,
        SQLSMALLINT sqlType, SQLULEN columnSize, SQLLEN* indicator);

    bool BindCol(int32 columnIndex, bool* value, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, float* value, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, double* value, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, int8* value, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, int16* value, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, int32* value, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, int64* value, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, TIMESTAMP_STRUCT* value, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, WCHAR* str, int32 size, SQLLEN* indicator);
    bool BindCol(int32 columnIndex, BYTE* bin, int32 size, SQLLEN* indicator);

private:
    bool BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType,
        SQLLEN bufferLength, SQLPOINTER value, SQLLEN* indicator);
    void HandleError(SQLSMALLINT handleType, SQLHANDLE handle);

private:
    SQLHDBC _connection = SQL_NULL_HANDLE;
    SQLHSTMT _statement = SQL_NULL_HANDLE;
    bool _connected = false;
    bool _inTransaction = false;
    bool _isBroken = false;
};