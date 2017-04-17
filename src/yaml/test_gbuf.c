#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../build/src/config.h"
#include "../gldns/gbuffer.h"

int
main(int argc, char *argv[])
{

  gldns_buffer *buf;
  char *json;

  buf = gldns_buffer_new(32); //does not initialize contains to 0
  if (!buf) {
    fprintf(stderr, "Could not assign buffer for json output");
    return -1;
  }

  fprintf(stderr, "buf contents:\n");
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 16; j++) {
      buf->_data[i*16 + j] = 30 + i*16 + j;
      fprintf(stderr, "0x%02x ", buf->_data[i*16 + j]);
    }
      fprintf(stderr, "\n");
  }
  fprintf(stderr, "\n");
  fprintf(stderr, "buf->data[buf->_position] is 0x%02x\n", buf->_data[buf->_position]);
  fprintf(stderr, "buf->_position is %d\n", buf->_position);
  fprintf(stderr, "buf->_limit is    %d\n", buf->_limit);
  fprintf(stderr, "buf->_capacity is %d\n", buf->_capacity);
  fprintf(stderr, "buf->_fixed is    %d\n", buf->_fixed);
  fprintf(stderr, "buf->_vfixed is   %d\n", buf->_vfixed);
  fprintf(stderr, "buf->_status_err  %d\n", buf->_status_err);
  fprintf(stderr, "\n");

  char* data = "1234123412341234";
  gldns_buffer_write(buf, data, strlen(data)); //copies the bytes excluding the null byte at the end
  fprintf(stderr, "buf is %s\n", (char*)buf->_data);
  fprintf(stderr, "buf->data[buf->_position] is 0x%02x\n", buf->_data[buf->_position]);
  fprintf(stderr, "buf->_position is %d\n", buf->_position);
  fprintf(stderr, "buf->_limit is    %d\n", buf->_limit);
  fprintf(stderr, "buf->_capacity is %d\n", buf->_capacity);
  fprintf(stderr, "buf->_fixed is    %d\n", buf->_fixed);
  fprintf(stderr, "buf->_vfixed is   %d\n", buf->_vfixed);
  fprintf(stderr, "buf->_status_err  %d\n", buf->_status_err);
  fprintf(stderr, "\n");

  fprintf(stderr, "buf remaining space is %d\n", gldns_buffer_capacity(buf)-gldns_buffer_position(buf));
  fprintf(stderr, "buf remaining space is %d\n", gldns_buffer_remaining(buf));

  gldns_buffer_write_string(buf, "5678567856785678"); //copy excludes the null byte at the end - 
  fprintf(stderr, "buf is %s\n", (char*)buf->_data);  //only works if the byte beyond end of buf is 0
  fprintf(stderr, "buf->data[buf->_position-1] is 0x%02x\n", buf->_data[buf->_position-1]);//beyond end of buf
  fprintf(stderr, "buf->_position is %d\n", buf->_position);
  fprintf(stderr, "buf->_limit is    %d\n", buf->_limit);
  fprintf(stderr, "buf->_capacity is %d\n", buf->_capacity);
  fprintf(stderr, "buf->_fixed is    %d\n", buf->_fixed);
  fprintf(stderr, "buf->_vfixed is   %d\n", buf->_vfixed);
  fprintf(stderr, "buf->_status_err  %d\n", buf->_status_err);
  fprintf(stderr, "\n");

  json = (char *) gldns_buffer_export(buf);
  gldns_buffer_free(buf);

  fprintf(stderr, "json is %s\n", json);
  free(json);

  return 0;
}
