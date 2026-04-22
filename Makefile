CXX = g++
CXXFLAGS = -O3 -march=native -mtune=native -std=c++20 \
           -flto -fno-exceptions -fno-rtti \
           -fomit-frame-pointer -Iinclude
LDFLAGS = -flto -Wl,-z,now -Wl,-z,relro -pthread

SRC = src/main.cpp
OUT = hft_simulation

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)

.PHONY: all clean
