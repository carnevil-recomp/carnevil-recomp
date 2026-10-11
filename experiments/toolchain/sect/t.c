int wr_init = 5;
const int ro_init = 6;
int uninit;
static int s_uninit;
static int s_init = 7;
const char *ptab[] = { "alpha", "beta" };
char *wptab[] = { "gamma", "delta" };
static const char *sptab[] = { "eps" };
const char msg[] = "const array";
char wmsg[] = "writable array";
float fv = 1.5f;
int f(int a) { return wr_init + ro_init + uninit + s_uninit + s_init + a + (int)ptab[a][0] + (int)wptab[a][0] + sptab[0][0] + msg[0] + wmsg[0] + (int)fv; }
