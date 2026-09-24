"""Runs a program in a window station of its own, which has its own clipboard.

    python run_in_window_station.py unittests.exe --gtest_filter=MSWindowsFilePasteClipboardTests.*

The clipboard tests skip themselves on the desktop's window station, since they
would overwrite what the user has copied.  This runs them where they can't.
The program's output is printed when it exits, and so is its exit code.

The functions can be imported to run several programs, each in a station of
its own, for instance a server and a client that mustn't touch the desktop.
"""
import ctypes
import os
import subprocess
import sys
import tempfile
from ctypes import wintypes

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

WINSTA_ALL_ACCESS = 0x37F
GENERIC_ALL = 0x10000000
CREATE_NO_WINDOW = 0x08000000
STARTF_USESTDHANDLES = 0x100
INFINITE = 0xFFFFFFFF
UOI_NAME = 2


class STARTUPINFOW(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("lpReserved", wintypes.LPWSTR),
                ("lpDesktop", wintypes.LPWSTR), ("lpTitle", wintypes.LPWSTR),
                ("dwX", wintypes.DWORD), ("dwY", wintypes.DWORD),
                ("dwXSize", wintypes.DWORD), ("dwYSize", wintypes.DWORD),
                ("dwXCountChars", wintypes.DWORD), ("dwYCountChars", wintypes.DWORD),
                ("dwFillAttribute", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
                ("wShowWindow", wintypes.WORD), ("cbReserved2", wintypes.WORD),
                ("lpReserved2", ctypes.c_void_p), ("hStdInput", wintypes.HANDLE),
                ("hStdOutput", wintypes.HANDLE), ("hStdError", wintypes.HANDLE)]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [("hProcess", wintypes.HANDLE), ("hThread", wintypes.HANDLE),
                ("dwProcessId", wintypes.DWORD), ("dwThreadId", wintypes.DWORD)]


class SECURITY_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("nLength", wintypes.DWORD), ("lpSecurityDescriptor", ctypes.c_void_p),
                ("bInheritHandle", wintypes.BOOL)]


user32.CreateWindowStationW.restype = wintypes.HANDLE
user32.CreateDesktopW.restype = wintypes.HANDLE
user32.GetProcessWindowStation.restype = wintypes.HANDLE
kernel32.CreateFileW.restype = wintypes.HANDLE


def check(result, what):
    if not result:
        raise OSError(ctypes.get_last_error(), what)
    return result


class Station:
    """A window station with one desktop; closed with close()."""

    def __init__(self):
        # unnamed: creating a named window station is denied to ordinary
        # users.  the system names it, and the desktop is found by that name
        self.handle = check(user32.CreateWindowStationW(None, 0, WINSTA_ALL_ACCESS, None),
                            "CreateWindowStation")
        name = ctypes.create_unicode_buffer(256)
        needed = wintypes.DWORD()
        check(user32.GetUserObjectInformationW(self.handle, UOI_NAME, name, ctypes.sizeof(name),
                                               ctypes.byref(needed)), "GetUserObjectInformation")
        self.name = name.value
        # a desktop is created in the window station of the calling process
        original = user32.GetProcessWindowStation()
        check(user32.SetProcessWindowStation(self.handle), "SetProcessWindowStation")
        try:
            self.desktop = check(user32.CreateDesktopW("Default", None, None, 0, GENERIC_ALL,
                                                       None), "CreateDesktop")
        finally:
            user32.SetProcessWindowStation(original)

    def start(self, arguments, output_path):
        """Starts a program here with its output going to output_path."""
        inherit = SECURITY_ATTRIBUTES(ctypes.sizeof(SECURITY_ATTRIBUTES), None, True)
        output = check(kernel32.CreateFileW(output_path, 0x40000000, 3, ctypes.byref(inherit),
                                            2, 0x80, None), "CreateFile")
        startup = STARTUPINFOW()
        startup.cb = ctypes.sizeof(STARTUPINFOW)
        startup.lpDesktop = f"{self.name}\\Default"
        startup.dwFlags = STARTF_USESTDHANDLES
        startup.hStdOutput = output
        startup.hStdError = output
        process = PROCESS_INFORMATION()
        command = ctypes.create_unicode_buffer(subprocess.list2cmdline(arguments))
        try:
            check(kernel32.CreateProcessW(None, command, None, None, True, CREATE_NO_WINDOW,
                                          None, None, ctypes.byref(startup),
                                          ctypes.byref(process)), "CreateProcess")
        finally:
            kernel32.CloseHandle(output)
        return process

    def close(self):
        user32.CloseDesktop(self.desktop)
        user32.CloseWindowStation(self.handle)


def wait(process, timeout_ms=INFINITE):
    """Waits for process to exit; returns its exit code, or None if it hasn't."""
    if kernel32.WaitForSingleObject(process.hProcess, timeout_ms) != 0:
        return None
    exit_code = wintypes.DWORD()
    kernel32.GetExitCodeProcess(process.hProcess, ctypes.byref(exit_code))
    return exit_code.value


def close(process):
    for handle in (process.hProcess, process.hThread):
        kernel32.CloseHandle(handle)


def main():
    station = Station()
    output_path = os.path.join(tempfile.gettempdir(), f"window-station-{os.getpid()}.log")
    process = station.start(sys.argv[1:], output_path)
    exit_code = wait(process)
    close(process)
    station.close()

    with open(output_path, encoding="utf-8", errors="replace") as log:
        sys.stdout.write(log.read())
    os.remove(output_path)
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
