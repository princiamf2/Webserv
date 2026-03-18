NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

SRCS = $(NAME).cpp\
	main.cpp

OBJS = $(SRCS:.cpp=.o)

RM = rm -f

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(OBJS) $(CXXFLAGS) -o $(NAME)
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

