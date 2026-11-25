#include "network/Server.hpp"
#include <csignal>
#include <cerrno>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>

static volatile sig_atomic_t g_running = 1;

static void handleSignal(int signal)
{
	(void)signal;
	g_running = 0;
}

void Server::setupSignals()
{
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);
	std::signal(SIGPIPE, SIG_IGN);
}

Server::Server()
	: _sockets(),
	  _clients(),
	  _pollFds(),
	  _serverConfigs()
{
}

Server::Server(std::vector<ServerConfig> serverConfigs)
	: _sockets(),
	  _clients(),
	  _pollFds(),
	  _serverConfigs(serverConfigs)
{
}

Server::~Server()
{
	for (std::vector<Client *>::iterator it = _clients.begin();
		 it != _clients.end(); ++it)
		delete *it;

	for (std::vector<Socket *>::iterator it = _sockets.begin();
		 it != _sockets.end(); ++it)
		delete *it;

	_clients.clear();
	_sockets.clear();
	_pollFds.clear();
}

void Server::start()
{
	for (size_t i = 0; i < _serverConfigs.size(); ++i)
	{
		const ServerConfig &config = _serverConfigs[i];

		std::cout << "Starting server on " << config.host
				  << ":" << config.port << std::endl;

		if (hasListeningSocket(config.host, config.port))
			continue;

		Socket *socket = new Socket(config.port, config.host);
		try
		{
			socket->create();
			socket->setReuseAddr();
			socket->setNonBlocking();
			socket->bindSocket();
			socket->listenSocket();
			addListeningSocket(socket);
		}
		catch (...)
		{
			delete socket;
			throw;
		}
	}

	std::cout << "Server started with " << _sockets.size()
			  << " listening socket(s)" << std::endl;
}

void Server::addListeningSocket(Socket *socket)
{
	struct pollfd pfd;

	pfd.fd = socket->getFd();
	pfd.events = POLLIN;
	pfd.revents = 0;

	_pollFds.push_back(pfd);
	try
	{
		_sockets.push_back(socket);
	}
	catch (...)
	{
		_pollFds.pop_back();
		throw;
	}
}

Socket *Server::findListeningSocket(int fd)
{
	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getFd() == fd)
			return _sockets[i];
	}
	return NULL;
}

void Server::run()
{
	while (g_running)
	{
		struct pollfd *fds = NULL;
		if (!_pollFds.empty())
			fds = &_pollFds[0];

		int result = poll(
			fds,
			static_cast<nfds_t>(_pollFds.size()),
			1000
		);

		if (result == -1)
		{
			if (errno == EINTR)
				continue;
			throw std::runtime_error("poll() failed");
		}

		if (result > 0)
		{
			size_t i = 0;
			while (g_running && i < _pollFds.size())
			{
				if (_pollFds[i].revents == 0)
				{
					++i;
					continue;
				}
				if (!handlePollEvent(i))
					++i;
			}
		}

		checkTimeouts();
	}

	std::cout << "\nShutting down server..." << std::endl;
}

bool Server::isListeningSocket(int fd) const
{
	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getFd() == fd)
			return true;
	}
	return false;
}

void Server::handleServerError(size_t index)
{
	if (index >= _pollFds.size())
		return;

	int fd = _pollFds[index].fd;
	std::cerr << "Error on listening socket fd " << fd << std::endl;

	for (std::vector<Socket *>::iterator it = _sockets.begin();
		 it != _sockets.end(); ++it)
	{
		if ((*it)->getFd() == fd)
		{
			delete *it;
			_sockets.erase(it);
			break;
		}
	}

	_pollFds.erase(_pollFds.begin() + index);
}

bool Server::handlePollEvent(size_t index)
{
	if (index >= _pollFds.size())
		return false;

	int fd = _pollFds[index].fd;
	short revents = _pollFds[index].revents;

	if (isListeningSocket(fd))
	{
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			handleServerError(index);
			return true;
		}

		if (revents & POLLIN)
			handleServerEvent(index);

		return false;
	}

	if (findCgiByFd(fd) != NULL)
	{
		handleCgiEvent(index);
		return findPollFd(fd) == -1;
	}

	handleClientEvent(index);
	return findPollFd(fd) == -1;
}

int Server::findPollFd(int fd) const
{
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
			return static_cast<int>(i);
	}
	return -1;
}

