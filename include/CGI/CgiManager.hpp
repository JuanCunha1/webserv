#pragma once

#include <vector>
#include <cstddef>

#include "CGI/CgiProcess.hpp"

/*
 * Associates one CGI process with the client that started it.
 * CgiTask does not own the CgiProcess by itself; CgiManager owns
 * processes after a successful add().
 */
class CgiTask
{
	private:
		int         _clientFd;
		CgiProcess *_process;

		CgiTask();
		CgiTask(const CgiTask &other);
		CgiTask &operator=(const CgiTask &other);

	public:
		CgiTask(int clientFd, CgiProcess *process);

		int getClientFd() const;
		CgiProcess *getProcess() const;

		bool usesFd(int fd) const;
};

class CgiManager
	{
	private:
		std::vector<CgiTask *> _tasks;

		CgiManager(const CgiManager &other);
		CgiManager &operator=(const CgiManager &other);

	public:
		CgiManager();
		~CgiManager();

		/*
		* On success, CgiManager takes ownership of process.
		* On failure, ownership remains with the caller.
		* A client can have only one active CGI task.
		*/
		bool add(int clientFd, CgiProcess *process);

		CgiTask *findCgiByClient(int clientFd) const;
		CgiTask *findCgiByFd(int pipeFd) const;

		bool remove(CgiTask *task);
		bool removeCgiByClient(int clientFd);

		size_t size() const;
		CgiTask *at(size_t index) const;
};