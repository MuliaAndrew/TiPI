#include <linux/io_uring.h>
#include <liburing.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define QUEUE_DEPTH 16
#define _BLOCK_SIZE 4096

void error_exit(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *src = argv[1];
  const char *dst = argv[2];

  struct io_uring ring;
  if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
    error_exit("io_uring_queue_init");
  }

  int src_fd = open(src, O_RDONLY);
  if (src_fd < 0) {
    error_exit("open source file");
  }

  int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (dst_fd < 0) {
    close(src_fd);
    error_exit("open destination file");
  }

  char *buffer = (char*)malloc(_BLOCK_SIZE);
  if (!buffer) {
    close(src_fd);
    close(dst_fd);
    error_exit("malloc");
  }

  off_t offset = 0;
  int ret;
  while (1) {
    // Submit read request
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
      fprintf(stderr, "Failed to get SQE.\n");
      break;
    }

    io_uring_prep_read(sqe, src_fd, buffer, _BLOCK_SIZE, offset);
    io_uring_submit(&ring);

    // Wait for read completion
    struct io_uring_cqe *cqe;
    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
      fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
      break;
    }

    if (cqe->res < 0) {
      fprintf(stderr, "Read failed: %s\n", strerror(-cqe->res));
      break;
    }

    int bytes_read = cqe->res;
    io_uring_cqe_seen(&ring, cqe);

    if (bytes_read == 0) {
      // End of file
      break;
    }

    // Submit write request
    sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
      fprintf(stderr, "Failed to get SQE.\n");
      break;
    }

    io_uring_prep_write(sqe, dst_fd, buffer, bytes_read, offset);
    io_uring_submit(&ring);

    // Wait for write completion
    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
      fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
      break;
    }

    if (cqe->res < 0) {
      fprintf(stderr, "Write failed: %s\n", strerror(-cqe->res));
      break;
    }

    io_uring_cqe_seen(&ring, cqe);

    offset += bytes_read;
  }

  free(buffer);
  close(src_fd);
  close(dst_fd);
  io_uring_queue_exit(&ring);

  return EXIT_SUCCESS;
}