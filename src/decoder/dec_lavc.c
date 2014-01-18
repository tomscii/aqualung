/*                                                     -*- linux-c -*-
    Copyright (C) 2005 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <config.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <libavutil/avutil.h>

#include "../common.h"
#include "../rb.h"
#include "dec_lavc.h"


/* uncomment this to get some debug info */
/* #define LAVC_DEBUG */

extern size_t sample_size;

/* Adapted from avcodec_decode_audio3() implementation found at:
 * https://raw.github.com/FFmpeg/FFmpeg/master/libavcodec/utils.c
 */
int decode_audio3(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr, AVPacket *avpkt) {
	int ret;
#if LIBAVCODEC_VERSION_MAJOR < 53
	ret = avcodec_decode_audio3(avctx, samples, frame_size_ptr, avpkt);
#else /* LIBAVCODEC_VERSION_MAJOR >= 53 */
	AVFrame frame = { { 0 } };
	int got_frame = 0;
	
	ret = avcodec_decode_audio4(avctx, &frame, &got_frame, avpkt);
	if (ret >= 0 && got_frame) {
		int ch, plane_size;
		int planar = av_sample_fmt_is_planar(avctx->sample_fmt);
		int data_size = av_samples_get_buffer_size(&plane_size, avctx->channels,
							   frame.nb_samples, avctx->sample_fmt, 1);
		if (*frame_size_ptr < data_size) {
			av_log(avctx, AV_LOG_ERROR, "output buffer size is too small for "
			       "the current frame (%d < %d)\n", *frame_size_ptr, data_size);
			return AVERROR(EINVAL);
		}
		memcpy(samples, frame.extended_data[0], plane_size);
		
		if (planar && avctx->channels > 1) {
			uint8_t *out = ((uint8_t *)samples) + plane_size;
			for (ch = 1; ch < avctx->channels; ch++) {
				memcpy(out, frame.extended_data[ch], plane_size);
				out += plane_size;
			}
		}
		*frame_size_ptr = data_size;
	} else {
		*frame_size_ptr = 0;
	}
#endif /* LIBAVCODEC_VERSION_MAJOR >= 53 */
	return ret;
}

/* return 1 if reached end of stream, 0 else */
int
decode_lavc(decoder_t * dec) {

        lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;
        file_decoder_t * fdec = dec->fdec;

	AVPacket packet;
        int16_t samples[MAX_AUDIO_FRAME_SIZE];
        float fsamples[MAX_AUDIO_FRAME_SIZE];
        int n_bytes = MAX_AUDIO_FRAME_SIZE;

	if (av_read_frame(pd->avFormatCtx, &packet) < 0)
		return 1;

	if (packet.stream_index == pd->audioStream) {

		decode_audio3(pd->avCodecCtx, samples, &n_bytes, &packet);
		if (n_bytes > 0) {
			int i;
			for (i = 0; i < n_bytes/2; i++) {
				fsamples[i] = samples[i] * fdec->voladj_lin / 32768.f;
			}
			rb_write(pd->rb, (char *)fsamples, n_bytes/2 * sample_size);
		}
	}
	av_free_packet(&packet);
        return 0;
}


