/*
 * winfstest.c
 *
 * Copyright (c) 2015, Bill Zissimopoulos. All rights reserved.
 *
 * Redistribution  and use  in source  and  binary forms,  with or  without
 * modification, are  permitted provided that the  following conditions are
 * met:
 *
 * 1.  Redistributions  of source  code  must  retain the  above  copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions  in binary  form must  reproduce the  above copyright
 * notice,  this list  of conditions  and the  following disclaimer  in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3.  Neither the  name  of the  copyright  holder nor  the  names of  its
 * contributors may  be used  to endorse or  promote products  derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY  THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND  ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING, BUT  NOT LIMITED
 * TO,  THE  IMPLIED  WARRANTIES  OF  MERCHANTABILITY  AND  FITNESS  FOR  A
 * PARTICULAR  PURPOSE ARE  DISCLAIMED.  IN NO  EVENT  SHALL THE  COPYRIGHT
 * HOLDER OR CONTRIBUTORS  BE LIABLE FOR ANY  DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL   DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO,  PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES; LOSS  OF USE,
 * DATA, OR  PROFITS; OR BUSINESS  INTERRUPTION) HOWEVER CAUSED AND  ON ANY
 * THEORY  OF LIABILITY,  WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE  OR OTHERWISE) ARISING IN  ANY WAY OUT OF  THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <windows.h>
#include <sddl.h>
#include <inttypes.h>
#include <stdio.h>

static int do_CreateFile(int argc, wchar_t **argv);
static int do_DeleteFile(int argc, wchar_t **argv);
static int do_CreateDirectory(int argc, wchar_t **argv);
static int do_RemoveDirectory(int argc, wchar_t **argv);
static int do_GetFileInformation(int argc, wchar_t **argv);
static int do_SetFileAttributes(int argc, wchar_t **argv);
static int do_SetFileTime(int argc, wchar_t **argv);
static int do_SetEndOfFile(int argc, wchar_t **argv);
static int do_GetFileSecurity(int argc, wchar_t **argv);
static int do_SetFileSecurity(int argc, wchar_t **argv);
static int do_FindFiles(int argc, wchar_t **argv);
static int do_FindStreams(int argc, wchar_t **argv);
static int do_MoveFileEx(int argc, wchar_t **argv);
static int do_RenameStream(int argc, wchar_t **argv);

static int always_print_last_error = 0;
static int wait_for_input_before_exit = 0;

static void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(1);
}

#define SYM(n)                          { L#n, n }
struct sym
{
    const wchar_t *name;
    DWORD value;
};
struct sym symtab[] =
{
    SYM(GENERIC_ALL),
    SYM(GENERIC_EXECUTE),
    SYM(GENERIC_READ),
    SYM(GENERIC_WRITE),
    SYM(DELETE),
    SYM(FILE_SHARE_READ),
    SYM(FILE_SHARE_WRITE),
    SYM(FILE_SHARE_DELETE),
    SYM(CREATE_ALWAYS),
    SYM(CREATE_NEW),
    SYM(OPEN_ALWAYS),
    SYM(OPEN_EXISTING),
    SYM(TRUNCATE_EXISTING),
    SYM(FILE_ATTRIBUTE_READONLY),
    SYM(FILE_ATTRIBUTE_HIDDEN),
    SYM(FILE_ATTRIBUTE_SYSTEM),
    SYM(FILE_ATTRIBUTE_DIRECTORY),
    SYM(FILE_ATTRIBUTE_ARCHIVE),
    SYM(FILE_ATTRIBUTE_DEVICE),
    SYM(FILE_ATTRIBUTE_NORMAL),
    SYM(FILE_ATTRIBUTE_TEMPORARY),
    SYM(FILE_ATTRIBUTE_SPARSE_FILE),
    SYM(FILE_ATTRIBUTE_REPARSE_POINT),
    SYM(FILE_ATTRIBUTE_COMPRESSED),
    SYM(FILE_ATTRIBUTE_OFFLINE),
    SYM(FILE_ATTRIBUTE_NOT_CONTENT_INDEXED),
    SYM(FILE_ATTRIBUTE_ENCRYPTED),
    SYM(FILE_ATTRIBUTE_INTEGRITY_STREAM),
    SYM(FILE_ATTRIBUTE_VIRTUAL),
    SYM(FILE_ATTRIBUTE_NO_SCRUB_DATA),
    SYM(FILE_ATTRIBUTE_EA),
    SYM(FILE_FLAG_WRITE_THROUGH),
    SYM(FILE_FLAG_OVERLAPPED),
    SYM(FILE_FLAG_NO_BUFFERING),
    SYM(FILE_FLAG_RANDOM_ACCESS),
    SYM(FILE_FLAG_SEQUENTIAL_SCAN),
    SYM(FILE_FLAG_DELETE_ON_CLOSE),
    SYM(FILE_FLAG_BACKUP_SEMANTICS),
    SYM(FILE_FLAG_POSIX_SEMANTICS),
    SYM(FILE_FLAG_SESSION_AWARE),
    SYM(FILE_FLAG_OPEN_REPARSE_POINT),
    SYM(FILE_FLAG_OPEN_NO_RECALL),
    SYM(FILE_FLAG_FIRST_PIPE_INSTANCE),
    SYM(OWNER_SECURITY_INFORMATION),
    SYM(GROUP_SECURITY_INFORMATION),
    SYM(DACL_SECURITY_INFORMATION),
    SYM(SACL_SECURITY_INFORMATION),
    SYM(MOVEFILE_REPLACE_EXISTING),
    SYM(MOVEFILE_COPY_ALLOWED),
    SYM(MOVEFILE_DELAY_UNTIL_REBOOT),
    SYM(MOVEFILE_WRITE_THROUGH),
    SYM(MOVEFILE_CREATE_HARDLINK),
    SYM(MOVEFILE_FAIL_IF_NOT_TRACKABLE),
};
static int symcmp(const void *p1, const void *p2)
{
    const struct sym *sym1 = (const struct sym *)p1;
    const struct sym *sym2 = (const struct sym *)p2;
    return wcscmp(sym1->name, sym2->name);
}
static void syminit(void)
{
    qsort(symtab, sizeof symtab / sizeof symtab[0], sizeof symtab[0], symcmp);
}
static struct sym *symget(const wchar_t *name)
{
    struct sym symkey = { .name = name };
    return bsearch(&symkey, symtab, sizeof symtab / sizeof symtab[0], sizeof symtab[0], symcmp);
}
static DWORD symval(const wchar_t *name)
{
    DWORD value = 0;
    while (name[0])
    {
        wchar_t part[128], *endp;
        endp = wcschr(name, L'+');
        if (0 == endp)
            endp = (wchar_t *)name + wcslen(name);
        if (endp - name >= sizeof part)
            fail("invalid constant %S", name);
        memcpy(part, name, (endp - name) * sizeof(wchar_t));
        part[endp - name] = L'\0';
        name = *endp ? endp + 1 : endp;
        value += wcstoul(part, &endp, 0);
        if (L'\0' != *endp)
        {
            struct sym *sym = symget(part);
            if (0 == sym)
                fail("unknown symbol %S", part);
            value += sym->value;
        }
    }
    return value;
}

#define API(n)                          { L#n, do_##n }
struct api
{
    const wchar_t *name;
    int (*fn)(int argc, wchar_t **argv);
};
struct api apitab[] =
{
    API(CreateFile),
    API(DeleteFile),
    API(CreateDirectory),
    API(RemoveDirectory),
    API(GetFileInformation),
    API(SetFileAttributes),
    API(SetFileTime),
    API(SetEndOfFile),
    API(GetFileSecurity),
    API(SetFileSecurity),
    API(FindFiles),
    API(FindStreams),
    API(MoveFileEx),
    API(RenameStream),
};
static int apicmp(const void *p1, const void *p2)
{
    const struct api *api1 = (const struct api *)p1;
    const struct api *api2 = (const struct api *)p2;
    return wcscmp(api1->name, api2->name);
}
static void apiinit(void)
{
    qsort(apitab, sizeof apitab / sizeof apitab[0], sizeof apitab[0], apicmp);
}
static struct api *apiget(const wchar_t *name)
{
    struct api apikey = { .name = name };
    return bsearch(&apikey, apitab, sizeof apitab / sizeof apitab[0], sizeof apitab[0], apicmp);
}

#define ERR(n)                          { L#n, n }
struct err
{
    const wchar_t *name;
    DWORD value;
};
struct err errtab[] =
{
#include "winerror.i"
};
static int errcmp(const void *p1, const void *p2)
{
    const struct err *err1 = (const struct err *)p1;
    const struct err *err2 = (const struct err *)p2;
    return (int)err1->value - (int)err2->value;
}
static void errinit(void)
{
    qsort(errtab, sizeof errtab / sizeof errtab[0], sizeof errtab[0], errcmp);
}
static struct err *errget(DWORD value)
{
    struct err errkey = { .value = value };
    return bsearch(&errkey, errtab, sizeof errtab / sizeof errtab[0], sizeof errtab[0], errcmp);
}
static const wchar_t *errstr(DWORD value)
{
    struct err *err = errget(value);
    if (0 != err)
        return err->name;
    static wchar_t errbuf[64];
    _snwprintf(errbuf, sizeof errbuf, L"ERROR(%u)", (unsigned)value);
    return errbuf;
}
static void errprint(int success)
{
    if (success && !always_print_last_error)
        printf("0\n");
    else
        printf("%S\n", errstr(GetLastError()));
}

static int do_CreateFile(int argc, wchar_t **argv)
{
    if (argc != 8)
        fail("usage: CreateFile FileName DesiredAccess ShareMode Sddl CreationDisposition FlagsAndAttributes 0");
    SECURITY_ATTRIBUTES SecurityAttributes;
    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.bInheritHandle = FALSE;
    SecurityAttributes.lpSecurityDescriptor = 0;
    if (0 != wcscmp(L"0", argv[4]))
    {
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            argv[4], SDDL_REVISION_1, &SecurityAttributes.lpSecurityDescriptor, 0))
            fail("invalid SDDL %S", argv[4]);
    }
    HANDLE h = CreateFileW(argv[1], symval(argv[2]), symval(argv[3]), &SecurityAttributes,
        symval(argv[5]), symval(argv[6]), 0);
    errprint(INVALID_HANDLE_VALUE != h);
    LocalFree(SecurityAttributes.lpSecurityDescriptor);
    return 0;
}
static int do_DeleteFile(int argc, wchar_t **argv)
{
    if (argc != 2)
        fail("usage: DeleteFile FileName");
    BOOL r = DeleteFileW(argv[1]);
    errprint(r);
    return 0;
}
static int do_CreateDirectory(int argc, wchar_t **argv)
{
    if (argc != 3)
        fail("usage: CreateDirectory PathName Sddl");
    SECURITY_ATTRIBUTES SecurityAttributes;
    SecurityAttributes.nLength = sizeof SecurityAttributes;
    SecurityAttributes.bInheritHandle = FALSE;
    SecurityAttributes.lpSecurityDescriptor = 0;
    if (0 != wcscmp(L"0", argv[2]))
    {
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            argv[2], SDDL_REVISION_1, &SecurityAttributes.lpSecurityDescriptor, 0))
            fail("invalid SDDL %S", argv[2]);
    }
    BOOL r = CreateDirectoryW(argv[1], &SecurityAttributes);
    errprint(r);
    LocalFree(SecurityAttributes.lpSecurityDescriptor);
    return 0;
}
static int do_RemoveDirectory(int argc, wchar_t **argv)
{
    if (argc != 2)
        fail("usage: RemoveDirectory PathName");
    BOOL r = RemoveDirectoryW(argv[1]);
    errprint(r);
    return 0;
}
static int do_GetFileInformation(int argc, wchar_t **argv)
{
    if (argc != 2)
        fail("usage: GetFileInformation FileName");
    HANDLE h = CreateFileW(argv[1],
        FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        errprint(0);
    else
    {
        BY_HANDLE_FILE_INFORMATION FileInfo;
        BOOL r = GetFileInformationByHandle(h, &FileInfo);
        if (r)
        {
            char btimebuf[32], atimebuf[32], mtimebuf[32];
            SYSTEMTIME systime;
            FileTimeToSystemTime(&FileInfo.ftCreationTime, &systime);
            snprintf(btimebuf, sizeof btimebuf, "%d-%02d-%02dT%02d:%02d:%02dZ",
                systime.wYear, systime.wMonth, systime.wDay,
                systime.wHour, systime.wMinute, systime.wSecond);
            FileTimeToSystemTime(&FileInfo.ftLastAccessTime, &systime);
            snprintf(atimebuf, sizeof atimebuf, "%d-%02d-%02dT%02d:%02d:%02dZ",
                systime.wYear, systime.wMonth, systime.wDay,
                systime.wHour, systime.wMinute, systime.wSecond);
            FileTimeToSystemTime(&FileInfo.ftLastWriteTime, &systime);
            snprintf(mtimebuf, sizeof mtimebuf, "%d-%02d-%02dT%02d:%02d:%02dZ",
                systime.wYear, systime.wMonth, systime.wDay,
                systime.wHour, systime.wMinute, systime.wSecond);
            errprint(1);
            printf("FileAttributes=%#" PRIx32 " "
                "CreationTime=%s LastAccessTime=%s LastWriteTime=%s "
                "VolumeSerialNumber=%#" PRIx32 " FileSize=%" PRIu64 " NumberOfLinks=%u FileIndex=%#" PRIx64
                "\n",
                FileInfo.dwFileAttributes,
                btimebuf, atimebuf, mtimebuf,
                FileInfo.dwVolumeSerialNumber,
                ((uint64_t)FileInfo.nFileSizeHigh << 32) | (uint64_t)FileInfo.nFileSizeLow,
                FileInfo.nNumberOfLinks,
                ((uint64_t)FileInfo.nFileIndexHigh << 32) | (uint64_t)FileInfo.nFileIndexLow);
        }
        else
            errprint(0);
        CloseHandle(h);
    }
    return 0;
}
static int do_SetFileAttributes(int argc, wchar_t **argv)
{
    if (argc != 3)
        fail("usage: SetFileAttributes FileName FileAttributes");
    BOOL r = SetFileAttributesW(argv[1], symval(argv[2]));
    errprint(r);
    return 0;
}
static int do_SetFileTime(int argc, wchar_t **argv)
{
    if (argc != 5)
        fail("usage: SetFileTime FileName CreationTime LastAccessTime LastWriteTime");
    HANDLE h = CreateFileW(argv[1],
        FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        errprint(0);
    else
    {
        FILETIME *CreationTime = 0, *LastAccessTime = 0, *LastWriteTime = 0, ft[3];
        SYSTEMTIME systime;
        memset(&systime, 0, sizeof systime);
        systime.wMonth = systime.wDay = 1;
        if (0 < swscanf(argv[2], L"%hd-%02hd-%02hdT%02hd:%02hd:%02hd",
            &systime.wYear, &systime.wMonth, &systime.wDay,
            &systime.wHour, &systime.wMinute, &systime.wSecond))
        {
            if (SystemTimeToFileTime(&systime, &ft[0]))
                CreationTime = &ft[0];
        }
        memset(&systime, 0, sizeof systime);
        systime.wMonth = systime.wDay = 1;
        if (0 < swscanf(argv[3], L"%hd-%02hd-%02hdT%02hd:%02hd:%02hd",
            &systime.wYear, &systime.wMonth, &systime.wDay,
            &systime.wHour, &systime.wMinute, &systime.wSecond))
        {
            if (SystemTimeToFileTime(&systime, &ft[1]))
                LastAccessTime = &ft[1];
        }
        memset(&systime, 0, sizeof systime);
        systime.wMonth = systime.wDay = 1;
        if (0 < swscanf(argv[4], L"%hd-%02hd-%02hdT%02hd:%02hd:%02hd",
            &systime.wYear, &systime.wMonth, &systime.wDay,
            &systime.wHour, &systime.wMinute, &systime.wSecond))
        {
            if (SystemTimeToFileTime(&systime, &ft[2]))
                LastWriteTime = &ft[2];
        }
        BOOL r = SetFileTime(h, CreationTime, LastAccessTime, LastWriteTime);
        errprint(r);
        CloseHandle(h);
    }
    return 0;
}
static int do_SetEndOfFile(int argc, wchar_t **argv)
{
    if (argc != 3)
        fail("usage: SetEndOfFile FileName Length");
    HANDLE h = CreateFileW(argv[1],
        GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        errprint(0);
    else
    {
        FILE_END_OF_FILE_INFO EofInfo;
        EofInfo.EndOfFile.QuadPart = wcstoull(argv[2], 0, 0);
        BOOL r = SetFileInformationByHandle(h, FileEndOfFileInfo, &EofInfo, sizeof EofInfo);
        errprint(r);
        CloseHandle(h);
    }
    return 0;
}
static int do_GetFileSecurity(int argc, wchar_t **argv)
{
    if (argc != 3)
        fail("usage: GetFileSecurity FileName SecurityInformation");
    SECURITY_INFORMATION SecurityInformation = symval(argv[2]);
    PSECURITY_DESCRIPTOR SecurityDescriptor = 0;
    PWSTR Sddl = 0;
    DWORD Size;
    BOOL r = GetFileSecurityW(argv[1], SecurityInformation, SecurityDescriptor, 0, &Size);
    if (!r && ERROR_INSUFFICIENT_BUFFER != GetLastError())
        errprint(0);
    else
    {
        SecurityDescriptor = malloc(Size);
        if (0 == SecurityDescriptor)
            fail("cannot alloc memory for security descriptor");
        r = GetFileSecurityW(argv[1], SecurityInformation, SecurityDescriptor, Size, &Size);
        if (!r)
            errprint(0);
        else
        {
            if (!ConvertSecurityDescriptorToStringSecurityDescriptorW(SecurityDescriptor,
                SDDL_REVISION_1,
                OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION,
                &Sddl, 0))
            {
                errprint(0);
            }
            else
            {
                errprint(1);
                printf("Sddl=\"%S\"\n", Sddl);
            }
            LocalFree(Sddl);
        }
        free(SecurityDescriptor);
    }
    return 0;
}
static int do_SetFileSecurity(int argc, wchar_t **argv)
{
    if (argc != 4)
        fail("usage: SetFileSecurity FileName SecurityInformation Sddl");
    SECURITY_INFORMATION SecurityInformation = symval(argv[2]);
    PSECURITY_DESCRIPTOR SecurityDescriptor;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        argv[3], SDDL_REVISION_1, &SecurityDescriptor, 0))
        fail("invalid SDDL %S", argv[3]);
    BOOL r = SetFileSecurityW(argv[1], SecurityInformation, SecurityDescriptor);
    errprint(r);
    LocalFree(SecurityDescriptor);
    return 0;
}
static int do_FindFiles(int argc, wchar_t **argv)
{
    if (argc != 2)
        fail("usage: FindFiles FileName");
    WIN32_FIND_DATAW FindData;
    HANDLE h = FindFirstFileW(argv[1], &FindData);
    if (INVALID_HANDLE_VALUE == h)
        errprint(0);
    else
    {
        errprint(1);
        do
        {
            char btimebuf[32], atimebuf[32], mtimebuf[32];
            SYSTEMTIME systime;
            FileTimeToSystemTime(&FindData.ftCreationTime, &systime);
            snprintf(btimebuf, sizeof btimebuf, "%d-%02d-%02dT%02d:%02d:%02dZ",
                systime.wYear, systime.wMonth, systime.wDay,
                systime.wHour, systime.wMinute, systime.wSecond);
            FileTimeToSystemTime(&FindData.ftLastAccessTime, &systime);
            snprintf(atimebuf, sizeof atimebuf, "%d-%02d-%02dT%02d:%02d:%02dZ",
                systime.wYear, systime.wMonth, systime.wDay,
                systime.wHour, systime.wMinute, systime.wSecond);
            FileTimeToSystemTime(&FindData.ftLastWriteTime, &systime);
            snprintf(mtimebuf, sizeof mtimebuf, "%d-%02d-%02dT%02d:%02d:%02dZ",
                systime.wYear, systime.wMonth, systime.wDay,
                systime.wHour, systime.wMinute, systime.wSecond);
            printf("FileAttributes=%#" PRIx32 " "
                "CreationTime=%s LastAccessTime=%s LastWriteTime=%s "
                "FileSize=%" PRIu64 " AlternateFileName=\"%S\" FileName=\"%S\""
                "\n",
                FindData.dwFileAttributes,
                btimebuf, atimebuf, mtimebuf,
                ((uint64_t)FindData.nFileSizeHigh << 32) | (uint64_t)FindData.nFileSizeLow,
                FindData.cAlternateFileName, FindData.cFileName);
        } while (FindNextFileW(h, &FindData));
        FindClose(h);
    }
    return 0;
}
static int do_FindStreams(int argc, wchar_t **argv)
{
    if (argc != 2)
        fail("usage: FindStreams FileName");
    WIN32_FIND_STREAM_DATA FindData;
    HANDLE h = FindFirstStreamW(argv[1], FindStreamInfoStandard, &FindData, 0);
    if (INVALID_HANDLE_VALUE == h)
        errprint(0);
    else
    {
        errprint(1);
        do
        {
            printf("StreamSize=%" PRIu64 " StreamName=\"%S\"\n",
                FindData.StreamSize.QuadPart, FindData.cStreamName);
        } while (FindNextStreamW(h, &FindData));
        FindClose(h);
    }
    return 0;
}
static int do_MoveFileEx(int argc, wchar_t **argv)
{
    if (argc != 4)
        fail("usage: MoveFileEx ExistingFileName NewFileName Flags");
    BOOL r = MoveFileExW(argv[1], argv[2], symval(argv[3]));
    errprint(r);
    return 0;
}
static int do_RenameStream(int argc, wchar_t **argv)
{
    if (argc != 4 || ':' != argv[2][0])
        fail("usage: RenameStream FileName:ExistingStreamName :NewStreamName ReplaceIfExists");
    fail("not implemented!");

    HANDLE h = CreateFileW(argv[1],
        DELETE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        0, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (INVALID_HANDLE_VALUE == h)
        errprint(0);
    else
    {
        typedef struct _FILE_RENAME_INFORMATION {
            BOOLEAN ReplaceIfExists;
            HANDLE RootDirectory;
            ULONG FileNameLength;
            WCHAR FileName[1];
        } FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;
        size_t namesize = wcslen(argv[2]) * sizeof(wchar_t);
        size_t infosize = sizeof(FILE_RENAME_INFORMATION) + namesize;
        FILE_RENAME_INFORMATION *RenameInfo = _alloca(infosize);
        memset(RenameInfo, 0, infosize);
        RenameInfo->ReplaceIfExists = !!(wcstoul(argv[3], 0, 0));
        RenameInfo->RootDirectory = 0;
        RenameInfo->FileNameLength = namesize;
        memcpy(RenameInfo->FileName, argv[2], namesize);
        /*
         * This seems to fail with ERROR_INVALID_HANDLE on Win32 and ERROR_INVALID_NAME on Win64.
         * Do not have the time to figure the exact reasons why right now.
         */
        BOOL r = SetFileInformationByHandle(h, FileRenameInfo, &RenameInfo, infosize);
        errprint(r);
        CloseHandle(h);
    }

    return 0;
}
static void usage()
{
    fprintf(stderr, "usage: winfstest [-w][-e] ApiName args...\n");
    for (size_t i = 0; sizeof apitab / sizeof apitab[0] > i; i++)
        fprintf(stderr, "    %S\n", apitab[i].name);
    exit(1);
}
int wmain(int argc, wchar_t **argv)
{
    syminit();
    apiinit();
    errinit();
    argc--; argv++;
    if (argc < 1)
        usage();
    if (0 == wcscmp(L"-w", argv[0]))
    {
        wait_for_input_before_exit = 1;
        argc--; argv++;
        if (argc < 1)
            usage();
    }
    if (0 == wcscmp(L"-e", argv[0]))
    {
        always_print_last_error = 1;
        argc--; argv++;
        if (argc < 1)
            usage();
    }
    struct api *api = apiget(argv[0]);
    if (0 == api)
        fail("cannot find API %S", argv[0]);
    int ec = api->fn(argc, argv);
    fflush(stdout);
    if (wait_for_input_before_exit)
    {
        char buf[80];
        fgets(buf, sizeof buf, stdin);
    }
    return ec;
}
