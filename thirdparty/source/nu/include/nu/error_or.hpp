#ifndef error_or_hpp_dKfa5ZhUpqI8k0sPhaQJmI
#define error_or_hpp_dKfa5ZhUpqI8k0sPhaQJmI

#include <system_error>

/// Error handling (`error_or`)
/// ===========================

/// .. namespace:: nu
namespace nu {

// LCOV_EXCL_START
/// .. class:: bad_error_or_access : public std::exception
///
///    Thrown by :class:`error_or` if an object is coerced to a value if it contains an error or if it is
///    coerced to an error if it contains a value.
struct bad_error_or_access : std::exception {
	virtual const char* what() const noexcept override
	{
		return "bad_error_or_access";
	}
};

template<typename Val>
static constexpr bool is_nothrow_swappable() noexcept
{
   return noexcept(std::swap(std::declval<Val&>(), std::declval<Val&>()));
}

template<typename Val>
static constexpr bool is_value_type() noexcept
{
   static_assert(!std::is_array<Val>::value, "Val must not be an array type");
   static_assert(!std::is_reference<Val>::value, "Val must not be a reference type");
   static_assert(!std::is_function<Val>::value, "Val must not be a function type");
   static_assert(std::is_move_constructible<Val>::value, "Val must be move-constructible");
   static_assert(sizeof(Val) > 0, "Val must be complete");

   return true;
}
// LCOV_EXCL_STOP


/// .. class:: template<typename T, typename Err = std::error_code> error_or
///
///    An instance of :class:`error_or` may contain either an error (of type :any:`Err`) or a value
///    (of type :any:`T`).
///
///    In addition to methods to query whether an object contains an error or a value, :class:`error_or`
///    provides an *apply* operation and a *reduce* operation. The *apply* operation can be used to
///    transform an object that contains a value, but retain the error if the object does not contain a
///    value. The *reduce* operation can be used to transform an :class:`error_or` into a new value
///    regardless of whether it contained a value or an error.
///
///    The template types :any:`T` and :any:`Err` must both be complete types that are move-constructible
///    and are neither an array type, reference type, or function type.
///    :any:`Err` must also be default-constructible and copy-constructible; it is recommended to ensure
///    that :any:`Err` is trivially copy- and move-constructible.
///    All operations of :any:`Err` must be noexcept.
///    (This restriction is placed on :any:`Err` to make all operations on :any:`error_or` instances
///    with errors uniformly noexcept.)
///
///    Example:
///
///    .. include:: /../tests/error_or_example.cpp
//+       :code: c++

template<typename T, typename Err = std::error_code>
class error_or {

public:
	static_assert(nu::is_value_type<T>(), "");
	static_assert(nu::is_value_type<Err>(), "");

	static_assert(std::is_nothrow_default_constructible<Err>::value, "Err must be default-constructible");
	static_assert(std::is_nothrow_move_constructible<Err>::value, "Err must be nothrow move-constructible");
	static_assert(std::is_nothrow_copy_constructible<Err>::value, "Err must be nothrow move-constructible");
	static_assert(std::is_nothrow_move_assignable<Err>::value, "Err must be nothrow move-assignable");
	static_assert(std::is_nothrow_destructible<Err>::value, "Err must be nothrow-destructible");
	static_assert(nu::is_nothrow_swappable<Err>(), "Err must be nothrow-swappable");

	/// .. function:: error_or() noexcept
	///
	///    Constructs the object holding a default-constructed :any:`Err` as its :func:`error`.
	///
	///    :noexcept: ``noexcept``
	error_or() noexcept:
		valid(false), u{with_error{}, {}}
	{
	}

	/// .. function:: error_or(Err e) noexcept
	///
	///    Constructs the object holding a `e` as its :func:`error`. The value is moved into
	///    the newly constructed object.
	///
	///    :noexcept: ``noexcept``
	error_or(Err error) noexcept:
		valid(false), u{with_error{}, std::move(error)}
	{
	}

