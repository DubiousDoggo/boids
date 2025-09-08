#ifndef UTIL_HH
#define UTIL_HH

#include <memory>
#include <string>
#include <stdexcept>
#include <sstream>

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    auto buf = std::make_unique<char[]>( size );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

template <typename T>
std::string to_string( const T& value ) {
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

template <typename T>
constexpr T clamp(const T& a, const T& mi, const T& ma) { return std::min(std::max(a, mi), ma); }

template<typename T>
T wrap(T value, T min, T max)
{
    T delta = max - min;
    while (value >= max) value -= delta;
    while (value <  min) value += delta;
    return value;
}
template <typename T>
T wrap(T value, T max) { return wrap<T>(value, 0, max); }

/** linear interpolation */
template <typename T>
constexpr T lerp(const T& v1, const T& v2, float t) {
    return v1 * (1 - t) + v2 * t;
}

#endif