void Server::handleServerEvent(size_t index)
{
	if (index >= _pollFds.size())
		return;

	int fd = _pollFds[index].fd;
	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getFd() == fd)
		{
			while (addClient(i))
				;
			return;
		}
	}
}

void Server::handleClientEvent(size_t index)
{
	if (index >= _pollFds.size())
		return;

	const int fd = _pollFds[index].fd;
	const short revents = _pollFds[index].revents;

	if (revents & POLLNVAL)
	{
		std::cerr << "Invalid client socket fd: " << fd << std::endl;
		removeClient(index);
		return;
	}

	if (revents & (POLLIN | POLLHUP))
	{
		int currentIndex = findPollFd(fd);
		if (currentIndex != -1)
			handleClientRead(static_cast<size_t>(currentIndex),
						  (revents & POLLHUP) != 0);
	}

	int currentIndex = findPollFd(fd);
	if (currentIndex == -1)
		return;

	if (revents & POLLOUT)
		handleClientWrite(static_cast<size_t>(currentIndex));

	currentIndex = findPollFd(fd);
	if (currentIndex == -1)
		return;

	if (revents & POLLERR)
	{
		int socketError = 0;
		socklen_t errorLength = sizeof(socketError);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
					   &socketError, &errorLength) == -1)
			socketError = errno;
		std::cerr << "Client socket error: fd=" << fd
				  << ", error=" << socketError << std::endl;
		removeClient(static_cast<size_t>(currentIndex));
		return;
	}
	if (revents & POLLHUP)
	{
		Client *client = findClient(fd);
		if (client == NULL)
		{
			removeClient(static_cast<size_t>(currentIndex));
			return;
		}

		if (client->isReadClosed())
		{
			if (client->hasDataToSend())
			{
				_pollFds[currentIndex].events &= ~POLLIN;
				_pollFds[currentIndex].events |= POLLOUT;
			}
			else
				removeClient(static_cast<size_t>(currentIndex));
		}
 	}
}

void Server::handleRequest(Client &client)
{
	if (!client.beginRequestHandling())
		return;

	const Request &request = client.getRequest();

	if (isCgiRequest(request))
	{
		startCgi(client, request);
		return;
	}

	std::cout << "Received request: "
			  << request.getMethod() << " "
			  << request.getTarget() << " "
			  << request.getVersion() << std::endl;

	// Temporário: substituir pelo ResponseBuilder.
	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 13\r\n"
		"Connection: close\r\n"
		"\r\n"
		"Hello, World!";

	client.setResponse(response);
	client.setShouldClose(true);

	int pollIndex = findPollFd(client.getFd());

	if (pollIndex != -1)
		_pollFds[pollIndex].events |= POLLOUT;
}

void Server::handleClientRead(size_t index, bool drainOnHangup)
{
	if (index >= _pollFds.size())
		return;

	const int fd = _pollFds[index].fd;
	Client *client = findClient(fd);

	if (client == NULL)
	{
		removeClient(index);
		return;
	}

	Client::IOResult result = Client::IO_SUCCESS;
	do
	{
		result = client->receive();

		if (result == Client::IO_ERROR)
		{
			int currentIndex = findPollFd(fd);
			if (currentIndex != -1)
				removeClient(static_cast<size_t>(currentIndex));
			return;
		}

		if (result == Client::IO_CLOSED)
		{
			client->setReadClosed(true);

			int currentIndex = findPollFd(fd);
			if (currentIndex != -1)
				_pollFds[currentIndex].events &= ~POLLIN;

			break;
		}

		if (result == Client::IO_WOULD_BLOCK)
			break;

	} while (drainOnHangup);

	if (result == Client::IO_CLOSED ||
		(result == Client::IO_WOULD_BLOCK && drainOnHangup))
	{
		client->setReadClosed(true);
		int currentIndex = findPollFd(fd);
		if (currentIndex != -1)
			_pollFds[currentIndex].events &= ~POLLIN;
	}
	else if (result == Client::IO_WOULD_BLOCK)
		return;

	if (client->hasRequestError())
	{
		std::cerr << "Request parsing error: "
				  << client->getRequestError() << std::endl;

		client->setResponse(
			"HTTP/1.1 400 Bad Request\r\n"
			"Content-Length: 0\r\n"
			"Connection: close\r\n"
			"\r\n"
		);
		client->setShouldClose(true);
	}
	else if (client->hasCompleteRequest())
		handleRequest(*client);

	if (client->isReadClosed() && !client->hasDataToSend())
	{
		int currentIndex = findPollFd(fd);
		if (currentIndex != -1)
			removeClient(static_cast<size_t>(currentIndex));
		return;
	}

	if (client->hasDataToSend())
	{
		int currentIndex = findPollFd(fd);
		if (currentIndex != -1)
			_pollFds[currentIndex].events |= POLLOUT;
	}
}

