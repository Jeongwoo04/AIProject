#include "pch.h"
#include "DBConnection.h"
#include <limits>

namespace
{
    bool IsOdbcSuccess(SQLRETURN ret)
    {
        return SQL_SUCCEEDED(ret) != 0;
    }
}

bool DBConnection::Connect(SQLHENV henv, const WCHAR* connectionString)
{
    if (henv == SQL_NULL_HANDLE || connectionString == nullptr)
        return false;

    // 재연결 시 기존 핸들을 먼저 정리한다.
    Clear();

    SQLRETURN ret = ::SQLAllocHandle(SQL_HANDLE_DBC, henv, &_connection);
    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_ENV, henv);
        _connection = SQL_NULL_HANDLE;
        _isBroken = true;
        return false;
    }

    // 입력 문자열 길이를 고정 MAX_PATH에 제한하지 않는다.
    ret = ::SQLDriverConnectW(
        _connection,
        nullptr,
        reinterpret_cast<SQLWCHAR*>(const_cast<WCHAR*>(connectionString)),
        SQL_NTS,
        nullptr,
        0,
        nullptr,
        SQL_DRIVER_NOPROMPT);

    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_DBC, _connection);
        Clear();
        _isBroken = true;
        return false;
    }
    if (ret == SQL_SUCCESS_WITH_INFO)
        HandleError(SQL_HANDLE_DBC, _connection);
    _connected = true;

    ret = ::SQLAllocHandle(SQL_HANDLE_STMT, _connection, &_statement);
    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_DBC, _connection);
        Clear();
        _isBroken = true;
        return false;
    }

    _inTransaction = false;
    _isBroken = false;
    return true;
}

void DBConnection::Clear()
{
    if (_connection != SQL_NULL_HANDLE && _inTransaction)
    {
        SQLRETURN ret = ::SQLEndTran(SQL_HANDLE_DBC, _connection, SQL_ROLLBACK);
        if (!IsOdbcSuccess(ret))
        {
            HandleError(SQL_HANDLE_DBC, _connection);
            _isBroken = true;
        }
        _inTransaction = false;
    }

    if (_statement != SQL_NULL_HANDLE)
    {
        ::SQLFreeHandle(SQL_HANDLE_STMT, _statement);
        _statement = SQL_NULL_HANDLE;
    }

    if (_connection != SQL_NULL_HANDLE)
    {
        // ODBC에서는 연결을 해제한 뒤 DBC 핸들을 해제한다.
        if (_connected)
        {
            SQLRETURN ret = ::SQLDisconnect(_connection);
            if (!IsOdbcSuccess(ret))
            {
                HandleError(SQL_HANDLE_DBC, _connection);
                _isBroken = true;
            }
            _connected = false;
        }

        ::SQLFreeHandle(SQL_HANDLE_DBC, _connection);
        _connection = SQL_NULL_HANDLE;
    }

    _inTransaction = false;
}

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
        HandleError(SQL_HANDLE_STMT, _statement); // 경고 진단도 확인
        return FetchResult::Success;
    case SQL_NO_DATA:
        return FetchResult::NoData;
    default:
        HandleError(SQL_HANDLE_STMT, _statement);
        _isBroken = true;
        return FetchResult::Error;
    }
}

int32 DBConnection::GetRowCount()
{
    if (_statement == SQL_NULL_HANDLE)
        return -1;

    SQLLEN count = 0;
    SQLRETURN ret = ::SQLRowCount(_statement, &count);
    if (!IsOdbcSuccess(ret) || count < 0 || count >(std::numeric_limits<int32>::max)())
        return -1;

    // 주의: SELECT의 전체 행 수를 보장하지 않는다. 주로 DML 영향 행 수에 사용한다.
    return static_cast<int32>(count);
}

void DBConnection::Unbind()
{
    if (_statement == SQL_NULL_HANDLE)
        return;

    SQLRETURN ret = ::SQLFreeStmt(_statement, SQL_UNBIND);
    if (!IsOdbcSuccess(ret))
        HandleError(SQL_HANDLE_STMT, _statement);

    ret = ::SQLFreeStmt(_statement, SQL_RESET_PARAMS);
    if (!IsOdbcSuccess(ret))
        HandleError(SQL_HANDLE_STMT, _statement);

    ret = ::SQLFreeStmt(_statement, SQL_CLOSE);
    if (!IsOdbcSuccess(ret))
        HandleError(SQL_HANDLE_STMT, _statement);
}

bool DBConnection::BeginTransaction()
{
    if (_connection == SQL_NULL_HANDLE || _inTransaction)
        return false;

    SQLRETURN ret = ::SQLSetConnectAttr(
        _connection, SQL_ATTR_AUTOCOMMIT,
        reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_OFF), 0);
    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_DBC, _connection);
        _isBroken = true;
        return false;
    }

    _inTransaction = true;
    return true;
}

