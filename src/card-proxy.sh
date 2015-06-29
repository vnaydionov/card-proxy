clear
make -j4
valgrind --tool=callgrind ./card_proxy
