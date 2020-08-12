#include <linux/limits.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <mntent.h>

#include <common/system/System.h>
#include <toolkit/XAttrTk.h>
#include <sys/xattr.h>

#include "testing/syscall_override.h"

#include <gtest/gtest.h>

TEST (XAttrTk_getXAttrNames, properly_forwards_path)
{
   //! \todo testing::random_string()
   std::string const path ("flkjelrkjlakscnlnselr");
   std::string seen_path;
   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [&] (const char* actual, char*, size_t) -> ssize_t
        {
           seen_path = actual;
           return 0;
        }
      );

   XAttrTk::getXAttrNames (path);

   ASSERT_EQ (path, seen_path);
}

TEST (XAttrTk_getXAttrNames, gives_large_enough_accessible_buffer)
{
   std::size_t seen_size;

   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [&] (const char*, char* buffer, size_t size) -> ssize_t
        {
           seen_size = size;
           std::fill (buffer, buffer + size, 0);
           return 0;
        }
      );

   XAttrTk::getXAttrNames ("");

   ASSERT_EQ (seen_size, size_t(XATTR_LIST_MAX));
}

TEST (XAttrTk_getXAttrNames, errors_throw)
{
   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [] (const char*, char*, size_t) -> ssize_t
        {
           errno = ENAMETOOLONG;
           return -1;
        }
      );

   ASSERT_THROW (XAttrTk::getXAttrNames (""), XAttrException);
}

TEST (XAttrTk_getXAttrNames, no_attributes)
{
   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [] (const char*, char*, size_t) -> ssize_t
        {
           return 0;
        }
      );

   std::vector<std::string> const names (XAttrTk::getXAttrNames (""));

   ASSERT_EQ (names, std::vector<std::string>());
}

TEST (XAttrTk_getXAttrNames, one_attribute)
{
   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [] (const char*, char* buffer, size_t) -> ssize_t
        {
           buffer[0] = 'a';
           buffer[1] = 'b';
           buffer[2] = 'c';
           buffer[3] = '\0';
           return 4;
        }
      );

   std::vector<std::string> const names (XAttrTk::getXAttrNames (""));

   ASSERT_EQ (names, std::vector<std::string> ({"abc"}));
}

TEST (XAttrTk_getXAttrNames, multiple_attributes)
{
   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [] (const char*, char* buffer, size_t) -> ssize_t
        {
           buffer[0] = 'a';
           buffer[1] = '\0';
           buffer[2] = 'b';
           buffer[3] = '\0';
           buffer[4] = 'c';
           buffer[5] = '\0';
           buffer[6] = 'd';
           buffer[7] = '\0';
           return 8;
        }
      );

   std::vector<std::string> const names (XAttrTk::getXAttrNames (""));

   ASSERT_EQ (names, std::vector<std::string> ({"a", "b", "c", "d"}));
}

TEST (XAttrTk_getXAttrNames, maximum_number_of_attributes)
{
   size_t expected_entries;
   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [&] (const char*, char* buffer, size_t size) -> ssize_t
        {
           // due to how stupid we fill the buffer only
           EXPECT_EQ (size % 2, 0u);
           expected_entries = size / 2;
           size_t pos (0);
           for (; pos < size; pos += 2)
           {
              buffer[pos] = 'x';
              buffer[pos + 1] = '\0';
           }
           return pos;
        }
      );

   std::vector<std::string> const names (XAttrTk::getXAttrNames (""));

   ASSERT_EQ (names.size(), expected_entries);
   for (auto& name : names)
   {
      ASSERT_EQ (name, "x");
   }
}


TEST (XAttrTk_getXAttrValue, properly_forwards_path_and_name)
{
   //! \todo testing::random_string()
   std::string const path ("flkjelrkjlakscnlnselr");
   std::string const name ("xcvbxcvbfdteru6er6elr");
   std::string seen_path;
   std::string seen_name;
   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [&] (const char* actual_path, const char* actual_name, void*, size_t) -> ssize_t
        {
           seen_path = actual_path;
           seen_name = actual_name;
           return 0;
        }
      );

   XAttrTk::getXAttrValue (path, name);

   ASSERT_EQ (path, seen_path);
   ASSERT_EQ (name, seen_name);
}