decoder_t *
lavc_decoder_init(file_decoder_t * fdec) {

        decoder_t * dec = NULL;

        if ((dec = calloc(1, sizeof(decoder_t))) == NULL) {
                fprintf(stderr, "dec_lavc.c: lavc_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->fdec = fdec;

        if ((dec->pdata = calloc(1, sizeof(lavc_pdata_t))) == NULL) {
                fprintf(stderr, "dec_lavc.c: lavc_decoder_new() failed: calloc error\n");
                return NULL;
        }

	dec->init = lavc_decoder_init;
	dec->destroy = lavc_decoder_destroy;
	dec->open = lavc_decoder_open;
	dec->send_metadata = lavc_decoder_send_metadata;
	dec->close = lavc_decoder_close;
	dec->read = lavc_decoder_read;
	dec->seek = lavc_decoder_seek;

	return dec;
}


void
lavc_decoder_destroy(decoder_t * dec) {

	free(dec->pdata);
	free(dec);
}


int
lavc_decoder_open(decoder_t * dec, char * filename) {

	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	int i;

#if LIBAVFORMAT_VERSION_MAJOR < 53
	if (av_open_input_file(&pd->avFormatCtx, filename, NULL, 0, NULL) != 0)
#else /* LIBAVFORMAT_VERSION_MAJOR >= 53 */
	if (avformat_open_input(&pd->avFormatCtx, filename, NULL, NULL) != 0)
#endif /* LIBAVFORMAT_VERSION_MAJOR >= 53 */
	{
		return DECODER_OPEN_BADLIB;
	}

#if LIBAVFORMAT_VERSION_MAJOR < 53
	if (av_find_stream_info(pd->avFormatCtx) < 0)
#else /* LIBAVFORMAT_VERSION_MAJOR >= 53 */
	if (avformat_find_stream_info(pd->avFormatCtx, NULL) < 0)
#endif /* LIBAVFORMAT_VERSION_MAJOR >= 53 */
	{
		return DECODER_OPEN_BADLIB;
	}

	/* debug */
#ifdef LAVC_DEBUG
	dump_format(pd->avFormatCtx, 0, filename, 0);
#endif /* LAVC_DEBUG */

	pd->audioStream = -1;
	for (i = 0; i < pd->avFormatCtx->nb_streams; i++) {
		if (pd->avFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			pd->audioStream = i;
			break;
		}
	}
	if (pd->audioStream == -1)
		return DECODER_OPEN_BADLIB;

	pd->avCodecCtx = pd->avFormatCtx->streams[pd->audioStream]->codec;
#if LIBAVCODEC_VERSION_MAJOR >= 53
	pd->avCodecCtx->get_buffer = avcodec_default_get_buffer;
	pd->avCodecCtx->release_buffer = avcodec_default_release_buffer;
#endif /* LIBAVCODEC_VERSION_MAJOR >= 53 */

	pd->time_base = pd->avFormatCtx->streams[pd->audioStream]->time_base;

	pd->avCodec = avcodec_find_decoder(pd->avCodecCtx->codec_id);
	if (pd->avCodec == NULL)
		return DECODER_OPEN_BADLIB;

#if LIBAVCODEC_VERSION_MAJOR < 53
	if (avcodec_open(pd->avCodecCtx, pd->avCodec) < 0)
#else /* LIBAVCODEC_VERSION_MAJOR >= 53 */
	if (avcodec_open2(pd->avCodecCtx, pd->avCodec, NULL) < 0)
#endif /* LIBAVCODEC_VERSION_MAJOR >= 53 */
	{
		return DECODER_OPEN_BADLIB;
	}

	if ((pd->avCodecCtx->channels != 1) && (pd->avCodecCtx->channels != 2)) {
		fprintf(stderr,
			"lavc_decoder_open: audio stream with %d channels is unsupported\n",
			pd->avCodecCtx->channels);
		return DECODER_OPEN_FERROR;
	}

        pd->is_eos = 0;
        pd->rb = rb_create(pd->avCodecCtx->channels * sample_size * RB_LAVC_SIZE);

	fdec->fileinfo.channels = pd->avCodecCtx->channels;
	fdec->fileinfo.sample_rate = pd->avCodecCtx->sample_rate;
	fdec->fileinfo.bps = pd->avCodecCtx->bit_rate;
	if (pd->avFormatCtx->duration > 0) {
		fdec->fileinfo.total_samples = pd->avFormatCtx->duration
			/ 1000000.0 * pd->avCodecCtx->sample_rate;
	} else {
		fdec->fileinfo.total_samples = 0;
	}

	fdec->file_lib = LAVC_LIB;
	snprintf(dec->format_str, MAXLEN-1, "%s/%s", pd->avFormatCtx->iformat->name, pd->avCodec->name);
	for (i = 0; dec->format_str[i] != '\0'; i++) {
		dec->format_str[i] = toupper(dec->format_str[i]);
	}
	
	return DECODER_OPEN_SUCCESS;
}


void
lavc_decoder_send_metadata(decoder_t * dec) {
}


void
lavc_decoder_close(decoder_t * dec) {

	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;

	avcodec_close(pd->avCodecCtx);

#if LIBAVFORMAT_VERSION_MAJOR < 53
	av_close_input_file(pd->avFormatCtx);
#else /* LIBAVFORMAT_VERSION_MAJOR >= 53 */
	avformat_close_input(&pd->avFormatCtx);
#endif /* LIBAVFORMAT_VERSION_MAJOR >= 53 */

	rb_free(pd->rb);
}


unsigned int
lavc_decoder_read(decoder_t * dec, float * dest, int num) {

	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;

        unsigned int numread = 0;
        unsigned int n_avail = 0;

        while ((rb_read_space(pd->rb) <
                num * pd->avCodecCtx->channels * sample_size) && (!pd->is_eos)) {

                pd->is_eos = decode_lavc(dec);
        }

        n_avail = rb_read_space(pd->rb) / (pd->avCodecCtx->channels * sample_size);

        if (n_avail > num)
                n_avail = num;

        rb_read(pd->rb, (char *)dest, n_avail *	pd->avCodecCtx->channels * sample_size);
        numread = n_avail;
        return numread;
}


void
lavc_decoder_seek(decoder_t * dec, unsigned long long seek_to_pos) {
	
	lavc_pdata_t * pd = (lavc_pdata_t *)dec->pdata;
	file_decoder_t * fdec = dec->fdec;
	char flush_dest;

	long long pos = fdec->fileinfo.total_samples - fdec->samples_left;
	int64_t timestamp = (double)seek_to_pos / fdec->fileinfo.sample_rate
		* pd->time_base.den / pd->time_base.num;
	int flags = (pos > seek_to_pos) ? AVSEEK_FLAG_BACKWARD : 0;

	if (av_seek_frame(pd->avFormatCtx, pd->audioStream, timestamp, flags) >= 0) {
		fdec->samples_left = fdec->fileinfo.total_samples - seek_to_pos;
		/* empty lavc decoder ringbuffer */
		while (rb_read_space(pd->rb))
			rb_read(pd->rb, &flush_dest, sizeof(char));
	} else {
		fprintf(stderr, "lavc_decoder_seek: warning: av_seek_frame() failed\n");
	}
}



// vim: shiftwidth=8:tabstop=8:softtabstop=8 :  

