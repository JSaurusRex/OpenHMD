/*
 * Rift position tracking
 * Copyright 2014-2015 Philipp Zabel
 * Copyright 2019 Jan Schmidt
 * SPDX-License-Identifier: BSL-1.0
 */

#include <libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "rift-sensor-tracker.h"

#include "rift-sensor-blobwatch.h"
#include "rift-sensor-ar0134.h"
#include "rift-sensor-esp770u.h"
#include "rift-sensor-uvc.h"

#include "rift-sensor-maths.h"
#include "rift-sensor-opencv.h"

#define ASSERT_MSG(_v, ...) if(!(_v)){ fprintf(stderr, __VA_ARGS__); exit(1); }
#define min(a,b) ((a) < (b) ? (a) : (b))

struct rift_sensor_ctx_s
{
  libusb_context *usb_ctx;
	libusb_device_handle *usb_devh;

  rift_led *leds;
	uint8_t num_leds;

  int stream_started;
  struct rift_sensor_uvc_stream stream;
  struct blobwatch* bw;
	struct blobservation* bwobs;

  dmat3 camera_matrix;
  double dist_coeffs[5];
};

void tracker_process_blobs(rift_sensor_ctx *ctx)
{
	struct blobservation* bwobs = ctx->bwobs;

  dmat3 *camera_matrix = &ctx->camera_matrix;
 	double dist_coeffs[5] = { 0, };
  dquat rot = { 0, };
	dvec3 trans = { 0, };

  /*
   * Estimate initial pose without previously known [rot|trans].
   */
  estimate_initial_pose(bwobs->blobs, bwobs->num_blobs, ctx->leds,
            ctx->num_leds, camera_matrix, dist_coeffs, &rot, &trans,
            false);
}

static int
rift_sensor_get_calibration(rift_sensor_ctx *ctx)
{
        uint8_t buf[128];
        double * const A = ctx->camera_matrix.m;
        double * const k = ctx->dist_coeffs;
        double fx, fy, cx, cy;
        double k1, k2, p1, p2;
        int ret;

        /* Read a 128-byte block at EEPROM address 0x1d000 */
        ret = rift_sensor_esp770u_flash_read(ctx->usb_devh, 0x1d000, buf, sizeof buf);
        if (ret < 0)
                return ret;

        fx = fy = *(float *)(buf + 0x30);
        cx = *(float *)(buf + 0x34);
        cy = *(float *)(buf + 0x38);

        k1 = *(float *)(buf + 0x48);
        k2 = *(float *)(buf + 0x4c);
        p1 = *(float *)(buf + 0x50);
        p2 = *(float *)(buf + 0x54);

        printf (" f = [ %7.3f %7.3f ], c = [ %7.3f %7.3f ]\n", fx, fy, cx, cy);
        printf (" k = [ %9.6f %9.6f %9.6f %9.6f ]\n", k1, k2, p1, p2);

        /*
         *     ⎡ fx 0  cx ⎤
         * A = ⎢ 0  fy cy ⎥
         *     ⎣ 0  0  1  ⎦
         */
        A[0] = fx;  A[1] = 0.0; A[2] = cx;
        A[3] = 0.0; A[4] = fy;  A[5] = cy;
        A[6] = 0.0; A[7] = 0.0; A[8] = 1.0;

        /*
         * k = [ k₁ k₂, p₁, p₂, k₃ ]
         */
        k[0] = k1; k[1] = k2; k[2] = p1; k[3] = p2;// k[4] = k3;

        return 0;
}

static void new_frame_cb(struct rift_sensor_uvc_stream *stream)
{
	int width = stream->width;
	int height = stream->height;

	printf(".");
	if(stream->payload_size != width * height) {
		printf("bad frame: %d\n", (int)stream->payload_size);
	}

	rift_sensor_ctx *sensor_ctx = stream->user_data;
	assert (sensor_ctx);

	/* FIXME: Get led pattern phase from sensor reports */
	uint8_t led_pattern_phase = 0;

	blobwatch_process(sensor_ctx->bw, stream->frame, width, height,
		led_pattern_phase, sensor_ctx->leds, sensor_ctx->num_leds,
		&sensor_ctx->bwobs);

	if (sensor_ctx->bwobs)
	{
		if (sensor_ctx->bwobs->num_blobs > 0)
		{
			tracker_process_blobs (sensor_ctx); 
#if 1
			printf("Blobs: %d\n", sensor_ctx->bwobs->num_blobs);

			for (int index = 0; index < sensor_ctx->bwobs->num_blobs; index++)
			{
				printf("Blob[%d]: %d,%d\n",
					index,
					sensor_ctx->bwobs->blobs[index].x,
					sensor_ctx->bwobs->blobs[index].y);
			}
		}
#endif
	}
}

int
rift_sensor_tracker_init (rift_sensor_ctx **ctx,
		const uint8_t radio_id[5], rift_led *leds, uint8_t num_leds)
{
  rift_sensor_ctx *sensor_ctx = NULL;
  int ret;

  sensor_ctx = calloc(1, sizeof (rift_sensor_ctx));
	sensor_ctx->leds = leds;
	sensor_ctx->num_leds = num_leds;

  ret = libusb_init(&sensor_ctx->usb_ctx);
  ASSERT_MSG(ret >= 0, "could not initialize libusb\n");

  /* FIXME: Traverse USB devices with libusb_get_device_list() */
  sensor_ctx->usb_devh = libusb_open_device_with_vid_pid(sensor_ctx->usb_ctx, 0x2833, CV1_PID);
  ASSERT_MSG(sensor_ctx->usb_devh, "could not find or open the Rift sensor camera\n");

  sensor_ctx->stream.frame_cb = new_frame_cb;
  sensor_ctx->stream.user_data = sensor_ctx;

  ret = rift_sensor_uvc_stream_start(sensor_ctx->usb_ctx, sensor_ctx->usb_devh, &sensor_ctx->stream);
  ASSERT_MSG(ret >= 0, "could not start streaming\n");
  sensor_ctx->stream_started = 1;

  sensor_ctx->bw = blobwatch_new(sensor_ctx->stream.width, sensor_ctx->stream.height);

  ret = rift_sensor_ar0134_init(sensor_ctx->usb_devh);
  if (ret < 0)
    goto fail;

  printf ("Found Rift Sensor. Connecting to Radio address 0x%02x%02x%02x%02x%02x\n",
    radio_id[0], radio_id[1], radio_id[2], radio_id[3], radio_id[4]);

  ret = rift_sensor_esp770u_setup_radio(sensor_ctx->usb_devh, radio_id);
  if (ret < 0)
    goto fail;

  ret = rift_sensor_get_calibration(sensor_ctx);
  if (ret < 0) {
    LOGE("Failed to read Rift sensor calibration data");
    return ret;
  }

  *ctx = sensor_ctx;
  return 0;

fail:
  if (sensor_ctx)
    rift_sensor_tracker_free (sensor_ctx);
  return ret;
}

void
rift_sensor_tracker_free (rift_sensor_ctx *sensor_ctx)
{
  if (!sensor_ctx)
    return;

  if (sensor_ctx->stream_started)
    rift_sensor_uvc_stream_stop(&sensor_ctx->stream);

  if (sensor_ctx->usb_devh)
    libusb_close (sensor_ctx->usb_devh);
  if (sensor_ctx->usb_ctx)
    libusb_exit (sensor_ctx->usb_ctx);
  free (sensor_ctx);
}