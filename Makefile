NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror #-std=c++98

C = ./Core/
SRCSCORE = $C$(NAME).cpp\
			$Cmain.cpp
OBJSCORE = $(SRCSCORE:.cpp=.o)


P = ./Parsing/
SRCSPARSING = $(P)ParseConfig.cpp
OBJSPARSING = $(SRCSPARSING:.cpp=.o)

RM = rm -f

all: $(NAME)

$(NAME): $(OBJSPARSING) $(OBJSCORE)
	$(CXX) $(OBJSPARSING) $(OBJSCORE) $(CXXFLAGS) -o $(NAME)
	@echo $(NAME)" compiled!\n"

%.o: %.cpp
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJSCORE) $(OBJSPARSING)

fclean:
	$(RM) $(OBJSCORE) $(OBJSPARSING) $(NAME)

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

