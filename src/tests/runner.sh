g++ -g main_test.cpp ../utils.cpp ../aes_crypter.cpp -o main_test -std=c++11 -lssl -lcrypto -Wall
valgrind -v --leak-check=full --show-reachable=yes --show-leak-kinds=all --track-fds=yes --leak-resolution=high ./main_test 
