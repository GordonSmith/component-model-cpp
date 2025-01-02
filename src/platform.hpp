#pragma once

#ifdef _WIN32
#ifdef CMCPP_EXPORTS
#define CMCPP_API __declspec(dllexport)
#else
#define CMCPP_API __declspec(dllimport)
#endif
#else
#define CMCPP_API
#endif
