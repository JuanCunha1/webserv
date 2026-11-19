#pragma once


#include <string>
#include <vector>
#include <cstddef>

#include "CGI/CgiProcess.hpp"

class CgiHandler
{
public:
    enum State
    {
        CGI_NOT_STARTED,
        CGI_RUNNING,
        CGI_FINISHED,
        CGI_FAILED
    };

    enum IOResult
    {
        IO_SUCCESS,
        IO_CLOSED,
        IO_ERROR,
        IO_OUTPUT_LIMIT
    };

private:
    CgiProcess  _process;
    State       _state;
    int         _httpStatus;
    std::string _httpReason;
    std::string _httpResponse;

    CgiHandler(const CgiHandler &other);
    CgiHandler &operator=(const CgiHandler &other);

    bool parseCgiOutput();
    bool parseStatusHeader(const std::string &value);
    static std::string trim(const std::string &value);
    static std::string statusReason(int status);
    static bool isValidHeaderName(const std::string &name);
    static bool isHopByHopHeader(const std::string &name);

public:
    explicit CgiHandler(size_t maxOutputSize);
    ~CgiHandler();

    void start(const std::string &interpreter,
               const std::string &scriptPath,
               const std::vector<std::string> &environment,
               const std::string &requestBody);

    IOResult handleStdinWritable();
    IOResult handleStdoutReadable();

    bool checkChildStatus();

    bool finalize();

    void terminate();

    int getStdinFd() const;
    int getStdoutFd() const;
    pid_t getPid() const;

    State getState() const;
    bool isRunning() const;
    bool isFinished() const;
    bool isFailed() const;
    bool isComplete() const;

    const std::string &getCgiOutput() const;
    const std::string &getHttpResponse() const;

    int getHttpStatus() const;
    const std::string &getHttpReason() const;

    bool hasInputToSend() const;
    bool isStdinClosed() const;
    bool isStdoutClosed() const;
    bool hasChildExited() const;
    std::time_t getStartTime() const;
};