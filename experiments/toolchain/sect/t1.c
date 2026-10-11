extern unsigned char *dma_buffer1; extern int flag; extern void g(void);
register unsigned char *GP asm("$gp");
void f(void) { GP = dma_buffer1; flag = 0; g(); }
