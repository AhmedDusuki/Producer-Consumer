.PHONY: all consumer producer clean

#Use GNU compiler
CC = g++

all: consumer producer

consumer:
	$(CC) consumer.cpp -o consumer

producer:
	$(CC) producer.cpp -o producer

clean: 
	rm -f consumer producer
