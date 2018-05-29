#include <common/toolkit/UnitTk.h>

#include <gtest/gtest.h>

TEST(UnitTk, gigabyteToByte)
{
   double gbValue = 1.0;
   int64_t byteValueExpected = 1073741824LL;
   int64_t byteValueCalc = UnitTk::gibibyteToByte(gbValue);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   gbValue = 10.0;
   byteValueExpected = 10737418240LL;
   byteValueCalc = UnitTk::gibibyteToByte(gbValue);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   gbValue = 10.598;
   byteValueExpected = 11379515850LL;

   byteValueCalc = UnitTk::gibibyteToByte(gbValue);
   ASSERT_EQ(byteValueExpected, byteValueCalc);
}

TEST(UnitTk, megabyteToByte)
{
   double mbValue = 1.0;
   int64_t byteValueExpected = 1048576LL;
   int64_t byteValueCalc = UnitTk::mebibyteToByte(mbValue);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   mbValue = 10.0;
   byteValueExpected = 10485760LL;
   byteValueCalc = UnitTk::mebibyteToByte(mbValue);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   mbValue = 10.598;
   byteValueExpected = 11112808LL;
   byteValueCalc = UnitTk::mebibyteToByte(mbValue);;
   ASSERT_EQ(byteValueExpected, byteValueCalc);
}

TEST(UnitTk, kilobyteToByte)
{
   double kbValue = 1.0;
   int64_t byteValueExpected = 1024LL;
   int64_t byteValueCalc = UnitTk::kibibyteToByte(kbValue);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   kbValue = 10.0;
   byteValueExpected = 10240LL;
   byteValueCalc = UnitTk::kibibyteToByte(kbValue);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   kbValue = 10.598;
   byteValueExpected = 10852LL;
   byteValueCalc = UnitTk::kibibyteToByte(kbValue);
   ASSERT_EQ(byteValueExpected, byteValueCalc);
}

TEST(UnitTk, byteToXbyte)
{
   std::string unit;
   int64_t value = 2048LL;
   double valueExpected = 2.0;
   double valueCalc = UnitTk::byteToXbyte(value, &unit);
   ASSERT_EQ(valueExpected, valueCalc);
   ASSERT_EQ(unit.compare("KiB"), 0);

   value = 10240LL;
   valueExpected = 10.0;
   valueCalc = UnitTk::byteToXbyte(value, &unit);
   ASSERT_EQ(valueExpected, valueCalc);
   ASSERT_EQ(unit.compare("KiB"), 0);

   value = 10843LL;
   valueExpected = 10.6;
   valueCalc = UnitTk::byteToXbyte(value, &unit, true);
   ASSERT_EQ(valueExpected, valueCalc);
   ASSERT_EQ(unit.compare("KiB"), 0);

   value = 1073741824LL;
   valueExpected = 1024.0;
   valueCalc = UnitTk::byteToXbyte(value, &unit);
   ASSERT_EQ(valueExpected, valueCalc);
   ASSERT_EQ(unit.compare("MiB"), 0);

   value = 10737418240LL;
   valueExpected = 10.0;
   valueCalc = UnitTk::byteToXbyte(value, &unit);
   ASSERT_EQ(valueExpected, valueCalc);
   ASSERT_EQ(unit.compare("GiB"), 0);

   value = 11446087843LL;
   valueExpected = 10.7;
   valueCalc = UnitTk::byteToXbyte(value, &unit, true);
   ASSERT_EQ(valueExpected, valueCalc);
   ASSERT_EQ(unit.compare("GiB"), 0);
}

TEST(UnitTk, xbyteToByte)
{
   std::string unit = "KiB";
   double value = 1.0;
   int64_t byteValueExpected = 1024LL;
   int64_t byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   value = 10.0;
   byteValueExpected = 10240LL;
   byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   value = 10.598;
   byteValueExpected = 10852LL;
   byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);


   unit = "MiB";
   value = 1.0;
   byteValueExpected = 1048576LL;
   byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   value = 10.0;
   byteValueExpected = 10485760LL;
   byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   value = 10.598;
   byteValueExpected = 11112808LL;
   byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);


   unit = "GiB";
   value = 1.0;
   byteValueExpected = 1073741824LL;
   byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   value = 10.0;
   byteValueExpected = 10737418240LL;
   byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);

   value = 10.598;
   byteValueExpected = 11379515850LL;
   byteValueCalc = UnitTk::xbyteToByte(value, unit);
   ASSERT_EQ(byteValueExpected, byteValueCalc);
}
