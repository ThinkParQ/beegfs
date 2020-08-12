#ifndef SET_H_
#define SET_H_

#include <common/threading/Mutex.h>
#include <database/SetFragment.h>
#include <database/SetFragmentCursor.h>
#include <database/Union.h>

#include <iomanip>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>

#include <limits.h>

#include <boost/format.hpp>

template<typename Data>
class Set {
   public:
      typedef SetFragment<Data> Fragment;
      typedef SetFragmentCursor<Data> Cursor;

   private:
      static const unsigned VERSION = 1;

      std::string basename;
      unsigned nextID;
      std::map<std::string, Fragment*> openFragments;
      bool dropped;

      Mutex mutex;

      typedef typename std::map<std::string, Fragment*>::iterator FragmentIter;

      Set(const Set&);
      Set& operator=(const Set&);

      std::string fragmentName(unsigned id)
      {
         std::stringstream ss;
         ss << basename << '.' << std::setw(5) << std::setfill('0') << id;
         return ss.str();
      }

      Fragment* getFragment(const std::string& file, bool allowCreate)
      {
         if(openFragments.count(file) )
            return openFragments[file];

         Fragment* fragment = new Fragment(file, allowCreate);
         openFragments[file] = fragment;
         return fragment;
      }

      void acceptFragment(Fragment* foreign)
      {
         const unsigned newID = nextID++;
         const std::string& file = fragmentName(newID);

         foreign->rename(file);

         openFragments[file] = foreign;
         saveConfig();
      }

      void removeFragment(const std::string& file)
      {
         Fragment* fragment = openFragments[file];
         openFragments.erase(file);
         fragment->drop();
         delete fragment;
      }

      void loadConfig(bool allowCreate)
      {
         std::ifstream in( (basename + ".t").c_str() );

         if(!in)
         {
            if(allowCreate)
               return;

            throw std::runtime_error("could not read set description file " + basename);
         }

         {
            char code;
            unsigned version;
            in >> code >> version;
            if (code != 'v' || version != VERSION) {
               throw std::runtime_error(str(
                        boost::format("cannot read set %1% with version %2%")
                           % basename
                           % version));
            }
         }

         in >> nextID;

         unsigned count = 0;
         in >> count;

         in.ignore(1, '\n');

         while(count > 0)
         {
            std::string file;

            std::getline(in, file);

            if(!in)
               break;

            getFragment(basename + file, false);
            count--;
         }

         if(!in || in.peek() != std::ifstream::traits_type::eof() )
            throw std::runtime_error("bad set description for " + basename);
      }

      void saveConfig()
      {
         std::ofstream out((basename + ".t").c_str(), std::ostream::trunc);

         out << 'v' << VERSION << '\n';
         out << nextID << '\n';
         out << openFragments.size() << '\n';

         for (FragmentIter it = openFragments.begin(), end = openFragments.end(); it != end; ++it) {
            if (it->second->filename().find(basename) != 0)
               throw std::runtime_error("set corrupted: " + basename);

            out << it->second->filename().substr(basename.size()) << '\n';
         }
      }

      static std::string makeAbsolute(const std::string& path)
      {
         std::string::size_type spos = path.find_last_of('/');
         if (spos == path.npos)
            spos = 0;
         else
            spos += 1;

         // can't/don't want to use ::basename because it may return static buffers
         const std::string basename = path.substr(spos);

         if (basename.empty() || basename == "." || basename == "..")
            throw std::runtime_error("bad path");

         if (path[0] == '/')
            return path;

         char cwd[PATH_MAX];

         if (::getcwd(cwd, sizeof(cwd)) == NULL)
            throw std::runtime_error("bad path");

         return cwd + ('/' + path);
      }

   public:
      Set(const std::string& basename, bool allowCreate = true)
         : basename(makeAbsolute(basename) ), nextID(0), dropped(false)
      {
         loadConfig(allowCreate);
      }

      ~Set()
      {
         if(dropped)
            return;

         saveConfig();

         for(FragmentIter it = openFragments.begin(), end = openFragments.end(); it != end; ++it)
            delete it->second;
      }

      size_t size()
      {
         size_t result = 0;

         for(FragmentIter it = openFragments.begin(), end = openFragments.end(); it != end; ++it)
            result += it->second->size();

         return result;
      }

      Fragment* newFragment()
      {
         const std::lock_guard<Mutex> lock(mutex);

         Fragment* result = getFragment(fragmentName(nextID++), true);
         saveConfig();

         return result;
      }

