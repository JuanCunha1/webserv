#include "CGI/CgiProcess.hpp"

CgiProcess::CgiProcess(size_t maxOutputSize)
	: _pid(-1),
	  _stdinFd(-1),
	  _stdoutFd(-1),
	  _inputBuffer(),
	  _outputBuffer(),
	  _inputOffset(0),
	  _maxOutputSize(maxOutputSize),
	  _startTime(0),
	  _stdinClosed(true),
	  _stdoutClosed(true),
	  _childExited(true),
	  _exitStatus(-1)
{
}

CgiProcess::~CgiProcess()
{
	closeInput();
	closeOutput();

	if (_pid > 0 && !_childExited)
	{
		pid_t pid = _pid;

		if (kill(pid, SIGKILL) == 0 || errno == ESRCH)
		{
			int status = 0;
			pid_t result;

			do
			{
				result = waitpid(pid, &status, 0);
			}
			while (result == -1 && errno == EINTR);

			if (result == pid)
			{
				_exitStatus = status;
				_childExited = true;
			}
			else if (result == -1 && errno == ECHILD)
				_childExited = true;
		}

		_pid = -1;
	}
}

void CgiProcess::closeFd(int &fd)
{
	if (fd != -1)
	{
		close(fd);
		fd = -1;
	}
}

void CgiProcess::closeInput()
{
	closeFd(_stdinFd);
	_stdinClosed = true;
}

void CgiProcess::closeOutput()
{
	closeFd(_stdoutFd);
	_stdoutClosed = true;
}

pid_t CgiProcess::getPid() const
{
	return _pid;
}

int CgiProcess::getStdinFd() const
{
	return _stdinFd;
}

int CgiProcess::getStdoutFd() const
{
	return _stdoutFd;
}

void CgiProcess::closePipe(int pipeFds[2])
{
	closeFd(pipeFds[0]);
	closeFd(pipeFds[1]);
}

void CgiProcess::createPipes(int inputPipe[2], int outputPipe[2])
{
	inputPipe[0] = -1;
	inputPipe[1] = -1;
	outputPipe[0] = -1;
	outputPipe[1] = -1;

	if (pipe(inputPipe) == -1)
		throw std::runtime_error(
			std::string("CGI stdin pipe failed: ") + std::strerror(errno));

	if (pipe(outputPipe) == -1)
	{
		int savedErrno = errno;
		closePipe(inputPipe);
		throw std::runtime_error(
			std::string("CGI stdout pipe failed: ") +
			std::strerror(savedErrno));
	}
}

void CgiProcess::buildEnvironment(
	const std::vector<std::string> &environment,
	std::vector<char *> &envp)
{
	envp.clear();
	envp.reserve(environment.size() + 1);

	for (size_t i = 0; i < environment.size(); ++i)
		envp.push_back(const_cast<char *>(environment[i].c_str()));

	envp.push_back(NULL);
}

void CgiProcess::executeChild(
	const int inputPipe[2],
	const int outputPipe[2],
	const std::string &interpreter,
	const std::string &scriptPath,
	char *envp[]) const
{
	if (dup2(inputPipe[0], STDIN_FILENO) == -1)
		_exit(126);

	if (dup2(outputPipe[1], STDOUT_FILENO) == -1)
		_exit(126);

	if (inputPipe[0] != STDIN_FILENO &&
		inputPipe[0] != STDOUT_FILENO)
		close(inputPipe[0]);

	if (inputPipe[1] != STDIN_FILENO &&
		inputPipe[1] != STDOUT_FILENO)
		close(inputPipe[1]);

	if (outputPipe[0] != STDIN_FILENO &&
		outputPipe[0] != STDOUT_FILENO)
		close(outputPipe[0]);

	if (outputPipe[1] != STDIN_FILENO &&
		outputPipe[1] != STDOUT_FILENO)
		close(outputPipe[1]);

	char *argv[3];
	argv[0] = const_cast<char *>(interpreter.c_str());
	argv[1] = const_cast<char *>(scriptPath.c_str());
	argv[2] = NULL;

	execve(interpreter.c_str(), argv, envp);
	_exit(127);
}

void CgiProcess::configureParent(
	int inputPipe[2],
	int outputPipe[2],
	const std::string &requestBody)
{
	closeFd(inputPipe[0]);
	closeFd(outputPipe[1]);

	_stdinFd = inputPipe[1];
	inputPipe[1] = -1;

	_stdoutFd = outputPipe[0];
	outputPipe[0] = -1;

	_inputBuffer = requestBody;
	_outputBuffer.clear();
	_inputOffset = 0;
	_startTime = std::time(NULL);
	_stdinClosed = false;
	_stdoutClosed = false;
	_childExited = false;
	_exitStatus = -1;

	if (fcntl(_stdinFd, F_SETFL, O_NONBLOCK) == -1)
	{
		int savedErrno = errno;
		throw std::runtime_error(
			std::string("fcntl CGI stdin failed: ") +
			std::strerror(savedErrno));
	}

	if (fcntl(_stdoutFd, F_SETFL, O_NONBLOCK) == -1)
	{
		int savedErrno = errno;
		throw std::runtime_error(
			std::string("fcntl CGI stdout failed: ") +
			std::strerror(savedErrno));
	}

	if (_inputBuffer.empty())
		closeInput();
}

