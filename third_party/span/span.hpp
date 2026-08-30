#pragma once

// Polyfill for C++20's std::span, for toolchains (like the TICLANG compiler
// used by CCS for this course) that do not implement <span>. If the
// toolchain already provides std::span, this header just includes <span> and
// changes nothing.
//
// Hand-written for this course to mirror the std::span API documented at
// https://en.cppreference.com/w/cpp/container/span - construction, iterators,
// element access, size_bytes, first/last/subspan, as_bytes/as_writable_bytes,
// and CTAD - so that reference page applies directly to code using this
// header. The one deliberate gap: std::span's compile-time Extent template
// parameter is not implemented, so every span here behaves like
// std::span<T, dynamic_extent>.

#if defined(__cpp_lib_span)

#include <span>

#else

#include <cstddef>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>

namespace std {

inline constexpr std::size_t dynamic_extent =
  std::numeric_limits<std::size_t>::max();

template<typename T>
class span;

template<typename>
struct hal_is_span : std::false_type
{};

template<typename T>
struct hal_is_span<span<T>> : std::true_type
{};

template<typename T>
class span
{
public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T*;
  using const_pointer = T const*;
  using reference = T&;
  using const_reference = T const&;
  using iterator = T*;
  using reverse_iterator = std::reverse_iterator<iterator>;

  constexpr span() noexcept = default;

  constexpr span(pointer p_data, size_type p_size) noexcept
    : m_data(p_data)
    , m_size(p_size)
  {
  }

  constexpr span(pointer p_first, pointer p_last) noexcept
    : m_data(p_first)
    , m_size(static_cast<size_type>(p_last - p_first))
  {
  }

  template<std::size_t N>
  constexpr span(element_type (&p_array)[N]) noexcept
    : m_data(p_array)
    , m_size(N)
  {
  }

  template<typename Container,
           typename = std::void_t<decltype(std::declval<Container&>().data()),
                                  decltype(std::declval<Container&>().size())>,
           typename =
             std::enable_if_t<!hal_is_span<std::remove_cv_t<Container>>::value>>
  constexpr span(Container& p_container) noexcept
    : m_data(p_container.data())
    , m_size(p_container.size())
  {
  }

  template<typename U,
           typename = std::enable_if_t<std::is_convertible_v<U (*)[], T (*)[]>>>
  constexpr span(span<U> const& p_other) noexcept
    : m_data(p_other.data())
    , m_size(p_other.size())
  {
  }

  [[nodiscard]] constexpr pointer data() const noexcept
  {
    return m_data;
  }

  [[nodiscard]] constexpr size_type size() const noexcept
  {
    return m_size;
  }

  [[nodiscard]] constexpr size_type size_bytes() const noexcept
  {
    return m_size * sizeof(element_type);
  }

  [[nodiscard]] constexpr bool empty() const noexcept
  {
    return m_size == 0;
  }

  [[nodiscard]] constexpr reference operator[](size_type p_index) const
  {
    return m_data[p_index];
  }

  [[nodiscard]] constexpr reference front() const
  {
    return m_data[0];
  }

  [[nodiscard]] constexpr reference back() const
  {
    return m_data[m_size - 1];
  }

  [[nodiscard]] constexpr iterator begin() const noexcept
  {
    return m_data;
  }

  [[nodiscard]] constexpr iterator end() const noexcept
  {
    return m_data + m_size;
  }

  [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept
  {
    return reverse_iterator(end());
  }

  [[nodiscard]] constexpr reverse_iterator rend() const noexcept
  {
    return reverse_iterator(begin());
  }

  [[nodiscard]] constexpr span<element_type> first(size_type p_count) const
  {
    return span<element_type>(m_data, p_count);
  }

  [[nodiscard]] constexpr span<element_type> last(size_type p_count) const
  {
    return span<element_type>(m_data + (m_size - p_count), p_count);
  }

  [[nodiscard]] constexpr span<element_type> subspan(
    size_type p_offset,
    size_type p_count = dynamic_extent) const
  {
    auto const count =
      (p_count == dynamic_extent) ? (m_size - p_offset) : p_count;
    return span<element_type>(m_data + p_offset, count);
  }

private:
  pointer m_data = nullptr;
  size_type m_size = 0;
};

template<typename T, std::size_t N>
span(T (&)[N]) -> span<T>;

template<
  typename Container,
  typename = std::enable_if_t<!hal_is_span<std::remove_cv_t<Container>>::value>>
span(Container&) -> span<typename Container::value_type>;

// Not constexpr: reinterpret_cast is not permitted in a constexpr context,
// same restriction the real std::span::as_bytes/as_writable_bytes are under.
template<typename T>
[[nodiscard]] span<std::byte const> as_bytes(span<T> p_span) noexcept
{
  return span<std::byte const>(
    reinterpret_cast<std::byte const*>(p_span.data()), p_span.size_bytes());
}

template<typename T, typename = std::enable_if_t<!std::is_const_v<T>>>
[[nodiscard]] span<std::byte> as_writable_bytes(span<T> p_span) noexcept
{
  return span<std::byte>(reinterpret_cast<std::byte*>(p_span.data()),
                         p_span.size_bytes());
}

}  // namespace std

#endif  // defined(__cpp_lib_span)
