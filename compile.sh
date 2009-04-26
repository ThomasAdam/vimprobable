gcc -o browser $(pkg-config --cflags --libs gtk+-2.0 webkit-1.0) -Wall -W main.c