      void sort()
      {
         if(openFragments.empty() )
            newFragment();

         std::multimap<size_t, Fragment*> sortedFragments;

         for(FragmentIter it = openFragments.begin(), end = openFragments.end(); it != end; ++it)
            it->second->flush();

         for(FragmentIter it = openFragments.begin(), end = openFragments.end(); it != end; ++it)
         {
            it->second->sort();
            sortedFragments.insert(std::make_pair(it->second->size(), it->second) );
         }

         static const unsigned MERGE_WIDTH = 4;

         if(sortedFragments.size() == 1)
            return;

         saveConfig();

         while(sortedFragments.size() > 1)
         {
            Fragment* inputs[MERGE_WIDTH] = {};
            unsigned inputCount = 0;

            for(inputCount = 0; inputCount < MERGE_WIDTH; inputCount++)
            {
               if(sortedFragments.empty() )
                  break;

               inputs[inputCount] = sortedFragments.begin()->second;
               sortedFragments.erase(sortedFragments.begin());
            }

            SetFragment<Data>* merged = getFragment(fragmentName(nextID++), true);

            struct op
            {
               static typename Data::KeyType key(const Data& d) { return d.pkey(); }
            };
            typedef typename Data::KeyType (*key_t)(const Data&);
            typedef Union<Cursor, Cursor, key_t> L1Union;

            switch(inputCount)
            {
            case 2: {
               L1Union u(Cursor(*inputs[0]), (Cursor(*inputs[1]) ), op::key);
               while(u.step() )
                  merged->append(*u.get() );
               break;
            }

            case 3: {
               Union<L1Union, SetFragmentCursor<Data>, key_t> u(
                  L1Union(Cursor(*inputs[0]), (Cursor(*inputs[1]) ), op::key),
                  Cursor(*inputs[2]),
                  op::key);
               while(u.step() )
                  merged->append(*u.get() );
               break;
            }

            case 4: {
               Union<L1Union, L1Union, key_t> u(
                  L1Union(Cursor(*inputs[0]), (Cursor(*inputs[1]) ), op::key),
                  L1Union(Cursor(*inputs[2]), (Cursor(*inputs[3]) ), op::key),
                  op::key);
               while(u.step() )
                  merged->append(*u.get() );
               break;
            }

            default:
               throw std::runtime_error("");
            }

            merged->flush();

            sortedFragments.insert(std::make_pair(merged->size(), merged) );

            for (unsigned i = 0; i < inputCount; i++)
               removeFragment(inputs[i]->filename() );

            saveConfig();
         }
      }

      Cursor cursor()
      {
         sort();

         return Cursor(*openFragments.begin()->second);
      }

      template<typename Key, typename Fn>
      std::pair<bool, Data> getByKeyProjection(const Key& key, Fn fn)
      {
         sort();

         if(openFragments.empty() )
            return std::make_pair(false, Data() );

         return openFragments.begin()->second->getByKeyProjection(key, fn);
      }

      std::pair<bool, Data> getByKey(const typename Data::KeyType& key)
      {
         struct ops
         {
            static typename Data::KeyType key(const typename Data::KeyType& key) { return key; }
         };

         return getByKeyProjection(key, ops::key);
      }

      void clear()
      {
         while(!openFragments.empty() )
            removeFragment(openFragments.begin()->first);

         nextID = 0;
         saveConfig();
      }

      void drop()
      {
         clear();
         if(::unlink( (basename + ".t").c_str() ) )
            throw std::runtime_error("could not unlink set file " + basename + ": " + std::string(strerror(errno)));

         dropped = true;
      }

      void mergeInto(Set& other)
      {
         for(FragmentIter it = openFragments.begin(), end = openFragments.end(); it != end; ++it)
            other.acceptFragment(it->second);

         openFragments.clear();
         saveConfig();
      }

      void makeUnique()
      {
         sort();

         Fragment* input = openFragments.begin()->second;
         Cursor cursor(*input);
         typename Data::KeyType key;

         if(!cursor.step() )
            return;

         Fragment* unique = newFragment();
         unique->append(*cursor.get() );
         key = cursor.get()->pkey();

         while(cursor.step() )
         {
            if(key == cursor.get()->pkey() )
               continue;

            unique->append(*cursor.get() );
            key = cursor.get()->pkey();
         }

         unique->flush();

         removeFragment(input->filename() );
         saveConfig();
      }
};

#endif
