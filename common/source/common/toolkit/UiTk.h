#pragma once

#include <boost/optional.hpp>
#include <iostream>
#include <string>

namespace uitk {

bool userYNQuestion(const std::string& question,
                    boost::optional<bool> defaultAnswer=boost::none,
                    std::istream& input=std::cin);

}

