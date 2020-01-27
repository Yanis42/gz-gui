#include <unistd.h>
#include <exception>
#include <functional>
#include <QtGlobal>
#include <QTemporaryDir>
#include "patcher.h"

#ifdef Q_OS_WIN
# include <windows.h>
#else
# include <fcntl.h>
# include <poll.h>
# include <sys/wait.h>
#endif

static std::string quote(const std::string &str)
{
    std::string esc_str;
    esc_str.push_back('"');
    for (auto c : str) {
        if (c == '"')
            esc_str.push_back('\\');
        esc_str.push_back(c);
    }
    esc_str.push_back('"');
    return esc_str;
}

#ifdef Q_OS_WIN
class unique_handle
{
public:
    unique_handle() noexcept
        : m_handle(INVALID_HANDLE_VALUE)
    {
    }
    explicit unique_handle(HANDLE handle) noexcept
        : m_handle(handle)
    {
    }
    unique_handle(const unique_handle &) = delete;
    unique_handle(unique_handle &&other) noexcept
    {
        std::swap(m_handle, other.m_handle);
    }

    ~unique_handle()
    {
        if (m_handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_handle);
    }

    unique_handle &operator=(const unique_handle &) = delete;
    unique_handle &operator=(unique_handle &&other) noexcept
    {
        std::swap(m_handle, other.m_handle);
        return *this;
    }

    operator bool() const noexcept
    {
        return m_handle != INVALID_HANDLE_VALUE;
    }

    HANDLE get() const noexcept
    {
        return m_handle;
    }
    HANDLE *ptr()
    {
        if (m_handle == INVALID_HANDLE_VALUE)
            return &m_handle;
        else
            throw std::bad_cast();
    }
    HANDLE release() noexcept
    {
        HANDLE handle = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return handle;
    }
    void reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept
    {
        *this = unique_handle(handle);
    }
    void reset(unique_handle &&other) noexcept
    {
        *this = std::move(other);
    }

private:
    HANDLE m_handle;
};

