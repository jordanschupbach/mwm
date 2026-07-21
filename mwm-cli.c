#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static void usage(FILE *stream) {
  fprintf(stream,
          "usage: mwm-cli -m <lua> [-s <socket>]\n"
          "\n"
          "Send a Lua expression or statement to a running mwm instance.\n"
          "\n"
          "options:\n"
          "  -m <lua>       Lua code to execute in mwm\n"
          "  -s <socket>    Override the mwm control socket path\n"
          "  -h, --help     Show this help text\n"
          "\n"
          "examples:\n"
          "  mwm-cli -m 'focus_workspace(\"docs\")'\n"
          "  mwm-cli -m 'focus_workspace(\"mail\")'\n"
          "  # focus_workspace(name) creates the workspace if needed,\n"
          "  # and removes the workspace you leave if it has no clients.\n"
          "  mwm-cli -m 'for i, name in ipairs(mwm.list_workspaces()) do "
          "print(i, name) end'\n"
          "  mwm-cli -m 'print(mwm.current_project())'\n"
          "  # nil unless the focused workspace is a saved project's.\n"
          "  mwm-cli -m 'mwm.switch_project(\"~/code/foo\")'\n"
          "  # like picking it in the project picker: adds it if new,\n"
          "  # switches to its workspace.\n");
}

static void get_socket_path(char *buf, size_t size) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  const char *display_name = getenv("DISPLAY");

  if (runtime && runtime[0] != '\0') {
    snprintf(buf, size, "%s/mwm-%s.sock", runtime,
             display_name && display_name[0] ? display_name : "display");
    return;
  }
  snprintf(buf, size, "/tmp/mwm-%s.sock",
           display_name && display_name[0] ? display_name : "display");
}

int main(int argc, char **argv) {
  const char *message = NULL;
  const char *socket_path = NULL;
  struct sockaddr_un addr;
  char default_socket[sizeof(addr.sun_path)];
  int fd;
  int i;
  int exit_code = 0;
  int saw_output = 0;
  int first_chunk = 1;
  char buffer[4096];
  ssize_t nread;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-m") == 0) {
      if (i + 1 >= argc) {
        usage(stderr);
        return 2;
      }
      message = argv[++i];
    } else if (strcmp(argv[i], "-s") == 0) {
      if (i + 1 >= argc) {
        usage(stderr);
        return 2;
      }
      socket_path = argv[++i];
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      usage(stdout);
      return 0;
    } else {
      usage(stderr);
      return 2;
    }
  }

  if (!message) {
    usage(stderr);
    return 2;
  }
  if (!socket_path) {
    get_socket_path(default_socket, sizeof(default_socket));
    socket_path = default_socket;
  }

  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    perror("connect");
    close(fd);
    return 1;
  }

  if (write(fd, message, strlen(message)) < 0) {
    perror("write");
    close(fd);
    return 1;
  }
  shutdown(fd, SHUT_WR);

  while ((nread = read(fd, buffer, sizeof(buffer))) > 0) {
    if (!saw_output && nread >= 4 && memcmp(buffer, "ERR ", 4) == 0) {
      exit_code = 1;
    }
    saw_output = 1;
    if (!first_chunk && fwrite("", 1, 0, stdout) < 0) {
      break;
    }
    first_chunk = 0;
    if (fwrite(buffer, 1, (size_t)nread, stdout) != (size_t)nread) {
      perror("fwrite");
      close(fd);
      return 1;
    }
  }

  if (nread < 0) {
    perror("read");
    close(fd);
    return 1;
  }

  close(fd);
  return exit_code;
}
