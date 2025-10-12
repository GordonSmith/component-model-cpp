#pragma once

#include "types.hpp"
#include "utils.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>

// Type mapper from WIT types to cmcpp types
class TypeMapper
{
private:
    static const std::vector<InterfaceInfo> *all_interfaces;

public:
    static void setInterfaces(const std::vector<InterfaceInfo> *interfaces);
    static std::string mapType(const std::string &witType, const InterfaceInfo *iface = nullptr);
};
