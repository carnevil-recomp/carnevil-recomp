extern unsigned char *dma_buffer1; extern int flag; extern void g(void);
register int *GP asm("$28");
void f(void) { flag = 0; GP[0] = 1; g(); }
