//
//  OS_Create_Process: C
//
// Return -1 on error.
// For right now, set flags to 1 for /wait.
//
int OS_Create_Process(
    const wchar_t *call,
    int argc,
    const wchar_t * argv[],
    u32 flags, u64 *pid,
    int *exit_code,
    u32 input_type,
    char *input,
    u32 input_len,
    u32 output_type,
    char **output,
    u32 *output_len,
    u32 err_type,
    char **err,
    u32 *err_len
) {
    UNUSED(argc);
    UNUSED(argv);

    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
//  REBOOL              is_NT;
//  OSVERSIONINFO       info;
    REBINT              result = -1;
    REBINT              ret = 0;
    HANDLE hOutputRead = 0, hOutputWrite = 0;
    HANDLE hInputWrite = 0, hInputRead = 0;
    HANDLE hErrorWrite = 0, hErrorRead = 0;
    wchar_t *cmd = NULL;
    char *oem_input = NULL;

    SECURITY_ATTRIBUTES sa;

    unsigned char flag_wait = FALSE;
    unsigned char flag_console = FALSE;
    unsigned char flag_shell = FALSE;
    unsigned char flag_info = FALSE;
    UNUSED(flag_info);
    UNUSED(flag_console);

    if (flags & FLAG_WAIT) flag_wait = TRUE;
    if (flags & FLAG_CONSOLE) flag_console = TRUE;
    if (flags & FLAG_SHELL) flag_shell = TRUE;
    if (flags & FLAG_INFO) flag_info = TRUE;

    // Set up the security attributes struct.
    sa.nLength= sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    si.cb = sizeof(si);
    si.lpReserved = NULL;
    si.lpDesktop = NULL;
    si.lpTitle = NULL;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.wShowWindow = SW_SHOWNORMAL;
    si.cbReserved2 = 0;
    si.lpReserved2 = NULL;

//  GetVersionEx(&info);
//  is_NT = info.dwPlatformId >= VER_PLATFORM_WIN32_NT;

    /* initialize output/error */
    if (output_type != NONE_TYPE
        && output_type != INHERIT_TYPE
        && (output == NULL
            || output_len == NULL)) {
        return -1;
    }
    if (output != NULL) *output = NULL;
    if (output_len != NULL) *output_len = 0;

    if (err_type != NONE_TYPE
        && err_type != INHERIT_TYPE
        && (err == NULL
            || err_len == NULL)) {
        return -1;
    }
    if (err != NULL) *err = NULL;
    if (err_len != NULL) *err_len = 0;

    switch (input_type) {
        case STRING_TYPE:
        case BINARY_TYPE:
            if (!CreatePipe(&hInputRead, &hInputWrite, NULL, 0)) {
                goto input_error;
            }
            /* make child side handle inheritable */
            if (!SetHandleInformation(hInputRead, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                goto input_error;
            }
            si.hStdInput = hInputRead;
            break;
        case FILE_TYPE:
            hInputRead = CreateFile(cast(wchar_t*, input), // REVIEW! (and all wchar_t*/char*)
                GENERIC_READ, /* desired mode*/
                0, /* shared mode*/
                &sa, /* security attributes */
                OPEN_EXISTING, /* Creation disposition */
                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, /* flag and attributes */
                NULL /* template */);
            si.hStdInput = hInputRead;
            break;
        case NONE_TYPE:
            si.hStdInput = 0;
            break;
        default:
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            break;
    }

    switch (output_type) {
        case STRING_TYPE:
        case BINARY_TYPE:
            if (!CreatePipe(&hOutputRead, &hOutputWrite, NULL, 0)) {
                goto output_error;
            }

            /* make child side handle inheritable */
            if (!SetHandleInformation(hOutputWrite, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                goto output_error;
            }
            si.hStdOutput = hOutputWrite;
            break;
        case FILE_TYPE:
            si.hStdOutput = CreateFile(*(LPCTSTR*)output,
                GENERIC_WRITE, /* desired mode*/
                0, /* shared mode*/
                &sa, /* security attributes */
                CREATE_NEW, /* Creation disposition */
                FILE_ATTRIBUTE_NORMAL, /* flag and attributes */
                NULL /* template */);

            if (si.hStdOutput == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS) {
                si.hStdOutput = CreateFile(*(LPCTSTR*)output,
                    GENERIC_WRITE, /* desired mode*/
                    0, /* shared mode*/
                    &sa, /* security attributes */
                    OPEN_EXISTING, /* Creation disposition */
                    FILE_ATTRIBUTE_NORMAL, /* flag and attributes */
                    NULL /* template */);
            }
            break;
        case NONE_TYPE:
            si.hStdOutput = 0;
            break;
        default:
            si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            break;
    }

    switch (err_type) {
        case STRING_TYPE:
        case BINARY_TYPE:
            if (!CreatePipe(&hErrorRead, &hErrorWrite, NULL, 0)) {
                goto error_error;
            }
            /* make child side handle inheritable */
            if (!SetHandleInformation(hErrorWrite, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
                goto error_error;
            }
            si.hStdError = hErrorWrite;
            break;
        case FILE_TYPE:
            si.hStdError = CreateFile(*(LPCTSTR*)err,
                GENERIC_WRITE, /* desired mode*/
                0, /* shared mode*/
                &sa, /* security attributes */
                CREATE_NEW, /* Creation disposition */
                FILE_ATTRIBUTE_NORMAL, /* flag and attributes */
                NULL /* template */);

            if (si.hStdError == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS) {
                si.hStdError = CreateFile(*(LPCTSTR*)err,
                    GENERIC_WRITE, /* desired mode*/
                    0, /* shared mode*/
                    &sa, /* security attributes */
                    OPEN_EXISTING, /* Creation disposition */
                    FILE_ATTRIBUTE_NORMAL, /* flag and attributes */
                    NULL /* template */);
            }
            break;
        case NONE_TYPE:
            si.hStdError = 0;
            break;
        default:
            si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
            break;
    }

    if (call == NULL) {
        /* command in argv */
        goto cleanup; /* NOT IMPLEMENTED*/
    } else {
        if (flag_shell) {
            const wchar_t *sh = L"cmd.exe /C ";
            size_t len = wcslen(sh) + wcslen(call) + 1;

            // other branch uses _wcsdup and free(), so we can't use
            // OS_ALLOC_N here (doesn't matter, not returning it to Rebol)
            cmd = cast(wchar_t*, malloc(len * sizeof(wchar_t)));
            cmd[0] = L'\0';
            wcscat(cmd, sh);
            wcscat(cmd, call);
        } else {
            // CreateProcess might write to this memory
            // Duplicate it to be safe
            cmd = _wcsdup(call);
        }
    }

    result = CreateProcess(
        NULL,                       // Executable name
        cmd,                        // Command to execute
        NULL,                       // Process security attributes
        NULL,                       // Thread security attributes
        TRUE,                       // Inherit handles, must be TRUE for I/O redirection
        NORMAL_PRIORITY_CLASS       // Creation flags
        | CREATE_DEFAULT_ERROR_MODE,
        NULL,                       // Environment
        NULL,                       // Current directory
        &si,                        // Startup information
        &pi                         // Process information
    );

    free(cmd);

    if (pid != NULL) *pid = pi.dwProcessId;

    if (hInputRead != NULL)
        CloseHandle(hInputRead);

    if (hOutputWrite != NULL)
        CloseHandle(hOutputWrite);

    if (hErrorWrite != NULL)
        CloseHandle(hErrorWrite);

    // Wait for termination:
    if (result && flag_wait) {
        HANDLE handles[3];
        int count = 0;
        DWORD wait_result = 0;
        DWORD output_size = 0;
        DWORD err_size = 0;

#define BUF_SIZE_CHUNK 4096

        if (hInputWrite != NULL && input_len > 0) {
            if (input_type == STRING_TYPE) {
                DWORD dest_len = 0;
                /* convert input encoding from UNICODE to OEM */
                // !!! Is cast to wchar_t here legal?
                dest_len = WideCharToMultiByte(CP_OEMCP, 0, cast(wchar_t*, input), input_len, oem_input, dest_len, NULL, NULL);
                if (dest_len > 0) {
                    // Not returning memory to Rebol, but we don't realloc or
                    // free, so it's all right to use OS_ALLOC_N anyway
                    oem_input = OS_ALLOC_N(char, dest_len);
                    if (oem_input != NULL) {
                        WideCharToMultiByte(CP_OEMCP, 0, cast(wchar_t*, input), input_len, oem_input, dest_len, NULL, NULL);
                        input_len = dest_len;
                        input = oem_input;
                        handles[count ++] = hInputWrite;
                    }
                }
            } else { /* BINARY_TYPE */
                handles[count ++] = hInputWrite;
            }
        }
        if (hOutputRead != NULL) {
            output_size = BUF_SIZE_CHUNK;
            *output_len = 0;

            // Might realloc(), can't use OS_ALLOC_N.  (This memory is not
            // passed back to Rebol, so it doesn't matter.)
            *output = cast(char*, malloc(output_size));
            handles[count ++] = hOutputRead;
        }
        if (hErrorRead != NULL) {
            err_size = BUF_SIZE_CHUNK;
            *err_len = 0;

            // Might realloc(), can't use OS_ALLOC_N.  (This memory is not
            // passed back to Rebol, so it doesn't matter.)
            *err = cast(char*, malloc(err_size));
            handles[count++] = hErrorRead;
        }

        while (count > 0) {
            wait_result = WaitForMultipleObjects(count, handles, FALSE, INFINITE);
            // If we test wait_result >= WAIT_OBJECT_0 it will tell us "always
            // true" with -Wtype-limits, since WAIT_OBJECT_0 is 0.  Take that
            // comparison out but add an assert in case you're on some abstracted
            // Windows and need to know that it isn't 0 for that implementation.
            assert(WAIT_OBJECT_0 == 0);
            if (wait_result < WAIT_OBJECT_0 + count) {
                int i = wait_result - WAIT_OBJECT_0;
                DWORD input_pos = 0;
                DWORD n = 0;

                if (handles[i] == hInputWrite) {
                    if (!WriteFile(hInputWrite, (char*)input + input_pos, input_len - input_pos, &n, NULL)) {
                        if (i < count - 1) {
                            memmove(&handles[i], &handles[i + 1], (count - i - 1) * sizeof(HANDLE));
                        }
                        count--;
                    } else {
                        input_pos += n;
                        if (input_pos >= input_len) {
                            /* done with input */
                            CloseHandle(hInputWrite);
                            hInputWrite = NULL;
                            OS_FREE(oem_input);
                            oem_input = NULL;
                            if (i < count - 1) {
                                memmove(&handles[i], &handles[i + 1], (count - i - 1) * sizeof(HANDLE));
                            }
                            count--;
                        }
                    }
                } else if (handles[i] == hOutputRead) {
                    if (!ReadFile(hOutputRead, *(char**)output + *output_len, output_size - *output_len, &n, NULL)) {
                        if (i < count - 1) {
                            memmove(&handles[i], &handles[i + 1], (count - i - 1) * sizeof(HANDLE));
                        }
                        count--;
                    } else {
                        *output_len += n;
                        if (*output_len >= output_size) {
                            output_size += BUF_SIZE_CHUNK;
                            *output = cast(char*, realloc(*output, output_size));
                            if (*output == NULL) goto kill;
                        }
                    }
                } else if (handles[i] == hErrorRead) {
                    if (!ReadFile(hErrorRead, *(char**)err + *err_len, err_size - *err_len, &n, NULL)) {
                        if (i < count - 1) {
                            memmove(&handles[i], &handles[i + 1], (count - i - 1) * sizeof(HANDLE));
                        }
                        count--;
                    } else {
                        *err_len += n;
                        if (*err_len >= err_size) {
                            err_size += BUF_SIZE_CHUNK;
                            *err = cast(char*, realloc(*err, err_size));
                            if (*err == NULL) goto kill;
                        }
                    }
                } else {
                    //printf("Error READ");
                    if (!ret) ret = GetLastError();
                    goto kill;
                }
            } else if (wait_result == WAIT_FAILED) { /* */
                //printf("Wait Failed\n");
                if (!ret) ret = GetLastError();
                goto kill;
            } else {
                //printf("Wait returns unexpected result: %d\n", wait_result);
                if (!ret) ret = GetLastError();
                goto kill;
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE); // check result??
        if (exit_code != NULL) {
            DWORD temp;
            GetExitCodeProcess(pi.hProcess, &temp);
            *exit_code = temp;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        if (output_type == STRING_TYPE && *output != NULL && *output_len > 0) {
            /* convert to wide char string */
            int dest_len = 0;
            wchar_t *dest = NULL;
            dest_len = MultiByteToWideChar(CP_OEMCP, 0, *output, *output_len, dest, 0);
            if (dest_len <= 0) {
                OS_FREE(*output);
                *output = NULL;
                *output_len = 0;
            }
            // We've already established that output is a malloc()'d pointer,
            // not one we got back from OS_ALLOC_N()
            dest = cast(wchar_t*, malloc(*output_len * sizeof(wchar_t)));
            if (dest == NULL) goto cleanup;
            MultiByteToWideChar(CP_OEMCP, 0, *output, *output_len, dest, dest_len);
            free(*output);
            *output = cast(char*, dest);
            *output_len = dest_len;
        }

        if (err_type == STRING_TYPE && *err != NULL && *err_len > 0) {
            /* convert to wide char string */
            int dest_len = 0;
            wchar_t *dest = NULL;
            dest_len = MultiByteToWideChar(CP_OEMCP, 0, *err, *err_len, dest, 0);
            if (dest_len <= 0) {
                OS_FREE(*err);
                *err = NULL;
                *err_len = 0;
            }
            // We've already established that output is a malloc()'d pointer,
            // not one we got back from OS_ALLOC_N()
            dest = cast(wchar_t*, malloc(*err_len * sizeof(wchar_t)));
            if (dest == NULL) goto cleanup;
            MultiByteToWideChar(CP_OEMCP, 0, *err, *err_len, dest, dest_len);
            free(*err);
            *err = cast(char*, dest);
            *err_len = dest_len;
        }
    } else if (result) {
        /* no wait */
        /* Close handles to avoid leaks */
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        /* CreateProcess failed */
        ret = GetLastError();
    }

    goto cleanup;

kill:
    if (TerminateProcess(pi.hProcess, 0)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        if (exit_code != NULL) {
            DWORD temp;
            GetExitCodeProcess(pi.hProcess, &temp);
            *exit_code = temp;
        }
    } else if (ret == 0) {
        ret = GetLastError();
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

cleanup:
    if (oem_input != NULL) {
        // Since we didn't need realloc() for oem_input, we used the
        // OS_ALLOC_N allocator.
        OS_FREE(oem_input);
    }

    if (output != NULL && *output != NULL && *output_len == 0) {
        free(*output);
    }

    if (err != NULL && *err != NULL && *err_len == 0) {
        free(*err);
    }

    if (hInputWrite != NULL)
        CloseHandle(hInputWrite);

    if (hOutputRead != NULL)
        CloseHandle(hOutputRead);

    if (hErrorRead != NULL)
        CloseHandle(hErrorRead);

    if (err_type == FILE_TYPE) {
        CloseHandle(si.hStdError);
    }

error_error:
    if (output_type == FILE_TYPE) {
        CloseHandle(si.hStdOutput);
    }

output_error:
    if (input_type == FILE_TYPE) {
        CloseHandle(si.hStdInput);
    }

input_error:
    return ret;  // meaning depends on flags
}


//
//  Insert_Command_Arg: C
//
// Insert an argument into a command line at the %1 position,
// or at the end if there is no %1. (An INSERT action.)
// Do not exceed the specified limit length.
//
// Too bad std Clib does not provide INSERT or REPLACE functions.
//
static void Insert_Command_Arg(wchar_t *cmd, const wchar_t *arg, REBCNT limit)
{
    #define HOLD_SIZE 2000
    wchar_t *spot;
    wchar_t hold[HOLD_SIZE + 4];

    if (wcslen(cmd) >= limit) return; // invalid case, ignore it.

    // Find %1:
    spot = wcsstr(cmd, L"%1");

    if (spot) {
        // Save rest of cmd line (such as end quote, -flags, etc.)
        wcsncpy(hold, spot+2, HOLD_SIZE);

        // Terminate at the arg location:
        spot[0] = 0;

        // Insert the arg:
        wcsncat(spot, arg, limit - wcslen(cmd) - 1);

        // Add back the rest of cmd:
        wcsncat(spot, hold, limit - wcslen(cmd) - 1);
    }
    else {
        wcsncat(cmd, L" ", 1);
        wcsncat(cmd, arg, limit - wcslen(cmd) - 1);
    }
}


#define MAX_BRW_PATH 2044

//
//  OS_Browse: C
//
REBOOL OS_Browse(const wchar_t *url)
{
    HKEY key;
    if (
        RegOpenKeyEx(
            HKEY_CLASSES_ROOT,
            L"http\\shell\\open\\command",
            0,
            KEY_READ,
            &key
        ) != ERROR_SUCCESS
    ){
        return 0;
    }

    wchar_t *path = OS_ALLOC_N(wchar_t, MAX_BRW_PATH + 4); // !!! why 4?

    DWORD len = MAX_BRW_PATH;

    DWORD type;
    DWORD flag = RegQueryValueEx(key, L"", 0, &type, cast(LPBYTE, path), &len);
    RegCloseKey(key);
    if (flag != ERROR_SUCCESS) {
        OS_FREE(path);
        return FALSE;
    }

    Insert_Command_Arg(path, cast(const wchar_t*, url), MAX_BRW_PATH);

    // Delegate to the same host routine used by CALL, but without any I/O
    // piping.
    //
    const wchar_t *argv[] = {path, NULL};
    int exit_code = 0;
    int result = OS_Create_Process(
        path,
        1,
        argv,
        0,
        NULL, // pid
        &exit_code,
        INHERIT_TYPE, // input_type
        NULL, // input
        0, // input_len
        INHERIT_TYPE, // output_type
        NULL, // output
        NULL, // output_len
        INHERIT_TYPE, // err_type
        NULL, // err
        NULL // err_len
    );

    OS_FREE(path);
    return LOGICAL(result == 0);
}
