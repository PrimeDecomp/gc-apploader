#include <ansi_files.h>
#include <stdio.h>
#include <string.h>

size_t fwrite(const void* buffer, size_t size, size_t count, FILE* stream) {
  unsigned char* write_ptr;
  size_t num_bytes, bytes_to_go, bytes_written;
  int ioresult, always_buffer;

  if (fwide(stream, 0) == 0)
    fwide(stream, -1);

  bytes_to_go = size * count;

  if (!bytes_to_go || stream->state.error || stream->mode.file_kind == __closed_file)
    return 0;

  if ((int)stream->mode.file_kind == __console_file)
    __stdio_atexit();

  always_buffer = !stream->mode.binary_io || (int)stream->mode.file_kind == __unavailable_file ||
                  stream->mode.buffer_mode == 2 || stream->mode.buffer_mode == 1;

  if (stream->state.io_state == __neutral) {
    if (stream->mode.io_mode & __reading) {
      stream->state.io_state = __writing;

      __prep_buffer(stream);
    }
  }

  if (stream->state.io_state != __writing) {
    set_error(stream);
    return 0;
  }

  write_ptr = (unsigned char*)buffer;
  bytes_written = 0;

  if (bytes_to_go && (stream->buffer_ptr != stream->buffer || always_buffer)) {
    stream->buffer_len = stream->buffer_size - (stream->buffer_ptr - stream->buffer);

    do {
      unsigned char* newline = NULL;

      num_bytes = stream->buffer_len;

      if (num_bytes > bytes_to_go)
        num_bytes = bytes_to_go;

      if (num_bytes) {
        memcpy(stream->buffer_ptr, write_ptr, num_bytes);

        write_ptr += num_bytes;
        bytes_written += num_bytes;
        bytes_to_go -= num_bytes;

        stream->buffer_ptr += num_bytes;
        stream->buffer_len -= num_bytes;
      }

      if (!stream->buffer_len && (int)stream->mode.file_kind == __unavailable_file) {
        bytes_written += bytes_to_go;
        break;
      }

      if (!stream->buffer_len || newline != NULL || (stream->mode.buffer_mode == 0)) {
        ioresult = __flush_buffer(stream, NULL);

        if (ioresult) {
          set_error(stream);
          bytes_to_go = 0;
          break;
        }
      }
    } while (bytes_to_go && always_buffer);
  }

  if (bytes_to_go && !always_buffer) {
    unsigned char* save_buffer = stream->buffer;
    size_t save_size = stream->buffer_size;

    stream->buffer = write_ptr;
    stream->buffer_size = bytes_to_go;
    stream->buffer_ptr = write_ptr + bytes_to_go;

    if (__flush_buffer(stream, &num_bytes) != 0)
      set_error(stream);

    bytes_written += num_bytes;

    stream->buffer = save_buffer;
    stream->buffer_size = save_size;

    __prep_buffer(stream);

    stream->buffer_len = 0;
  }

  if (stream->mode.buffer_mode != 2)
    stream->buffer_len = 0;

  return ((bytes_written + size - 1) / size);
}
