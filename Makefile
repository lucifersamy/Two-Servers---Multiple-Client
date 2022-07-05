hellomake: serverY.c client.c 
	gcc -o serverY serverY.c -lm -Wall -pthread -lrt -I.
	gcc -o client client.c -lm -Wall -pthread -lrt -I.