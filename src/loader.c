/*
 * Copyright (C) 2015, 2016 Igalia S.L.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "loader-private.h"

#ifdef WIN32
#include <windows.h>
typedef HMODULE LibraryHandle;
#else
#include <dlfcn.h>
#include <stdarg.h>
typedef void * LibraryHandle;
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void* s_impl_library = 0;
static struct wpe_loader_interface* s_impl_loader = 0;

void report_error(const char * fmt, ...)
{
    char msg[512];
    va_list valist;
    va_start(valist, fmt);
    size_t count = vsnprintf(msg, sizeof(msg), fmt, valist);
    fwrite(msg, sizeof(char), count, stderr);
#ifdef WIN32
    // cause nobody pays attention to stderr on windows...
    MessageBoxA(NULL, msg, "Error", MB_ICONERROR | MB_OK);
#endif
    va_end(valist);
}

#ifdef WIN32
void report_last_error(const char * message)
{
    //https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
    DWORD errorMessageID = GetLastError();
    if (errorMessageID) {
        LPSTR messageBuffer = NULL;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
        report_error("wpe: %s: %s\n", message, messageBuffer);
        LocalFree(messageBuffer);
    }
}
#endif

LibraryHandle openlibrary(const char * library_name, const char * context)
{
#ifdef WIN32
    LibraryHandle handle = LoadLibraryA(library_name);
    if (!handle) {
        char message[256];
        sprintf(message, "Failed to load library %s (%s)", library_name, context);
        report_last_error(message);
    }
#else
    LibraryHandle handle = dlopen(library_name, RTLD_NOW);
    if (!handle) {
        report_error("wpe: could not load %s: %s\n", context, dlerror());
    }
#endif
    return handle;
}

void * loadsymbol(LibraryHandle library, const char * symbol)
{
#ifdef WIN32
    void * ret = GetProcAddress(library, symbol);
    if (!ret) {
        char message[256];
        sprintf(message, "Failed to load symbol %s", symbol);
        report_last_error(message);
    }
    return ret;
#else
    return dlsym(library, symbol);
#endif
}

void
load_impl_library()
{
#ifdef WPE_BACKEND
    s_impl_library = openlibrary(WPE_BACKEND, "compile-time defined WPE_BACKEND");
#else
#ifdef WIN32
    static const char library_name[] = "WPEBackend-default.dll";
#else
    static const char library_name[] = "libWPEBackend-default.so";
#endif

    // Get the impl library from an environment variable.
    char* env_library_name = getenv("WPE_BACKEND_LIBRARY");
    if (env_library_name) {
        s_impl_library = openlibrary(env_library_name, "WPE_BACKEND_LIBRARY environment variable");
    } else {
        // Load libWPEBackend-default.so by ... default.
        s_impl_library = openlibrary(library_name, "Default backend library");
    }
#endif
    if (!s_impl_library) {
        abort();
    }

    s_impl_loader = loadsymbol(s_impl_library, "_wpe_loader_interface");
}

void*
wpe_load_object(const char* object_name)
{
    if (!s_impl_library)
        load_impl_library();

    if (s_impl_loader)
        return s_impl_loader->load_object(object_name);

    void* object = loadsymbol(s_impl_library, object_name);
    if (!object)
        report_error("wpe_load_object: failed to load object with name '%s'\n", object_name);

    return object;
}
