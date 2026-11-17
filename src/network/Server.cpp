#include "network/Server.hpp"
#include <csignal>
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
			/*
			** Process the current poll set. If an entry is removed, the next
			** entry shifts into its index, so do not increment in that case.
			** Newly accepted clients have revents == 0 and are not processed
			** until the next poll() call.
			*/
			size_t i = 0;
			while (g_running && i < _pollFds.size())
			{
				if (_pollFds[i].revents == 0)
				{
					++i;
					continue;
				}

				bool removed = handlePollEvent(i);
				if (!removed)
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
			addClient(i);
			return;
		}
	}
}

void Server::handleClientEvent(size_t index)
{
	if (index >= _pollFds.size())
		return;

	int fd = _pollFds[index].fd;
	short revents = _pollFds[index].revents;

	if (revents & POLLNVAL)
	{
		removeClient(index);
		return;
	}

	/*
	** POLLHUP can arrive together with POLLIN while unread request bytes
	** are still available. Read first; do not discard those bytes merely
	** because the peer has closed its sending side.
	*/
	if (revents & (POLLIN | POLLHUP))
	{
		int currentIndex = findPollFd(fd);
		if (currentIndex != -1)
			handleClientRead(static_cast<size_t>(currentIndex));
	}

	int currentIndex = findPollFd(fd);
	if (currentIndex == -1)
		return;

	if (revents & POLLOUT)
		handleClientWrite(static_cast<size_t>(currentIndex));

	currentIndex = findPollFd(fd);
	if (currentIndex == -1)
		return;

	/* POLLERR is treated as a fatal socket condition. */
	if (revents & POLLERR)
	{
		std::cerr << "Client socket error: " << fd << std::endl;
		removeClient(static_cast<size_t>(currentIndex));
	}
}

void Server::handleClientRead(size_t index)
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

	Client::IOResult result = client->receive();
	if (result == Client::IO_ERROR)
	{
		removeClient(index);
		return;
	}

	/*
	** TODO: Parse client->getRequest() here.
	** - Detect the end of the HTTP headers and request body.
	** - Enforce client body-size limits using _maxBodySize.
	** - Select the matching server and location configuration.
	** - Build the HTTP response and call client->setResponse(...).
	** - Set shouldClose when the request requires Connection: close.
	*/

	if (client->hasDataToSend())
		_pollFds[index].events |= POLLOUT;

	if (result == Client::IO_CLOSED)
	{
		client->setReadClosed(true);
		_pollFds[index].events &= ~POLLIN;

		/*
		** The parser above must get a chance to process any bytes already
		** received before this close decision is made.
		*/
		if (!client->hasDataToSend())
		{
			removeClient(index);
			return;
		}
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
	if (result != Client::IO_SUCCESS)
	{
		removeClient(index);
		return;
	}

	if (client->hasDataToSend())
	{
		_pollFds[index].events |= POLLOUT;
		return;
	}

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
			delete *it; // Client destructor closes the client FD.
			_clients.erase(it);
			break;
		}
	}

	_pollFds.erase(_pollFds.begin() + index);
}

void Server::addClient(size_t socketIndex)
{
	if (socketIndex >= _sockets.size())
		return;

	Socket *listeningSocket = _sockets[socketIndex];
	int clientFd = listeningSocket->acceptConnection();
	if (clientFd == -1)
		return;

	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1)
	{
		close(clientFd);
		throw std::runtime_error(
			"Failed to set client socket non-blocking"
		);
	}

	/*
	** Do not use socketIndex to index _serverConfigs: duplicate listeners
	** are skipped in start(), so the two vector indices are not guaranteed
	** to correspond. Find a configuration matching this listener instead.
	*/
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
		close(clientFd);
		throw std::runtime_error(
			"No server configuration found for listening socket"
		);
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

	for (size_t i = 0; i < _clients.size(); ++i)
	{
		Client *client = _clients[i];
		if (client->shouldClose())
			continue;

		if (!client->isTimedOut(now, CLIENT_TIMEOUT))
			continue;

		int fd = client->getFd();
		int pollIndex = findPollFd(fd);
		if (pollIndex == -1)
			continue;

		if (client->hasDataToSend())
		{
			/* Preserve a response already being sent; close after it drains. */
			client->setShouldClose(true);
			_pollFds[pollIndex].events |= POLLOUT;
			continue;
		}

		/* TODO: Generate the configured 408 error page when error pages exist. */
		std::string response =
			"HTTP/1.1 408 Request Timeout\r\n"
			"Content-Length: 0\r\n"
			"Connection: close\r\n"
			"\r\n";

		client->setResponse(response);
		client->setShouldClose(true);
		_pollFds[pollIndex].events |= POLLOUT;
	}
}