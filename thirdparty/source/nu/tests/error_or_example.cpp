using nu::error_or;
using nu::make_error_or;

// default-constructed objects contain Err(), which by default is std::error_code
error_or<int> e;
error_or<int> i(1);

// et will still contain Err()
error_or<int> et = e / [] (int i) { return i+1; };
ASSERT_EQ(et.error(), std::error_code());

// it will contain 2
error_or<int> it = i / [] (int i) { return i+1; };
ASSERT_EQ(it.value(), 2);

// er will be set to -1
int er = e.reduce(
    [] (int i) { return i; },
    [] (std::error_code) { return -1; });
ASSERT_EQ(er, -1);

// ir will be set to 1
int ir = i.reduce(
    [] (int i) { return i; },
    [] (std::error_code) { return -1; });
ASSERT_EQ(ir, 1);

// % can be used to chain operations that may themselves fail
error_or<double> d =
    make_error_or<int>(1)
    / [] (int i) { return i - 1; }
    % [] (int i) -> error_or<double> {
        if (i != 0)
            return i * 3.14;
        else
            return {std::make_error_code(std::errc::invalid_argument)};
    };
ASSERT_FALSE(d);
