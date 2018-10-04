#include <nu/error_or.hpp>

#include <gtest/gtest.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

// LCOV_EXCL_START

namespace {

TEST(error_or, example)
{
#include "error_or_example.cpp"
}

using namespace nu;

struct ctor_throws {
	ctor_throws(ctor_throws&&);
	ctor_throws& operator=(ctor_throws&&) noexcept;
};

struct dtor_throws {
	~dtor_throws() noexcept(false);
};

struct swap_throws {
};

[[gnu::unused]]
void swap(swap_throws&, swap_throws) {}

struct called_mask {
	bool dtor;
	bool swap;
};

struct track_calls {
	called_mask* called;
	int v;

	track_calls(called_mask& c, int v): called(&c), v(v) {}

	~track_calls() {
		called->dtor = true;
	}

	friend void swap(track_calls& a, track_calls& b) noexcept {
		std::swap(a, b);
		a.called->swap = true;
		b.called->swap = true;
	}
};

}

TEST(error_or, noexceptness)
{
	using std::declval;

	EXPECT_TRUE(std::is_nothrow_default_constructible<error_or<int>>::value);

	EXPECT_TRUE(std::is_nothrow_destructible<error_or<int>>::value);
	EXPECT_FALSE(std::is_nothrow_destructible<error_or<dtor_throws>>::value);

	EXPECT_TRUE((std::is_nothrow_constructible<error_or<int>, std::error_code>::value));

	EXPECT_TRUE((std::is_nothrow_constructible<error_or<int>, int>::value));
	EXPECT_FALSE((std::is_nothrow_constructible<error_or<ctor_throws>, ctor_throws>::value));

	EXPECT_TRUE((std::is_nothrow_move_constructible<error_or<int>>::value));
	EXPECT_FALSE((std::is_nothrow_move_constructible<error_or<ctor_throws>>::value));

	EXPECT_TRUE(noexcept(swap(declval<error_or<int>&>(), declval<error_or<int>&>())));
	EXPECT_FALSE(noexcept(swap(declval<error_or<swap_throws>&>(), declval<error_or<swap_throws>&>())));

	EXPECT_TRUE((std::is_nothrow_move_assignable<error_or<int>>::value));
	EXPECT_FALSE((std::is_nothrow_move_assignable<error_or<ctor_throws>>::value));
}

TEST(error_or, validity)
{
	{
		error_or<int> d;

		EXPECT_FALSE(d);
		EXPECT_TRUE(!d);
	}
	{
		error_or<int> e(make_error_code(std::errc::invalid_argument));

		EXPECT_FALSE(e);
		EXPECT_TRUE(!e);
	}
	{
		error_or<int> v(0);

		EXPECT_TRUE(v);
		EXPECT_FALSE(!v);
	}
}

TEST(error_or, dtor)
{
	{
		called_mask called{};

		{
			error_or<track_calls> e;
		}
		EXPECT_FALSE(called.dtor);
	}

	{
		called_mask called{};

		{
			error_or<track_calls> e(make_error_code(std::errc::invalid_argument));
		}
		EXPECT_FALSE(called.dtor);
	}

	{
		called_mask called{};

		{
			error_or<track_calls> e{track_calls{called, 0}};
		}
		EXPECT_TRUE(called.dtor);
	}
}

TEST(error_or, swap)
{
	{
		error_or<track_calls> d1(make_error_code(std::errc::no_message_available));
		error_or<track_calls> d2(make_error_code(std::errc::argument_out_of_domain));

		swap(d1, d2);

		EXPECT_EQ(d1.error(), std::errc::argument_out_of_domain);
		EXPECT_EQ(d2.error(), std::errc::no_message_available);
	}
	{
		called_mask called = {};

		error_or<track_calls> d1(make_error_code(std::errc::io_error));
		error_or<track_calls> d2(track_calls{called, 1});

		swap(d1, d2);

		EXPECT_EQ(d1.value().v, 1);
		EXPECT_EQ(d2.error(), std::errc::io_error);
	}
	{
		called_mask called1 = {};
		called_mask called2 = {};

		error_or<track_calls> d1(track_calls{called1, 1});
		error_or<track_calls> d2(track_calls{called2, 2});

		swap(d1, d2);

		EXPECT_EQ(d1.value().v, 2);
		EXPECT_EQ(d2.value().v, 1);
	}
}

TEST(error_or, move_construct)
{
	{
		error_or<track_calls> d1;
		error_or<track_calls> d2(std::move(d1));

		EXPECT_FALSE(d1);
		EXPECT_FALSE(d2);
	}
	{
		called_mask called = {};

		error_or<track_calls> d2(track_calls{called, 1});
		error_or<track_calls> d1(std::move(d2));

		EXPECT_EQ(d1.value().v, 1);
		EXPECT_FALSE(d2);
		EXPECT_FALSE(called.swap);
	}
}