bool DBConnection::Commit()
{
    if (_connection == SQL_NULL_HANDLE || !_inTransaction)
        return false;

    SQLRETURN ret = ::SQLEndTran(SQL_HANDLE_DBC, _connection, SQL_COMMIT);
    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_DBC, _connection);
        _isBroken = true;
        return false;
    }

    ret = ::SQLSetConnectAttr(
        _connection, SQL_ATTR_AUTOCOMMIT,
        reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0);
    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_DBC, _connection);
        _isBroken = true;
        return false;
    }

    _inTransaction = false;
    return true;
}

bool DBConnection::Rollback()
{
    if (_connection == SQL_NULL_HANDLE || !_inTransaction)
        return false;

    SQLRETURN ret = ::SQLEndTran(SQL_HANDLE_DBC, _connection, SQL_ROLLBACK);
    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_DBC, _connection);
        _isBroken = true;
        return false;
    }

    ret = ::SQLSetConnectAttr(
        _connection, SQL_ATTR_AUTOCOMMIT,
        reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), 0);
    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_DBC, _connection);
        _isBroken = true;
        return false;
    }

    _inTransaction = false;
    return true;
}

bool DBConnection::BindParam(int32 paramIndex, bool* value, SQLLEN* indicator)
{
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 1,
        sizeof(bool), value, indicator);
}

bool DBConnection::BindParam(int32 paramIndex, float* value, SQLLEN* indicator)
{
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_FLOAT, SQL_REAL, 7,
        sizeof(float), value, indicator);
}

bool DBConnection::BindParam(int32 paramIndex, double* value, SQLLEN* indicator)
{
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 15,
        sizeof(double), value, indicator);
}

bool DBConnection::BindParam(int32 paramIndex, int8* value, SQLLEN* indicator)
{
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_STINYINT, SQL_SMALLINT, 5,
        sizeof(int8), value, indicator);
}

bool DBConnection::BindParam(int32 paramIndex, int16* value, SQLLEN* indicator)
{
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_SSHORT, SQL_SMALLINT, 5,
        sizeof(int16), value, indicator);
}

bool DBConnection::BindParam(int32 paramIndex, int32* value, SQLLEN* indicator)
{
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 10,
        sizeof(int32), value, indicator);
}

bool DBConnection::BindParam(int32 paramIndex, int64* value, SQLLEN* indicator)
{
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 19,
        sizeof(int64), value, indicator);
}

bool DBConnection::BindParam(int32 paramIndex, TIMESTAMP_STRUCT* value, SQLLEN* indicator)
{
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP,
        SQL_TYPE_TIMESTAMP, 3, sizeof(TIMESTAMP_STRUCT), value, indicator);
}

bool DBConnection::BindParam(int32 paramIndex, const WCHAR* str, SQLLEN* indicator)
{
    if (str == nullptr)
        return BindParamNull(paramIndex, SQL_C_WCHAR, SQL_WVARCHAR, 1, indicator);

    const SQLULEN charCount = static_cast<SQLULEN>(::wcslen(str));
    const SQLULEN byteCount = (charCount + 1) * sizeof(WCHAR);
    const SQLSMALLINT sqlType = (charCount > WVARCHAR_MAX) ? SQL_WLONGVARCHAR : SQL_WVARCHAR;

    if (indicator == nullptr)
        return false;
    *indicator = SQL_NTS;

    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_WCHAR, sqlType,
        (charCount == 0 ? 1 : charCount), static_cast<SQLLEN>(byteCount),
        const_cast<WCHAR*>(str), indicator, SQL_NTS);
}

bool DBConnection::BindParam(int32 paramIndex, const BYTE* bin, int32 size, SQLLEN* indicator)
{
    if (size < 0 || indicator == nullptr)
        return false;

    if (bin == nullptr)
        return BindParamNull(paramIndex, SQL_C_BINARY, SQL_VARBINARY, 1, indicator);

    *indicator = size;
    const SQLSMALLINT sqlType = (size > BINARY_MAX) ? SQL_LONGVARBINARY : SQL_VARBINARY;
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, SQL_C_BINARY, sqlType,
        (size == 0 ? 1 : static_cast<SQLULEN>(size)), size,
        const_cast<BYTE*>(bin), indicator, size);
}