TEST (XAttrTk_getXAttrValue, gives_large_enough_accessible_buffer)
{
   std::size_t seen_size;

   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [&] (const char*, const char*, void* buffer, size_t size) -> ssize_t
        {
           seen_size = size;
           std::fill ( static_cast<char*> (buffer)
                     , static_cast<char*> (buffer) + size
                     , 0
                     );
           return 0;
        }
      );

   XAttrTk::getXAttrValue ("", "");

   ASSERT_EQ (seen_size, std::size_t(XATTR_SIZE_MAX));
}

TEST (XAttrTk_getXAttrValue, errors_throw)
{
   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [] (const char*, const char*, void*, size_t) -> ssize_t
        {
           errno = ENAMETOOLONG;
           return -1;
        }
      );

   ASSERT_THROW (XAttrTk::getXAttrValue ("", ""), XAttrException);
}

TEST (XAttrTk_getXAttrValue, empty_contents)
{
   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [] (const char*, const char*, void*, size_t) -> ssize_t
        {
           return 0;
        }
      );

   XAttrValue const data (XAttrTk::getXAttrValue ("", ""));

   ASSERT_EQ (data, XAttrValue());
}

TEST (XAttrTk_getXAttrValue, data_containing_nulls)
{
   XAttrValue const returned_data {'a', '\0', 'b', '\0', 'c'};

   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [&] (const char*, const char*, void* buffer, size_t) -> ssize_t
        {
           std::copy ( returned_data.begin(), returned_data.end()
                     , static_cast<char*> (buffer)
                     );
           return returned_data.size();
        }
      );

   XAttrValue const data (XAttrTk::getXAttrValue ("", ""));

   ASSERT_EQ (data, returned_data);
}

TEST (XAttrTk_getXAttrValue, huge_data)
{
   XAttrValue const returned_data (XATTR_SIZE_MAX, 'X');

   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [&] (const char*, const char*, void* buffer, size_t) -> ssize_t
        {
           std::copy ( returned_data.begin(), returned_data.end()
                     , static_cast<char*> (buffer)
                     );
           return returned_data.size();
        }
      );

   XAttrValue const data (XAttrTk::getXAttrValue ("", ""));

   ASSERT_EQ (data, returned_data);
}


TEST (XAttrTk_getXAttrs, gets_values_only_if_given_names)
{
   int llistxattr_calls (0);
   int lgetxattr_calls (0);

   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [&] (const char*, char*, size_t) -> ssize_t
        {
           ++llistxattr_calls;
           return 0;
        }
      );
   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [&] (const char*, const char*, void* buffer, size_t) -> ssize_t
        {
           ++lgetxattr_calls;
           return 0;
        }
      );

   XAttrMap const attributes (XAttrTk::getXAttrs (""));

   ASSERT_EQ (attributes, XAttrMap());
   ASSERT_EQ (llistxattr_calls, 1);
   ASSERT_EQ (lgetxattr_calls, 0);
}

TEST (XAttrTk_getXAttrs, gets_values_for_all_listed_names)
{
   XAttrMap const returned_values {{"a", {}}, {"b", {}}};
   std::vector<std::string> const expected_requested_names ({"a", "b"});

   int llistxattr_calls (0);
   int lgetxattr_calls (0);

   std::vector<std::string> requested_names;

   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [&] (const char*, char* buffer, size_t) -> ssize_t
        {
           ++llistxattr_calls;
           buffer[0] = 'a';
           buffer[1] = '\0';
           buffer[2] = 'b';
           buffer[3] = '\0';
           return 4;
        }
      );
   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [&] (const char*, const char* name, void* buffer, size_t) -> ssize_t
        {
           ++lgetxattr_calls;
           requested_names.push_back (name);
           auto const& value (returned_values.at (name));
           std::copy ( value.begin(), value.end()
                     , static_cast<char*> (buffer)
                     );
           return value.size();
        }
      );

   XAttrMap const attributes (XAttrTk::getXAttrs (""));

   ASSERT_EQ (attributes, returned_values);
   ASSERT_EQ (llistxattr_calls, 1);
   ASSERT_EQ (lgetxattr_calls, 2);
   ASSERT_EQ (requested_names, expected_requested_names);
}

