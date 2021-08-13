all:
	gcc -Wall schedule.c -o schedule -lpthread -lm
	
clean:
	rm schedule
