#pragma once

#include <vector>

#include "network/Socket.hpp"
#include "network/Client.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <poll.h>
#include <cerrno>
#include "config/ConfigParser.hpp"

class Client;
class Socket;

static const int CLIENT_TIMEOUT = 20;

class Server
{
	private:
		std::vector<Socket *>		_sockets;
		std::vector<Client *>		_clients;
		std::vector<struct pollfd>	_pollFds;
		std::vector<ServerConfig>   _serverConfigs;
		
		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		void	addClient(size_t socketIndex);
		void	removeClient(size_t index);

		void	handlePollEvent(size_t index);
		void	handleServerEvent(size_t index);
		void	handleClientEvent(size_t index);
		void	handleClientRead(size_t index);
		void	handleClientWrite(size_t index);
		void	handleServerError(size_t index);

		void	addListeningSocket(Socket *socket);
		bool	isListeningSocket(int fd) const;
		bool	hasListeningSocket(const std::string &host, int port) const;
		Socket	*findListeningSocket(int fd);
		Client	*findClient(int fd);
		
		
		ServerConfig *findServerConfig(int port);
		LocationConfig *findLocation(ServerConfig &server,
							 const std::string &uri);
	public:
		Server(std::vector<ServerConfig> serverConfigs);
		~Server();

		void checkTimeouts();

		void setupSignals();
		void start();
		void run();
};