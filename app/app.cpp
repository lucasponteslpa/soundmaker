/*
              APP DO PROJETO DE DRIVER INTERFACE HARDWARE-SOFTWARE
        EDUARDO BARRETO - FERNANDO WANDERLEY - FRANCISCO SOARES - LUCAS PONTES
                            EBB2 - FBWN - FSSN - LPA
                                  VERSÃO 3.0
                                  INTEGRAÇÃO:
                                  SOUNDMAKER
                             GETMIDI(CONTROLADOR)
                                  PERIFÉRICOS
                                  C-Assembly
                                  Interface
                                  Otimização
*/
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <ctype.h>
#include <byteswap.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>
#include <libintl.h>
#include <omp.h>
#include "main.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

extern "C" {void seno(float a, float* b);}
extern "C" {void pulso(float a, float* b);}

App my_app;

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#define _(msgid) gettext (msgid)
#define gettext_noop(msgid) msgid
#define N_(msgid) gettext_noop (msgid)

#define MAX_CHANNELS  16

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPOSE_ID(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define LE_SHORT(v)   (v)
#define LE_INT(v)   (v)
#define BE_SHORT(v)   bswap_16(v)
#define BE_INT(v)   bswap_32(v)
#else /* __BIG_ENDIAN */
#define COMPOSE_ID(a,b,c,d) ((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define LE_SHORT(v)   bswap_16(v)
#define LE_INT(v)   bswap_32(v)
#define BE_SHORT(v)   (v)
#define BE_INT(v)   (v)
#endif

static char              *device      = "default";       /* playback device */
static snd_pcm_format_t   format      = SND_PCM_FORMAT_S16; /* sample format */
static unsigned int       rate        = 48000;              /* stream rate */
static unsigned int       channels    = 1;              /* count of channels */
static unsigned int       limit       = 10000;          //duracao do som, so ajustar para tocar um tamanho qualquer
static unsigned int       speaker     = 0;              /* count of channels */
static unsigned int       buffer_time = 0;              /* ring buffer length in us */
static unsigned int       period_time = 0;              /* period time in us */
static unsigned int       nperiods    = 4;                  /* number of periods */

typedef struct PIO {
	int dado;
	int qsys;
} pio;
static float notas[12] = {523.25, 554.37, 587.33, 622.25, 659.26, 698.46, 739.99, 783.99, 830.61, 880.00, 932.33, 987.77};
int dev;

static snd_pcm_uframes_t  buffer_size;
static snd_pcm_uframes_t  period_size;
static float freq;
static int pulseOrSin = 0;

static const char *const channel_name[MAX_CHANNELS] = {
	/*  4 */ N_("Center"),
	/*  0 */ N_("Front Left"),
	/*  1 */ N_("Front Right"),
	/*  2 */ N_("Rear Left"),
	/*  3 */ N_("Rear Right"),
	/*  5 */ N_("LFE"),
	/*  6 */ N_("Side Left"),
	/*  7 */ N_("Side Right"),
	/*  8 */ N_("Channel 9"),
	/*  9 */ N_("Channel 10"),
	/* 10 */ N_("Channel 11"),
	/* 11 */ N_("Channel 12"),
	/* 12 */ N_("Channel 13"),
	/* 13 */ N_("Channel 14"),
	/* 14 */ N_("Channel 15"),
	/* 15 */ N_("Channel 16")
};

static const int  channels4[] = {
	0, /* Front Left  */
	1, /* Front Right */
	3, /* Rear Right  */
	2, /* Rear Left   */
};
static const int  channels6[] = {
	0, /* Front Left  */
	4, /* Center      */
	1, /* Front Right */
	3, /* Rear Right  */
	2, /* Rear Left   */
	5, /* LFE         */
};
static const int  channels8[] = {
	0, /* Front Left  */
	4, /* Center      */
	1, /* Front Right */
	7, /* Side Right  */
	3, /* Rear Right  */
	2, /* Rear Left   */
	6, /* Side Left   */
	5, /* LFE         */
};
static const int  supported_formats[] = {
	SND_PCM_FORMAT_S8,
	SND_PCM_FORMAT_S16_LE,
	SND_PCM_FORMAT_S16_BE,
	SND_PCM_FORMAT_FLOAT_LE,
	SND_PCM_FORMAT_S32_LE,
	SND_PCM_FORMAT_S32_BE,
	-1
};

float f_amort(float x){
	float y, aux;
	aux = (1 / expf(x));
	x = fmodf(x, 6.28);
	seno(x, &y);
	return (y * aux);
}

float f_quad (float x){
	float y;
	x = fmodf(x, 6.28);
	seno(x, &y);
	if (y > 0){
		return 1;
	}
	else return 0;
}

void press(int );
void unpress(int );
void switch_trd(int );
void unswitch_trd(int );
void octave(int );
void timeFreq(float, float );
void finish();
void send_msg(char *, struct my_msgbuf, int);

static void generate_sine(uint8_t *frames, int channel, int count, double *_phase) {
	double phase = *_phase;
	double max_phase = 1.0 / freq;
	double step = 1.0 /(double)rate;
	double res;
	float fres;
	int    chn;
	int32_t  ires;
	int8_t *samp8 = (int8_t*) frames;
	int16_t *samp16 = (int16_t*) frames;
	int32_t *samp32 = (int32_t*) frames;
	float   *samp_f = (float*) frames;
	float senoValue;
	float grau;

	while (count-- > 0) {
		for(chn=0;chn<channels;chn++) {
			grau = ((phase * 2 * M_PI) / max_phase - M_PI);
			if (!pulseOrSin) seno(grau, &senoValue);
			else if(pulseOrSin == 1) pulso(grau, &senoValue);
			else if(pulseOrSin > 1 && pulseOrSin < 4) senoValue = f_amort(grau);
			else senoValue = f_quad(grau);
			switch (format) {
				case SND_PCM_FORMAT_S8:
					if (chn==channel) {

						res = senoValue * 0x03fffffff; /* Don't use MAX volume */
						ires = res;
						*samp8++ = ires >> 24;
					} else {
						*samp8++ = 0;
					}
					break;
				case SND_PCM_FORMAT_S16_LE:
					if (chn==channel) {
						res = senoValue * 0x03fffffff; /* Don't use MAX volume */
						ires = res;
						*samp16++ = LE_SHORT(ires >> 16);
					} else {
						*samp16++ = 0;
					}
					break;
				case SND_PCM_FORMAT_S16_BE:
					if (chn==channel) {
						res = senoValue * 0x03fffffff; /* Don't use MAX volume */
						ires = res;
						*samp16++ = BE_SHORT(ires >> 16);
					} else {
						*samp16++ = 0;
					}
					break;
				case SND_PCM_FORMAT_FLOAT_LE:
					if (chn==channel) {
						res = senoValue * 0.75 ; /* Don't use MAX volume */
						fres = res;
						*samp_f++ = fres;
					} else {
						*samp_f++ = 0.0;
					}
					break;
				case SND_PCM_FORMAT_S32_LE:
					if (chn==channel) {
						res = senoValue * 0x03fffffff; /* Don't use MAX volume */
						ires = res;
						*samp32++ = LE_INT(ires);
					} else {
						*samp32++ = 0;
					}
					break;
				case SND_PCM_FORMAT_S32_BE:
					if (chn==channel) {
						res = senoValue * 0x03fffffff; /* Don't use MAX volume */
						ires = res;
						*samp32++ = BE_INT(ires);
					} else {
						*samp32++ = 0;
					}
					break;
				default:
					;
			}
		}

		phase += step;
		if (phase >= max_phase)
			phase -= max_phase;
	}

	*_phase = phase;
}

static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access) {
	unsigned int rrate;
	int          err;
	snd_pcm_uframes_t     period_size_min;
	snd_pcm_uframes_t     period_size_max;
	snd_pcm_uframes_t     buffer_size_min;
	snd_pcm_uframes_t     buffer_size_max;

	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		fprintf(stderr, _("Broken configuration for playback: no configurations available: %s\n"), snd_strerror(err));
		return err;
	}

	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, access);
	if (err < 0) {
		fprintf(stderr, _("Access type not available for playback: %s\n"), snd_strerror(err));
		return err;
	}

	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, format);
	if (err < 0) {
		fprintf(stderr, _("Sample format not available for playback: %s\n"), snd_strerror(err));
		return err;
	}

	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, channels);
	if (err < 0) {
		fprintf(stderr, _("Channels count (%i) not available for playbacks: %s\n"), channels, snd_strerror(err));
		return err;
	}

	/* set the stream rate */
	rrate = rate;
	err = snd_pcm_hw_params_set_rate(handle, params, rate, 0);
	if (err < 0) {
		fprintf(stderr, _("Rate %iHz not available for playback: %s\n"), rate, snd_strerror(err));
		return err;
	}

	if (rrate != rate) {
		fprintf(stderr, _("Rate doesn't match (requested %iHz, get %iHz, err %d)\n"), rate, rrate, err);
		return -EINVAL;
	}

	//printf(_("Rate set to %iHz (requested %iHz)\n"), rrate, rate);
	/* set the buffer time */
	err = snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
	err = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
	err = snd_pcm_hw_params_get_period_size_min(params, &period_size_min, NULL);
	err = snd_pcm_hw_params_get_period_size_max(params, &period_size_max, NULL);
	//printf(_("Buffer size range from %lu to %lu\n"),buffer_size_min, buffer_size_max);
	//printf(_("Period size range from %lu to %lu\n"),period_size_min, period_size_max);
	if (period_time > 0) {
		printf(_("Requested period time %u us\n"), period_time);
		err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, NULL);
		if (err < 0) {
			fprintf(stderr, _("Unable to set period time %u us for playback: %s\n"),
					period_time, snd_strerror(err));
			return err;
		}
	}
	if (buffer_time > 0) {
		printf(_("Requested buffer time %u us\n"), buffer_time);
		err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, NULL);
		if (err < 0) {
			fprintf(stderr, _("Unable to set buffer time %u us for playback: %s\n"),
					buffer_time, snd_strerror(err));
			return err;
		}
	}
	if (! buffer_time && ! period_time) {
		buffer_size = buffer_size_max;
		if (! period_time)
			buffer_size = (buffer_size / nperiods) * nperiods;
		//printf(_("Using max buffer size %lu\n"), buffer_size);
		err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
		if (err < 0) {
			fprintf(stderr, _("Unable to set buffer size %lu for playback: %s\n"),
					buffer_size, snd_strerror(err));
			return err;
		}
	}
	if (! buffer_time || ! period_time) {
		//printf(_("Periods = %u\n"), nperiods);
		err = snd_pcm_hw_params_set_periods_near(handle, params, &nperiods, NULL);
		if (err < 0) {
			fprintf(stderr, _("Unable to set nperiods %u for playback: %s\n"),
					nperiods, snd_strerror(err));
			return err;
		}
	}

	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		fprintf(stderr, _("Unable to set hw params for playback: %s\n"), snd_strerror(err));
		return err;
	}

	snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
	snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
	//printf(_("was set period_size = %lu\n"),period_size);
	//printf(_("was set buffer_size = %lu\n"),buffer_size);
	if (2*period_size > buffer_size) {
		fprintf(stderr, _("buffer to small, could not use\n"));
		return -EINVAL;
	}

	return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams) {
	int err;

	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		fprintf(stderr, _("Unable to determine current swparams for playback: %s\n"), snd_strerror(err));
		return err;
	}

	/* start the transfer when a buffer is full */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, buffer_size);
	if (err < 0) {
		fprintf(stderr, _("Unable to set start threshold mode for playback: %s\n"), snd_strerror(err));
		return err;
	}

	/* allow the transfer when at least period_size frames can be processed */
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
	if (err < 0) {
		fprintf(stderr, _("Unable to set avail min for playback: %s\n"), snd_strerror(err));
		return err;
	}

	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		fprintf(stderr, _("Unable to set sw params for playback: %s\n"), snd_strerror(err));
		return err;
	}

	return 0;
}