bool DBConnection::BindParamEx(int32 paramIndex, SQLSMALLINT direction,
    SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN columnSize,
    SQLLEN bufferLength, SQLPOINTER value, SQLLEN* indicator,
    SQLLEN initialIndicator)
{
    if (_statement == SQL_NULL_HANDLE || paramIndex <= 0 ||
        paramIndex > (std::numeric_limits<SQLUSMALLINT>::max)() ||
        indicator == nullptr || bufferLength < 0 ||
        (direction != SQL_PARAM_INPUT && direction != SQL_PARAM_OUTPUT &&
            direction != SQL_PARAM_INPUT_OUTPUT && direction != SQL_RETURN_VALUE))
        return false;

    *indicator = initialIndicator;
    SQLRETURN ret = ::SQLBindParameter(
        _statement, static_cast<SQLUSMALLINT>(paramIndex), direction,
        cType, sqlType, columnSize, 0, value, bufferLength, indicator);

    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_STMT, _statement);
        return false;
    }
    if (ret == SQL_SUCCESS_WITH_INFO)
        HandleError(SQL_HANDLE_STMT, _statement);
    return true;
}

bool DBConnection::BindParamNull(int32 paramIndex, SQLSMALLINT cType,
    SQLSMALLINT sqlType, SQLULEN columnSize, SQLLEN* indicator)
{
    if (indicator == nullptr)
        return false;
    *indicator = SQL_NULL_DATA;
    return BindParamEx(paramIndex, SQL_PARAM_INPUT, cType, sqlType,
        columnSize, 0, nullptr, indicator, SQL_NULL_DATA);
}

bool DBConnection::BindCol(int32 columnIndex, bool* value, SQLLEN* indicator)
{
    return BindCol(columnIndex, SQL_C_BIT, sizeof(bool), value, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, float* value, SQLLEN* indicator)
{
    return BindCol(columnIndex, SQL_C_FLOAT, sizeof(float), value, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, double* value, SQLLEN* indicator)
{
    return BindCol(columnIndex, SQL_C_DOUBLE, sizeof(double), value, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, int8* value, SQLLEN* indicator)
{
    return BindCol(columnIndex, SQL_C_STINYINT, sizeof(int8), value, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, int16* value, SQLLEN* indicator)
{
    return BindCol(columnIndex, SQL_C_SSHORT, sizeof(int16), value, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, int32* value, SQLLEN* indicator)
{
    return BindCol(columnIndex, SQL_C_SLONG, sizeof(int32), value, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, int64* value, SQLLEN* indicator)
{
    return BindCol(columnIndex, SQL_C_SBIGINT, sizeof(int64), value, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, TIMESTAMP_STRUCT* value, SQLLEN* indicator)
{
    return BindCol(columnIndex, SQL_C_TYPE_TIMESTAMP, sizeof(TIMESTAMP_STRUCT), value, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, WCHAR* str, int32 size, SQLLEN* indicator)
{
    if (size <= 0)
        return false;
    return BindCol(columnIndex, SQL_C_WCHAR,
        static_cast<SQLLEN>(size) * sizeof(WCHAR), str, indicator);
}

bool DBConnection::BindCol(int32 columnIndex, BYTE* bin, int32 size, SQLLEN* indicator)
{
    if (size < 0)
        return false;
    return BindCol(columnIndex, SQL_C_BINARY, size, bin, indicator);
}

bool DBConnection::BindCol(SQLUSMALLINT columnIndex, SQLSMALLINT cType,
    SQLLEN bufferLength, SQLPOINTER value, SQLLEN* indicator)
{
    if (_statement == SQL_NULL_HANDLE || columnIndex == 0 ||
        bufferLength < 0 || indicator == nullptr)
        return false;

    SQLRETURN ret = ::SQLBindCol(_statement, columnIndex, cType,
        value, bufferLength, indicator);
    if (!IsOdbcSuccess(ret))
    {
        HandleError(SQL_HANDLE_STMT, _statement);
        return false;
    }
    if (ret == SQL_SUCCESS_WITH_INFO)
        HandleError(SQL_HANDLE_STMT, _statement);
    return true;
}

void DBConnection::HandleError(SQLSMALLINT handleType, SQLHANDLE handle)
{
    if (handle == SQL_NULL_HANDLE)
        return;

    SQLWCHAR state[6] = {};
    SQLWCHAR message[SQL_MAX_MESSAGE_LENGTH + 1] = {};
    SQLINTEGER nativeError = 0;
    SQLSMALLINT messageLength = 0;

    for (SQLSMALLINT record = 1; ; ++record)
    {
        SQLRETURN ret = ::SQLGetDiagRecW(handleType, handle, record,
            state, &nativeError, message, SQL_MAX_MESSAGE_LENGTH, &messageLength);
        if (ret == SQL_NO_DATA)
            break;
        if (!SQL_SUCCEEDED(ret))
            break;

        // TODO: 프로젝트 공용 Logger로 교체할 것. 현재는 진단 레코드를 콘솔에 출력.
        std::wcout << L"[ODBC][" << state << L"][" << nativeError
            << L"] " << message << std::endl;
    }
}