void Server::handleClientWrite(size_t index)
{
	if (index >= _pollFds.size())
		return;

	int fd = _pollFds[index].fd;
	Client *client = findClient(fd);

	if (client == NULL)
	{
		removeClient(index);
		return;
	}

	Client::IOResult result = client->sendData();

	if (result == Client::IO_ERROR)
	{
		removeClient(index);
		return;
	}

	if (result == Client::IO_WOULD_BLOCK)
		return;

	if (client->hasDataToSend())
		return;

	_pollFds[index].events &= ~POLLOUT;

	if (client->shouldClose() || client->isReadClosed())
	{
		removeClient(index);
		return;
	}
}

Client *Server::findClient(int fd)
{
	for (size_t i = 0; i < _clients.size(); ++i)
	{
		if (_clients[i]->getFd() == fd)
			return _clients[i];
	}
	return NULL;
}

void Server::removeClient(size_t index)
{
	if (index >= _pollFds.size())
		return;

	int fd = _pollFds[index].fd;
	if (isListeningSocket(fd))
		return;

	for (std::vector<Client *>::iterator it = _clients.begin();
		 it != _clients.end(); ++it)
	{
		if ((*it)->getFd() == fd)
		{
			delete *it;
			_clients.erase(it);
			break;
		}
	}

	_pollFds.erase(_pollFds.begin() + index);
}

bool Server::addClient(size_t socketIndex)
{
	if (socketIndex >= _sockets.size())
		return false;

	Socket *listeningSocket = _sockets[socketIndex];
	int clientFd = listeningSocket->acceptConnection();
	if (clientFd == -1)
		return false;

	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1)
	{
		std::cerr << "Failed to set client socket non-blocking: "
				  << std::strerror(errno) << std::endl;
		close(clientFd);
		return true;
	}

	size_t configIndex = 0;
	while (configIndex < _serverConfigs.size())
	{
		if (_serverConfigs[configIndex].host == listeningSocket->getHost()
			&& _serverConfigs[configIndex].port == listeningSocket->getPort())
			break;
		++configIndex;
	}

	if (configIndex == _serverConfigs.size())
	{
		std::cerr << "Failed to set client socket non-blocking: "
				  << std::strerror(errno) << std::endl;
		close(clientFd);
		return true;
	}

	Client *client = NULL;
	try
	{
		client = new Client(
			clientFd,
			listeningSocket->getPort(),
			_serverConfigs[configIndex].maxBodySize
		);
	}
	catch (...)
	{
		close(clientFd);
		throw;
	}

	try
	{
		_clients.push_back(client);
	}
	catch (...)
	{
		delete client;
		throw;
	}

	struct pollfd clientPollFd;
	clientPollFd.fd = clientFd;
	clientPollFd.events = POLLIN;
	clientPollFd.revents = 0;

	try
	{
		_pollFds.push_back(clientPollFd);
	}
	catch (...)
	{
		_clients.pop_back();
		delete client;
		throw;
	}
	return true;
}

bool Server::hasListeningSocket(const std::string &host, int port) const
{
	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getHost() == host
			&& _sockets[i]->getPort() == port)
			return true;
	}
	return false;
}

void Server::checkTimeouts()
{
	std::time_t now = std::time(NULL);

	size_t i = 0;
	while (i < _clients.size())
	{
		Client *client = _clients[i];
		

		if (!client->isTimedOut(now, CLIENT_TIMEOUT))
		{
			++i;
			continue;
		}

		int pollIndex = findPollFd(client->getFd());
		if (pollIndex == -1)
		{
			delete client;
			_clients.erase(_clients.begin() + i);
			continue;
		}

		if (client->shouldClose() || client->hasDataToSend())
		{
			removeClient(static_cast<size_t>(pollIndex));
			continue;
		}

		/* TODO: Generate the configured 408 error page when error pages exist. */
		const std::string response =
			"HTTP/1.1 408 Request Timeout\r\n"
			"Content-Length: 0\r\n"
			"Connection: close\r\n"
			"\r\n";

		client->setResponse(response);
		client->setShouldClose(true);
		client->refreshActivityTimer();
		_pollFds[pollIndex].events &= ~POLLIN;
		_pollFds[pollIndex].events |= POLLOUT;
		++i;
	}
}

