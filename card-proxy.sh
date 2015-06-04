export YBORM_URL=sqlite+sqlite:///home/skydreamer/workspace/test_db.sqlite
clear
make
valgrind ./src/card_proxy
