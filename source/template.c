#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/ios.h>
#include <unistd.h>
#include <string.h>

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void print(ioctlv* names, int size, int term_width) {
	for (s32 i = 0; i < size; ++i) {
		char* str = names[i].data;
		size_t len = strlen(str);
		if (len > term_width - 2) {
			strcpy(str + (term_width - 4), "...");
		}
		printf("\x1b[%i;0H  %s\n", i, str);
	}
}

int min(int a, int b) {
	return a < b ? a : b;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
	//---------------------------------------------------------------------------------

	// Initialise the video system
	VIDEO_Init();

	// This function initialises the attached controllers
	WPAD_Init();
	PAD_Init();

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

	int term_width;
	int term_height;
	CON_GetMetrics(&term_width, &term_height);
	term_height -= 1;
	term_width -= 1;


	// The console understands VT terminal escape codes
	// This positions the cursor on row 2, column 0
	// we can use variables for this with format codes too
	// e.g. printf ("\x1b[%d;%dH", row, column );

	s32 fd = IOS_Open("/dev/dolphin", 2);
	s32 ret = 0;
	s32 disc = 0;
	s32 size = 0;

	ioctlv* names;

	if (fd != -1) {
		ioctlv get_size[] = {{&size, sizeof(size)}};
		ret = IOS_Ioctlv(fd, 0x06, 0, 1, get_size);
		if (ret == 0) {
			names = malloc(size * sizeof(ioctlv));
			for (s32 i = 0; i < size; ++i) {
				char* buf = malloc(257);
				buf[256] = 0;
				names[i] = (ioctlv){buf, 256};
			}
			ret = IOS_Ioctlv(fd, 0x07, 0, size, names);
			if (ret != 0) {
				free(names);
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
		printf("\x1b[0;0H");
		printf("\x1b[2J");
		s32 page = disc / term_height;
		s32 page_pos = disc % term_height;
		print(names + page * term_height, min(term_height, size - (page * term_height)), term_width);
		printf("\x1b[%d;0H*", page_pos);
		WPAD_ScanPads();
		PAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);
		u16 gc_pressed = PAD_ButtonsDown(0);
		if ((pressed & WPAD_BUTTON_HOME) || (gc_pressed & PAD_BUTTON_START)) {
			break;
		} else if ((pressed & WPAD_BUTTON_A) || (gc_pressed & PAD_BUTTON_A)) {
			ioctlv change_disc[] = {{&disc, sizeof(disc)}};
			ret = IOS_Ioctlv(fd, 0x08, 1, 0, change_disc);
			if (ret != 0) {
				printf("0x08 = %d\n", ret);
			} else {
				break;
			}
		} else if ((pressed & WPAD_BUTTON_UP) || (gc_pressed & PAD_BUTTON_UP)) {
			printf("\x1b[%d;0H ", page_pos);
			if (disc > 0) --disc;
		} else if ((pressed & WPAD_BUTTON_DOWN) || (gc_pressed & PAD_BUTTON_DOWN)) {
			printf("\x1b[%d;0H ", page_pos);
			if (disc < size - 1) ++disc;
		}
		VIDEO_WaitVSync();
	}

	IOS_Close(fd);
	SYS_ResetSystem(SYS_RETURNTOMENU,0,0);

	return 0;
}
