/*
 * Copyright (c) 2010 Spotify Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 * OSX AudioQueue output driver.
 *
 * This file is part of the libspotify examples suite.
 */

#include <AudioToolbox/AudioQueue.h>
#include "audio.h"

#define BUFFER_COUNT 3
typedef struct AQPlayerState {
    AudioStreamBasicDescription   desc;
    AudioQueueRef                 queue;
    AudioQueueBufferRef           buffers[BUFFER_COUNT];
    AudioQueueBufferRef           free_buffers[BUFFER_COUNT];
	unsigned free_buffer_count;
    unsigned buffer_size;
	bool playing;
	bool running;
	bool should_stop;
} state_t;


void audio_reset(audio_player_t *player) {
	state_t *state = (state_t *) player->internal_state;
	state->playing = false;
	AudioQueueReset(state->queue);
}

void audio_finish(audio_player_t *player) {
	state_t *state = (state_t *) player->internal_state;
	state->should_stop = true;
}

void audio_pause(audio_player_t *player) {
	state_t *state = (state_t *) player->internal_state;
	AudioQueuePause(state->queue);
	state->playing = false;
	if (player->on_stop) {
		player->on_stop(player);
	}
}

void audio_start(audio_player_t *player) {
	state_t *state = (state_t *) player->internal_state;
	if (noErr != AudioQueueStart(state->queue, NULL)) puts("AudioQueueStart failed");
	state->playing = true;
	if (state->running) {
		if (player->on_start) {
			player->on_start(player);
		}
	}
}

static void handle_audio(audio_player_t *player, audio_fifo_data_t *afd, AudioQueueBufferRef bufout) {
	state_t *state = (state_t *) player->internal_state;
	OSStatus result;
    bufout->mAudioDataByteSize = afd->nsamples * sizeof(short) * afd->channels;

    assert(bufout->mAudioDataByteSize <= state->buffer_size);
    memcpy(bufout->mAudioData, afd->samples, bufout->mAudioDataByteSize);

	AudioQueueRef queue = state->queue;

    result = AudioQueueEnqueueBuffer(queue, bufout, 0, NULL);

    free(afd);
}

static void audio_callback(void *aux, AudioQueueRef aq, AudioQueueBufferRef bufout) {
	audio_player_t *player = (audio_player_t *)aux;
	state_t *state = (state_t *) player->internal_state;
	
	if (state->playing) {
		audio_player_t *player = aux;
		audio_fifo_data_t *afd = audio_get_nowait(&player->af);
		if (afd) {
			handle_audio(player, afd, bufout);
			return;
		}
		else if (state->should_stop) {
			AudioQueueStop(state->queue, false);
			state->should_stop = false;
		}
	}

	state->free_buffers[state->free_buffer_count++] = bufout;
}

static void stopped_callback(void *aux, AudioQueueRef queue, AudioQueuePropertyID property) {
	audio_player_t *player = (audio_player_t *) aux;
	state_t *state = (state_t *) player->internal_state;

	UInt32 is_running;
	UInt32 size = (UInt32) sizeof(is_running);
	AudioQueueGetProperty(queue, kAudioQueueProperty_IsRunning, &is_running, &size);

	state->running = state->playing = is_running ? true : false;

	if (state->playing) {
		if (player->on_start) {
			player->on_start(player);
		}
	}
	else {
		if (player->on_stop) {
			player->on_stop(player);
		}
	}
}

void audio_enqueue(audio_player_t *player, audio_fifo_data_t *afd) {
	state_t *state = (state_t *) player->internal_state;
	state->playing = true;
	if (state->free_buffer_count > 0) {
		handle_audio(player, afd, state->free_buffers[--state->free_buffer_count]);
	}
	else {
		TAILQ_INSERT_TAIL(&player->af.q, afd, link);
		player->af.qlen += afd->nsamples;
	}
}

void audio_init(audio_player_t *player) {
    int i;
    TAILQ_INIT(&player->af.q);
    player->af.qlen = 0;

    pthread_mutex_init(&player->af.mutex, NULL);
    pthread_cond_init(&player->af.cond, NULL);

	player->internal_state = malloc(sizeof(state_t));
	state_t *state = (state_t *) player->internal_state;
    bzero(state, sizeof(state_t));

    state->desc.mFormatID = kAudioFormatLinearPCM;
    state->desc.mFormatFlags = kAudioFormatFlagIsSignedInteger	| kAudioFormatFlagIsPacked;
    state->desc.mSampleRate = 44100;
    state->desc.mChannelsPerFrame = 2;
    state->desc.mFramesPerPacket = 1;
    state->desc.mBytesPerFrame = sizeof (short) * state->desc.mChannelsPerFrame;
    state->desc.mBytesPerPacket = state->desc.mBytesPerFrame;
    state->desc.mBitsPerChannel = (state->desc.mBytesPerFrame*8) / state->desc.mChannelsPerFrame;
    state->desc.mReserved = 0;

    state->buffer_size = state->desc.mBytesPerFrame * state->desc.mSampleRate;

    if (noErr != AudioQueueNewOutput(&state->desc, audio_callback, player, NULL, NULL, 0, &state->queue)) {
		fprintf(stderr, "audioqueue error\n");
		return;
    }

	AudioQueueAddPropertyListener(state->queue, kAudioQueueProperty_IsRunning, stopped_callback, player);

    // Start some empty playback so we'll get the callbacks that fill in the actual audio.
    for (i = 0; i < BUFFER_COUNT; ++i) {
		AudioQueueAllocateBuffer(state->queue, state->buffer_size, &state->buffers[i]);
		state->free_buffers[i] = state->buffers[i];
    }
	state->free_buffer_count = BUFFER_COUNT;
	state->playing = false;
	state->should_stop = false;
}

void audio_get_position(audio_player_t *player, int *position) {
	state_t *state = (state_t *) player->internal_state;
	AudioTimeStamp ts;
	AudioQueueGetCurrentTime(state->queue, NULL, &ts, NULL);
	//printf("mSampleTime: %f\n", ts.mSampleTime);
	if (state->playing) {
		//printf("actual ? time: %f\n", ts.mSampleTime / state->desc.mSampleRate);
	}
//	*position = ts.mSampleTime;
}
