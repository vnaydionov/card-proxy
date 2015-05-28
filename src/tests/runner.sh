g++ -g main_test.cpp ../utils.cpp ../aes_crypter.cpp -o main_test -std=c++11 -lssl -lcrypto -Wall
valgrind --leak-check=full --track-fds=yes --show-reachable=yes --leak-resolution=high ./main_test

