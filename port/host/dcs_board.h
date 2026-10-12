/*
 * dcs_board.h - the port's stand-in for CarnEvil's DCS2 sound board (ADSP-2115 + 2 MB DRAM),
 * seen by the game through the I/O ASIC sound port (ioctl device 4). The game's own sound
 * library (src/sound/sound.c) resets it, uploads the DCS operating system and the *.BNK banks
 * word by word, and sends 16-bit sound calls; this module answers exactly as the board would
 * and turns the calls into audio with the vendored DCS decoder (port/third_party/dcsdecoder).
 */
#ifndef PORT_DCS_BOARD_H
#define PORT_DCS_BOARD_H
#ifdef __cplusplus
extern "C" {
#endif
void	 dcs_reset(void);			/* sound control bit 0 pulsed low then high */
void	 dcs_write(unsigned int word);		/* FIOCSETSOUNDDATA: host -> board */
unsigned dcs_status(void);			/* FIOCGETSOUNDSTATUS */
unsigned dcs_read(void);			/* FIOCGETSOUNDDATA: board -> host */
void	 dcs_clear_sth(void);			/* FIOCSETSTHDATA 0: the host took the word */
void	 dcs_render(short *out, int nsamples);	/* mono 16-bit at 31250 Hz, any thread */
void	 dcs_set_verbose(int on);
#ifdef __cplusplus
}
#endif
#endif