	/// .. function:: error_or(T value)
	///
	///    Constructs the object holding ``value`` as its :func:`value`. The provided value
	///    is moved into the object.
	///
	///    :noexcept: ``noexcept`` if the move constructor of :any:`T` is ``noexcept``
	error_or(T value) noexcept(std::is_nothrow_move_constructible<T>::value):
		valid(true), u{with_data{}, std::move(value)}
	{
	}

	/// .. function:: error_or(error_or&& other)
	///
	///    Constructs the object with the state of ``other``. The state of ``other`` is moved
	///    into the newly constructed object; ``other`` is in an indefinite state afterwards.
	///
	///    :noexcept: |swap(T,T)-noexcept|
	error_or(error_or&& other) noexcept(noexcept(other.swap(other))):
		error_or()
	{
		swap(other);
	}

	~error_or() noexcept(std::is_nothrow_destructible<T>::value)
	{
		if (valid)
			u.value.~T();
		else
			u.error.~Err();
	}

	/// .. function:: error_or& operator=(error_or&& other)
	///
	///    Moves the state of ``other`` into ``*this``. Afterwards, ``other`` is in an
	///    indefinite state.
	///
	///    :noexcept: |swap(T,T)-noexcept|
	error_or& operator=(error_or&& other)
		noexcept(noexcept(error_or(std::move(other)).swap(std::declval<error_or&>())))
	{
		error_or(std::move(other)).swap(*this);
		return *this;
	}

	/// .. function:: void swap(error_or& other)
	///
	///    Swaps the states of ``*this`` and ``other``.
	///
	///    :noexcept: |swap(T,T)-noexcept|
	void swap(error_or& other) noexcept(nu::is_nothrow_swappable<T>())
	{
		using std::swap;

		if (valid) {
			if (other.valid) {
				swap(u.value, other.u.value);
			} else {
				T tmp(std::move(u.value));

				u.value.~T();
				new (&u.error) Err(std::move(other.u.error)); // LCOV_EXCL_LINE (check for NULL)

				other.u.error.~Err();
				new (&other.u.value) T(std::move(tmp)); // LCOV_EXCL_LINE (check for NULL)

				swap(valid, other.valid);
			}
		} else if (other.valid) {
			other.swap(*this);
		} else {
			swap(u.error, other.u.error);
		}
	}

	/// .. function:: explicit operator bool() const noexcept
	///
	///    :returns: ``true`` if ``*this`` contains a value, ``false`` otherwise
	///    :noexcept: ``noexcept``
	explicit operator bool() const noexcept { return valid; }
	/// .. function:: bool operator!() const noexcept
	///
	///    :returns: ``false`` if ``*this`` contains a value, ``true`` otherwise
	///    :noexcept: ``noexcept``
	bool operator!() const noexcept { return !bool(*this); }

	/// .. function:: template<typename Fn> auto operator/(Fn f) const
	/// .. function:: template<typename Fn> auto operator/(Fn f)
	///
	///    If ``*this`` contains a value, calls ``f(get_or_release())`` and returns the result as an
	///    ``error_or<R, Err>`` object, where ``R`` is the return type of :any:`Fn`.
	///
	///    These operators are useful because they can provide exception guarantees that the usual
	///    sequences ``if (e) on_value(e.value()); else on_error(e.error());`` cannot provide, since
	///    both :func:`value` and :func:`error` may throw.
	///
	///    :returns: ``*this ? error_or<R, Err>(f(get_or_release())) : error_or<R, Err>(error())``
	///    :noexcept: ``noexcept`` if ``f`` is ``noexcept``
	template<typename Fn>
	auto operator/(Fn f) const noexcept(noexcept(f(std::declval<const T&>())))
		-> error_or<typename std::result_of<Fn(const T&)>::type, Err>
	{
		return valid
			? error_or<typename std::result_of<Fn(const T&)>::type, Err>{f(u.value)}
			: error_or<typename std::result_of<Fn(const T&)>::type, Err>{u.error};
	}