void CgiProcess::start(
	const std::string &interpreter,
	const std::string &scriptPath,
	const std::vector<std::string> &environment,
	const std::string &requestBody)
{
	if (interpreter.empty() || scriptPath.empty())
		throw std::runtime_error("CGI interpreter or script path is empty");

	if (_pid != -1 || _stdinFd != -1 || _stdoutFd != -1)
		throw std::runtime_error("CGI process has already been started");

	std::vector<char *> envp;
	buildEnvironment(environment, envp);

	int inputPipe[2];
	int outputPipe[2];
	createPipes(inputPipe, outputPipe);

	pid_t childPid = fork();

	if (childPid == -1)
	{
		int savedErrno = errno;
		closePipe(inputPipe);
		closePipe(outputPipe);
		throw std::runtime_error(
			std::string("CGI fork failed: ") + std::strerror(savedErrno));
	}

	if (childPid == 0)
		executeChild(inputPipe, outputPipe, interpreter, scriptPath, &envp[0]);

	_pid = childPid;

	try
	{
		configureParent(inputPipe, outputPipe, requestBody);
	}
	catch (...)
	{
		closePipe(inputPipe);
		closePipe(outputPipe);
		terminate();
		throw;
	}

	closePipe(inputPipe);
	closePipe(outputPipe);
}

CgiProcess::IOResult CgiProcess::writeInput()
{
	if (_stdinClosed || _stdinFd == -1)
		return IO_CLOSED;

	if (_inputOffset >= _inputBuffer.size())
	{
		closeInput();
		return IO_CLOSED;
	}

	ssize_t count = write(
		_stdinFd,
		_inputBuffer.data() + _inputOffset,
		_inputBuffer.size() - _inputOffset);

	if (count > 0)
	{
		_inputOffset += static_cast<size_t>(count);

		if (_inputOffset >= _inputBuffer.size())
		{
			_inputBuffer.clear();
			_inputOffset = 0;
			closeInput();
			return IO_CLOSED;
		}

		return IO_SUCCESS;
	}

	if (count == -1)
	{
		if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			return IO_WOULD_BLOCK;

		if (errno == EPIPE)
		{
			closeInput();
			return IO_ERROR;
		}
	}

	closeInput();
	return IO_ERROR;
}

CgiProcess::IOResult CgiProcess::readOutput()
{
	if (_stdoutClosed || _stdoutFd == -1)
		return IO_CLOSED;

	char buffer[4096];

	ssize_t count = read(_stdoutFd, buffer, sizeof(buffer));

	if (count > 0)
	{
		size_t amount = static_cast<size_t>(count);

		if (_maxOutputSize != 0 &&
			(_outputBuffer.size() > _maxOutputSize ||
			 amount > _maxOutputSize - _outputBuffer.size()))
		{
			closeOutput();
			return IO_LIMIT_EXCEEDED;
		}

		_outputBuffer.append(buffer, amount);
		return IO_SUCCESS;
	}

	if (count == 0)
	{
		closeOutput();
		return IO_CLOSED;
	}

	if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
		return IO_WOULD_BLOCK;

	closeOutput();
	return IO_ERROR;
}

bool CgiProcess::checkChildStatus()
{
	if (_pid <= 0 || _childExited)
		return true;

	int status = 0;
	pid_t result = waitpid(_pid, &status, WNOHANG);

	if (result == 0)
		return false;

	if (result == -1)
	{
		if (errno == EINTR)
			return false;

		if (errno == ECHILD)
		{
			_childExited = true;
			_pid = -1;
			return true;
		}

		throw std::runtime_error(
			std::string("CGI waitpid failed: ") + std::strerror(errno));
	}

	_childExited = true;
	_exitStatus = status;
	_pid = -1;
	return true;
}

void CgiProcess::terminate()
{
	closeInput();
	closeOutput();

	if (_pid <= 0 || _childExited)
		return;

	pid_t pid = _pid;

	if (kill(pid, SIGKILL) == -1 && errno != ESRCH)
		return;

	int status = 0;
	pid_t result;

	do
	{
		result = waitpid(pid, &status, 0);
	}
	while (result == -1 && errno == EINTR);

	if (result == pid)
	{
		_exitStatus = status;
		_childExited = true;
		_pid = -1;
	}
	else if (result == -1 && errno == ECHILD)
	{
		_childExited = true;
		_pid = -1;
	}
}

const std::string &CgiProcess::getOutput() const
{
	return _outputBuffer;
}

bool CgiProcess::hasInputToSend() const
{
	return !_stdinClosed && _inputOffset < _inputBuffer.size();
}

bool CgiProcess::isStdinClosed() const
{
	return _stdinClosed;
}

bool CgiProcess::isStdoutClosed() const
{
	return _stdoutClosed;
}

bool CgiProcess::hasChildExited() const
{
	return _childExited;
}

bool CgiProcess::isFinished() const
{
	return _childExited && _stdoutClosed && _stdinClosed;
}

std::time_t CgiProcess::getStartTime() const
{
	return _startTime;
}

int CgiProcess::getExitStatus() const
{
	return _exitStatus;
}

bool CgiProcess::hasSucceeded() const
{
	return _childExited
		&& _stdinClosed
		&& _stdoutClosed
		&& _exitStatus != -1
		&& WIFEXITED(_exitStatus)
		&& WEXITSTATUS(_exitStatus) == 0;
}

bool CgiProcess::hasTimedOut(
	std::time_t now,
	std::time_t timeout) const
{
	return _startTime > 0
		&& !_childExited
		&& now - _startTime >= timeout;
}