TEST(error_or, move_assign)
{
	{
		error_or<track_calls> d1;
		error_or<track_calls> d2;

		d2 = std::move(d1);

		EXPECT_FALSE(d1);
		EXPECT_FALSE(d2);
	}
	{
		called_mask called = {};

		error_or<track_calls> d2(track_calls{called, 1});
		error_or<track_calls> d1;

		d1 = std::move(d2);

		EXPECT_EQ(d1.value().v, 1);
		EXPECT_FALSE(d2);
		EXPECT_FALSE(called.swap);
	}
}

TEST(error_or, value__error)
{
	called_mask c;

	error_or<track_calls> err(make_error_code(std::errc::operation_not_supported));
	error_or<track_calls> value(track_calls{c, 1});

	ASSERT_THROW(err.value(), bad_error_or_access);
	ASSERT_THROW(value.error(), bad_error_or_access);
	ASSERT_THROW(static_cast<const error_or<track_calls>&&>(err).value(), bad_error_or_access);
	ASSERT_THROW(const_cast<const error_or<track_calls>&>(err).value(), bad_error_or_access);
	ASSERT_THROW(err.release_value(), bad_error_or_access);

	ASSERT_THROW(value.error(), bad_error_or_access);

	ASSERT_EQ(value.value().v, 1);
	ASSERT_EQ(const_cast<const error_or<track_calls>&>(value).value().v, 1);
	ASSERT_EQ(static_cast<const error_or<track_calls>&&>(error_or<track_calls>(value.value())).value().v, 1);
	ASSERT_EQ(error_or<track_calls>(value.value()).value().v, 1);
	ASSERT_EQ(error_or<track_calls>(value.value()).release_value().v, 1);
}

TEST(error_or, apply)
{
	called_mask c;

	error_or<track_calls> e(make_error_code(std::errc::io_error));
	error_or<track_calls> v(track_calls{c, 1});

	auto conv = [] (track_calls x) { return track_calls{*x.called, x.v + 1}; };
	auto convM = [] (track_calls&& x) { return track_calls{*x.called, x.v + 1}; };
	auto convI = [] (track_calls) { return 0; };
	auto convE = [] (std::error_code) { return 1.; };
	auto convC = [] (track_calls) -> error_or<char> { return 17; };

	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<const decltype(e)&>(e) / convI),
				error_or<int>>::value));
	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<decltype(e)&>(e) / convI),
				error_or<int>>::value));
	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<decltype(e)&&>(e) / convI),
				error_or<int>>::value));

	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<const decltype(e)&>(e).reduce(convI, convE)),
				double>::value));
	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<decltype(e)&>(e).reduce(convI, convE)),
				double>::value));
	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<decltype(e)&&>(e).reduce(convI, convE)),
				double>::value));
	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<const decltype(e)&>(e) % convC),
				error_or<char>>::value));
	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<decltype(e)&>(e) % convC),
				error_or<char>>::value));
	EXPECT_TRUE(
			(std::is_same<
				decltype(static_cast<decltype(e)&&>(e) % convC),
				error_or<char>>::value));

	{
		EXPECT_EQ((static_cast<const decltype(e)&>(e) / conv).error(), std::errc::io_error);
		EXPECT_EQ((static_cast<const decltype(e)&>(e) % convC).error(), std::errc::io_error);

		EXPECT_EQ((static_cast<const decltype(e)&>(e).reduce(convI, convE)), 1);

		EXPECT_EQ((static_cast<const decltype(v)&>(v) / conv).value().v, 2);
		EXPECT_EQ((static_cast<const decltype(v)&>(v) % convC).value(), 17);

		EXPECT_EQ((static_cast<const decltype(v)&>(v).reduce(convI, convE)), 0);
	}

	{
		EXPECT_EQ((static_cast<decltype(e)&>(e) / conv).error(), std::errc::io_error);
		EXPECT_EQ((static_cast<decltype(e)&>(e) % convC).error(), std::errc::io_error);

		EXPECT_EQ((static_cast<decltype(e)&>(e).reduce(convI, convE)), 1);

		EXPECT_EQ((static_cast<decltype(v)&>(v) / conv).value().v, 2);
		EXPECT_EQ((static_cast<decltype(v)&>(v) % convC).value(), 17);

		EXPECT_EQ((static_cast<decltype(v)&>(v).reduce(convI, convE)), 0);
	}

	{
		EXPECT_EQ((decltype(e)(e.error()) / convM).error(), std::errc::io_error);
		EXPECT_EQ((decltype(e)(e.error()) % convC).error(), std::errc::io_error);

		EXPECT_EQ((decltype(e)(e.error()).reduce(convI, convE)), 1);

		EXPECT_EQ((decltype(v)(v.value()) / convM).value().v, 2);
		EXPECT_EQ((decltype(v)(v.value()) % convC).value(), 17);

		EXPECT_EQ((decltype(v)(v.value()).reduce(convI, convE)), 0);
	}
}