	template<typename Fn>
	auto operator/(Fn f) noexcept(noexcept(f(std::declval<T&&>())))
		-> error_or<typename std::result_of<Fn(T&&)>::type, Err>
	{
		return valid
			? error_or<typename std::result_of<Fn(T&&)>::type, Err>{f(std::move(u.value))}
			: error_or<typename std::result_of<Fn(T&&)>::type, Err>{std::move(u.error)};
	}

	/// .. function:: template<typename R = void, typename TrueFn, typename FalseFn> ↩
	///                   auto reduce(TrueFn t, FalseFn f) const
	/// .. function:: template<typename R = void, typename TrueFn, typename FalseFn> ↩
	///                   auto reduce(TrueFn t, FalseFn f)
	///
	///    If ``*this`` contains a value, calls ``t(get_or_release())``, otherwise calls
	///    ``f(error())``.
	///    The result of both ``t(get_or_release())`` and ``f(error())`` is converted to type
	///    ``RT`` and returned, where ``RT`` is ``R`` if ``R`` is not ``void``, otherwise
	///    ``std:common_type<TT, FT>::type`` where ``TT`` and ``FT`` are the result types of
	///    ``t(get_or_release())`` and ``f(error())`` respectively.
	///    The result types of ``t`` and ``f`` must each be a complete type that is neither
	///    an array type, a reference type, or a function type.
	///
	///    These functions are useful because they can provide exception guarantees that the usual
	///    sequences ``if (e) on_value(e.value()); else on_error(e.error());`` cannot provide, since
	///    both :func:`value` and :func:`error` may throw.
	///
	///    :returns: ``*this ? t(get_or_release()) : f(error())``
	///    :noexcept: ``noexcept`` if ``t`` is ``noexcept`` and ``f`` is ``noexcept``.
	template<typename R = void, typename TrueFn, typename FalseFn>
	auto reduce(TrueFn t, FalseFn f) const
		noexcept(
			noexcept(t(std::declval<const T&>()))
			&& noexcept(f(std::declval<const Err&>())))
		-> typename std::conditional<
				std::is_void<R>::value,
				typename std::common_type<
					decltype(t(std::declval<const T&>())),
					decltype(f(std::declval<const Err&>()))>::type,
				R>::type
	{
		static_assert(nu::is_value_type<decltype(t(std::declval<const T&>()))>(), "");
		static_assert(nu::is_value_type<decltype(f(std::declval<const Err&>()))>(), "");

		return valid
			? t(u.value)
			: f(u.error);
	}

	template<typename R = void, typename TrueFn, typename FalseFn>
	auto reduce(TrueFn t, FalseFn f)
		noexcept(
			noexcept(t(std::declval<T&&>()))
			&& noexcept(f(std::declval<const Err&>())))
		-> typename std::conditional<
				std::is_void<R>::value,
				typename std::common_type<
					decltype(t(std::declval<T&&>())),
					decltype(f(std::declval<const Err&>()))>::type,
				R>::type
	{
		static_assert(nu::is_value_type<decltype(t(std::declval<T&&>()))>(), "");
		static_assert(nu::is_value_type<decltype(f(std::declval<const Err&>()))>(), "");

		return valid
			? t(std::move(u.value))
			: f(std::move(u.error));
	}

	/// .. function:: template<typename Fn> auto operator%(Fn f) const
	/// .. function:: template<typename Fn> auto operator%(Fn f)
	///
	///    If ``*this`` contains a value, calls ``fn(get_or_release())`` and returns the result.
	///    If ``*this`` does not contain a value, returns ``error()`` cast to the result type
	///    of ``fn(get_or_release())``.
	///    The result type must be a complete type that is neither an array type, a reference
	///    type, or a function type.
	///
	///    These operators are provided as a shorthand for ``reduce(fn1, std::move<Err>)``,
	///    where ``fn1`` would usually be a function that returns another instance of :any:`error_or`.
	///    By using the ``%`` operator, multiple calls to functions returning :any:`error_or` can
	///    be easily chained if only the last error in the chain is important, for example if a
	///    program must open a file and invoke an ioctl on it in one operation that appears atomic
	///    to the consumer.
	///
	///    :returns: ``*this ? fn(get_or_release()) : error()``
	///    :noexcept: ``noexcept`` if ``fn`` is ``noexcept``.
	template<typename Fn>
	auto operator%(Fn fn) const noexcept(noexcept(fn(std::declval<const T&>())))
		-> decltype(fn(std::declval<const T&>()))
	{
		static_assert(nu::is_value_type<decltype(fn(u.value))>(), "");

		return valid
			? fn(u.value)
			: decltype(fn(u.value)){u.error};
	}

