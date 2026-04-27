CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
TARGET = cyberbank
OBJS = main.o Bank.o Account.o Transaction.o Crypto.o Storage.o
LDLIBS =

# Build with SQLite support: `make SQLITE=1`
ifeq ($(SQLITE),1)
CXXFLAGS += -DCYBERBANK_HAVE_SQLITE
LDLIBS += -lsqlite3
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDLIBS)

main.o: main.cpp Bank.h Storage.h
	$(CXX) $(CXXFLAGS) -c main.cpp

Bank.o: Bank.cpp Bank.h Account.h Storage.h
	$(CXX) $(CXXFLAGS) -c Bank.cpp

Account.o: Account.cpp Account.h Transaction.h Crypto.h
	$(CXX) $(CXXFLAGS) -c Account.cpp

Transaction.o: Transaction.cpp Transaction.h
	$(CXX) $(CXXFLAGS) -c Transaction.cpp

Crypto.o: Crypto.cpp Crypto.h
	$(CXX) $(CXXFLAGS) -c Crypto.cpp

Storage.o: Storage.cpp Storage.h Account.h
	$(CXX) $(CXXFLAGS) -c Storage.cpp

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
