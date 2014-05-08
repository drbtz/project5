all:
	gcc -c -fpic mfs.c -Wall -Werror -o mfs.o
	gcc -c -fpic udp.c -Wall -Werror -o udp.o
	gcc -shared -o libmfs.so mfs.o udp.o
	gcc -Wall -c server.c -o server.o
	gcc -o server server.o udp.o 	
	gcc -Wall -c client.c -o client.o
	gcc -lmfs -L. -o client client.c -Wall -Werror

clean:
	rm -f server.o udp.o client.o mfs.o libmfs.so server client mane
