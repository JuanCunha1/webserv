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
COMMON_SRCS = src/protocol/RequestParser.cpp \
              src/protocol/Request.cpp \
              src/protocol/Response.cpp \
              src/protocol/ResponseBuilder.cpp \
              src/protocol/handlers/GetHandler.cpp \
              src/protocol/handlers/PostHandler.cpp \
              src/protocol/handlers/DeleteHandler.cpp \
              src/protocol/handlers/ErrorHandler.cpp \
			  src/protocol/handlers/CgiHandler.cpp \
              src/protocol/MimeTypes.cpp \
			  src/Utils.cpp
CGI_SRCS = \
	$(SRC_DIR)/CGI/CgiHandler.cpp \
	$(SRC_DIR)/CGI/CgiProcess.cpp

CONFIG_SRCS = \
	$(SRC_DIR)/config/ConfigParser.cpp \
	$(SRC_DIR)/config/ConfigTokenizer.cpp \
	$(SRC_DIR)/config/ConfigValidator.cpp


SRCS = \
	$(SRC_DIR)/main.cpp \
	$(CONFIG_SRCS) \
	$(NETWORK_SRCS) \
	#$(COMMON_SRCS) \
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
