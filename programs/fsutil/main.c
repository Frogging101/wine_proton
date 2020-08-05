/*
 * Copyright 2016 Austin English
 * Copyright 2016 Michael Müller
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <windows.h>

#include "wine/debug.h"
#include "resources.h"

WINE_DEFAULT_DEBUG_CHANNEL(fsutil);

static void output_write(const WCHAR *str, DWORD wlen)
{
    DWORD count, ret;

    ret = WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), str, wlen, &count, NULL);
    if (!ret)
    {
        DWORD len;
        char  *msgA;

        /* On Windows WriteConsoleW() fails if the output is redirected. So fall
         * back to WriteFile(), assuming the console encoding is still the right
         * one in that case.
         */
        len = WideCharToMultiByte(GetConsoleOutputCP(), 0, str, wlen, NULL, 0, NULL, NULL);
        msgA = HeapAlloc(GetProcessHeap(), 0, len * sizeof(char));
        if (!msgA) return;

        WideCharToMultiByte(GetConsoleOutputCP(), 0, str, wlen, msgA, len, NULL, NULL);
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msgA, len, &count, FALSE);
        HeapFree(GetProcessHeap(), 0, msgA);
    }
}

static int WINAPIV output_string(int msg, ...)
{
    WCHAR out[8192];
    __ms_va_list arguments;
    int len;

    __ms_va_start(arguments, msg);
    len = FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE, NULL, msg, 0, out, ARRAY_SIZE(out), &arguments);
    __ms_va_end(arguments);

    if (len == 0 && GetLastError() != NO_ERROR)
        WINE_FIXME("Could not format string: le=%u, msg=%d\n", GetLastError(), msg);
    else
        output_write(out, len);

    return 0;
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    int i, ret = 0;

    TRACE("Command line:");
    for (i = 0; i < argc; i++)
        TRACE(" %s", debugstr_w(argv[i]));
    TRACE("\n");

    if (argc > 1)
    {
        FIXME("unsupported command %s\n", debugstr_w(argv[1]));
        ret = 1;
    }

    output_string(STRING_USAGE);
    return ret;
}
