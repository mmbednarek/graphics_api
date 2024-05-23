#pragma once

#include "triglav/Name.hpp"

#include <set>
#include <map>
#include <string>
#include <optional>

namespace triglav::io {

class CommandLine {
 public:
   void parse(int argc, const char** argv);

   [[nodiscard]] bool is_enabled(Name flag) const;
   [[nodiscard]] std::optional<std::string> arg(Name flag) const;

   [[nodiscard]] static CommandLine& the();

 private:
   std::set<Name> m_enabledFlags;
   std::map<Name, std::string> m_arguments;
};

}