static int xrun_recovery(snd_pcm_t *handle, int err) {
	if (err == -EPIPE) {  /* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			fprintf(stderr, _("Can't recovery from underrun, prepare failed: %s\n"), snd_strerror(err));
		return 0;
	}
	else if (err == -ESTRPIPE) {

		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1); /* wait until the suspend flag is released */

		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				fprintf(stderr, _("Can't recovery from suspend, prepare failed: %s\n"), snd_strerror(err));
		}

		return 0;
	}

	return err;
}

static int write_buffer(snd_pcm_t *handle, uint8_t *ptr, int cptr)
{
	int err;

	while (cptr > 0) {

		err = snd_pcm_writei(handle, ptr, cptr);

		if (err == -EAGAIN)
			continue;

		if (err < 0) {
			fprintf(stderr, _("Write error: %d,%s\n"), err, snd_strerror(err));
			if (xrun_recovery(handle, err) < 0) {
				fprintf(stderr, _("xrun_recovery failed: %d,%s\n"), err, snd_strerror(err));
				return -1;
			}
			break;  /* skip one period */
		}

		ptr += snd_pcm_frames_to_bytes(handle, err);
		cptr -= err;
	}
	return 0;
}

static int write_loop(snd_pcm_t *handle, int channel, int periods, uint8_t *frames)
{
	double phase = 0;
	int  pattern = 0;
	int    err, n, j, k;
	generate_sine(frames, channel, /*period_size*/limit, &phase);
	if ((err = write_buffer(handle, frames, limit)) < 0)
		return err;
	if (buffer_size > period_size) {
		snd_pcm_drain(handle);
		snd_pcm_prepare(handle);
	}
	return 0;

}

