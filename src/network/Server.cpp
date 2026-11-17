#include "network/Server.hpp"
#include <csignal>

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
	:	_sockets(),
		_clients(),
		_pollFds(),
		_serverConfigs()
{
}

Server::Server(std::vector<ServerConfig> serverConfigs)
	:	_sockets(),
		_clients(),
		_pollFds(),
		_serverConfigs(serverConfigs)
{
}

Server::Server(const Server &other)
	: _sockets(other._sockets)
	, _clients(other._clients)
	, _pollFds(other._pollFds)
	, _serverConfigs(other._serverConfigs)
{ }
Server &Server::operator=(const Server &other)
{
	if (this != &other)
	{
		_sockets = other._sockets;
		_clients = other._clients;
		_pollFds = other._pollFds;
		_serverConfigs = other._serverConfigs;
	}
	return *this;
}

Server::~Server()
{
	for (std::vector<Client *>::iterator it = _clients.begin();
		 it != _clients.end(); ++it)
	{
		delete *it;
	}
	for (std::vector<Socket *>::iterator it = _sockets.begin();
		 it != _sockets.end(); ++it)
	{
		delete *it;
	}
	_sockets.clear();
	_pollFds.clear();
}

void Server::start()
{
	for (size_t i = 0; i < _serverConfigs.size(); ++i)
	{
		const ServerConfig &config = _serverConfigs[i];
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
	std::cout << "Server started with "
			  << _sockets.size()
			  << " listening socket(s)"
			  << std::endl;
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
		if (result == 0)
		{
			checkTimeouts();
			continue;
		}
		size_t i = 0;

		while (g_running &&i < _pollFds.size())
		{
			if (_pollFds[i].revents == 0)
			{
				++i;
				continue;
			}
			size_t oldSize = _pollFds.size();

			handlePollEvent(i);

		
			if (_pollFds.size() < oldSize)
				++i;
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
	int fd = _pollFds[index].fd;

	std::cerr << "Error on listening socket fd "
			  << fd << std::endl;

	for (size_t i = 0; i < _sockets.size(); ++i)
	{
		if (_sockets[i]->getFd() == fd)
		{
			delete _sockets[i];
			_sockets.erase(_sockets.begin() + i);
			break;
		}
	}

	_pollFds.erase(_pollFds.begin() + index);
}

void Server::handlePollEvent(size_t index)
{
	if (index >= _pollFds.size())
		return;

	int fd = _pollFds[index].fd;
	short revents = _pollFds[index].revents;

	if (isListeningSocket(fd))
	{
		if (revents & (POLLERR | POLLHUP | POLLNVAL))
		{
			handleServerError(index);
			return;
		}

		if (revents & POLLIN)
			handleServerEvent(index);

		return;
	}

	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		removeClient(index);
		return;
	}

	if (revents & POLLIN)
	{
		handleClientRead(index);
		if (findClient(fd) == NULL)
			return;
	}

	if (revents & POLLOUT)
	{
		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].fd == fd)
			{
				handleClientWrite(i);
				return;
			}
		}
	}
}


void Server::handleServerEvent(size_t index)
{
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

	short revents = _pollFds[index].revents;

	if (revents & (POLLERR | POLLHUP | POLLNVAL))
	{
		std::cout << "Client disconnected: "
				  << _pollFds[index].fd
				  << std::endl;
		removeClient(index);
		return;
	}
	if (revents & POLLIN)
	{
		handleClientRead(index);
		if (index >= _pollFds.size())
			return;
	}
	if (revents & POLLOUT)
		handleClientWrite(index);
}

void Server::handleClientRead(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);

	if (client == NULL)
	{
		removeClient(index);
		return;
	}

	int result = client->receive();

	if (result == 0 || result == -2)
	{
		removeClient(index);
		return;
	}

	if (result == -1)
		return;

	//std::string response = "";

	//response += handlerResult.staticResponse.getHeadersAsString();
	//response += handlerResult.staticResponse.getBody();

	//client->setResponse(response);

	//_pollFds[index].events |= POLLOUT;
}

