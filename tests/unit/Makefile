# Unit tests for backend/pocketbook's pure parts. Host compiler only.
#   make -C tests/unit
SRC := ../../backend/pocketbook
OUT := ../../build/unit
CXXFLAGS := -std=c++11 -O1 -g -Wall -Wextra -Werror -I$(SRC)

all: test

SRCS := $(SRC)/gray-map.cpp $(SRC)/layout.cpp $(SRC)/refresh-policy.cpp $(SRC)/keypad.cpp

$(OUT)/test_backend: test_backend.cpp $(SRCS) $(wildcard $(SRC)/*.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -o $@ test_backend.cpp $(SRCS) -lm

test: $(OUT)/test_backend
	$(OUT)/test_backend

.PHONY: all test
