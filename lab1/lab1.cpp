#include <iostream>
#include <Windows.h>
#include <string>
#include <iostream>
#include <vector>
#include <tchar.h>

HWND bufferHwnd;

bool CALLBACK enum_windows_proc(HWND hwnd, LPARAM lParam) {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    if (!bufferHwnd && processId == (DWORD)lParam) {
        bufferHwnd = hwnd;
        return false;
    }
    return true;
}

class Process {
private:
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    HWND window;

    const wchar_t* widen(const std::string& narrow, std::wstring& wide) {
        size_t length = narrow.size();
        if (length > 0) {
            wide.resize(length);
            length = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), (int)length, (wchar_t*)wide.c_str(), (int)length);
            wide.resize(length);
        }
        else {
            wide.clear();
        }
        return wide.c_str();
    }

    void findWindow() {
        bufferHwnd = nullptr;
        EnumWindows((WNDENUMPROC)enum_windows_proc, this->pi.dwProcessId);
        this->window = bufferHwnd;
    }

public:

    Process() : window(nullptr) {}

    HANDLE gethProcess() {
        return this->pi.hProcess;
    }

    void launchProcess(std::string app) {
        // Prepare handles.
        ZeroMemory(&this->si, sizeof(this->si));
        this->si.cb = sizeof(this->si);
        ZeroMemory(&this->pi, sizeof(this->pi));

        //Prepare CreateProcess args
        std::wstring app_w;
        widen(app, app_w);
        const wchar_t* app_const = app_w.c_str();

        // Start the child process.
        if (!CreateProcessW(
            app_const,      // app path
            nullptr,     // Command line (needs to include app path as first argument. args seperated by whitepace)
            NULL,           // Process handle not inheritable
            NULL,           // Thread handle not inheritable
            FALSE,          // Set handle inheritance to FALSE
            CREATE_NEW_CONSOLE,
            NULL,           // Use parent's environment block
            NULL,           // Use parent's starting directory
            &this->si,            // Pointer to STARTUPINFO structure
            &this->pi)           // Pointer to PROCESS_INFORMATION structure
            ) {
            std::cout << "CreateProcess failed (" << GetLastError() << ")\n";
            throw std::exception("Could not create child process");
        } else {
            std::cout << "[          ] Successfully launched child process" << std::endl;
        }
        return;
    }

    bool isActive() {
        if (this->pi.hProcess == NULL) {
            std::cout << "Process handle is closed or invalid (" << GetLastError() << ").\n";
            return false;
        }

        DWORD lpExitCode = 0;
        if (GetExitCodeProcess(this->pi.hProcess, &lpExitCode) == 0) {
            std::cout << "Cannot return exit code (" << GetLastError() << ").\n";
            throw std::exception("Cannot return exit code");
        } else {
            if (lpExitCode == STILL_ACTIVE) {
                return true;
            } else {
                return false;
            }
        }
    }

    bool stop() {
        if (this->pi.hProcess == NULL) {
            std::cout << "Process handle invalid. Possibly allready been closed (" << GetLastError() << ").\n";
            return false;
        }

        // Terminate Process
        if (!TerminateProcess(this->pi.hProcess, 1)) {
            std::cout << "ExitProcess failed (" << GetLastError() << ").\n";
            return false;
        }

        // Wait until child process exits.
        if (WaitForSingleObject(this->pi.hProcess, INFINITE) == WAIT_FAILED) {
            std::cout << "Wait for exit process failed(" << GetLastError() << ").\n";
            return false;
        }

        // Close process and thread handles.
        if (!CloseHandle(this->pi.hProcess)) {
            std::cout << "Cannot close process handle(" << GetLastError() << ").\n";
            return false;
        } else {
            this->pi.hProcess = NULL;
        }

        if (!CloseHandle(this->pi.hThread)) {
            std::cout << "Cannot close thread handle (" << GetLastError() << ").\n";
            return false;
        } else {
            this->pi.hProcess = NULL;
        }
        return true;
    }

    HWND getWindow() {
        if (this->window == nullptr) {
            this->findWindow();
        }
        return this->window;
    }

    std::wstring getName() {
        wchar_t buffer[256];
        GetWindowText(this->getWindow(), (LPWSTR)buffer, 255);
        return std::wstring(buffer);
    }

    std::wstring getStatus() {
        if (this->isActive()) {
            return L"Active";
        }
        return L"Terminated";
    }

    HANDLE getHandle() {
        return this->pi.hProcess;
    }

    ~Process() {
        if (this->isActive()) {
            this->stop();
        }
    }
};

std::vector<Process*> listProcesses;

void printListProcesses() {
    int id = 0;
    for (auto process : listProcesses) {
        if (!process) {
            continue;
        }
        std::wcout << id << ". " << process->getName() << ". Status: " << process->getStatus() << std::endl;
        id++;
    }
}

int main() {
    Process* bufferProccess = nullptr;
    bool stopped = false;
    while (!stopped) {
        int op;
        std::string path;
        std::cout << "Print 1, if you want to execute program.\nPrint 2, if you want to check list of executed programs.\nPrint 3, if you want to close program.\nPrint 4, if you want to exit" << std::endl;
        std::cin >> op;
        std::getline(std::cin, path);

        switch (op){
            case 1: {
                std::cout << "print the path to the executable" << std::endl;
                std::getline(std::cin, path);
                try {
                    bufferProccess = new Process();
                    bufferProccess->launchProcess(path);
                    listProcesses.push_back(bufferProccess);
                } catch (std::exception error) {
                    std::cout << std::string(error.what()) << "\n";
                }
                break;
            }
            case 2: {
                printListProcesses();
                break;
            }
            case 3: {
                printListProcesses();
                int number = 0;
                while (true) {
                    std::cin >> number;
                    std::getline(std::cin, path);
                    if (number < 0 || number >= (int)listProcesses.size()) {
                        std::cout << "Incorrect number. Print number in range [0; " << listProcesses.size() << "]" << std::endl;
                    } else {
                        break;
                    }
                }
                auto response = SendMessage(listProcesses[number]->getWindow(), WM_CLOSE, 0, 0);
                if (GetLastError()) {
                    std::cout << "Error occured when send wm close message. " << GetLastError() << std::endl;
                }
                break;
            }
            case 4: {
                if (!listProcesses.size()) {
                    stopped = true;
                    break;
                }
                std::string op;
                std::cout << "Do you want to wait all child processes?\nPrint y/n" << std::endl;
                std::cin >> op;
                if (op == "y") {
                    HANDLE* processHandles = new HANDLE[listProcesses.size()];
                    for (size_t i = 0; i < listProcesses.size(); i++) {
                        processHandles[i] = listProcesses[i]->getHandle();
                    }
                    WaitForMultipleObjects(listProcesses.size(), processHandles, true, INFINITE);
                } else {
                    for (auto process : listProcesses) {
                        if (process->isActive()) {
                            auto response = SendMessage(process->getWindow(), WM_CLOSE, 0, 0);
                            if (GetLastError()) {
                                std::cout << "Error occured when send wm close message. " << GetLastError();
                            }
                        }
                    }
                }
                stopped = true;
                break;
            }
            default: {
                std::cout << "Incorrect number\n";
                std::getline(std::cin, path);
                break;
            }
        }
    }

    for (auto process : listProcesses) {
        delete process;
    }

	return 0;
}