void Server::handleClientWrite(size_t index)
{
	Client *client = findClient(_pollFds[index].fd);
	if (client == NULL)
	{
		removeClient(index);
		return;
	}
	/*
	std::cout << "[DEBUG HTTP] Intentando enviar " << client->getResponseBuffer().size() 
		  << " bytes al cliente FD " << client->getFd() << std::endl;
	*/
	int result = client->sendData();
	if (result < 0)
	{
		removeClient(index);
		return;
	}
	if (client->hasDataToSend())
	{
		_pollFds[index].events |= POLLOUT;
		return;
	}

	// Toda a resposta foi enviada.
	_pollFds[index].events &= ~POLLOUT;

	if (client->shouldClose())
	{
		removeClient(index);
		return;
	}

	/*
	// Solo volvemos a escuchar si ya vaciamos todo el buffer de salida
	if (client->hasDataToSend()) {
		_pollFds[index].events |= POLLOUT; 
	} else {
		_pollFds[index].events = POLLIN; 
	}
	*/
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

void Server::addClient(size_t socketIndex)
{
	if (socketIndex >= _sockets.size())
		return;
	int clientFd =
		_sockets[socketIndex]->acceptConnection();
	if (clientFd == -1)
		return;
	int flags = fcntl(clientFd, F_GETFL, 0);
	if (flags == -1 ||
		fcntl(clientFd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		close(clientFd);
		throw std::runtime_error(
			"Failed to set client socket non-blocking");
	}
	Client *client = NULL;
	try
	{
		client = new Client(
			clientFd,
			_sockets[socketIndex]->getPort(),
			_serverConfigs[socketIndex].maxBodySize
		);
		_clients.push_back(client);
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
	catch (...)
	{
		if (client == NULL)
			close(clientFd);

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

		if (!client->isTimedOut(now, CLIENT_TIMEOUT))
			continue;

		int fd = client->getFd();

		for (size_t j = 0; j < _pollFds.size(); ++j)
		{
			if (_pollFds[j].fd != fd)
				continue;

			std::cerr << "Client timeout: "
					  << fd
					  << std::endl;
			std::string response = "HTTP/1.1 408 Request Timeout\r\n"
								   "Content-Length: 0\r\n"
								   "Connection: close\r\n"
								   "\r\n";
			/*Response errorRes =
				ResponseBuilder::buildErrorResponse(
					408,
					"Request Timeout"
				);

			std::string response =
				errorRes.getHeadersAsString()
				+ errorRes.getBody();
*/
			client->setResponse(response);
			client->setShouldClose(true);

			_pollFds[j].events |= POLLOUT;

			break;
		}
	}
}
//! No hacen falta pq ya usamos las de Ainhoa
/*
ConfigServer *Server::findServerConfig(int port)
{
	for (size_t i = 0; i < _serverConfigs.size(); ++i)
	{
		if (_serverConfigs[i].port == port)
			return &_serverConfigs[i];
	}

	return NULL;
}

ConfigLocation *Server::findLocation(ConfigServer &server,
									 const std::string &uri)
{
	ConfigLocation *bestMatch = NULL;
	size_t bestLength = 0;

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		const ConfigLocation &location = server.locations[i];

		if (location.path.empty())
			continue;

		bool match = false;

		if (location.path == "/")
		{
			match = true;
		}
		else if (uri == location.path)
		{
			match = true;
		}
		else if (uri.compare(0, location.path.length(),
								location.path) == 0)
		{
			if (location.path[location.path.length() - 1] == '/' ||
				(uri.length() > location.path.length() && uri[location.path.length()] == '/')
			)
			{
				match = true;
			}
		}

		if (match && location.path.length() > bestLength)
		{
			bestMatch = &server.locations[i];
			bestLength = location.path.length();
		}
	}

	return bestMatch;
}
*/

