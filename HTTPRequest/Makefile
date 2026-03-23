NAME     = webserv_test_http
CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
INCLUDES = -I. -I../Parsing

HTTP_SRC = HttpParser.cpp \
           HttpResponse.cpp \
           HttpResponseBuilder.cpp \
           RequestHandler.cpp \
           main_test.cpp

PARSING_SRC = ../Parsing/ParseConfig.cpp \
              ../Parsing/ParseServer.cpp \
              ../Parsing/ParseLocation.cpp

OBJ_DIR = obj

OBJ = $(OBJ_DIR)/HttpParser.o \
      $(OBJ_DIR)/HttpResponse.o \
      $(OBJ_DIR)/HttpResponseBuilder.o \
      $(OBJ_DIR)/RequestHandler.o \
      $(OBJ_DIR)/main_test.o \
      $(OBJ_DIR)/ParseConfig.o \
      $(OBJ_DIR)/ParseServer.o \
      $(OBJ_DIR)/ParseLocation.o

DEPS = $(OBJ:.o=.d)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/HttpParser.o: HttpParser.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR)/HttpResponse.o: HttpResponse.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR)/HttpResponseBuilder.o: HttpResponseBuilder.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR)/RequestHandler.o: RequestHandler.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR)/main_test.o: main_test.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR)/ParseConfig.o: ../Parsing/ParseConfig.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR)/ParseServer.o: ../Parsing/ParseServer.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR)/ParseLocation.o: ../Parsing/ParseLocation.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -f $(OBJ) $(DEPS)
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re

-include $(DEPS)