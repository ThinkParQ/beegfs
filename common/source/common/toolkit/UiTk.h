#ifndef COMMON_UITK_H_
#define COMMON_UITK_H_

#include <boost/optional.hpp>
#include <iostream>
#include <string>

namespace uitk {

bool userYNQuestion(const std::string& question,
                    boost::optional<bool> defaultAnswer=boost::none,
                    std::istream& input=std::cin);

}

#endif  //COMMON_UITK_H_
