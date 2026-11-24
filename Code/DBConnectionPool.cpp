#include "pch.h"
#include "DBConnectionPool.h"
#include <algorithm>

DBConnectionPool::DBConnectionPool() {}
DBConnectionPool::~DBConnectionPool() { Clear(); }

bool DBConnectionPool::Connect(int32 connectionCount, const WCHAR* connectionString)
{
    if (connectionCount <= 0 || connectionString == nullptr)
        return false;

    _connectionString = connectionString;

    WRITE_LOCK;

    // 이미 초기화된 풀이라면 재초기화하지 않음
    if (_environment != SQL_NULL_HANDLE || !_connections.empty())
        return false;

    // Connect 실패 시 지금까지 생성한 리소스 정리
    auto cleanup = [this]()
        {
            for (DBConnection* connection : _connections)
            {
                connection->Clear();
                S1_Delete(connection);
            }
            _connections.clear();

            if (_environment != SQL_NULL_HANDLE)
            {
                ::SQLFreeHandle(SQL_HANDLE_ENV, _environment);
                _environment = SQL_NULL_HANDLE;
            }
        };

    SQLRETURN ret = ::SQLAllocHandle(
        SQL_HANDLE_ENV,
        SQL_NULL_HANDLE,
        &_environment);

    if (!SQL_SUCCEEDED(ret))
    {
        cleanup();
        return false;
    }

    ret = ::SQLSetEnvAttr(
        _environment,
        SQL_ATTR_ODBC_VERSION,
        reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3),
        0);

    if (!SQL_SUCCEEDED(ret))
    {
        cleanup();
        return false;
    }

    for (int32 i = 0; i < connectionCount; i++)
    {
        DBConnection* connection = S1_New<DBConnection>();

        if (connection->Connect(_environment, connectionString) == false)
        {
            connection->Clear();
            S1_Delete(connection);

            cleanup();
            return false;
        }

        _connections.push_back(connection);
    }

    return true;
}

void DBConnectionPool::Clear()
{
    WRITE_LOCK;

    for (DBConnection* connection : _connections)
    {
        connection->Clear();
        S1_Delete(connection);
    }
    _connections.clear();

    if (_environment != SQL_NULL_HANDLE)
    {
        ::SQLFreeHandle(SQL_HANDLE_ENV, _environment);
        _environment = SQL_NULL_HANDLE;
    }
}

DBConnection* DBConnectionPool::Pop()
{
    WRITE_LOCK;

    if (_connections.empty())
        return nullptr;

    DBConnection* connection = _connections.back();
    _connections.pop_back();

    return connection;
}

void DBConnectionPool::Push(DBConnection* connection)
{
    if (connection == nullptr)
        return;

    WRITE_LOCK;

    // 이미 풀에 들어 있는 연결인지 확인
    if (std::find(_connections.begin(), _connections.end(), connection)
        != _connections.end())
        return;

    // Broken 연결 복구
    if (connection->IsBroken())
    {
        if (!connection->Connect(_environment, _connectionString))
            return;
    }

    _connections.push_back(connection);
}