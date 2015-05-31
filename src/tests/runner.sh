g++ -g core_tests.cpp ../utils.cpp ../aes_crypter.cpp -o core_tests -std=c++11 -lssl -lcrypto -Wall
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes --leak-resolution=high ./core_tests
