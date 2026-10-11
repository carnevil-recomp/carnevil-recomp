extern unsigned char *dma_buffer1; extern int flag; extern void g(void);
#pragma loadgp
void f(void) { flag = 0; g(); }
#pragma loadgp dma_buffer1
void f2(void) { flag = 0; g(); }