void soundmaker(float f){
	snd_pcm_t            *handle;
	int                   err, morehelp;
	snd_pcm_hw_params_t  *hwparams;
	snd_pcm_sw_params_t  *swparams;
	uint8_t              *frames;
	int                   chn;
	const int        *fmt;
	double    time1,time2,time3;
	unsigned int    n, nloops;
	struct   timeval  tv1,tv2;

	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_sw_params_alloca(&swparams);
	freq = f;
	freq = freq < 30.0 ? 30.0 : freq;
	freq = freq > 5000.0 ? 5000.0 : freq;


	nloops = 1;
	morehelp = 0;

	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		printf(_("Playback open error: %d,%s\n"), err,snd_strerror(err));
    close(dev);
    fflush(stdout);
		exit(EXIT_FAILURE);
	}

	if ((err = set_hwparams(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		printf(_("Setting of hwparams failed: %s\n"), snd_strerror(err));
		snd_pcm_close(handle);
    close(dev);
    fflush(stdout);
		exit(EXIT_FAILURE);
	}
	if ((err = set_swparams(handle, swparams)) < 0) {
		printf(_("Setting of swparams failed: %s\n"), snd_strerror(err));
		snd_pcm_close(handle);
    close(dev);
    fflush(stdout);
		exit(EXIT_FAILURE);
	}
	frames = (uint8_t *) malloc(snd_pcm_frames_to_bytes(handle, period_size));
	if (frames == NULL) {
		fprintf(stderr, _("No enough memory\n"));
    close(dev);
    fflush(stdout);
		exit(EXIT_FAILURE);
	}
	for (n = 0; ! nloops || n < nloops; n++) {

		gettimeofday(&tv1, NULL);
		for(chn = 0; chn < channels; chn++) {
			int channel=chn;
			if (channels == 4) {
				channel=channels4[chn];
			}
			if (channels == 6) {
				channel=channels6[chn];
			}
			if (channels == 8) {
				channel=channels8[chn];
			}

			err = write_loop(handle, channel, ((rate*3)/period_size), frames);

			if (err < 0) {
				fprintf(stderr, _("Transfer failed: %s\n"), snd_strerror(err));
				free(frames);
				snd_pcm_close(handle);
        close(dev);
        fflush(stdout);
				exit(EXIT_SUCCESS);
			}
		}
		gettimeofday(&tv2, NULL);
		time1 = (double)tv1.tv_sec + ((double)tv1.tv_usec / 1000000.0);
		time2 = (double)tv2.tv_sec + ((double)tv2.tv_usec / 1000000.0);
		time3 = time2 - time1;
		//printf(_("Executed for = %.2lf seconds\nFrequency = %.2f hz\n\n"), time3, freq);
    timeFreq(time3, freq);
	}
	free(frames);
	snd_pcm_close(handle);

}


unsigned char hexdigit[] = {0x3F, 0x06, 0x5B, 0x4F,
	0x66, 0x6D, 0x7D, 0x07, 
	0x7F, 0x6F, 0x77, 0x7C,
	0x39, 0x5E, 0x79, 0x71};

void errormessage(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	putc('\n', stderr);
}

void get_midi(){
	int status, boolean=0, nota;
	int mode = SND_RAWMIDI_NONBLOCK;
	snd_rawmidi_t* midiin = NULL;
	const char* portname = "hw:1,0,0";  // see alsarawportlist.c example program
	if ((status = snd_rawmidi_open(&midiin, NULL, portname, mode)) < 0) {
		errormessage("Problem opening MIDI input: %s", snd_strerror(status));
    close(dev);
    fflush(stdout);
		exit(1);
	}

	int maxcount = 1000;   // Exit after this many bytes have been received.
	int count = 0;         // Current count of bytes received.
	char buffer[1];        // Storage for input buffer received
	count = 0;
	int n = 0;
	int i;
	status = 0 ;
	for (i = 0; i < 10000; ++i)
	{
		status = 0;
		count = 0;
		while (status != -EAGAIN) {
			status = snd_rawmidi_read(midiin, buffer, 1);
			if ((status < 0) && (status != -EBUSY) && (status != -EAGAIN)) {
				errormessage("Problem reading MIDI input: %s",snd_strerror(status));
			} else if (status >= 0) {
				count++;
				if ((unsigned char)buffer[0] >= 0x80) {
					if((unsigned char)buffer[0] == 0x80)boolean = 0;
					else boolean = 1;
				} else if(count < 3){
					nota = (unsigned char)buffer[0];
					press(nota % 12);
					soundmaker(notas[nota % 12]);
					unpress(nota % 12);

				}
				fflush(stdout);
			}
		}
		if(boolean){
			press(nota % 12);
			soundmaker(notas[nota % 12]);
			unpress(nota % 12);
		} 
	} 
	snd_rawmidi_close(midiin);
	midiin  = NULL;
}

struct my_msgbuf bufapp;
int msqidapp;

int main() 
{
	int i, j, k;
	int volume = 0;
	pio switches, botoes, display, greenLeds;
	dev = open("/dev/de2i150_altera", O_RDWR);
	char stringVolume[256] = "sh -c \"pactl set-sink-mute 0 false ; pactl set-sink-volume 0 ";
	char volumeChar[10];
	int milhares, centenas, dezenas, unidades;
  int pulseOrSinAntigo = 10;
  int execute = 1;
	float oitavas[9][12] =  {{16.35, 17.32, 18.35, 19.45, 20.60, 21.83, 23.12, 24.50, 25.96, 27.50, 29.14, 30.87},
		{32.70, 34.65, 36.71, 38.89, 41.20, 43.65, 46.25, 49.00, 51.91, 55.00, 58.27, 61.74},
		{65.41, 69.30, 73.42, 77.78, 82.41, 87.31, 92.50, 98.00, 103.83, 110.00, 116.54, 123.47},
		{130.81, 138.59, 146.83, 155.56, 164.81, 174.61, 185.00, 196.00, 207.62, 220.00, 233.08, 246.94},
		{261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88},
		{523.25, 554.37, 587.33, 622.25, 659.26, 698.49, 739.99, 789.99, 830.61, 880.00, 932.33, 987.77},
		{1046.50, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760.00, 1864.66, 1975.53},
		{2093.00, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520.00, 3729.31, 3951.07},
		{4186.01, 4434.92, 4698.64, 4978.03, 5274.04, 5587.62, 5919.91, 6271.93, 6644.88, 7040.00, 7458.62, 7902.13}};

	pid_t pid = fork();

    bufapp.mtype = 1;

    switches.qsys = 1; //Setando para pegar dos switches na função read
    botoes.qsys = 2; //Setando para pegar dos botoes na função read
    botoes.dado = 15;
    greenLeds.qsys = 3; //Setando para acender o led verde
    greenLeds.dado = 1 << 5; //Setando para acender o 6 led verde
    display.qsys = 6; //Setando para escrever nos últimos displays na função write


    write(dev, &greenLeds, sizeof(pio));

	if (pid > 0)
		my_app.run();
	else {

		int msqid;
		key_t key;

		if ((key = ftok("app.cpp", 'B')) == -1) {
			perror("ftok");
      close(dev);
      botoes.dado = 0;
      fflush(stdout);
			exit(1);
		}

		if ((msqidapp = msgget(key, 0644 | IPC_CREAT)) == -1) {
			perror("msgget");
      close(dev);
      botoes.dado = 0;
      fflush(stdout);
			exit(1);
		}
		//O volume começa zerado
		display.dado = hexdigit[0] | (hexdigit[0] << 8) | (hexdigit[0] << 16) | (hexdigit[0] << 24);
		display.dado = ~display.dado;
		write(dev, &display, sizeof(pio)); 
		system("sh -c \"pactl set-sink-mute 0 false ; pactl set-sink-volume 0 0\%\""); //Começando com o volume zerado

#pragma omp parallel num_threads(4) shared(volume, switches, botoes, display, greenLeds, execute) private (j)
		{
			while(execute) 
			{
				strcpy(stringVolume, "sh -c \"pactl set-sink-mute 0 false ; pactl set-sink-volume 0 ");
				switches.qsys = 1; //Setando para pegar dos switches na função read
				botoes.qsys = 2; //Setando para pegar dos botoes na função read
				display.qsys = 6; //Setando para escrever nos últimos displays na função write

        read(dev, &switches, sizeof(pio)); //Le os botoes
        read(dev, &switches, sizeof(pio)); //Garante que pegou os valores certo
        pulseOrSinAntigo = pulseOrSin;
				pulseOrSin = switches.dado & 0xF; 
        if (pulseOrSinAntigo != pulseOrSin)
        {
          switch (pulseOrSin) {
            case 0:
            unswitch_trd(0);
            unswitch_trd(1);
            unswitch_trd(2);
            break;
            case 1:
            unswitch_trd(0);
            unswitch_trd(1);
            switch_trd(2);
            break;
            case 2:
            unswitch_trd(0);
            switch_trd(1);
            unswitch_trd(2);
            break;
            case 3:
            unswitch_trd(0);
            switch_trd(1);
            switch_trd(2);
            break;
            case 4:
            switch_trd(0);
            unswitch_trd(1);
            unswitch_trd(2);
            break;
            case 5:
            switch_trd(0);
            unswitch_trd(1);
            switch_trd(2);
            break;
            case 6:
            switch_trd(0);
            switch_trd(1);
            unswitch_trd(2);
            break;
            case 7:
            switch_trd(0);
            switch_trd(1);
            switch_trd(2);
            break;
          }
        }
        
#pragma omp master
        {
          read(dev, &botoes, sizeof(pio)); //Le os botoes
          read(dev, &botoes, sizeof(pio)); //Garante que pegou os valores certo
          if (botoes.dado == 14 || botoes.dado == 13) 
          {
            if(botoes.dado == 14)  //Último botão pressionado
            {
              volume += 5;
              usleep(85000);
            }
            else if (botoes.dado == 13) //Penultimo botão pressionado
            {
              if (volume >= 5) //O volume nunca vai ser menor que 0
              {
                volume -= 5;
              }
              usleep(85000);
            }
            milhares = volume/1000;
            centenas = (volume%1000)/100;
            dezenas = (volume%100)/10;
            unidades = volume%10;

            display.dado = hexdigit[unidades] | (hexdigit[dezenas] << 8) | (hexdigit[centenas] << 16) | (hexdigit[milhares] << 24);
            display.dado = ~display.dado;

            write(dev, &display, sizeof(pio));

            sprintf (volumeChar, "%d", volume);
            strcat(stringVolume, volumeChar);
            strcat(stringVolume, "\%\"");

            system(stringVolume); //Alterando o volume
          }
        }


#pragma omp single 
        {
          if (botoes.dado == 11)
          {
            greenLeds.dado = greenLeds.dado << 1;
            if (greenLeds.dado > 256)
            {
              greenLeds.dado = 1;
            }
              write(dev, &greenLeds, sizeof(pio));
            for (j = 0; j < 8; j++)
            {
              if (greenLeds.dado == (1 << j))
              {
                octave(j % 4);
                for (k = 0; k < 12; k++)
                {
                  notas[k] = oitavas[j][k];
                }
              }
            }
            usleep(300000);
          }
        }

#pragma omp single 
        {
#pragma omp critical
  				{
  					get_midi();
  				}
        }
       if (botoes.dado <= 7)
       {
          execute = 0;
       }
      }
		}     

		display.dado = hexdigit[0] | (hexdigit[0] << 8) | (hexdigit[0] << 16) | (hexdigit[0] << 24);
		display.dado = ~display.dado;
		write(dev, &display, sizeof(pio));

		display.qsys = 5;
		write(dev, &display, sizeof(pio)); 

		greenLeds.dado = 1 << 5;
		write(dev, &greenLeds, sizeof(pio));
		close(dev);

    if (msgctl(msqidapp,IPC_RMID, NULL) == -1) {
    perror("msgctl");
    close(dev);
    botoes.dado = 0;
    fflush(stdout);
    exit(1);
    }
  finish();

  return 0;
	}
}

void timeFreq(float f1, float f2) {
	char buffer[100];
	sprintf(buffer, "tfr%f_%f", f1, f2);
	send_msg(buffer, bufapp, msqidapp);
}

void octave(int i) {
	char buffer[100];
	sprintf(buffer, "oct%d", i);
	send_msg(buffer, bufapp, msqidapp);
}

void press(int i) {
	char buffer[100];
	sprintf(buffer, "pct%d", i);
	send_msg(buffer, bufapp, msqidapp);

}

void unpress(int i) {
	char buffer[100];
	sprintf(buffer, "uct%d", i);
	send_msg(buffer, bufapp, msqidapp);
}

void switch_trd(int i) {
	char buffer[100];
	sprintf(buffer, "std%d", i);
	send_msg(buffer, bufapp, msqidapp);
}

void unswitch_trd(int i) {
	char buffer[100];
	sprintf(buffer, "utd%d", i);
	send_msg(buffer, bufapp, msqidapp);
}

void finish() {
	char buffer[100];
	sprintf(buffer, "finish");
	send_msg(buffer, bufapp, msqidapp);
}

void send_msg(char *msg, struct my_msgbuf buf, int msqid) {
	strncpy(buf.mtext, msg, sizeof buf.mtext);
	int len = strlen(buf.mtext);

	if (buf.mtext[len-1] == '\n') buf.mtext[len-1] = '\0';

	if (msgsnd(msqid, &buf, len+1, 0) == -1)
		perror("msgsnd"); 
}