#define TRY_WINAPI(f,...) \
{ \
    if (!(f)(__VA_ARGS__)) \
        throw winapi_exception(GetLastError(), #f); \
}

class winapi_exception : public std::exception
{
public:
    winapi_exception(DWORD dwErrorCode, const char *s)
        : m_dwErrorCode(dwErrorCode)
    {
        LPSTR lpMsgBuf;

        TRY_WINAPI(FormatMessageA,
                   FORMAT_MESSAGE_ALLOCATE_BUFFER
                   | FORMAT_MESSAGE_FROM_SYSTEM
                   | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, m_dwErrorCode,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   reinterpret_cast<LPSTR>(&lpMsgBuf), 0, nullptr)

        try {
            m_error_str = std::string(s) + ": " + lpMsgBuf;
        }
        catch (...) {
            LocalFree(lpMsgBuf);
            throw;
        }

        LocalFree(lpMsgBuf);
    }

    DWORD error_code() const noexcept
    {
        return m_dwErrorCode;
    }
    const char *what() const noexcept override
    {
        return m_error_str.c_str();
    }

private:
    DWORD m_dwErrorCode;
    std::string m_error_str;
};
#endif

class unique_fileno
{
public:
    unique_fileno() noexcept
        : m_fileno(-1)
    {
    }
    explicit unique_fileno(int fileno) noexcept
        : m_fileno(fileno)
    {
    }
    unique_fileno(const unique_fileno &) = delete;
    unique_fileno(unique_fileno &&other) noexcept
    {
        std::swap(m_fileno, other.m_fileno);
    }

    ~unique_fileno()
    {
        if (m_fileno != -1)
            close(m_fileno);
    }

    unique_fileno &operator=(const unique_fileno &) = delete;
    unique_fileno &operator=(unique_fileno &&other) noexcept
    {
        std::swap(m_fileno, other.m_fileno);
        return *this;
    }

    operator bool() const noexcept
    {
        return m_fileno != -1;
    }

    int get() const noexcept
    {
        return m_fileno;
    }
    int *ptr()
    {
        if (m_fileno == -1)
            return &m_fileno;
        else
            throw std::bad_cast();
    }
    int release() noexcept
    {
        int fileno = m_fileno;
        m_fileno = -1;
        return fileno;
    }
    void reset(int fileno = -1) noexcept
    {
        *this = unique_fileno(fileno);
    }
    void reset(unique_fileno &&other) noexcept
    {
        *this = std::move(other);
    }

private:
    int m_fileno;
};

#define TRY_POSIX(f,...) \
{ \
    if ((f)(__VA_ARGS__) == -1) \
        throw posix_exception(errno, #f); \
}

class posix_exception : public std::exception
{
public:
    posix_exception(int error_code, const char *s)
        : m_error_code(error_code)
    {
        m_error_str = std::string(s) + ": " + strerror(m_error_code);
    }

    int error_code() const noexcept
    {
        return m_error_code;
    }
    const char *what() const noexcept override
    {
        return m_error_str.c_str();
    }

private:
    int m_error_code;
    std::string m_error_str;
};

static int invoke_subprogram(const std::string &cmd, const std::string &input,
                             std::function<void(const std::string &)> stdout_fn,
                             std::function<void(const std::string &)> stderr_fn)
{
#ifdef Q_OS_WIN
    unique_handle hStdInRd;
    unique_handle hStdInWr;
    unique_handle hStdOutRd;
    unique_handle hStdOutWr;
    unique_handle hStdErrRd;
    unique_handle hStdErrWr;

    {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        TRY_WINAPI(CreatePipe, hStdInRd.ptr(), hStdInWr.ptr(), &sa, 0)
        TRY_WINAPI(SetHandleInformation, hStdInWr.get(),
                   HANDLE_FLAG_INHERIT, 0)

        TRY_WINAPI(CreatePipe, hStdOutRd.ptr(), hStdOutWr.ptr(), &sa, 0)
        TRY_WINAPI(SetHandleInformation, hStdOutRd.get(),
                   HANDLE_FLAG_INHERIT, 0)

        TRY_WINAPI(CreatePipe, hStdErrRd.ptr(), hStdErrWr.ptr(), &sa, 0)
        TRY_WINAPI(SetHandleInformation, hStdErrRd.get(),
                   HANDLE_FLAG_INHERIT, 0)
    }

    unique_handle hChildProcess;
    unique_handle hChildThread;

    {
        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdInput = hStdInRd.get();
        si.hStdOutput = hStdOutWr.get();
        si.hStdError = hStdErrWr.get();
        si.dwFlags = STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        TRY_WINAPI(CreateProcessA, nullptr, const_cast<char*>(cmd.c_str()),
                   nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                   &si, &pi)
        hChildProcess = unique_handle(pi.hProcess);
        hChildThread = unique_handle(pi.hThread);

        hStdInRd.reset();
        hStdOutWr.reset();
        hStdErrWr.reset();
    }

    {
        DWORD dwWritten;
        TRY_WINAPI(WriteFile, hStdInWr.get(), input.c_str(),
                   static_cast<DWORD>(input.size()), &dwWritten, nullptr)
        hStdInWr.reset();
    }

    while (true) {
        bool stdout_hup = false;
        bool stderr_hup = false;
        char output_buf[1024];
        DWORD dwBytes;

        if (!PeekNamedPipe(hStdOutRd.get(), nullptr, 0, nullptr,
                           &dwBytes, nullptr))
        {
            DWORD dwErrorCode = GetLastError();
            if (dwErrorCode == ERROR_BROKEN_PIPE)
                stdout_hup = true;
            else
                throw winapi_exception(dwErrorCode, "PeekNamedPipe");
        }
        else if (dwBytes != 0) {
            TRY_WINAPI(ReadFile, hStdOutRd.get(), output_buf, 1024,
                       &dwBytes, nullptr)
            stdout_fn(std::string(output_buf, dwBytes));
        }

        if (!PeekNamedPipe(hStdErrRd.get(), nullptr, 0, nullptr,
                           &dwBytes, nullptr))
        {
            DWORD dwErrorCode = GetLastError();
            if (dwErrorCode == ERROR_BROKEN_PIPE)
                stderr_hup = true;
            else
                throw winapi_exception(dwErrorCode, "PeekNamedPipe");
        }
        else if (dwBytes != 0) {
            TRY_WINAPI(ReadFile, hStdErrRd.get(), output_buf, 1024,
                       &dwBytes, nullptr)
            stderr_fn(std::string(output_buf, dwBytes));
        }

        if (stdout_hup && stderr_hup)
            break;
    }
    hStdOutRd.reset();
    hStdErrRd.reset();

    WaitForSingleObject(hChildProcess.get(), INFINITE);
    DWORD dwStatus;
    TRY_WINAPI(GetExitCodeProcess, hChildProcess.get(), &dwStatus)

    return static_cast<int>(dwStatus);
#else
    unique_fileno stdin_rd;
    unique_fileno stdin_wr;
    unique_fileno stdout_rd;
    unique_fileno stdout_wr;
    unique_fileno stderr_rd;
    unique_fileno stderr_wr;

    {
        int pipefd[2];

        TRY_POSIX(pipe, pipefd)
        stdin_rd = unique_fileno(pipefd[0]);
        stdin_wr = unique_fileno(pipefd[1]);

        TRY_POSIX(pipe, pipefd)
        stdout_rd = unique_fileno(pipefd[0]);
        stdout_wr = unique_fileno(pipefd[1]);

        TRY_POSIX(pipe, pipefd)
        stderr_rd = unique_fileno(pipefd[0]);
        stderr_wr = unique_fileno(pipefd[1]);
    }

    pid_t cpid = fork();

    if (cpid == -1)
        throw posix_exception(errno, "fork");
    else if (cpid == 0) {
        stdin_wr.reset();
        stdout_rd.reset();
        stderr_rd.reset();
        if (dup2(stdin_rd.get(), STDIN_FILENO) == -1)
            exit(EXIT_FAILURE);
        if (dup2(stdout_wr.get(), STDOUT_FILENO) == -1)
            exit(EXIT_FAILURE);
        if (dup2(stderr_wr.get(), STDERR_FILENO) == -1)
            exit(EXIT_FAILURE);
        if (execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr) == -1)
            exit(EXIT_FAILURE);
    }
    else {
        stdin_rd.reset();
        stdout_wr.reset();
        stderr_wr.reset();
    }

    TRY_POSIX(write, stdin_wr.get(), input.c_str(), input.size())
    close(stdin_wr.get());

    struct pollfd pollfds[] =
    {
        {stdout_rd.get(), POLLIN, 0},
        {stderr_rd.get(), POLLIN, 0},
    };
    while (true) {
        bool stdout_hup = false;
        bool stderr_hup = false;
        char output_buf[1024];
        ssize_t n_bytes;

        TRY_POSIX(poll, pollfds, 2, -1)

        if (pollfds[0].revents) {
            n_bytes = read(stdout_rd.get(), output_buf, 1024);
            if (n_bytes == -1)
                throw posix_exception(errno, "read");
            else if (n_bytes == 0)
                stdout_hup = true;
            else {
                stdout_fn(std::string(output_buf,
                                      static_cast<size_t>(n_bytes)));
            }
        }

        if (pollfds[1].revents) {
            n_bytes = read(stderr_rd.get(), output_buf, 1024);
            if (n_bytes == -1)
                throw posix_exception(errno, "read");
            else if (n_bytes == 0)
                stderr_hup = true;
            else {
                stderr_fn(std::string(output_buf,
                                      static_cast<size_t>(n_bytes)));
            }
        }

        if (stdout_hup && stderr_hup)
            break;
    }
    stdout_rd.reset();
    stderr_rd.reset();

    int status;
    TRY_POSIX(waitpid, cpid, &status, 0)

    return WEXITSTATUS(status);
#endif
}

Patcher::Patcher(const PatcherSettings &settings, QWidget *parent)
    : QThread(parent)
    , settings(settings)
{
}

int Patcher::getResult()
{
    wait();
    if (eptr)
        std::rethrow_exception(eptr);
    return result;
}

int Patcher::patch()
{
    auto output_to_log = [this](const std::string &str)
    {
        emit output(QString::fromStdString(str));
    };
    std::string output_str;
    auto output_to_str = [&output_str](const std::string &str)
    {
        output_str += str;
    };

#ifdef Q_OS_WIN
    std::string gru = "bin\\gru.exe";
    std::string gzinject = "bin\\gzinject.exe";
    _putenv_s("GZINJECT", gzinject.c_str());
#else
    std::string gru = "bin/gru";
    std::string gzinject = "bin/gzinject";
    setenv("GZINJECT", gzinject.c_str(), 1);
#endif

    int status = 0;
    QTemporaryDir tmpdir;
    if (!tmpdir.isValid())
        throw std::runtime_error(tmpdir.errorString().toStdString());

    switch (settings.patch_mode) {
        case PatcherSettings::patch_mode_t::ROM: {
            std::string rom_path = tmpdir.filePath("gz.z64").toStdString();
            std::string cmd;

            cmd = gru + " lua/patch-rom.lua -s -o " + quote(rom_path) + " "
                  + quote(settings.rom_path);

            emit output(QString::fromStdString("executing: " + cmd + "\n"));
            output_str.clear();
            status = invoke_subprogram(cmd, "", output_to_str, output_to_log);
            if (status != 0)
                break;

            std::string gz_rom_name = std::move(output_str);
            while (!gz_rom_name.empty() && isspace(gz_rom_name.back()))
                gz_rom_name.pop_back();

            if (settings.opt_ucode) {
                cmd = gru + " lua/inject_ucode.lua " + quote(rom_path) + " "
                      + quote(settings.ucode_path);
                emit output(QString::fromStdString("executing: " + cmd + "\n"));
                status = invoke_subprogram(cmd, "", output_to_log,
                                           output_to_log);
                if (status != 0)
                    break;
            }

            QString save_name;
            emit needSaveFileName(&save_name, "Save as...", gz_rom_name.c_str(),
                                  "Nintendo 64 ROM (Big Endian) (*.z64)");
            if (save_name.isEmpty()) {
                status = 0;
                break;
            }

            emit output("saving: " + save_name + "\n");
            QFile rom_file(rom_path.c_str());
            rom_file.rename(save_name);
            if (rom_file.error() == QFile::RenameError) {
              QFile::remove(save_name);
              rom_file.rename(save_name);
            }
            if (rom_file.error() != QFile::NoError)
              throw std::runtime_error(rom_file.errorString().toStdString());

            break;
        }
        case PatcherSettings::patch_mode_t::WAD: {
            std::string key_path = tmpdir.filePath("common-key.bin")
                    .toStdString();
            std::string extract_path = tmpdir.filePath("wadextract")
                    .toStdString();
            std::string wad_path = tmpdir.filePath("gz.wad").toStdString();
            std::string cmd;

            cmd = gzinject + " -a genkey -k " + quote(key_path);
            emit output(QString::fromStdString("executing: " + cmd + "\n"));
            status = invoke_subprogram(cmd, "45e", output_to_log,
                                       output_to_log);
            if (status != 0)
                break;

            cmd = gru + " lua/patch-wad.lua -s -k " + quote(key_path) + " -d "
                  + quote(extract_path);
            if (settings.wad_remap == PatcherSettings::wad_remap_t::RAPHNET)
                cmd += " --raphnet";
            else if (settings.wad_remap == PatcherSettings::wad_remap_t::NONE)
                cmd += " --disable-controller-remappings";
            if (!settings.channel_id.empty())
                cmd += " -i " + quote(settings.channel_id);
            if (!settings.channel_title.empty())
                cmd += " -t " + quote(settings.channel_title);
            cmd += " -r " + std::to_string(settings.wad_region);
            if (settings.opt_extrom)
                cmd += " -m " + quote(settings.extrom_path);
            cmd += " -o " + quote(wad_path) + " " + quote(settings.wad_path);

            emit output(QString::fromStdString("executing: " + cmd + "\n"));
            output_str.clear();
            status = invoke_subprogram(cmd, "", output_to_str, output_to_log);
            if (status != 0)
                break;

            std::string gz_wad_name = std::move(output_str);
            while (!gz_wad_name.empty() && isspace(gz_wad_name.back()))
                gz_wad_name.pop_back();

            QString save_name;
            emit needSaveFileName(&save_name, "Save as...", gz_wad_name.c_str(),
                                  "Nintendo Wii WAD (*.wad)");
            if (save_name.isEmpty()) {
                status = 0;
                break;
            }

            emit output("saving: " + save_name + "\n");
            QFile wad_file(wad_path.c_str());
            wad_file.rename(save_name);
            if (wad_file.error() == QFile::RenameError) {
              QFile::remove(save_name);
              wad_file.rename(save_name);
            }
            if (wad_file.error() != QFile::NoError)
              throw std::runtime_error(wad_file.errorString().toStdString());

            break;
        }
    }

    return status;
}

void Patcher::run()
{
    try {
        result = patch();
    }
    catch (...) {
        eptr = std::current_exception();
    }
}
