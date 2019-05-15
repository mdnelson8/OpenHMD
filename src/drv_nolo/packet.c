// Copyright 2017, Joey Ferwerda.
// SPDX-License-Identifier: BSL-1.0
/*
 * OpenHMD - Free and Open Source API and drivers for immersive technology.
 * Original implementation by: Yann Vernier.
 */

/* NOLO VR - Packet Decoding and Utilities */


#include <stdio.h>
#include "nolo.h"
#include <inttypes.h>

inline static uint8_t read8(const unsigned char** buffer)
{
	uint8_t ret = **buffer;
	*buffer += 1;
	return ret;
}

inline static int16_t read16(const unsigned char** buffer)
{
	int16_t ret = **buffer | (*(*buffer + 1) << 8);
	//printf("converting data: %" PRIu16 "\n", ret);
	*buffer += 2;
	return ret;
}

inline static uint32_t read32(const unsigned char** buffer)
{
	uint32_t ret = **buffer | (*(*buffer + 1) << 8) | (*(*buffer + 2) << 16) | (*(*buffer + 3) << 24);
	*buffer += 4;
	return ret;
}

#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

#define CRYPT_WORDS (64-4)/4
#define CRYPT_OFFSET 1

void btea_decrypt(uint32_t *v, int n, int base_rounds, uint32_t const key[4])
{
	uint32_t y, z, sum;
	unsigned p, rounds, e;

	/* Decoding Part */
	rounds = base_rounds + 52/n;
	sum = rounds*DELTA;
	y = v[0];

	do {
		e = (sum >> 2) & 3;
		for (p=n-1; p>0; p--) {
			z = v[p-1];
			y = v[p] -= MX;
		}

		z = v[n-1];
		y = v[0] -= MX;
		sum -= DELTA;
	} while (--rounds);
}

void nolo_decrypt_data(unsigned char* buf)
{
	static const uint32_t key[4] = {0x875bcc51, 0xa7637a66, 0x50960967, 0xf8536c51};
	uint32_t cryptpart[CRYPT_WORDS];

	// Decrypt encrypted portion
	for (int i = 0; i < CRYPT_WORDS; i++) {
	cryptpart[i] =
		((uint32_t)buf[CRYPT_OFFSET+4*i  ]) << 0  |
		((uint32_t)buf[CRYPT_OFFSET+4*i+1]) << 8  |
		((uint32_t)buf[CRYPT_OFFSET+4*i+2]) << 16 |
		((uint32_t)buf[CRYPT_OFFSET+4*i+3]) << 24;
	}

	btea_decrypt(cryptpart, CRYPT_WORDS, 1, key);

	for (int i = 0; i < CRYPT_WORDS; i++) {
		buf[CRYPT_OFFSET+4*i  ] = cryptpart[i] >> 0;
		buf[CRYPT_OFFSET+4*i+1] = cryptpart[i] >> 8;
		buf[CRYPT_OFFSET+4*i+2] = cryptpart[i] >> 16;
		buf[CRYPT_OFFSET+4*i+3] = cryptpart[i] >> 24;
	}
}

void nolo_decode_position(const unsigned char* data, vec3f* pos)
{
	const double scale = 0.0001f;
	//const double scale = 1;

	//pos->x = (scale * (int16_t)(data[0]<<8 | data[1]));
	//pos->y = (scale * (int16_t)(data[2]<<8 | data[3]));
	//pos->z = (scale *          (data[4]<<8 | data[5]));

	pos->x = scale*read16(&data);
	pos->y = scale*read16(&data);
	pos->z = scale*read16(&data);
}

void nolo_decode_orientation(const unsigned char* data, nolo_sample* smp)
{		
	// acceleration
	for(int i = 0; i < 3; i++){
		smp->accel[i] = read16(&data);
	}

	data += 6;

	// gyro
	for(int i = 0; i < 3; i++){
		smp->gyro[i] = read16(&data);
	}
}

void nolo_decode_controller(drv_priv* priv, const unsigned char* data)
{
	uint8_t bit, buttonstate;

	if (data[0] != 2 || data[1] != 1) {
	// Unknown version
	/* Happens when controllers aren't on.
	std::cout << "Nolo: Unknown controller "
	  << " version " << (int)data[0] << " " << (int)data[1]
	  << std::endl;
	*/
		return;
	}

	vec3f position;
	quatf orientation;

	//nolo_decode_position(data+3, &position);
	//nolo_decode_orientation(data+3+3*2, &orientation);

	//Change button state
	buttonstate = data[3+3*2+4*2];
	for (bit=0; bit<6; bit++)
		priv->controller_values[bit] = (buttonstate & 1<<bit ? 1 : 0);

	priv->controller_values[6] = data[3+3*2+4*2+2]; //X Pad
	priv->controller_values[7] = data[3+3*2+4*2+2+1]; //Y Pad

	priv->base.position = position;
	priv->base.rotation = orientation;
}

void nolo_decode_hmd_marker(drv_priv* priv, const unsigned char* data)
{
	//if (data[0] != 2 || data[1] != 1) {
		/* Happens with corrupt packages (mixed with controller data)
		std::cout << "Nolo: Unknown headset marker"
		  << " version " << (int)data[0] << " " << (int)data[1]
		  << std::endl;
		*/
		// Unknown version
	//	return;
	//}

	vec3f homepos;
	vec3f position;
	quatf orientation;// = {.0, .0, .0, 1};
	nolo_sample smp;
	data += 25; //Skip header?

	nolo_decode_position(data, &position);
	//data += 6; //position
	//nolo_decode_position(data+3+3*2, &homepos);
	data += 6;
	nolo_decode_orientation(data, &smp);

	// Tracker viewer kept using the home for head.
	// Something wrong with how they handle the descriptors.
	
	priv->sample = smp;
	//printf("accel: %i - %i - %i\n", priv->sample.accel[0], priv->sample.accel[1], priv->sample.accel[2]);
	//priv->base.position = position;
	//priv->base.rotation = orientation;
}

void nolo_decode_base_station(drv_priv* priv, const unsigned char* data)
{
	// Unknown version
	if (data[0] != 2 || data[1] != 1)
		return;
}
