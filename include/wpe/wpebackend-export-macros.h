#pragma once

#ifdef _MSC_VER
#ifdef WPEBackend_EXPORTS
#define WPEBackend_EXPORT __declspec(dllexport)
#else
#define WPEBackend_EXPORT __declspec(dllimport)
#endif
#else
#define WPEBackend_EXPORT __attribute__((visibility("default")))
#endif
