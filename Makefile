all: server client

server: mftpserve.c mftp.h
	cc -o server mftpserve.c
client: mftp.c mftp.h
	cc -o client mftp.c
clean:
	rm server client
