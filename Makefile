NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
CPPFLAGS = -Iinclude -MMD -MP


RM = rm -rf

SRC_DIR = src
OBJ_DIR = obj

NETWORK_SRCS = \
	$(SRC_DIR)/network/Socket.cpp \
	$(SRC_DIR)/network/Server.cpp \
	$(SRC_DIR)/network/Client.cpp \


# Archivos comunes que necesitan tanto el servidor como los tests
COMMON_SRCS = $(SRC_DIR)/protocol/RequestParser.cpp \
              $(SRC_DIR)/protocol/Request.cpp \
              $(SRC_DIR)/protocol/Response.cpp \
              $(SRC_DIR)/protocol/ResponseBuilder.cpp \
			  $(SRC_DIR)/Utils.cpp

CGI_SRCS = \
	$(SRC_DIR)/CGI/CgiHandler.cpp \
	$(SRC_DIR)/CGI/CgiProcess.cpp \
	$(SRC_DIR)/CGI/CgiManager.cpp

CONFIG_SRCS = \
	$(SRC_DIR)/config/ConfigParser.cpp \
	$(SRC_DIR)/config/ConfigTokenizer.cpp \
	$(SRC_DIR)/config/ConfigValidator.cpp


SRCS = \
	$(SRC_DIR)/main.cpp \
	$(CONFIG_SRCS) \
	$(NETWORK_SRCS) \
	$(COMMON_SRCS) \
	$(CGI_SRCS) \
	
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS) Makefile
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

run: all
	./$(NAME)

debug: CXXFLAGS += -g3
debug: re

.PHONY: all clean fclean re run debug