TEST (XAttrTk_getXAttrs, throws_on_list_error)
{
   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [&] (const char*, char*, size_t) -> ssize_t
        {
           errno = ENOTSUP;
           return -1;
        }
      );
   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [&] (const char*, const char*, void* buffer, size_t) -> ssize_t
        {
           return 0;
        }
      );

   ASSERT_THROW (XAttrTk::getXAttrs (""), XAttrException);
}

TEST (XAttrTk_getXAttrs, throws_on_get_error)
{
   beegfs::testing::syscall::scoped_override const _llistxattr
      ( beegfs::testing::syscall::llistxattr
      , [&] (const char*, char* buffer, size_t) -> ssize_t
        {
           buffer[0] = 'a';
           buffer[1] = '\0';
           return 2;
        }
      );
   beegfs::testing::syscall::scoped_override const _lgetxattr
      ( beegfs::testing::syscall::lgetxattr
      , [&] (const char*, const char*, void* buffer, size_t) -> ssize_t
        {
           errno = ENOTSUP;
           return -1;
        }
      );

   ASSERT_THROW (XAttrTk::getXAttrs (""), XAttrException);
}


TEST (XAttrTk_setXAttrs, properly_forwards_parameters)
{
   //! \todo testing::random_string()
   std::string const path ("flkjelrkjlakscnlnselr");
   std::string const name ("xcvbxcvbfdteru6er6elr");
   std::vector<char> const value {6, 34, 7, 4, 4, 8, 6, 1, 52};
   std::string seen_path;
   std::string seen_name;
   std::vector<char> seen_value;
   int seen_flags;
   beegfs::testing::syscall::scoped_override const _lsetxattr
      ( beegfs::testing::syscall::lsetxattr
      , [&] ( const char* actual_path
            , const char* actual_name
            , const void* buffer
            , size_t size
            , int flags
            ) -> int
        {
           seen_path = actual_path;
           seen_name = actual_name;
           auto const b (static_cast<char const*> (buffer));
           seen_value = std::vector<char> (b, b + size);
           seen_flags = flags;
           return 0;
        }
      );

   XAttrTk::setXAttrs (path, XAttrMap {{name, value}});

   ASSERT_EQ (seen_path, path);
   ASSERT_EQ (seen_name, name);
   ASSERT_EQ (seen_value, value);
   ASSERT_EQ (seen_flags, XATTR_CREATE);
}

TEST (XAttrTk_setXAttrs, calls_for_each_attr_and_passes_correct_data)
{
   XAttrMap const attrs
     { {"a", {'\0', 6}}
     , {"b", {1, 6, 3}}
     , {"d", {}}
     , {std::string (XATTR_LIST_MAX, 'x'), std::vector<char> (XATTR_SIZE_MAX, 'X')}
     };
   XAttrMap requested_attrs;
   beegfs::testing::syscall::scoped_override const _lsetxattr
      ( beegfs::testing::syscall::lsetxattr
      , [&] (const char*, const char* name, const void* buffer, size_t size, int) -> int
        {
           auto const b (static_cast<char const*> (buffer));
           requested_attrs[name] = std::vector<char> (b, b + size);
           return 0;
        }
      );

   XAttrTk::setXAttrs ("", attrs);

   ASSERT_EQ (requested_attrs, attrs);
}

TEST (XAttrTk_setXAttrs, calls_nothing_if_no_attrs_given)
{
   int call_count (0);
   beegfs::testing::syscall::scoped_override const _lsetxattr
      ( beegfs::testing::syscall::lsetxattr
      , [&] (const char*, const char* name, const void* buffer, size_t size, int) -> int
        {
           ++call_count;
           return 0;
        }
      );

   XAttrTk::setXAttrs ("", XAttrMap());

   ASSERT_EQ (call_count, 0);
}

TEST (XAttrTk_setXAttrs, throws_on_failure)
{
   beegfs::testing::syscall::scoped_override const _lsetxattr
      ( beegfs::testing::syscall::lsetxattr
      , [&] (const char*, const char* name, const void* buffer, size_t size, int) -> int
        {
           errno = -ENOSPC;
           return -1;
        }
      );

   ASSERT_THROW (XAttrTk::setXAttrs ("", XAttrMap {{{}, {}}}), XAttrException);
}
