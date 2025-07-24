#ifndef NETWORK_INIT_H
#define NETWORK_INIT_H

/* Initialize network stack */
int network_init(void);

/* Process network packets - call this in main loop */
void network_process(void);

#endif /* NETWORK_INIT_H */