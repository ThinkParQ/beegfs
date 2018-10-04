#include "UiTk.h"

#include <boost/algorithm/string.hpp>

bool uitk::userYNQuestion(const std::string& question,
                          boost::optional<bool> defaultAnswer,
                          std::istream& input)
{
   while (true) {
      std::cout << question
                << " ["
                << (defaultAnswer.value_or(false) ? 'Y' : 'y')
                << "/"
                << (defaultAnswer.value_or(true)  ? 'n' : 'N')
                << "] ? " << std::flush;

      // read answer from input stream
      const auto answer = [&input] () {
         std::string answer;
         std::getline(input, answer);
         boost::to_upper(answer);
         boost::trim(answer);
         return answer;
      }();

      if (answer.empty())
      {
         if (defaultAnswer.is_initialized())
         {
            return defaultAnswer.get();
         }
         else
         {
            std::cin.clear();
            std::cout << std::endl;
         }
      }

      if ("Y" == answer || "YES" == answer)
         return true;

      if ("N" == answer || "NO" == answer)
         return false;
   }

}
