#pragma once

#include <vector>

#include "network/Socket.hpp"
#include "network/Client.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <poll.h>
#include "config/ConfigParser.hpp"
#include "protocol/RequestParser.hpp"
#include "config/ServerConfig.hpp"
#include "config/LocationConfig.hpp"
#include "protocol/Request.hpp"
#include "CGI/CgiProcess.hpp"

static const int CLIENT_TIMEOUT = 5;

class Server
{
	public:
		struct CgiTask
		{
			int clientFd;
			CgiProcess *process;
		};

	private:
		std::vector<Socket *>		_sockets;
		std::vector<Client *>		_clients;
		std::vector<struct pollfd>	_pollFds;
		std::vector<ServerConfig>	_serverConfigs;
		RequestParser				_requestParser;
		

		std::vector<CgiTask *> _cgiTasks;

		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		bool	addClient(size_t socketIndex);
		void	removeClient(size_t index);

		bool	handlePollEvent(size_t index);
		void	handleServerEvent(size_t index);
		void	handleClientEvent(size_t index);
		void	handleClientRead(size_t index, bool drainOnHangup);
		void	handleClientWrite(size_t index);
		void	handleServerError(size_t index);
		int		findPollFd(int fd) const;

		void	addListeningSocket(Socket *socket);
		bool	isListeningSocket(int fd) const;
		bool	hasListeningSocket(const std::string &host, int port) const;

		Socket	*findListeningSocket(int fd);
		Client	*findClient(int fd);

		ServerConfig	*findServerConfig(int port);
		LocationConfig	*findLocation(ServerConfig &server,
			const std::string &uri);

		void	handleRequest(Client &client);


		CgiTask *findCgiByFd(int fd);
		CgiTask *findCgiByClient(int clientFd);

		void addCgiToPoll(CgiTask *task);
		void removeCgiFromPoll(CgiTask *task);

		void handleCgiEvent(size_t index);
		void handleCgiRead(CgiTask *task);
		void handleCgiWrite(CgiTask *task);

		void finishCgi(CgiTask *task);
		void checkCgiTimeouts();
		bool isCgiRequest(const Request &request) const;
		bool startCgi(Client &client, const Request &request);

	public:
		Server(std::vector<ServerConfig> serverConfigs);
		~Server();

		void	checkTimeouts();
		void	setupSignals();
		void	start();
		void	run();
};