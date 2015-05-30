g++ -g main_test.cpp ../utils.cpp ../aes_crypter.cpp -o card_proxy_tests -std=c++11 -lssl -lcrypto -Wall
valgrind -v --leak-check=full --show-reachable=yes --show-leak-kinds=all --track-fds=yes --leak-resolution=high ./card_proxy_tests 