	template<typename Fn>
	auto operator%(Fn fn) noexcept(noexcept(fn(std::declval<T&&>())))
		-> decltype(fn(std::declval<T&&>()))
	{
		static_assert(nu::is_value_type<decltype(fn(std::move(u.value)))>(), "");

		return valid
			? fn(std::move(u.value))
			: decltype(fn(std::move(u.value))){std::move(u.error)};
	}

	/// .. function:: const Err& error() const
	///
	///    :returns: The error stored in ``*this``.
	///    :throws nu\:\:bad_error_or_access: If ``*this`` contains a value.
	const Err& error() const
	{
		if (valid)
			throw bad_error_or_access();

		return u.error;
	}

	/// .. function:: const T& value() const &
	///
	///    :returns: The value stored in ``*this``.
	///    :throws nu\:\:bad_error_or_access: If ``*this`` contains an error.
	const T& value() const
	{
		if (!valid)
			throw bad_error_or_access();

		return u.value;
	}

	/// .. function:: T&& release_value()
	///
	///    :returns: The value stored in ``*this``.
	///    :throws nu\:\:bad_error_or_access: If ``*this`` contains an error.
	T&& release_value()
	{
		if (!valid)
			throw bad_error_or_access();

		valid = false;
		return std::move(u.value);
	}

private:
	enum class with_error {};
	enum class with_data {};

	bool valid;
	union U {
		~U() {}

		U(with_error, Err e) noexcept:
			error(e)
		{}

		U(with_data, T value) noexcept(std::is_nothrow_move_constructible<T>::value):
			value(std::move(value))
		{}

		Err error;
		T value;
	} u;

	/// .. function:: private const T& get_or_release() const noexcept
	/// .. function:: private T&& get_or_release() noexcept
	///
	///    **Exposition only**: if called by const reference, returns a const reference
	///    to the stored value. If called by non-const reference, returns an rvalue reference
	///    to the stored value.
};
//-

/// Free functions for :class:`error_or`
/// ------------------------------------

/// .. function:: template<typename T, typename Err> ↩
///                   void swap(error_or<T, Err>& a, error_or<T, Err>& b)
///
///    Swap two :class:`error_or` instances.
///
///    :noexcept: |swap(T,T)-noexcept|
template<typename T, typename Err>
inline void swap(error_or<T, Err>& a, error_or<T, Err>& b) noexcept(noexcept(a.swap(b)))
{
	a.swap(b);
}

/// .. function:: template<typename T, typename E = std::error_code, typename... Args> ↩
///                   error_or<T> make_error_or(Args&&... args)
///
///    Creates a new :any:`error_or` instance with the given arguments as
///    ``error_or<T, E>(T{std::forward<Args>(args)...})``.
///
///    :noexcept: ``noexcept(error_or<T, E>(T{std::forward<Args>(args)...}))``
template<typename T, typename E = std::error_code, typename... Args>
inline error_or<T> make_error_or(Args&&... args)
	noexcept(noexcept(error_or<T, E>(T{std::forward<Args>(args)...})))
{
	return error_or<T, E>(T{std::forward<Args>(args)...});
}

}

/// .. |swap(T,T)-noexcept| replace::
///    ``noexcept`` if the function ``<unspecified> swap(T&, T&)`` is ``noexcept``,
///    where ``swap`` is either ``std::swap`` or found by ADL.

#endif
