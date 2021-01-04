CXX = g++
CFLAGS = -std=c++14
#-O2 -Wall -g 

TARGET = server
OBJS = util.cpp threadpool.cpp http_conn.cpp http_server.cpp main.cpp

all: $(OBJS) 
	$(CXX) -o $(TARGET) $(OBJS) -lpthread

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)




