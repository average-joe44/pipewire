#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER 4096

int download_file(int client_fd, const char *local_file_name, long remote_file_size) {
	FILE *file = fopen(local_file_name, "wb");
	if (file == NULL) {
		printf("failed to make file\n");
		return 0;
	}

	char file_buf[BUFFER];
	int chunk = 0;
	long total_recv = 0;

	while (total_recv < remote_file_size) {
		long remain = remote_file_size - total_recv;
		int to_read = (remain < BUFFER) ? remain : BUFFER;

		chunk = recv(client_fd, file_buf, to_read, 0);
		if (chunk <= 0) {
			printf("connection lost while transfering file\n");
			break;
		}
		fwrite(file_buf, 1, chunk, file);
		total_recv += chunk;
	}
	fclose(file);

	return (total_recv == remote_file_size);
}

int upload_file(int client_fd, const char *local_file_name) {
	FILE* file = fopen(local_file_name, "rb");
	if (file == NULL) {
		printf("file not found\n");
		return 0;
	}
	fseek(file, 0, SEEK_END);
	long filesize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char file_buf[BUFFER];
	int byte_read;
	long total_sent = 0;

	while ((byte_read = fread(file_buf, 1, BUFFER, file)) > 0) {
		int sent = send(client_fd, file_buf, byte_read, 0);
		if (sent <= 0) {
			printf("connection lost while transfering file\n");
			break;
		}
		total_sent += sent;
	}
	fclose(file);

	return (total_sent == filesize);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		return 1;
	}

	int port = atoi(argv[1]);
	if (port <= 0 || port > 65535) {
		printf("Invalid port, only 1-65535\n");
		return 1;
	}

	int server_fd, client_fd;
	struct sockaddr_in addr;
	int opt = 1;
	int addrlen = sizeof(addr);
	char buffer[BUFFER] = {0};

	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
		perror("Setsockopt failed");
		exit(EXIT_FAILURE);
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Bind failed");
		exit(EXIT_FAILURE);
	}

	printf("Listening on port %d\n", port);
	if (listen(server_fd, 3) < 0) {
		perror("Listen failed");
		exit(EXIT_FAILURE);
	}

	if ((client_fd = accept(server_fd, (struct sockaddr *)&addr, (socklen_t*)&addrlen)) < 0) {
		perror("Accept failed");
		exit(EXIT_FAILURE);
	}

	printf("Connection estabilished!\n");

	if (getpeername(client_fd, (struct sockaddr*)&addr, &addrlen) == 0) {
		printf("Connected to %s  | port %d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
	}

	printf("Type your commands below:\n\n");

	fd_set read_fds;
	while (1) {

		FD_ZERO(&read_fds);
		FD_SET(STDIN_FILENO, &read_fds);
		FD_SET(client_fd, &read_fds);

		select(client_fd + 1, &read_fds, NULL, NULL, NULL);

		if (FD_ISSET(client_fd, &read_fds)) {
			int valread = read(client_fd, buffer, BUFFER);
			if (valread == 0) {
				printf("\nConnection closed\n");
				break;
			}
			buffer[valread] = '\0';
			printf("%s", buffer);
			fflush(stdout);
			
		}
		if (FD_ISSET(STDIN_FILENO, &read_fds)) {
			int valread = read(STDIN_FILENO, buffer, BUFFER);
			buffer[valread] = '\0';

			char cmd[20] = {0};
			char filename1[256] = {0};
			char filename2[256] = {0};

			int parsed = sscanf(buffer, "%s %s %s", cmd, filename1, filename2);

			if (strcmp(cmd, "upload") == 0 && strlen(filename2) > 0) {
				printf("uploading\n");

				FILE *file = fopen(filename1, "rb");
				if (file == NULL) {
					printf("file not found\n");
					
					continue;
				}

				fseek(file, 0, SEEK_END);
				long filesize = ftell(file);
				fseek(file, 0, SEEK_SET);

				if (filesize <= 0) {
					printf("Can't chunk file bytes");
					
					continue;
				}

				char remote_cmd[450];
				sprintf(remote_cmd, "head -c %ld > %s\n", filesize, filename2);
				send(client_fd, remote_cmd, strlen(remote_cmd), 0);
				usleep(100000);

				if (upload_file(client_fd, filename1)) {
					printf("uploaded %ld byte\n", filesize);
				} else {
					printf("uploaded %ld byte: file might be corrupted\n", filesize);
				}
				
			}
			else if (strcmp(cmd, "download") == 0 && strlen(filename2) > 0) {

				char check_cmd[350];
				sprintf(check_cmd, "if [ -f \'%s' ]; then stat -c %%s \'%s\'; else echo \'NOT_FOUND\'; fi\n", filename1, filename1);
				send(client_fd, check_cmd, strlen(check_cmd), 0);

				char size_buf[64] = {0};
				long remote_file_size = 0;

				int byte_recv = recv(client_fd, size_buf, sizeof(size_buf) - 1, 0);
				if (byte_recv <= 0) {
					printf("connection lost when checking target file\n");
					
					continue;
				}
				size_buf[byte_recv] = '\0';

				if (strstr(size_buf, "NOT_FOUND") != NULL) {
					printf("file not found\n");
					
					continue;
				}

				remote_file_size = atol(size_buf);
				if (remote_file_size <= 0) {
					printf("file size is not valid (empty byte)\n");
					
					continue;
				}
				printf("downloading %ld byte\n", remote_file_size);

				char pipe[350];
				sprintf(pipe, "cat \"%s\"\n", filename1);
				send(client_fd, pipe, strlen(pipe), 0);
				usleep(100000);

				if (download_file(client_fd, filename2, remote_file_size)) {
					printf("downloaded\n");
				} else {
					printf("downloaded: file might be corrupted\n");
				}
			}
			else {
				send(client_fd, buffer, valread, 0);
				usleep(50000);	
			}
		}
	}
	close(client_fd);
	close(server_fd);
	return 0;
}
