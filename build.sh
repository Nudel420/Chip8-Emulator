exe_name=chip8
c_file=main.c

clang $c_file -o $exe_name -O1 -Wall -std=c99 -Wno-missing-braces -I include/ -L /lib/ -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -fsanitize=address
echo $exe_name was successfully built
