all: png apng

png: load1png load2png load5apng load6apng save1png

apng: load3apng load4apng rgb2gray save2apng

load1png: load1png.c
	gcc -std=c99 -Wall -pedantic -O2 -o load1png load1png.c -lz -lpng 

load2png: load2png.c
	gcc -std=c99 -Wall -pedantic -O2 -o load2png load2png.c -lz -lpng 

load3apng: load3apng.c
	gcc -std=c99 -Wall -pedantic -O2 -o load3apng load3apng.c -lz -lpng 

load4apng: load4apng.c
	gcc -std=c99 -Wall -pedantic -O2 -o load4apng load4apng.c -lz -lpng 

load5apng: load5apng.c
	gcc -std=c99 -Wall -pedantic -O2 -o load5apng load5apng.c -lz -lpng 

load6apng: load6apng.c
	gcc -std=c99 -Wall -pedantic -O2 -o load6apng load6apng.c -lz -lpng 

rgb2gray: rgb2gray.c
	gcc -std=c99 -Wall -pedantic -O2 -o rgb2gray rgb2gray.c -lz -lpng 

save1png: save1png.c
	gcc -std=c99 -Wall -pedantic -O2 -o save1png save1png.c -lz -lpng 

save2apng: save2apng.c
	gcc -std=c99 -Wall -pedantic -O2 -o save2apng save2apng.c -lz -lpng 

clean: 
	rm -f load1png
	rm -f load2png
	rm -f load3apng
	rm -f load4apng
	rm -f load5apng
	rm -f load6apng
	rm -f rgb2gray
	rm -f save1png
	rm -f save2apng

test: 
	./load1png clock4.png
	./load2png clock4.png
	./load3apng clock4.png
	./load4apng clock4.png
	./load5apng clock4.png
	./load6apng clock4.png
	./rgb2gray clock4.png
	./save1png
	./save2apng
