default:
	gcc -g *.c -lm -lpthread -o os

clean:
	rm -rf os

