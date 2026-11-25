#include "CGI/CgiManager.hpp"

#include <stdexcept>

CgiTask::CgiTask(int clientFd, CgiProcess *process)
	: _clientFd(clientFd),
	  _process(process)
{
	if (clientFd < 0 || process == NULL)
		throw std::invalid_argument("Invalid CGI task");
}

int CgiTask::getClientFd() const
{
	return _clientFd;
}

CgiProcess *CgiTask::getProcess() const
{
	return _process;
}

bool CgiTask::usesFd(int fd) const
{
	if (fd < 0 || _process == NULL)
		return false;

	return (_process->getStdinFd() == fd ||
			_process->getStdoutFd() == fd);
}

CgiManager::CgiManager()
	: _tasks()
{
}

CgiManager::~CgiManager()
{
	for (size_t i = 0; i < _tasks.size(); ++i)
	{
		delete _tasks[i]->getProcess();
		delete _tasks[i];
	}

	_tasks.clear();
}

bool CgiManager::add(int clientFd, CgiProcess *process)
{
	if (clientFd < 0 || process == NULL)
		return false;

	if (findCgiByClient(clientFd) != NULL)
		return false;

	CgiTask *task = new CgiTask(clientFd, process);

	try
	{
		_tasks.push_back(task);
	}
	catch (...)
	{
		delete task;
		throw;
	}

	return true;
}

CgiTask *CgiManager::findCgiByClient(int clientFd) const
{
	for (size_t i = 0; i < _tasks.size(); ++i)
	{
		if (_tasks[i]->getClientFd() == clientFd)
			return _tasks[i];
	}

	return NULL;
}

CgiTask *CgiManager::findCgiByFd(int pipeFd) const
{
	for (size_t i = 0; i < _tasks.size(); ++i)
	{
		if (_tasks[i]->usesFd(pipeFd))
			return _tasks[i];
	}

	return NULL;
}

bool CgiManager::remove(CgiTask *task)
{
	if (task == NULL)
		return false;

	for (std::vector<CgiTask *>::iterator it = _tasks.begin();
		 it != _tasks.end(); ++it)
	{
		if (*it == task)
		{
			delete (*it)->getProcess();
			delete *it;
			_tasks.erase(it);
			return true;
		}
	}

	return false;
}

bool CgiManager::removeCgiByClient(int clientFd)
{
	CgiTask *task = findCgiByClient(clientFd);

	if (task == NULL)
		return false;

	return remove(task);
}

size_t CgiManager::size() const
{
	return _tasks.size();
}

CgiTask *CgiManager::at(size_t index) const
{
	if (index >= _tasks.size())
		return NULL;

	return _tasks[index];
}