#pragma once
#include "DBConnection.h"
#include <type_traits>
#include <string>

// ParamCount / ColumnCount는 각각 0~64 범위만 지원한다.
constexpr uint64 MakeFullBits(int32 count)
{
    return count == 0 ? uint64{ 0 } :
        ((uint64{ 1 } << (count - 1)) | MakeFullBits(count - 1));
}

template<int32 ParamCount, int32 ColumnCount>
class DBBind
{
    static_assert(ParamCount >= 0 && ParamCount <= 64, "ParamCount must be 0~64");
    static_assert(ColumnCount >= 0 && ColumnCount <= 64, "ColumnCount must be 0~64");

public:
    DBBind(DBConnection& dbConnection, const WCHAR* query)
        : _dbConnection(dbConnection), _query(query != nullptr ? query : L"")
    {
        ::memset(_paramIndex, 0, sizeof(_paramIndex));
        ::memset(_columnIndex, 0, sizeof(_columnIndex));
        _paramFlag = 0;
        _columnFlag = 0;
        _validQuery = (query != nullptr);
        _dbConnection.Unbind();
    }

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

    FetchResult Fetch()
    {
        return _dbConnection.Fetch();
    }

    // BindParam/BindCol의 성공 여부를 호출 측에서도 확인할 수 있다.
    template<typename T>
    typename std::enable_if<!std::is_pointer<T>::value && !std::is_array<T>::value, bool>::type
        BindParam(int32 idx, T& value)
    {
        if (!IsValidParamIndex(idx))
            return false;

        const bool ok = _dbConnection.BindParam(idx + 1, &value, &_paramIndex[idx]);
        SetFlag(_paramFlag, idx, ok);
        return ok;
    }

    bool BindParam(int32 idx, const WCHAR* value)
    {
        if (!IsValidParamIndex(idx))
            return false;

        const bool ok = _dbConnection.BindParam(idx + 1, value, &_paramIndex[idx]);
        SetFlag(_paramFlag, idx, ok);
        return ok;
    }

    template<int32 N>
    bool BindParam(int32 idx, const WCHAR(&value)[N])
    {
        return BindParam(idx, static_cast<const WCHAR*>(value));
    }

    // BYTE 등 바이너리 배열용. WCHAR 배열은 위의 문자열 오버로드가 처리한다.
    template<typename T, int32 N>
    typename std::enable_if<!std::is_same<T, WCHAR>::value, bool>::type
        BindParam(int32 idx, T(&value)[N])
    {
        return BindBinaryParam(idx, reinterpret_cast<const BYTE*>(value),
            static_cast<int32>(sizeof(T) * N));
    }

    template<typename T>
    bool BindParam(int32 idx, T* value, int32 byteSize)
    {
        return BindBinaryParam(idx, reinterpret_cast<const BYTE*>(value), byteSize);
    }

    // NULL 파라미터는 DB 타입을 호출부에서 명시한다.
    bool BindParamNull(int32 idx, SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN columnSize)
    {
        if (!IsValidParamIndex(idx))
            return false;

        const bool ok = _dbConnection.BindParamNull(
            idx + 1, cType, sqlType, columnSize, &_paramIndex[idx]);
        SetFlag(_paramFlag, idx, ok);
        return ok;
    }

    // OUTPUT / INPUT_OUTPUT 파라미터용 저수준 바인딩.
    // indicator는 DBBind 내부에서 관리하며, OUTPUT 문자열/바이너리는 value 버퍼 크기를 정확히 전달해야 한다.
    bool BindParamEx(int32 idx, SQLSMALLINT direction,
        SQLSMALLINT cType, SQLSMALLINT sqlType, SQLULEN columnSize,
        SQLLEN bufferLength, SQLPOINTER value, SQLLEN initialIndicator = 0)
    {
        if (!IsValidParamIndex(idx))
            return false;

        const bool ok = _dbConnection.BindParamEx(idx + 1, direction,
            cType, sqlType, columnSize, bufferLength, value, &_paramIndex[idx],
            initialIndicator);
        SetFlag(_paramFlag, idx, ok);
        return ok;
    }

    template<typename T>
    typename std::enable_if<!std::is_pointer<T>::value && !std::is_array<T>::value, bool>::type
        BindCol(int32 idx, T& value)
    {
        if (!IsValidColumnIndex(idx))
            return false;

        const bool ok = _dbConnection.BindCol(idx + 1, &value, &_columnIndex[idx]);
        SetFlag(_columnFlag, idx, ok);
        return ok;
    }

    template<int32 N>
    bool BindCol(int32 idx, WCHAR(&value)[N])
    {
        if (!IsValidColumnIndex(idx))
            return false;

        const bool ok = _dbConnection.BindCol(idx + 1, value, N, &_columnIndex[idx]);
        SetFlag(_columnFlag, idx, ok);
        return ok;
    }

    bool BindCol(int32 idx, WCHAR* value, int32 charCapacity)
    {
        if (!IsValidColumnIndex(idx) || value == nullptr || charCapacity <= 0)
            return false;

        const bool ok = _dbConnection.BindCol(idx + 1, value, charCapacity, &_columnIndex[idx]);
        SetFlag(_columnFlag, idx, ok);
        return ok;
    }

    template<typename T, int32 N>
    typename std::enable_if<!std::is_same<T, WCHAR>::value, bool>::type
        BindCol(int32 idx, T(&value)[N])
    {
        if (!IsValidColumnIndex(idx))
            return false;

        const bool ok = _dbConnection.BindCol(idx + 1,
            reinterpret_cast<BYTE*>(value), static_cast<int32>(sizeof(T) * N),
            &_columnIndex[idx]);
        SetFlag(_columnFlag, idx, ok);
        return ok;
    }

    SQLLEN GetParamIndicator(int32 idx) const
    {
        return IsValidParamIndex(idx) ? _paramIndex[idx] : SQL_NULL_DATA;
    }

    SQLLEN GetColumnIndicator(int32 idx) const
    {
        return IsValidColumnIndex(idx) ? _columnIndex[idx] : SQL_NULL_DATA;
    }

private:
    bool BindBinaryParam(int32 idx, const BYTE* data, int32 byteSize)
    {
        if (!IsValidParamIndex(idx) || byteSize < 0)
            return false;

        const bool ok = _dbConnection.BindParam(
            idx + 1, data, byteSize, &_paramIndex[idx]);
        SetFlag(_paramFlag, idx, ok);
        return ok;
    }

    bool IsValidParamIndex(int32 idx) const
    {
        return idx >= 0 && idx < ParamCount;
    }

    bool IsValidColumnIndex(int32 idx) const
    {
        return idx >= 0 && idx < ColumnCount;
    }

    static void SetFlag(uint64& flags, int32 idx, bool success)
    {
        const uint64 bit = (uint64{ 1 } << idx);
        if (success)
            flags |= bit;
        else
            flags &= ~bit;
    }

private:
    DBConnection& _dbConnection;
    std::wstring _query; // 쿼리 문자열의 수명을 DBBind가 소유한다.
    SQLLEN _paramIndex[ParamCount > 0 ? ParamCount : 1];
    SQLLEN _columnIndex[ColumnCount > 0 ? ColumnCount : 1];
    uint64 _paramFlag = 0;
    uint64 _columnFlag = 0;
    bool _validQuery = false;
};