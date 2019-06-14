// Copyright 2013, Fredrik Hultin.
// Copyright 2013, Jakob Bornecrantz.
// SPDX-License-Identifier: BSL-1.0
/*
 * OpenHMD - Free and Open Source API and drivers for immersive technology.
 */

/* Oculus Rift Driver Internal Interface */


#ifndef RIFT_H
#define RIFT_H

#include "../openhmdi.h"

#define FEATURE_BUFFER_SIZE 256

typedef enum {
	RIFT_CMD_SENSOR_CONFIG = 2,
	RIFT_CMD_IMU_CALIBRATION = 3, /* Not used. The HMD does calibration handling */
	RIFT_CMD_RANGE = 4,
	RIFT_CMD_DK1_KEEP_ALIVE = 8,
	RIFT_CMD_DISPLAY_INFO = 9,
	RIFT_CMD_TRACKING_CONFIG = 0xc,
	RIFT_CMD_POSITION_INFO = 0xf,
	RIFT_CMD_PATTERN_INFO = 0x10,
	RIFT_CMD_CV1_KEEP_ALIVE = 0x11,
	RIFT_CMD_RADIO_CONTROL = 0x1a,
	RIFT_CMD_RADIO_DATA = 0x1b,
	RIFT_CMD_ENABLE_COMPONENTS = 0x1d,
} rift_sensor_feature_cmd;

typedef enum {
	RIFT_CF_SENSOR,
	RIFT_CF_HMD
} rift_coordinate_frame;

typedef enum {
	RIFT_IRQ_SENSORS_DK1 = 1,
	RIFT_IRQ_SENSORS_DK2 = 11
} rift_irq_cmd;

typedef enum {
	RIFT_DT_NONE,
	RIFT_DT_SCREEN_ONLY,
	RIFT_DT_DISTORTION
} rift_distortion_type;

typedef enum {
	RIFT_COMPONENT_DISPLAY = 1,
	RIFT_COMPONENT_AUDIO = 2,
	RIFT_COMPONENT_LEDS = 4
} rift_component_type;

// Sensor config flags
#define RIFT_SCF_RAW_MODE           0x01
#define RIFT_SCF_CALIBRATION_TEST   0x02
#define RIFT_SCF_USE_CALIBRATION    0x04
#define RIFT_SCF_AUTO_CALIBRATION   0x08
#define RIFT_SCF_MOTION_KEEP_ALIVE  0x10
#define RIFT_SCF_COMMAND_KEEP_ALIVE 0x20
#define RIFT_SCF_SENSOR_COORDINATES 0x40

typedef enum {
	RIFT_TRACKING_ENABLE        	= 0x01,
	RIFT_TRACKING_AUTO_INCREMENT	= 0x02,
	RIFT_TRACKING_USE_CARRIER    	= 0x04,
	RIFT_TRACKING_SYNC_INPUT    	= 0x08,
	RIFT_TRACKING_VSYNC_LOCK    	= 0x10,
	RIFT_TRACKING_CUSTOM_PATTERN	= 0x20
} rift_tracking_config_flags;

#define RIFT_TRACKING_EXPOSURE_US_DK2           350
#define RIFT_TRACKING_EXPOSURE_US_CV1           399
#define RIFT_TRACKING_PERIOD_US_DK2             16666
#define RIFT_TRACKING_PERIOD_US_CV1             19200
#define RIFT_TRACKING_VSYNC_OFFSET              0
#define RIFT_TRACKING_DUTY_CYCLE                0x7f

typedef struct {
	uint16_t command_id;
	uint16_t accel_scale;
	uint16_t gyro_scale;
	uint16_t mag_scale;
} pkt_sensor_range;

typedef struct {
	int32_t accel[3];
	int32_t gyro[3];
} pkt_tracker_sample;

typedef struct {
	uint8_t num_samples;
	uint16_t total_sample_count;
	int16_t temperature;
	uint32_t timestamp;
	pkt_tracker_sample samples[3];
	int16_t mag[3];

	uint16_t frame_count;        /* HDMI input frame count */
	uint32_t frame_timestamp;    /* HDMI vsync timestamp */
	uint8_t frame_id;            /* frame id pixel readback */
	uint8_t led_pattern_phase;
	uint16_t exposure_count;
	uint32_t exposure_timestamp;
} pkt_tracker_sensor;

typedef struct {
    uint16_t command_id;
    uint8_t flags;
    uint16_t packet_interval;
    uint16_t keep_alive_interval; // in ms
} pkt_sensor_config;

typedef struct {
	uint16_t command_id;
	uint8_t pattern;
	uint8_t flags;
	uint8_t reserved;
	uint16_t exposure_us;
	uint16_t period_us;
	uint16_t vsync_offset;
	uint8_t duty_cycle;
} pkt_tracking_config;

typedef struct {
	uint16_t command_id;
	rift_distortion_type distortion_type;
	uint8_t distortion_type_opts;
	uint16_t h_resolution, v_resolution;
	float h_screen_size, v_screen_size;
	float v_center;
	float lens_separation;
	float eye_to_screen_distance[2];
	float distortion_k[6];
} pkt_sensor_display_info;

typedef struct {
	uint16_t command_id;
	uint16_t keep_alive_interval;
} pkt_keep_alive;

typedef struct {
	uint8_t flags;
	int32_t pos_x;
	int32_t pos_y;
	int32_t pos_z;
	int16_t dir_x;
	int16_t dir_y;
	int16_t dir_z;
	uint8_t index;
	uint8_t num;
	uint8_t type;
} pkt_position_info;

typedef struct {
	uint8_t pattern_length;
	uint32_t pattern;
	uint16_t index;
	uint16_t num;
} pkt_led_pattern_report;

typedef struct {
	// Relative position in micrometers
	vec3f pos;
	// Normal
	vec3f dir;
} rift_led;

bool decode_sensor_range(pkt_sensor_range* range, const unsigned char* buffer, int size);
bool decode_sensor_display_info(pkt_sensor_display_info* info, const unsigned char* buffer, int size);
bool decode_sensor_config(pkt_sensor_config* config, const unsigned char* buffer, int size);
bool decode_tracker_sensor_msg_dk1(pkt_tracker_sensor* msg, const unsigned char* buffer, int size);
bool decode_tracker_sensor_msg_dk2(pkt_tracker_sensor* msg, const unsigned char* buffer, int size);
bool decode_position_info(pkt_position_info* p, const unsigned char* buffer, int size);
bool decode_led_pattern_info(pkt_led_pattern_report * p, const unsigned char* buffer, int size);
bool decode_radio_address(uint8_t radio_address[5], const unsigned char* buffer, int size);

void vec3f_from_rift_vec(const int32_t* smp, vec3f* out_vec);

int encode_tracking_config(unsigned char* buffer, const pkt_tracking_config* tracking);
int encode_sensor_config(unsigned char* buffer, const pkt_sensor_config* config);
int encode_dk1_keep_alive(unsigned char* buffer, const pkt_keep_alive* keep_alive);
int encode_enable_components(unsigned char* buffer, bool display, bool audio, bool leds);
int encode_radio_control_cmd(unsigned char* buffer, uint8_t a, uint8_t b, uint8_t c);

void dump_packet_sensor_range(const pkt_sensor_range* range);
void dump_packet_sensor_config(const pkt_sensor_config* config);
void dump_packet_sensor_display_info(const pkt_sensor_display_info* info);
void dump_packet_tracker_sensor(const pkt_tracker_sensor* sensor);

#endif
