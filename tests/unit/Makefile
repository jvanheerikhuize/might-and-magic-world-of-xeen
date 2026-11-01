# Unit tests for backend/pocketbook's pure parts. Host compiler only.
#   make -C tests/unit
SRC := ../../backend/pocketbook
OUT := ../../build/unit
CXXFLAGS := -std=c++11 -O1 -g -Wall -Wextra -Werror -I$(SRC)

all: test

$(OUT)/test_backend: test_backend.cpp $(SRC)/gray-map.cpp $(SRC)/refresh-policy.cpp $(SRC)/gray-map.h $(SRC)/refresh-policy.h
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -o $@ test_backend.cpp $(SRC)/gray-map.cpp $(SRC)/refresh-policy.cpp -lm

test: $(OUT)/test_backend
	$(OUT)/test_backend

.PHONY: all test
