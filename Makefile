NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror #-std=c++98

C = ./Core/
SRCSCORE = $C$(NAME).cpp\
			$CCore.cpp\
			$CServer.cpp\
			$Cmain.cpp\
			$Cutils.cpp\

OBJSCORE = $(SRCSCORE:.cpp=.o)


P = ./Parsing/
SRCSPARSING = $(P)ParseConfig.cpp \
              $(P)ParseServer.cpp \
              $(P)ParseLocation.cpp
OBJSPARSING = $(SRCSPARSING:.cpp=.o)


H = ./HTTPRequest/
SRCSHTTP = $(H)HttpParser.cpp \
			$(H)HttpResponse.cpp\
			$(H)HttpModule.cpp\
			$(H)HttpResponseBuilder.cpp\
			$(H)RequestHandler.cpp\

OBJSHTTP = $(SRCSHTTP:.cpp=.o)

SRCS = $(SRCSHTTP) $(SRCSPARSING) $(SRCSCORE)
OBJS = $(OBJSHTTP) $(OBJSPARSING) $(OBJSCORE)

RM = rm -f

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) -o $(NAME)
	@echo $(NAME)" compiled!\n"

debug: $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) -DD=1 -o $(NAME)
	@echo $(NAME)" compiled!\n"

%.o: %.cpp
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS)

fclean:
	$(RM) $(OBJS) $(NAME)

re:
	@make fclean
	@make all

run:
	@make re
	@make clean
	@echo "Running " $(NAME) "!\n\n"
	./$(NAME)


print-name:
	@echo $(NAME)

