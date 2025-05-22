// Copyright 2021 Alexey Kutepov <reximkut@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// ============================================================
//
// minirent — 0.0.1 — A subset of dirent interface for Windows.
//
// https://github.com/tsoding/minirent
//
// ============================================================
//
// ChangeLog (https://semver.org/ is implied)
//
//    0.0.2 Automatically include dirent.h on non-Windows
//          platforms
//    0.0.1 First Official Release

#ifndef _WIN32
#include <dirent.h>
#else // _WIN32

#ifndef MINIRENT_H_
#define MINIRENT_H_

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct dirent
{
    char d_name[MAX_PATH];
};

typedef struct DIR DIR;

DIR *opendir(const char *dirpath);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#endif // MINIRENT_H_

#ifdef MINIRENT_IMPLEMENTATION

struct DIR
{
    HANDLE hFind;
    WIN32_FIND_DATAW data;
    struct dirent *dirent;
};

DIR *opendir(const char *dirpath)
{
    assert(dirpath != NULL);
    if (!(*dirpath)) {
        errno = ENOENT;
        return NULL;
    }

    int charCount;
    char buffer[MAX_PATH];
    wchar_t wBuffer[MAX_PATH];

    charCount = snprintf(buffer, MAX_PATH, "%s\\*", dirpath);   // snprintf return DONT count the NULL terminator
    if(charCount < 1 || charCount >= MAX_PATH) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    DIR *dir = (DIR *)calloc(1, sizeof(DIR));

    SetLastError(0);
    charCount = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wBuffer, MAX_PATH);
    if (charCount < 1 || charCount > MAX_PATH) {                // MultiByteToWideChar return count the NULL terminator
        switch (GetLastError()) {
        case ERROR_INSUFFICIENT_BUFFER:
            errno = ENAMETOOLONG;
            break;
        case ERROR_INVALID_FLAGS:
        case ERROR_INVALID_PARAMETER:
            errno = EINVAL;
            break;
        case ERROR_NO_UNICODE_TRANSLATION:
            errno = EILSEQ;
            break;
        default:
            errno = ENOSYS;
            break;
        }
        goto fail;
    }

    dir->hFind = FindFirstFileW(wBuffer, &dir->data);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        // TODO: opendir should set errno accordingly on FindFirstFile fail
        // https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror
        DWORD err = GetLastError();
        if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
            errno = ENOENT;
        else
            errno = ENOSYS;
        goto fail;
    }

    return dir;

fail:
    if (dir) {
        free(dir);
    }

    return NULL;
}

struct dirent *readdir(DIR *dirp)
{
    assert(dirp != NULL);

    if (dirp->dirent == NULL) {
        dirp->dirent = (struct dirent *)calloc(1, sizeof(struct dirent));
    } else {
        if(!FindNextFileW(dirp->hFind, &dirp->data)) {
            if (GetLastError() != ERROR_NO_MORE_FILES) {
                // TODO: readdir should set errno accordingly on FindNextFile fail
                // https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror
                errno = ENOSYS;
            }

            return NULL;
        }
    }

    memset(dirp->dirent->d_name, 0, sizeof(dirp->dirent->d_name));

    // Set errno or just crash if conversion failed???
    SetLastError(0);
    int charCount = WideCharToMultiByte(CP_UTF8, 0, dirp->data.cFileName, -1, dirp->dirent->d_name, sizeof(dirp->dirent->d_name), NULL, NULL);

#if 1
    assert((charCount > 0 && charCount <= (int)sizeof(dirp->dirent->d_name)));  // WideCharToMultiByte return count the NULL terminator
#else
    if (charCount < 1 || charCount > (int)sizeof(dirp->dirent->d_name)) {       // WideCharToMultiByte return count the NULL terminator
        switch (GetLastError())
        {
        case ERROR_INSUFFICIENT_BUFFER:
            errno = ENAMETOOLONG;
            break;
        case ERROR_INVALID_FLAGS:
        case ERROR_INVALID_PARAMETER:
            errno = EINVAL;
            break;
        case ERROR_NO_UNICODE_TRANSLATION:
            errno = EILSEQ;
            break;
        default:
            errno = ENOSYS;
            break;
        }
        return NULL;
    }
#endif

    return dirp->dirent;
}

int closedir(DIR *dirp)
{
    assert(dirp != NULL);

    if(!FindClose(dirp->hFind)) {
        // TODO: closedir should set errno accordingly on FindClose fail
        // https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror
        errno = ENOSYS;
        return -1;
    }

    if (dirp->dirent) {
        free(dirp->dirent);
    }
    free(dirp);

    return 0;
}

#endif // MINIRENT_IMPLEMENTATION
#endif // _WIN32
