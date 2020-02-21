#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/ios.h>
#include <unistd.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
	//---------------------------------------------------------------------------------

	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();

	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	// Initialise the console, required for printf
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();


	// The console understands VT terminal escape codes
	// This positions the cursor on row 2, column 0
	// we can use variables for this with format codes too
	// e.g. printf ("\x1b[%d;%dH", row, column );

	s32 fd = IOS_Open("/dev/dolphin", 2);
	s32 ret = 0;
	s32 disc = 0;
	s32 size = 0;

	if (fd != -1) {
		ioctlv get_size[] = {{&size, sizeof(size)}};
		ret = IOS_Ioctlv(fd, 0x06, 0, 1, get_size);
		if (ret == 0) {
			ioctlv* get_names = malloc(size * sizeof(ioctlv));
			for (s32 i = 0; i < size; ++i) {
				char* buf = malloc(256);
				get_names[i] = (ioctlv){buf, 256};
			}
			ret = IOS_Ioctlv(fd, 0x07, 0, size, get_names);
			if (ret == 0) {
				for (s32 i = 0; i < size; ++i) {
					printf("  %.256s\n", (char*) get_names[i].data);
					free(get_names[i].data);
				}
			} else {
				printf("0x07 = %d", ret);
				sleep(5);
				SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
				return 1;
			}
		} else {
			printf("0x06 = %d\nsize = %d\n", ret, size);
			sleep(5);
			SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
			return 1;
		}
	} else {
		sleep(5);
		SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
		return 1;
	}

	while(1) {
		printf("\x1b[%d;0H*", disc);
		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		if (pressed & WPAD_BUTTON_HOME) {
			break;
		} else if (pressed & WPAD_BUTTON_A) {
			ioctlv change_disc[] = {{&disc, sizeof(disc)}};
			ret = IOS_Ioctlv(fd, 0x08, 1, 0, change_disc);
			if (ret != 0) {
				printf("0x08 = %d\n", ret);
			} else {
				break;
			}
		} else if (pressed & WPAD_BUTTON_UP) {
			printf("\x1b[%d;0H ", disc);
			if (disc > 0) --disc;
		} else if (pressed & WPAD_BUTTON_DOWN) {
			printf("\x1b[%d;0H ", disc);
			if (disc < size - 1) ++disc;
		}
		VIDEO_WaitVSync();
	}

	IOS_Close(fd);
	SYS_ResetSystem(SYS_RETURNTOMENU,0,0);

	return 0;
}
