# just type make to compile all dapps
all: zmigrate oraclefeed

zmigrate:
	$(CC) zmigrate.c -o zmigrate -lm

oraclefeed:
	$(CC) oraclefeed.c -o oraclefeed -lm

clean:
	rm zmigrate oraclefeed
