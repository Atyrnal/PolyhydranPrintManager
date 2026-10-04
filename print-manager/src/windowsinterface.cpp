#include "windowsinterface.h"

#ifdef Q_OS_WIN
#include "globalstate.hpp"

WindowsInterface::WindowsInterface(QObject* parent) : OsInterface(parent) {}

DWORD WindowsInterface::findProcessId(const QString& exeName) { //Windows shenanigans to find process by exename
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); //ProcessScanner

    if (Process32First(snapshot, &entry)) { //Scan Processes until we find one that matches
        do {
            if (QString::fromWCharArray(entry.szExeFile).compare(exeName, Qt::CaseInsensitive) == 0) { //if exename matches
                DWORD pid = entry.th32ProcessID;
                CloseHandle(snapshot); //Stop scanner
                return pid;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return 0; //return 0 if not found / not open
}

void WindowsInterface::bringWindowToFront(DWORD pid) {
    HWND hwnd = nullptr;
    HWND mainHwnd = nullptr;

    //Scan instances of specified process for one with a window
    while ((hwnd = FindWindowEx(nullptr, hwnd, nullptr, nullptr))) {
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);

        // Skip windows not belonging to target process
        if (windowPid != pid)
            continue;

        // Skip invisible windows
        if (!IsWindowVisible(hwnd))
            continue;

        // Skip child/owned windows (dialogs, popups)
        if (GetWindow(hwnd, GW_OWNER) != nullptr)
            continue;

        // Found a visible, top-level window for this process
        mainHwnd = hwnd;
        break;
    }

    if (mainHwnd) {
        ShowWindow(mainHwnd, SW_RESTORE);//Show the window
        SetForegroundWindow(mainHwnd); //Bring it to front
        Log::write("WindowsInterface", "Brought OrcaSlicer main window to front");
    } else {
        Error::handle("WindowsInterfaceError", "Could not find OrcaSlicer window", El::Trivial);
    }
}

void WindowsInterface::orcaButtonClicked() {
    //TODO: replace this with settings stuff
    QString exeName = "orca-slicer.exe";
    QString exePath = "C:/Program Files/OrcaSlicer/orca-slicer.exe";

    //Find orcaslicer process

    DWORD pid = findProcessId(exeName);

    if (pid != 0) { //if process running, bring it to front instead of starting a new oen
        Log::write("WindowsInterface", "Bringing running OrcaSlicer instance to front");
        bringWindowToFront(pid);
    } else if (QFile::exists(exePath)){ //Otherwise launch it (if it is installed)
        Log::write("WindowsInterface", "Launching OrcaSlicer instance");
        QProcess::startDetached(exePath);
    } else {
        Error::handle("WindowsInterfaceError", "OrcaSlicer installation not found", El::Warning);
    }
}
#endif