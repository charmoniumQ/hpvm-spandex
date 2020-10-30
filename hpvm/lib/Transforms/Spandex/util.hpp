#pragma once
#include <string>
#include <regex>
#include <cxxabi.h>
#include "enum.h"
#include "result.h"

using str = std::string;

template <typename T>
using ResultStr = Result<T, str>;

str demangle(const std::string &input) {
  int status = -1;
  std::string real_input =
      input.substr(input.size() - 7) == std::string{"_cloned"}
          ? input.substr(0, input.size() - 7)
          : input;

  char *output =
      abi::__cxa_demangle(real_input.c_str(), nullptr, nullptr, &status);
  if (status == 0) {
    std::string real_output =
        std::regex_replace(output, std::regex("\\(.*\\)"), "");
    return real_output;
  } else {
    // dbgs() << "abi::__cxa_demangle returned " << status << " on " <<
    // real_input
    //        << "\n";
    return real_input;
  }
}

template <typename T>
T& ptr2ref(T* t) {
	assert(t);
	return *t;
}

template <typename T>
const T& ptr2ref(const T* t) {
	assert(t);
	return *t;
}
