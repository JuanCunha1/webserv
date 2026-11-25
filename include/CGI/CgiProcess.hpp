#pragma once

#include <cstddef>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#include <stdexcept>
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <sys/types.h>

class CgiProcess
{
	public:
		enum IOResult
		{
			IO_SUCCESS,
			IO_WOULD_BLOCK,
			IO_CLOSED,
			IO_ERROR,
			IO_LIMIT_EXCEEDED
		};

	private:
		pid_t       _pid;

		int         _stdinFd;
		int         _stdoutFd;

		std::string _inputBuffer;
		std::string _outputBuffer;

		size_t      _inputOffset;
		size_t      _maxOutputSize;

		std::time_t _startTime;

		bool        _stdinClosed;
		bool        _stdoutClosed;
		bool        _childExited;

		int         _exitStatus;

		CgiProcess(const CgiProcess &other);
		CgiProcess &operator=(const CgiProcess &other);

		void closeInput();
		void closeOutput();
		void closeFd(int &fd);

		void createPipes(int inputPipe[2], int outputPipe[2]);
		void closePipe(int pipeFds[2]);

		void buildEnvironment(
			const std::vector<std::string> &environment,
			std::vector<char *> &envp);

		void executeChild(
			const int inputPipe[2],
			const int outputPipe[2],
			const std::string &interpreter,
			const std::string &scriptPath,
			char *envp[]) const;

		void configureParent(
			int inputPipe[2],
			int outputPipe[2],
		const std::string &requestBody);

	public:
		explicit CgiProcess(size_t maxOutputSize);
		~CgiProcess();

		CgiProcess::IOResult writeInput();
		CgiProcess::IOResult readOutput();

		void start(
			const std::string &interpreter,
			const std::string &scriptPath,
			const std::vector<std::string> &environment,
			const std::string &requestBody);

		bool checkChildStatus();
		void terminate();

		pid_t getPid() const;
		int getStdinFd() const;
		int getStdoutFd() const;

		bool hasInputToSend() const;
		bool isFinished() const;

		const std::string &getOutput() const;

		bool isStdinClosed() const;
		bool isStdoutClosed() const;
		bool hasChildExited() const;

		std::time_t getStartTime() const;
		int getExitStatus() const;

		bool hasSucceeded() const;
		bool hasTimedOut(std::time_t now, std::time_t timeout) const;
};