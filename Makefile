all:
	gcc -c -fpic mfs.c -Wall -Werror -o mfs.o
	gcc -Wall -c -fpic udp.c -o udp.o
	gcc -shared -o libmfs.so mfs.o udp.o
	gcc -Wall -c server.c -o server.o
	gcc -o server server.o udp.o 	
	gcc -Wall -c client.c -o client.o
	gcc -lmfs -L. -o client client.c -Wall -Werror
	export LD_LIBRARY_PATH=.

clean:
	rm -f server.o udp.o client.o mfs.o libmfs.so server client mane
