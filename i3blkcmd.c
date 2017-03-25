#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


#include "common.h"
#include "net.h"
//#include "volume.c"

#define BATTERY "bat"
#define VOLUME "vol"
#define ETHERNET "eth"
#define WIRELESS "wl"
#define CMD_DATE "date"
#define CMD_TIME "time"
#define SCR_BRT "scrbrt"

#define BLOCK_TXT_LEN 1024

static char blockln[BLOCK_TXT_LEN];

char * date_output() {
	char buf[32];
	const char *cmd = "date +%Y-%m-%d";
	extn_cmd(cmd, buf, sizeof(buf));

	sprintf(blockln, "  %s", buf);
	return blockln;
}

char * time_output() {
	char buf[32];
	const char *cmd = "date +%H:%M";
	extn_cmd(cmd, buf, sizeof(buf));

	sprintf(blockln, "  %s", buf);
	return blockln;
}

char * bat_output() {
	char *cmd;
	char chrg_now[32];
	char chrg_full[32];
	char status[32];

	cmd = "cat /sys/class/power_supply/BAT0/charge_now";
	extn_cmd(cmd, chrg_now, sizeof(chrg_now));

	cmd = "cat /sys/class/power_supply/BAT0/charge_full";
	extn_cmd(cmd, chrg_full, sizeof(chrg_full));

	int chrgn = atoi(chrg_now);
	int chrgf = atoi(chrg_full);

	cmd = "cat /sys/class/power_supply/BAT0/status";
	extn_cmd(cmd, status, sizeof(status));

	if ( ! strcmp(status, "Charging"))
		strcpy(status, "CHR");
	if ( ! strcmp(status, "Discharging"))
		strcpy(status, "BAT");
	if ( ! strcmp(status, "Full"))
		strcpy(status, "FULL");

	float ratio = min(1, (float) chrgn / chrgf);
	sprintf(blockln, "  %s %0.2f%%", status,ratio * 100);

	return blockln;
}


char * scrbrt_output() {
	char *cmd;
	char brt_now[32];
	char brt_max[32];

	cmd = "cat /sys/class/backlight/intel_backlight/brightness";
	extn_cmd(cmd, brt_now, sizeof(brt_now));

	cmd = "cat /sys/class/backlight/intel_backlight/max_brightness";
	extn_cmd(cmd, brt_max, sizeof(brt_max));

	int brtn = atoi(brt_now);
	int brtm = atoi(brt_max);

	float ratio = min(1, (float) brtn / brtm);
	sprintf(blockln, "   %0.0f%%", ratio * 100);

	return blockln;
}


char * vol_output() {
	char *cmd;
	char vdata[124];
	
	cmd = "amixer get Master | tail -1";
	extn_cmd(cmd, vdata, sizeof(vdata));

	const char *delim = " ";

	char *tok = strtok(vdata, delim);

	if (tok == NULL)
		error_block("VOL PARSE FAIL");

	bool muted = false;
	int volume = 0;

	while (tok != NULL) {
		const int toklen = strlen(tok);

		if (tok[0] == '[' && toklen > 3) {

			if (tok[2] == '%' || tok[3] == '%' || tok[4] == '%') {
				tok[toklen - 2] = '\0';
				volume = atoi(tok + 1); 
			}

			else {
				tok[toklen - 1] = '\0';
				if ( ! strcmp("off", (tok + 1)))
					muted = true;
			}

		}

		tok = strtok(NULL, delim);
	}


	if (muted)
		sprintf(blockln, "  OFF (%d%%)", volume);
	else 
		sprintf(blockln, "  %d%%", volume);
	
	return blockln;
}


int main(int argc, char *args[]) {

	if (argc < 2) 
		error_block("ARGS ERROR");
	
	else {
		char *cmd = args[1];
		char *fg_clr = (argc > 2) ? args[2] : "";
		char *bg_clr = (argc > 3) ? args[3] : "";

		char *block_txt = 
			! strcmp(cmd, BATTERY)  ?  bat_output()  :
			! strcmp(cmd, VOLUME)   ?  vol_output()  :
			! strcmp(cmd, CMD_DATE) ?  date_output() :
			! strcmp(cmd, CMD_TIME) ?  time_output() :
			! strcmp(cmd, SCR_BRT) 	?  scrbrt_output() :
			! strcmp(cmd, ETHERNET) ?  net_output(INTF_ETH) :
			! strcmp(cmd, WIRELESS) ?  net_output(INTF_WL) : NULL;


		//if (argc > 4 && isdigit(args[4]))
		//	set_block_padding(atoi(args[4]));

		if (block_txt == NULL)
			error_block("INVALID CMD");
		else
			write_block(fg_clr, bg_clr, block_txt);

		if (block_txt != blockln && strcmp(block_txt, EMPTY_BLOCK)) // OBS! This is super bad 
			free(block_txt);
	}	
}
