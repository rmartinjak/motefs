#ifndef SERIAL_H
#define SERIAL_H

int serial_connect_dev(const char *dev, unsigned rate);
int serial_connect_sf(const char *host, unsigned port);

void serial_lock(void);
void serial_unlock(void);

int serial_send(int node, int op, const uint8_t *data, int len);
int serial_receive(int *node, int *op, int *result, uint8_t *data, int len);

#endif
