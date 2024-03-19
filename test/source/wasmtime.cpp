#include <doctest/doctest.h>

#include <iostream>
#include <string>
#include <val.hpp>

TEST_CASE("component-model-cpp") {
  std::cout << "component-model-cpp" << std::endl;
  // using namespace greeter;

  // Greeter greeter("Tests");

  // CHECK(greeter.greet(LanguageCode::EN) == "Hello, Tests!");
  // CHECK(greeter.greet(LanguageCode::DE) == "Hallo Tests!");
  // CHECK(greeter.greet(LanguageCode::ES) == "¡Hola Tests!");
  // CHECK(greeter.greet(LanguageCode::FR) == "Bonjour Tests!");
}

TEST_CASE("component-model-cpp version") {
  // static_assert(std::string_view(GREETER_VERSION) == std::string_view("1.0"));
  // CHECK(std::string(GREETER_VERSION) == std::string("1.0"));
}
