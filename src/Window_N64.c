#include "Core.h"
#if defined CC_BUILD_N64
#include "Window.h"
#include "Platform.h"
#include "Input.h"
#include "Event.h"
#include "Graphics.h"
#include "String.h"
#include "Funcs.h"
#include "Bitmap.h"
#include "Errors.h"
#include "ExtMath.h"
#include <libdragon.h>

static cc_bool launcherMode;

struct _DisplayData DisplayInfo;
struct _WindowData WindowInfo;

void Window_Init(void) {
    display_init(RESOLUTION_320x240, DEPTH_32_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);
    //display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE_FETCH_ALWAYS);
    
	DisplayInfo.Width  = display_get_width();
	DisplayInfo.Height = display_get_height();
	DisplayInfo.ScaleX = 0.5f;
	DisplayInfo.ScaleY = 0.5f;
	
	Window_Main.Width   = DisplayInfo.Width;
	Window_Main.Height  = DisplayInfo.Height;
	Window_Main.Focused = true;
	Window_Main.Exists  = true;

	Input.Sources = INPUT_SOURCE_GAMEPAD;
	DisplayInfo.ContentOffsetX = 10;
	DisplayInfo.ContentOffsetY = 10;
	joypad_init();

	// change defaults to make more sense for N64
	cc_uint8* binds = (cc_uint8*)KeyBind_GamepadDefaults;
	binds[KEYBIND_INVENTORY]    = CCPAD_B;
	binds[KEYBIND_PLACE_BLOCK]  = CCPAD_Z;
	binds[KEYBIND_HOTBAR_RIGHT] = CCPAD_L;
	binds[KEYBIND_DELETE_BLOCK] = CCPAD_R;

	binds[KEYBIND_FORWARD]   = CCPAD_CUP;
	binds[KEYBIND_BACK]      = CCPAD_CDOWN;
	binds[KEYBIND_LEFT]      = CCPAD_CLEFT;
	binds[KEYBIND_RIGHT]     = CCPAD_CRIGHT;
}

void Window_Free(void) { }

void Window_Create2D(int width, int height) { launcherMode = true;  }
void Window_Create3D(int width, int height) { launcherMode = false; }

void Window_SetTitle(const cc_string* title) { }
void Clipboard_GetText(cc_string* value) { }
void Clipboard_SetText(const cc_string* value) { }

int Window_GetWindowState(void) { return WINDOW_STATE_FULLSCREEN; }
cc_result Window_EnterFullscreen(void) { return 0; }
cc_result Window_ExitFullscreen(void)  { return 0; }
int Window_IsObscured(void)            { return 0; }

void Window_Show(void) { }
void Window_SetSize(int width, int height) { }

void Window_RequestClose(void) {
	Event_RaiseVoid(&WindowEvents.Closing);
}


/*########################################################################################################################*
*----------------------------------------------------Input processing-----------------------------------------------------*
*#########################################################################################################################*/
static void HandleButtons(joypad_buttons_t btns) {
	Gamepad_SetButton(CCPAD_L, btns.l);
	Gamepad_SetButton(CCPAD_R, btns.r);
	
	Gamepad_SetButton(CCPAD_A, btns.a);
	Gamepad_SetButton(CCPAD_B, btns.b);
	Gamepad_SetButton(CCPAD_Z, btns.z);
	
	Gamepad_SetButton(CCPAD_START,  btns.start);
	
	Gamepad_SetButton(CCPAD_LEFT,   btns.d_left);
	Gamepad_SetButton(CCPAD_RIGHT,  btns.d_right);
	Gamepad_SetButton(CCPAD_UP,     btns.d_up);
	Gamepad_SetButton(CCPAD_DOWN,   btns.d_down);

	Gamepad_SetButton(CCPAD_CLEFT,  btns.c_left);
	Gamepad_SetButton(CCPAD_CRIGHT, btns.c_right);
	Gamepad_SetButton(CCPAD_CUP,    btns.c_up);
	Gamepad_SetButton(CCPAD_CDOWN,  btns.c_down);
}

#define AXIS_SCALE 8.0f
static void ProcessAnalogInput(joypad_inputs_t* inputs, double delta) {
	int x = inputs->stick_x;
	int y = inputs->stick_y;

	if (Math_AbsI(x) <= 8) x = 0;
	if (Math_AbsI(y) <= 8) y = 0;	
	
	Gamepad_SetAxis(PAD_AXIS_RIGHT, x / AXIS_SCALE, -y / AXIS_SCALE, delta);
}

void Window_ProcessEvents(double delta) {
	joypad_poll();
	
	joypad_inputs_t inputs = joypad_get_inputs(JOYPAD_PORT_1);
	HandleButtons(inputs.btn);
	ProcessAnalogInput(&inputs, delta);
}

void Cursor_SetPosition(int x, int y) { } // Makes no sense for PSP
void Window_EnableRawMouse(void)  { Input.RawMode = true;  }
void Window_DisableRawMouse(void) { Input.RawMode = false; }
void Window_UpdateRawMouse(void)  { }


/*########################################################################################################################*
*------------------------------------------------------Framebuffer--------------------------------------------------------*
*#########################################################################################################################*/
void Window_AllocFramebuffer(struct Bitmap* bmp) {
	bmp->scan0 = (BitmapCol*)Mem_Alloc(bmp->width * bmp->height, 4, "window pixels");
}

void Window_DrawFramebuffer(Rect2D r, struct Bitmap* bmp) {
	surface_t* fb  = display_get();
	cc_uint32* src = (cc_uint32*)bmp->scan0;
	cc_uint8*  dst = (cc_uint8*)fb->buffer;

	for (int y = 0; y < bmp->height; y++) 
	{
		Mem_Copy(dst + y * fb->stride,
				 src + y * bmp->width, 
				 bmp->width * 4);
	}
	
    display_show(fb);
}

void Window_FreeFramebuffer(struct Bitmap* bmp) {
	Mem_Free(bmp->scan0);
}


/*########################################################################################################################*
*------------------------------------------------------Soft keyboard------------------------------------------------------*
*#########################################################################################################################*/
void OnscreenKeyboard_Open(struct OpenKeyboardArgs* args) { /* TODO implement */ }
void OnscreenKeyboard_SetText(const cc_string* text) { }
void OnscreenKeyboard_Draw2D(Rect2D* r, struct Bitmap* bmp) { }
void OnscreenKeyboard_Draw3D(void) { }
void OnscreenKeyboard_Close(void) { /* TODO implement */ }


/*########################################################################################################################*
*-------------------------------------------------------Misc/Other--------------------------------------------------------*
*#########################################################################################################################*/
void Window_ShowDialog(const char* title, const char* msg) {
	/* TODO implement */
	Platform_LogConst(title);
	Platform_LogConst(msg);
}

cc_result Window_OpenFileDialog(const struct OpenFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}

cc_result Window_SaveFileDialog(const struct SaveFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}
#endif
