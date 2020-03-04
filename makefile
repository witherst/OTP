CC=gcc
CFLAGS=-g -std=c99

keygen: keygen.c
	$(CC) $(CFLAGS) -o keygen keygen.c

otp_enc: otp_enc.c
	$(CC) $(CFLAGS) -o otp_enc otp_enc.c

otp_enc_d: otp_enc_d.c
	$(CC) $(CFLAGS) -o otp_enc_d otp_enc_d.c

all: keygen otp_enc otp_enc_d

clean:
	rm -rf *.o keygen otp_enc otp_enc_d
