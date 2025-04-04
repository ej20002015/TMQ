#pragma once

#include <clickhouse/client.h>

#include <chrono>

namespace TMQ
{

// SELECT query type functions

template<typename T>
T extractColVal( std::shared_ptr<clickhouse::Column> column, size_t row )
{
    if constexpr( std::is_same_v<T, std::string> )
    {
        return std::string( column->As<clickhouse::ColumnString>()->At( row ) );
    }
    else if constexpr( std::is_same_v<T, uint64_t> )
    {
        return column->As<clickhouse::ColumnUInt64>()->At( row );
    }
    else if constexpr( std::is_same_v<T, std::chrono::system_clock::time_point> )
    {
        return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>( std::chrono::nanoseconds( column->As<clickhouse::ColumnInt64>()->At( row ) ) )
        );
    }
    else if constexpr( std::is_same_v<T, int32_t> )
    {
        return column->As<clickhouse::ColumnInt32>()->At( row );
    }
    else if constexpr( std::is_same_v<T, bool> )
    {
        return static_cast< bool >( column->As<clickhouse::ColumnUInt8>()->At( row ) );
    }
    else
    {
        static_assert( false, "Unsupported type in ClickHouse query." );
    }
}

// INSERT query type functions

template<typename T>
struct CHColType
{
    static_assert( false, "Unsupported type in ClickHouse query." );
};

template<>
struct CHColType<std::string>
{
    using Type = clickhouse::ColumnString;
};

template<>
struct CHColType<uint64_t>
{
    using Type = clickhouse::ColumnUInt64;
};

template<>
struct CHColType<std::chrono::system_clock::time_point>
{
    using Type = clickhouse::ColumnUInt64;
};

template<>
struct CHColType<int32_t>
{
    using Type = clickhouse::ColumnInt32;
};

template<>
struct CHColType<bool>
{
    using Type = clickhouse::ColumnUInt8;
};


template<typename T>
constexpr T convToCHType( const T& value ) {
    return value;
}

inline uint64_t convToCHType( const std::chrono::system_clock::time_point& tp ) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>( tp.time_since_epoch() ).count();
}

}