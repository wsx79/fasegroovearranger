// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GuideProcess.h"
#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <spawn.h>
 #include <signal.h>
 #include <sys/wait.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <cerrno>
 #if JUCE_MAC
  #include <crt_externs.h>
 #else
  extern char** environ;
 #endif
#endif
namespace fga {
juce::Result runGuideProcess(const juce::StringArray& args, const juce::File& log,
                            int timeoutMs, const std::function<bool()>& cancelled) {
    if (args.isEmpty() || cancelled()) return juce::Result::fail("Guide cancelled.");
    const auto began = juce::Time::getMillisecondCounterHiRes();
    auto stopReason = [&]() -> juce::String {
        if (cancelled()) return "Guide cancelled.";
        if (juce::Time::getMillisecondCounterHiRes() - began > timeoutMs) return "Guide process time limit exceeded.";
        if (log.getSize() > 1024 * 1024) return "Guide diagnostic output limit exceeded.";
        return {};
    };
#if JUCE_WINDOWS
    // Windows command-line quoting follows the C runtime argv rules, not cmd.exe.
    auto quote = [](const juce::String& value) {
        juce::String out = "\""; int slashes = 0;
        for (auto c : value) {
            if (c == '\\') { ++slashes; continue; }
            if (c == '"') out += juce::String::repeatedString("\\", slashes * 2 + 1);
            else out += juce::String::repeatedString("\\", slashes);
            slashes = 0; out += juce::String::charToString(c);
        }
        return out + juce::String::repeatedString("\\", slashes * 2) + "\"";
    };
    juce::String command;
    for (const auto& arg : args) { if (command.isNotEmpty()) command += " "; command += quote(arg); }
    std::wstring wide = command.toWideCharPointer();
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE output = CreateFileW(log.getFullPathName().toWideCharPointer(), GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (output == INVALID_HANDLE_VALUE || input == INVALID_HANDLE_VALUE || !job) {
        if (output != INVALID_HANDLE_VALUE) CloseHandle(output);
        if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
        if (job) CloseHandle(job);
        return juce::Result::fail("Cannot create guide process handles.");
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    STARTUPINFOW startup{}; startup.cb = sizeof(startup); startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input; startup.hStdOutput = output; startup.hStdError = output;
    PROCESS_INFORMATION process{};
    const bool started = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))
        && CreateProcessW(nullptr, wide.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
    CloseHandle(input); CloseHandle(output);
    if (!started) { CloseHandle(job); return juce::Result::fail("Cannot start guide tool. Check native Perl/PATH."); }
    if (!AssignProcessToJobObject(job, process.hProcess) || ResumeThread(process.hThread) == (DWORD)-1) {
        TerminateProcess(process.hProcess, 1); WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(job);
        return juce::Result::fail("Cannot supervise guide process tree.");
    }
    CloseHandle(process.hThread);
    juce::String reason;
    DWORD waitResult;
    while ((waitResult = WaitForSingleObject(process.hProcess, 20)) == WAIT_TIMEOUT) {
        reason = stopReason(); if (reason.isNotEmpty()) break;
    }
    if (reason.isNotEmpty() || waitResult == WAIT_FAILED) {
        TerminateJobObject(job, 1); WaitForSingleObject(process.hProcess, INFINITE);
    }
    DWORD code = 1; GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess); CloseHandle(job);
    if (reason.isEmpty()) reason = stopReason();
    if (reason.isNotEmpty()) return juce::Result::fail(reason);
    if (waitResult == WAIT_FAILED) return juce::Result::fail("Cannot wait for guide process.");
#else
    // posix_spawn avoids allocating/locking after fork in the multithreaded DAW.
    std::vector<std::string> strings; for (const auto& arg : args) strings.push_back(arg.toStdString());
    std::vector<char*> argv; for (auto& s : strings) argv.push_back(s.data()); argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    if (posix_spawn_file_actions_init(&actions) != 0) return juce::Result::fail("Cannot prepare guide process.");
    if (posix_spawnattr_init(&attributes) != 0) { posix_spawn_file_actions_destroy(&actions); return juce::Result::fail("Cannot prepare guide process."); }
    int error = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    error |= posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log.getFullPathName().toRawUTF8(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
    error |= posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    error |= posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    error |= posix_spawnattr_setpgroup(&attributes, 0);
    pid_t pid = -1;
   #if JUCE_MAC
    auto environment = *_NSGetEnviron();
   #else
    auto environment = environ;
   #endif
    if (!error) error = posix_spawnp(&pid, argv[0], &actions, &attributes, argv.data(), environment);
    posix_spawn_file_actions_destroy(&actions); posix_spawnattr_destroy(&attributes);
    if (error) return juce::Result::fail("Cannot start guide tool. Check Perl/PATH (" + juce::String(error) + ").");
    int status = 0; juce::String reason;
    for (;;) {
        const auto result = waitpid(pid, &status, WNOHANG);
        if (result == pid) break;
        if (result < 0 && errno != EINTR) { reason = "Cannot wait for guide process."; break; }
        reason = stopReason(); if (reason.isNotEmpty()) break;
        juce::Thread::sleep(20);
    }
    // Kill surviving descendants even when the parent has already exited.
    ::kill(-pid, SIGKILL);
    if (reason.isEmpty()) reason = stopReason();
    if (reason.isNotEmpty()) { while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {} return juce::Result::fail(reason); }
    const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    if (code == 0) return juce::Result::ok();
    juce::MemoryBlock diagnostic;
    if (auto input = log.createInputStream()) input->readIntoMemoryBlock(diagnostic, 1024);
    return juce::Result::fail("Guide tool failed (exit " + juce::String((int)code) + "). "
        + juce::String::fromUTF8(static_cast<const char*>(diagnostic.getData()), (int)diagnostic.getSize()));
}
}