void Server::addCgiToPoll(CgiTask *task)
{
	CgiProcess *cgi = task->process;

	struct pollfd pfd;

	if (cgi->getStdinFd() != -1)
	{
		pfd.fd = cgi->getStdinFd();
		pfd.events = POLLOUT;
		pfd.revents = 0;
		_pollFds.push_back(pfd);
	}

	if (cgi->getStdoutFd() != -1)
	{
		pfd.fd = cgi->getStdoutFd();
		pfd.events = POLLIN;
		pfd.revents = 0;
		_pollFds.push_back(pfd);
	}
}

void Server::handleCgiWrite(CgiTask *task)
{
	CgiProcess *cgi = task->process;

	CgiProcess::IOResult result = cgi->writeInput();

	if (result == CgiProcess::IO_ERROR)
	{
		cgi->terminate();
		return;
	}

	if (cgi->getStdinFd() == -1)
	{
		// Remover o descritor antigo do poll.
		// O CGI já recebeu todo o request body.
	}
}

void Server::handleCgiRead(CgiTask *task)
{
	CgiProcess *cgi = task->process;

	CgiProcess::IOResult result = cgi->readOutput();

	if (result == CgiProcess::IO_ERROR ||
		result == CgiProcess::IO_LIMIT_EXCEEDED)
	{
		cgi->terminate();
		return;
	}

	// Se o stdout foi fechado, remover o seu fd do poll.
}

void Server::handleCgiEvent(size_t index)
{
	if (index >= _pollFds.size())
		return;

	int fd = _pollFds[index].fd;
	short events = _pollFds[index].revents;

	CgiTask *task = findCgiByFd(fd);

	if (task == NULL)
		return;

	if (events & POLLNVAL)
	{
		task->process->terminate();
		return;
	}

	if (fd == task->process->getStdinFd() &&
		(events & POLLOUT))
	{
		handleCgiWrite(task);
	}

	if (fd == task->process->getStdoutFd() &&
		(events & (POLLIN | POLLHUP)))
	{
		handleCgiRead(task);
	}

	if (events & POLLERR)
		task->process->terminate();
}

bool Server::isCgiRequest(const Request &request) const
{
	const std::string &target = request.getTarget();
	size_t dotPos = target.rfind('.');
	if (dotPos == std::string::npos)
		return false;

	std::string extension = target.substr(dotPos + 1);
	return extension == "cgi" || extension == "pl" || extension == "py";
}

bool Server::startCgi(Client &client, const Request &request)
{
	const std::string &target = request.getTarget();
	size_t dotPos = target.rfind('.');
	if (dotPos == std::string::npos)
		return false;

	std::string extension = target.substr(dotPos + 1);
	std::string interpreter;
	if (extension == "cgi")
		interpreter = "/usr/bin/perl";
	else if (extension == "pl")
		interpreter = "/usr/bin/perl";
	else if (extension == "py")
		interpreter = "/usr/bin/python3";
	else
		return false;

	CgiProcess *cgi = new CgiProcess(1024 * 1024);

	try
	{
//		cgi->start(
//			interpreter,
//			target,
//			nullptr,
//			request.getBody()
//		);
	}
	catch (...)
	{
		delete cgi;
		throw;
	}

	CgiTask *task = new CgiTask();
	task->clientFd = client.getFd();
	task->process = cgi;

	try
	{
		addCgiToPoll(task);
	}
	catch (...)
	{
		delete task;
		throw;
	}

	try
	{
		_cgiTasks.push_back(task);
	}
	catch (...)
	{
		delete task;
		throw;
	}

	return true;
}

Server::CgiTask *Server::findCgiByFd(int fd)
{
	for (std::vector<CgiTask *>::iterator it = _cgiTasks.begin(); it != _cgiTasks.end(); ++it)
	{
		if ((*it)->process->getStdinFd() == fd || (*it)->process->getStdoutFd() == fd)
			return *it;
	}
	return NULL;
}

// Finds and returns the CgiTask associated with the given client file descriptor, or NULL if not found.
Server::CgiTask *Server::findCgiByClient(int clientFd)
{
	for (std::vector<CgiTask *>::iterator it = _cgiTasks.begin(); it != _cgiTasks.end(); ++it)
	{
		if ((*it)->clientFd == clientFd)
			return *it;
	}
	return NULL;
}