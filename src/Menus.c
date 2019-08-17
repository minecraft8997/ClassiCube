#include "Menus.h"
#include "Widgets.h"
#include "Game.h"
#include "Event.h"
#include "Platform.h"
#include "Inventory.h"
#include "Drawer2D.h"
#include "Graphics.h"
#include "Funcs.h"
#include "Model.h"
#include "Generator.h"
#include "Server.h"
#include "Chat.h"
#include "ExtMath.h"
#include "Window.h"
#include "Camera.h"
#include "Http.h"
#include "Block.h"
#include "World.h"
#include "Formats.h"
#include "BlockPhysics.h"
#include "MapRenderer.h"
#include "TexturePack.h"
#include "Audio.h"
#include "Screens.h"
#include "Gui.h"
#include "Deflate.h"
#include "Stream.h"
#include "Builder.h"
#include "Logger.h"

#define MenuScreen_Layout Screen_Layout FontDesc titleFont, textFont;
struct MenuScreen { MenuScreen_Layout };

/* Describes a menu option button */
struct MenuOptionDesc {
	short dir, y;
	const char* name;
	Widget_LeftClick OnClick;
	Button_Get GetValue; Button_Set SetValue;
};
struct SimpleButtonDesc { int x, y; const char* title; Widget_LeftClick onClick; };


/*########################################################################################################################*
*--------------------------------------------------------Menu base--------------------------------------------------------*
*#########################################################################################################################*/
static void Menu_Button(void* s, int i, struct ButtonWidget* btn, int width, const String* text, const FontDesc* font, Widget_LeftClick onClick, int horAnchor, int verAnchor, int x, int y) {
	ButtonWidget_Make(btn, width, onClick, horAnchor, verAnchor, x, y);
	ButtonWidget_Set(btn, text, font);
	((struct Screen*)s)->widgets[i] = (struct Widget*)btn;
}

static void Menu_Label(void* s, int i, struct TextWidget* label, const String* text, const FontDesc* font, int horAnchor, int verAnchor, int x, int y) {
	TextWidget_Make(label, horAnchor, verAnchor, x, y);
	TextWidget_Set(label,  text, font);
	((struct Screen*)s)->widgets[i] = (struct Widget*)label;
}

static void Menu_Input(void* s, int i, struct MenuInputWidget* input, int width, const String* text, FontDesc* font, struct MenuInputDesc* desc, int horAnchor, int verAnchor, int x, int y) {
	MenuInputWidget_Create(input, width, 30, text, font, desc);
	Widget_SetLocation(input, horAnchor, verAnchor, x, y);
	input->base.showCaret = true;
	((struct Screen*)s)->widgets[i] = (struct Widget*)input;
}

static void Menu_Back(void* s, int i, struct ButtonWidget* btn, const char* label, const FontDesc* font, Widget_LeftClick onClick) {
	int width = Gui_ClassicMenu ? 400 : 200;
	String msg = String_FromReadonly(label);
	Menu_Button(s, i, btn, width, &msg, font, onClick, ANCHOR_CENTRE, ANCHOR_MAX, 0, 25);
}

CC_NOINLINE static void Menu_MakeBack(struct ButtonWidget* btn, Widget_LeftClick onClick) {
	int width = Gui_ClassicMenu ? 400 : 200;
	ButtonWidget_Make(btn, width, onClick, ANCHOR_CENTRE, ANCHOR_MAX, 0, 25);
}

CC_NOINLINE static void Menu_MakeTitleFont(FontDesc* font) {
	Drawer2D_MakeFont(font, 16, FONT_STYLE_BOLD);
}

CC_NOINLINE static void Menu_MakeBodyFont(FontDesc* font) {
	Drawer2D_MakeFont(font, 16, FONT_STYLE_NORMAL);
}


static void Menu_ContextLost(void* screen) {
	struct Screen* s = (struct Screen*)screen;
	struct Widget** widgets = s->widgets;
	int i;
	
	for (i = 0; i < s->numWidgets; i++) {
		if (!widgets[i]) continue;
		Elem_Free(widgets[i]);
	}
}

static void Menu_OnResize(void* screen) {
	struct Screen* s = (struct Screen*)screen;
	struct Widget** widgets = s->widgets;
	int i;
	
	for (i = 0; i < s->numWidgets; i++) {
		if (!widgets[i]) continue;
		Widget_Reposition(widgets[i]);
	}
}

static void Menu_RenderWidgets(void* screen, double delta) {
	struct Screen* s = (struct Screen*)screen;
	struct Widget** widgets = s->widgets;
	int i;
	
	for (i = 0; i < s->numWidgets; i++) {
		if (!widgets[i]) continue;
		Elem_Render(widgets[i], delta);
	}
}

static void Menu_RenderBounds(void) {
	/* These were sourced by taking a screenshot of vanilla
	Then using paint to extract the colour components
	Then using wolfram alpha to solve the glblendfunc equation */
	PackedCol topCol    = PACKEDCOL_CONST(24, 24, 24, 105);
	PackedCol bottomCol = PACKEDCOL_CONST(51, 51, 98, 162);
	Gfx_Draw2DGradient(0, 0, Window_Width, Window_Height, topCol, bottomCol);
}

static int Menu_DoMouseDown(void* screen, int x, int y, MouseButton btn) {
	struct Screen* s = (struct Screen*)screen;
	struct Widget** widgets = s->widgets;
	int i, count = s->numWidgets;

	/* iterate backwards (because last elements rendered are shown over others) */
	for (i = count - 1; i >= 0; i--) {
		struct Widget* w = widgets[i];
		if (!w || !Widget_Contains(w, x, y)) continue;
		if (w->disabled) return i;

		if (w->MenuClick && btn == MOUSE_LEFT) {
			w->MenuClick(s, w);
		} else {
			Elem_HandlesMouseDown(w, x, y, btn);
		}
		return i;
	}
	return -1;
}
static bool Menu_MouseDown(void* screen, int x, int y, MouseButton btn) {
	return Menu_DoMouseDown(screen, x, y, btn) >= 0;
}

static int Menu_DoMouseMove(void* screen, int x, int y) {
	struct Screen* s = (struct Screen*)screen;
	struct Widget** widgets = s->widgets;
	int i, count = s->numWidgets;

	for (i = 0; i < count; i++) {
		struct Widget* w = widgets[i];
		if (w) w->active = false;
	}

	for (i = count - 1; i >= 0; i--) {
		struct Widget* w = widgets[i];
		if (!w || !Widget_Contains(w, x, y)) continue;

		w->active = true;
		return i;
	}
	return -1;
}

static bool Menu_MouseMove(void* screen, int x, int y) {
	return Menu_DoMouseMove(screen, x, y) >= 0;
}

static bool Menu_MouseUp(void* screen, int x, int y, MouseButton btn) { return true; }
static bool Menu_KeyPress(void* screen, char keyChar) { return true; }
static bool Menu_KeyUp(void* screen, Key key) { return true; }


/*########################################################################################################################*
*------------------------------------------------------Menu utilities-----------------------------------------------------*
*#########################################################################################################################*/
static int Menu_Index(void* screen, void* widget) {
	struct Screen* s = (struct Screen*)screen;
	struct Widget** widgets = s->widgets;
	int i;

	struct Widget* w = (struct Widget*)widget;
	for (i = 0; i < s->numWidgets; i++) {
		if (widgets[i] == w) return i;
	}
	return -1;
}

static void Menu_Remove(void* screen, int i) {
	struct Screen* s = (struct Screen*)screen;
	struct Widget** widgets = s->widgets;

	if (widgets[i]) { Elem_TryFree(widgets[i]); }
	widgets[i] = NULL;
}

static void Menu_HandleFontChange(struct Screen* s) {
	Event_RaiseVoid(&ChatEvents.FontChanged);
	Elem_HandlesMouseMove(s, Mouse_X, Mouse_Y);
}

static int Menu_Int(const String* str)          { int v; Convert_ParseInt(str, &v); return v; }
static float Menu_Float(const String* str)      { float v; Convert_ParseFloat(str, &v); return v; }
static PackedCol Menu_HexCol(const String* str) { PackedCol v; PackedCol_TryParseHex(str, &v); return v; }
#define Menu_ReplaceActive(screen) Gui_FreeActive(); Gui_SetActive(screen);

static void Menu_SwitchOptions(void* a, void* b)        { OptionsGroupScreen_Show(); }
static void Menu_SwitchPause(void* a, void* b)          { PauseScreen_Show(); }
static void Menu_SwitchClassicOptions(void* a, void* b) { Menu_ReplaceActive(ClassicOptionsScreen_MakeInstance()); }

static void Menu_SwitchKeysClassic(void* a, void* b)      { Menu_ReplaceActive(ClassicKeyBindingsScreen_MakeInstance()); }
static void Menu_SwitchKeysClassicHacks(void* a, void* b) { Menu_ReplaceActive(ClassicHacksKeyBindingsScreen_MakeInstance()); }
static void Menu_SwitchKeysNormal(void* a, void* b)       { Menu_ReplaceActive(NormalKeyBindingsScreen_MakeInstance()); }
static void Menu_SwitchKeysHacks(void* a, void* b)        { Menu_ReplaceActive(HacksKeyBindingsScreen_MakeInstance()); }
static void Menu_SwitchKeysOther(void* a, void* b)        { Menu_ReplaceActive(OtherKeyBindingsScreen_MakeInstance()); }
static void Menu_SwitchKeysMouse(void* a, void* b)        { Menu_ReplaceActive(MouseKeyBindingsScreen_MakeInstance()); }

static void Menu_SwitchMisc(void* a, void* b)      { Menu_ReplaceActive(MiscOptionsScreen_MakeInstance()); }
static void Menu_SwitchGui(void* a, void* b)       { Menu_ReplaceActive(GuiOptionsScreen_MakeInstance()); }
static void Menu_SwitchGfx(void* a, void* b)       { Menu_ReplaceActive(GraphicsOptionsScreen_MakeInstance()); }
static void Menu_SwitchHacks(void* a, void* b)     { Menu_ReplaceActive(HacksSettingsScreen_MakeInstance()); }
static void Menu_SwitchEnv(void* a, void* b)       { Menu_ReplaceActive(EnvSettingsScreen_MakeInstance()); }
static void Menu_SwitchNostalgia(void* a, void* b) { Menu_ReplaceActive(NostalgiaScreen_MakeInstance()); }

static void Menu_SwitchGenLevel(void* a, void* b)        { GenLevelScreen_Show(); }
static void Menu_SwitchClassicGenLevel(void* a, void* b) { ClassicGenScreen_Show(); }
static void Menu_SwitchLoadLevel(void* a, void* b)       { LoadLevelScreen_Show(); }
static void Menu_SwitchSaveLevel(void* a, void* b)       { SaveLevelScreen_Show(); }
static void Menu_SwitchTexPacks(void* a, void* b)        { TexturePackScreen_Show(); }
static void Menu_SwitchHotkeys(void* a, void* b)         { HotkeyListScreen_Show(); }
static void Menu_SwitchFont(void* a, void* b)            { FontListScreen_Show(); }


/*########################################################################################################################*
*--------------------------------------------------------ListScreen-------------------------------------------------------*
*#########################################################################################################################*/
struct ListScreen;
#define LIST_SCREEN_ITEMS 5
#define LIST_SCREEN_EMPTY "-----"

static struct ListScreen {
	Screen_Layout
	struct ButtonWidget buttons[LIST_SCREEN_ITEMS];
	struct ButtonWidget left, right, done;
	FontDesc font;
	float wheelAcc;
	int currentIndex;
	Widget_LeftClick EntryClick;
	void (*LoadEntries)(struct ListScreen* s);
	void (*UpdateEntry)(struct ListScreen* s, struct ButtonWidget* btn, const String* text);
	const char* titleText;
	struct TextWidget title, page;
	StringsBuffer entries;
} ListScreen;

static STRING_REF String ListScreen_UNSAFE_Get(struct ListScreen* s, int index) {
	static const String str = String_FromConst(LIST_SCREEN_EMPTY);

	if (index >= 0 && index < s->entries.count) {
		return StringsBuffer_UNSAFE_Get(&s->entries, index);
	}
	return str;
}

static void ListScreen_UpdatePage(struct ListScreen* s) {
	String page; char pageBuffer[STRING_SIZE];
	int beg, end;
	int num, pages;

	beg = LIST_SCREEN_ITEMS;
	end = s->entries.count - LIST_SCREEN_ITEMS;
	s->left.disabled  = s->currentIndex <  beg;
	s->right.disabled = s->currentIndex >= end;

	if (Game_ClassicMode) return;
	num   = (s->currentIndex / LIST_SCREEN_ITEMS) + 1;
	pages = Math_CeilDiv(s->entries.count, LIST_SCREEN_ITEMS);
	if (pages == 0) pages = 1;

	String_InitArray(page, pageBuffer);
	String_Format2(&page, "&7Page %i of %i", &num, &pages);
	TextWidget_Set(&s->page, &page, &s->font);
}

static void ListScreen_UpdateEntry(struct ListScreen* s, struct ButtonWidget* button, const String* text) {
	ButtonWidget_Set(button, text, &s->font);
}

static void ListScreen_RedrawEntries(struct ListScreen* s) {
	String str;
	int i;

	for (i = 0; i < LIST_SCREEN_ITEMS; i++) {
		str = ListScreen_UNSAFE_Get(s, s->currentIndex + i);
		s->UpdateEntry(s, &s->buttons[i], &str);
	}
}

static void ListScreen_SetCurrentIndex(struct ListScreen* s, int index) {
	if (index >= s->entries.count) { index = s->entries.count - 1; }
	if (index < 0) index = 0;

	s->currentIndex = index;
	ListScreen_RedrawEntries(s);
	ListScreen_UpdatePage(s);
}

static void ListScreen_PageClick(struct ListScreen* s, bool forward) {
	int delta = forward ? LIST_SCREEN_ITEMS : -LIST_SCREEN_ITEMS;
	ListScreen_SetCurrentIndex(s, s->currentIndex + delta);
}

static void ListScreen_MoveBackwards(void* screen, void* b) {
	struct ListScreen* s = (struct ListScreen*)screen;
	ListScreen_PageClick(s, false);
}

static void ListScreen_MoveForwards(void* screen, void* b) {
	struct ListScreen* s = (struct ListScreen*)screen;
	ListScreen_PageClick(s, true);
}

static void ListScreen_QuickSort(int left, int right) {
	StringsBuffer* buffer = &ListScreen.entries; 
	uint32_t* keys = buffer->flagsBuffer; uint32_t key;

	while (left < right) {
		int i = left, j = right;
		String pivot = StringsBuffer_UNSAFE_Get(buffer, (i + j) >> 1);
		String strI, strJ;		

		/* partition the list */
		while (i <= j) {
			while ((strI = StringsBuffer_UNSAFE_Get(buffer, i), String_Compare(&pivot, &strI)) > 0) i++;
			while ((strJ = StringsBuffer_UNSAFE_Get(buffer, j), String_Compare(&pivot, &strJ)) < 0) j--;
			QuickSort_Swap_Maybe();
		}
		/* recurse into the smaller subset */
		QuickSort_Recurse(ListScreen_QuickSort)
	}
}

CC_NOINLINE static void ListScreen_Sort(struct ListScreen* s) {
	if (s->entries.count) {
		ListScreen_QuickSort(0, s->entries.count - 1);
	}
}

static String ListScreen_UNSAFE_GetCur(struct ListScreen* s, void* widget) {
	int i = Menu_Index(s, widget);
	return ListScreen_UNSAFE_Get(s, s->currentIndex + i);
}

static void ListScreen_Select(struct ListScreen* s, const String* str) {
	String entry;
	int i;

	for (i = 0; i < s->entries.count; i++) {
		entry = StringsBuffer_UNSAFE_Get(&s->entries, i);
		if (!String_CaselessEquals(&entry, str)) continue;

		s->currentIndex = i;
		return;
	}
}

static bool ListScreen_KeyDown(void* screen, Key key) {
	struct ListScreen* s = (struct ListScreen*)screen;
	if (key == KEY_LEFT || key == KEY_PAGEUP) {
		ListScreen_PageClick(s, false);
	} else if (key == KEY_RIGHT || key == KEY_PAGEDOWN) {
		ListScreen_PageClick(s, true);
	} else {
		return false;
	}
	return true;
}

static bool ListScreen_MouseScroll(void* screen, float delta) {
	struct ListScreen* s = (struct ListScreen*)screen;
	int steps = Utils_AccumulateWheelDelta(&s->wheelAcc, delta);

	if (steps) ListScreen_SetCurrentIndex(s, s->currentIndex - steps);
	return true;
}

static void ListScreen_Init(void* screen) {
	static struct Widget* widgets[LIST_SCREEN_ITEMS + 3 + 2] = {
		(struct Widget*)&ListScreen.buttons[0], (struct Widget*)&ListScreen.buttons[1],
		(struct Widget*)&ListScreen.buttons[2], (struct Widget*)&ListScreen.buttons[3],
		(struct Widget*)&ListScreen.buttons[4], (struct Widget*)&ListScreen.left,
		(struct Widget*)&ListScreen.right,      (struct Widget*)&ListScreen.done,
		(struct Widget*)&ListScreen.title,      (struct Widget*)&ListScreen.page
	};
	struct ListScreen* s = (struct ListScreen*)screen;
	int i;

	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);
	s->wheelAcc   = 0.0f;
	s->currentIndex = 0;

	for (i = 0; i < LIST_SCREEN_ITEMS; i++) { 
		ButtonWidget_Make(&s->buttons[i],  300, s->EntryClick,
					ANCHOR_CENTRE, ANCHOR_CENTRE,    0, (i - 2) * 50);
	}

	ButtonWidget_Make(&s->left,  40, ListScreen_MoveBackwards,
					ANCHOR_CENTRE, ANCHOR_CENTRE, -220,    0);
	ButtonWidget_Make(&s->right, 40, ListScreen_MoveForwards,
					ANCHOR_CENTRE, ANCHOR_CENTRE,  220,    0);
	TextWidget_Make(&s->title, 
					ANCHOR_CENTRE, ANCHOR_CENTRE,    0, -155);
	TextWidget_Make(&s->page,  
					ANCHOR_CENTRE,    ANCHOR_MAX,    0,   75);

	Menu_MakeBack(&s->done, Menu_SwitchPause);
	Menu_MakeTitleFont(&s->font);
	s->LoadEntries(s);
}

static void ListScreen_Render(void* screen, double delta) {
	Menu_RenderBounds();
	Gfx_SetTexturing(true);
	Menu_RenderWidgets(screen, delta);
	Gfx_SetTexturing(false);
}

static void ListScreen_Free(void* screen) {
	struct ListScreen* s = (struct ListScreen*)screen;
	Font_Free(&s->font);
	StringsBuffer_Clear(&s->entries);
}

static void ListScreen_ContextRecreated(void* screen) {
	struct ListScreen* s = (struct ListScreen*)screen;
	ListScreen_RedrawEntries(s);

	ButtonWidget_SetConst(&s->left,  "<",          &s->font);
	ButtonWidget_SetConst(&s->right, ">",          &s->font);
	TextWidget_SetConst(&s->title,   s->titleText, &s->font);
	ButtonWidget_SetConst(&s->done, "Done",        &s->font);
	ListScreen_UpdatePage(s);
}

static struct ScreenVTABLE ListScreen_VTABLE = {
	ListScreen_Init,    ListScreen_Render, ListScreen_Free,
	ListScreen_KeyDown, Menu_KeyUp,        Menu_KeyPress,
	Menu_MouseDown,     Menu_MouseUp,      Menu_MouseMove,  ListScreen_MouseScroll,
	Menu_OnResize,      Menu_ContextLost,  ListScreen_ContextRecreated
};
void ListScreen_Show(void) {
	struct ListScreen* s = &ListScreen;
	s->grabsInput   = true;
	s->closable     = true;
	s->VTABLE       = &ListScreen_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_MENU);
}


/*########################################################################################################################*
*--------------------------------------------------------MenuScreen-------------------------------------------------------*
*#########################################################################################################################*/
static bool MenuScreen_KeyDown(void* screen, Key key) { return key < KEY_F1 || key > KEY_F35; }
static bool MenuScreen_MouseScroll(void* screen, float delta) { return true; }

static void MenuScreen_Init(void* screen) {
	struct MenuScreen* s = (struct MenuScreen*)screen;
	if (!s->titleFont.Size) Menu_MakeTitleFont(&s->titleFont);
	if (!s->textFont.Size)  Menu_MakeBodyFont(&s->textFont);
}

static void MenuScreen_Render(void* screen, double delta) {
	Menu_RenderBounds();
	Gfx_SetTexturing(true);
	Menu_RenderWidgets(screen, delta);
	Gfx_SetTexturing(false);
}

static void MenuScreen_Free(void* screen) {
	struct MenuScreen* s = (struct MenuScreen*)screen;
	Font_Free(&s->titleFont);
	Font_Free(&s->textFont);
}


/*########################################################################################################################*
*-------------------------------------------------------PauseScreen-------------------------------------------------------*
*#########################################################################################################################*/
static struct PauseScreen {
	MenuScreen_Layout
	struct ButtonWidget buttons[8];
} PauseScreen_Instance;

static void PauseScreen_Quit(void* a, void* b) { Window_Close(); }
static void PauseScreen_Game(void* a, void* b) { Gui_Remove((struct Screen*)&PauseScreen_Instance); }

static void PauseScreen_CheckHacksAllowed(void* screen) {
	struct PauseScreen* s = (struct PauseScreen*)screen;
	if (Gui_ClassicMenu) return;
	s->buttons[4].disabled = !LocalPlayer_Instance.Hacks.CanAnyHacks; /* select texture pack */
}

static void PauseScreen_MakeButtons(struct PauseScreen* s, int width, const struct SimpleButtonDesc* descs, int count) {
	int i;
	for (i = 0; i < count; i++) {
		String text = String_FromReadonly(descs[i].title);
		Menu_Button(s, i, &s->buttons[i], width, &text, &s->titleFont, descs[i].onClick,
			ANCHOR_CENTRE, ANCHOR_CENTRE, descs[i].x, descs[i].y);
	}
}

static void PauseScreen_ContextRecreated(void* screen) {
	static const String quitMsg = String_FromConst("Quit game");
	struct PauseScreen* s = (struct PauseScreen*)screen;

	static const struct SimpleButtonDesc classicDescs[5] = {
		{    0, -100, "Options...",             Menu_SwitchClassicOptions },
		{    0,  -50, "Generate new level...",  Menu_SwitchClassicGenLevel },
		{    0,    0, "Load level...",          Menu_SwitchLoadLevel },
		{    0,   50, "Save level...",          Menu_SwitchSaveLevel },
		{    0,  150, "Nostalgia options...",   Menu_SwitchNostalgia }
	};
	static const struct SimpleButtonDesc modernDescs[6] = {
		{ -160,  -50, "Options...",             Menu_SwitchOptions   },
		{  160,  -50, "Generate new level...",  Menu_SwitchGenLevel  },
		{  160,    0, "Load level...",          Menu_SwitchLoadLevel },
		{  160,   50, "Save level...",          Menu_SwitchSaveLevel },
		{ -160,    0, "Change texture pack...", Menu_SwitchTexPacks  },
		{ -160,   50, "Hotkeys...",             Menu_SwitchHotkeys   }
	};

	if (Gui_ClassicMenu) {
		PauseScreen_MakeButtons(s, 400, classicDescs, 5);
		Menu_Back(s, 5, &s->buttons[5], "Back to game", &s->titleFont, PauseScreen_Game);

		/* Disable nostalgia options in classic mode */
		if (Game_ClassicMode) Menu_Remove(s, 4);
		s->widgets[6] = NULL;
		s->widgets[7] = NULL;
	} else {
		PauseScreen_MakeButtons(s, 300, modernDescs, 6);
		Menu_Button(s, 6, &s->buttons[6], 120, &quitMsg, &s->titleFont, PauseScreen_Quit,
			ANCHOR_MAX, ANCHOR_MAX, 5, 5);
		Menu_Back(s,   7, &s->buttons[7], "Back to game",&s->titleFont, PauseScreen_Game);
	}

	if (!Server.IsSinglePlayer) {
		s->buttons[1].disabled = true;
		s->buttons[2].disabled = true;
	}
	PauseScreen_CheckHacksAllowed(s);
}

static void PauseScreen_Init(void* screen) {
	struct PauseScreen* s = (struct PauseScreen*)screen;
	MenuScreen_Init(s);
	Event_RegisterVoid(&UserEvents.HackPermissionsChanged, s, PauseScreen_CheckHacksAllowed);
}

static void PauseScreen_Free(void* screen) {
	struct PauseScreen* s = (struct PauseScreen*)screen;
	MenuScreen_Free(s);
	Event_UnregisterVoid(&UserEvents.HackPermissionsChanged, s, PauseScreen_CheckHacksAllowed);
}

static struct ScreenVTABLE PauseScreen_VTABLE = {
	PauseScreen_Init,   MenuScreen_Render,  PauseScreen_Free,
	MenuScreen_KeyDown, Menu_KeyUp,         Menu_KeyPress,
	Menu_MouseDown,     Menu_MouseUp,       Menu_MouseMove,   MenuScreen_MouseScroll,
	Menu_OnResize,      Menu_ContextLost,   PauseScreen_ContextRecreated
};
void PauseScreen_Show(void) {
	static struct Widget* widgets[8];
	struct PauseScreen* s = &PauseScreen_Instance;

	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);

	s->VTABLE = &PauseScreen_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_MENU);
}


/*########################################################################################################################*
*--------------------------------------------------OptionsGroupScreen-----------------------------------------------------*
*#########################################################################################################################*/
static struct OptionsGroupScreen {
	MenuScreen_Layout
	int selectedI;
	struct ButtonWidget buttons[7];
	struct TextWidget desc;	
	struct ButtonWidget done;	
} OptionsGroupScreen;

static const char* optsGroup_descs[7] = {
	"&eMusic/Sound, view bobbing, and more",
	"&eChat options, gui scale, font settings, and more",
	"&eFPS limit, view distance, entity names/shadows",
	"&eSet key bindings, bind keys to act as mouse clicks",
	"&eHacks allowed, jump settings, and more",
	"&eEnv colours, water level, weather, and more",
	"&eSettings for resembling the original classic",
};
static const struct SimpleButtonDesc optsGroup_btns[7] = {
	{ -160, -100, "Misc options...",      Menu_SwitchMisc       },
	{ -160,  -50, "Gui options...",       Menu_SwitchGui        },
	{ -160,    0, "Graphics options...",  Menu_SwitchGfx        },
	{ -160,   50, "Controls...",          Menu_SwitchKeysNormal },
	{  160,  -50, "Hacks settings...",    Menu_SwitchHacks      },
	{  160,    0, "Env settings...",      Menu_SwitchEnv        },
	{  160,   50, "Nostalgia options...", Menu_SwitchNostalgia  }
};

static void OptionsGroupScreen_CheckHacksAllowed(void* screen) {
	struct OptionsGroupScreen* s = (struct OptionsGroupScreen*)screen;
	s->buttons[5].disabled = !LocalPlayer_Instance.Hacks.CanAnyHacks; /* env settings */
}

CC_NOINLINE static void OptionsGroupScreen_UpdateDesc(struct OptionsGroupScreen* s) {
	TextWidget_SetConst(&s->desc, optsGroup_descs[s->selectedI], &s->textFont);
}

static void OptionsGroupScreen_ContextRecreated(void* screen) {
	struct OptionsGroupScreen* s = (struct OptionsGroupScreen*)screen;
	int i;
	for (i = 0; i < Array_Elems(optsGroup_btns); i++) {
		ButtonWidget_SetConst(&s->buttons[i], optsGroup_btns[i].title, &s->titleFont);
	}

	ButtonWidget_SetConst(&s->done, "Done", &s->titleFont);
	if (s->selectedI >= 0) OptionsGroupScreen_UpdateDesc(s);
	OptionsGroupScreen_CheckHacksAllowed(s);
}

static void OptionsGroupScreen_Init(void* screen) {
	static struct Widget* widgets[9] = {
		(struct Widget*)&OptionsGroupScreen.buttons[0], (struct Widget*)&OptionsGroupScreen.buttons[1],
		(struct Widget*)&OptionsGroupScreen.buttons[2], (struct Widget*)&OptionsGroupScreen.buttons[3],
		(struct Widget*)&OptionsGroupScreen.buttons[4], (struct Widget*)&OptionsGroupScreen.buttons[5],
		(struct Widget*)&OptionsGroupScreen.buttons[6], (struct Widget*)&OptionsGroupScreen.desc,
		(struct Widget*)&OptionsGroupScreen.done
	};
	struct OptionsGroupScreen* s = (struct OptionsGroupScreen*)screen;
	int i;

	MenuScreen_Init(s);
	Event_RegisterVoid(&UserEvents.HackPermissionsChanged, s, OptionsGroupScreen_CheckHacksAllowed);

	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);
	s->selectedI  = -1;

	for (i = 0; i < Array_Elems(optsGroup_btns); i++) {
		ButtonWidget_Make(&s->buttons[i], 300, optsGroup_btns[i].onClick,
						ANCHOR_CENTRE, ANCHOR_CENTRE, optsGroup_btns[i].x, optsGroup_btns[i].y);
	}

	Menu_MakeBack(&s->done, Menu_SwitchPause);
	TextWidget_Make(&s->desc, ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 100);
}

static void OptionsGroupScreen_Free(void* screen) {
	struct OptionsGroupScreen* s = (struct OptionsGroupScreen*)screen;
	MenuScreen_Free(s);
	Event_UnregisterVoid(&UserEvents.HackPermissionsChanged, s, OptionsGroupScreen_CheckHacksAllowed);
}

static bool OptionsGroupScreen_MouseMove(void* screen, int x, int y) {
	struct OptionsGroupScreen* s = (struct OptionsGroupScreen*)screen;
	int i = Menu_DoMouseMove(s, x, y);
	if (i == -1 || i == s->selectedI) return true;
	if (i >= Array_Elems(optsGroup_descs)) return true;

	s->selectedI = i;
	OptionsGroupScreen_UpdateDesc(s);
	return true;
}

static struct ScreenVTABLE OptionsGroupScreen_VTABLE = {
	OptionsGroupScreen_Init, MenuScreen_Render,  OptionsGroupScreen_Free,
	MenuScreen_KeyDown,      Menu_KeyUp,         Menu_KeyPress,
	Menu_MouseDown,          Menu_MouseUp,       OptionsGroupScreen_MouseMove, MenuScreen_MouseScroll,
	Menu_OnResize,           Menu_ContextLost,   OptionsGroupScreen_ContextRecreated
};
void OptionsGroupScreen_Show(void) {
	struct OptionsGroupScreen* s = &OptionsGroupScreen;
	s->grabsInput = true;
	s->closable   = true;
	s->VTABLE     = &OptionsGroupScreen_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_MENU);
}


/*########################################################################################################################*
*----------------------------------------------------EditHotkeyScreen-----------------------------------------------------*
*#########################################################################################################################*/
static struct EditHotkeyScreen {
	MenuScreen_Layout
	struct HotkeyData curHotkey, origHotkey;
	int selectedI;
	bool supressNextPress;
	struct MenuInputWidget input;
	struct ButtonWidget buttons[6];
} EditHotkeyScreen_Instance;

static void EditHotkeyScreen_Make(struct EditHotkeyScreen* s, int i, int x, int y, const String* text, Widget_LeftClick onClick) {
	Menu_Button(s, i, &s->buttons[i], 300, text, &s->titleFont, onClick,
		ANCHOR_CENTRE, ANCHOR_CENTRE, x, y);
}

static void HotkeyListScreen_MakeFlags(int flags, String* str);
static void EditHotkeyScreen_MakeFlags(int flags, String* str) {
	if (flags == 0) String_AppendConst(str, " None");
	HotkeyListScreen_MakeFlags(flags, str);
}

static void EditHotkeyScreen_MakeBaseKey(struct EditHotkeyScreen* s, Widget_LeftClick onClick) {
	String text; char textBuffer[STRING_SIZE];
	String_InitArray(text, textBuffer);

	String_AppendConst(&text, "Key: ");
	String_AppendConst(&text, Key_Names[s->curHotkey.Trigger]);
	EditHotkeyScreen_Make(s, 0, 0, -150, &text, onClick);
}

static void EditHotkeyScreen_MakeModifiers(struct EditHotkeyScreen* s, Widget_LeftClick onClick) {
	String text; char textBuffer[STRING_SIZE];
	String_InitArray(text, textBuffer);

	String_AppendConst(&text, "Modifiers:");
	EditHotkeyScreen_MakeFlags(s->curHotkey.Flags, &text);
	EditHotkeyScreen_Make(s, 1, 0, -100, &text, onClick);
}

static void EditHotkeyScreen_MakeLeaveOpen(struct EditHotkeyScreen* s, Widget_LeftClick onClick) {
	String text; char textBuffer[STRING_SIZE];
	String_InitArray(text, textBuffer);

	String_AppendConst(&text, "Input stays open: ");
	String_AppendConst(&text, s->curHotkey.StaysOpen ? "ON" : "OFF");
	EditHotkeyScreen_Make(s, 2, -100, 10, &text, onClick);
}

static void EditHotkeyScreen_BaseKey(void* screen, void* b) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	s->selectedI = 0;
	s->supressNextPress = true;
	ButtonWidget_SetConst(&s->buttons[0], "Key: press a key..", &s->titleFont);
}

static void EditHotkeyScreen_Modifiers(void* screen, void* b) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	s->selectedI = 1;
	s->supressNextPress = true;
	ButtonWidget_SetConst(&s->buttons[1], "Modifiers: press a key..", &s->titleFont);
}

static void EditHotkeyScreen_LeaveOpen(void* screen, void* b) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	/* Reset 'waiting for key..' state of two other buttons */
	if (s->selectedI == 0) {
		EditHotkeyScreen_MakeBaseKey(s, EditHotkeyScreen_BaseKey);
		s->supressNextPress = false;
	} else if (s->selectedI == 1) {
		EditHotkeyScreen_MakeModifiers(s, EditHotkeyScreen_Modifiers);
		s->supressNextPress = false;
	}

	s->selectedI = -1;
	s->curHotkey.StaysOpen = !s->curHotkey.StaysOpen;
	EditHotkeyScreen_MakeLeaveOpen(s, EditHotkeyScreen_LeaveOpen);
}

static void EditHotkeyScreen_SaveChanges(void* screen, void* b) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	struct HotkeyData hk = s->origHotkey;

	if (hk.Trigger) {
		Hotkeys_Remove(hk.Trigger, hk.Flags);
		Hotkeys_UserRemovedHotkey(hk.Trigger, hk.Flags);
	}

	hk = s->curHotkey;
	if (hk.Trigger) {
		String text = s->input.base.text;
		Hotkeys_Add(hk.Trigger, hk.Flags, &text, hk.StaysOpen);
		Hotkeys_UserAddedHotkey(hk.Trigger, hk.Flags, hk.StaysOpen, &text);
	}

	Gui_Remove((struct Screen*)screen);
	HotkeyListScreen_Show();
}

static void EditHotkeyScreen_RemoveHotkey(void* screen, void* b) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	struct HotkeyData hk = s->origHotkey;

	if (hk.Trigger) {
		Hotkeys_Remove(hk.Trigger, hk.Flags);
		Hotkeys_UserRemovedHotkey(hk.Trigger, hk.Flags);
	}

	Gui_Remove((struct Screen*)screen);
	HotkeyListScreen_Show();
}

static void EditHotkeyScreen_Render(void* screen, double delta) {
	PackedCol grey = PACKEDCOL_CONST(150, 150, 150, 255);
	int x, y;
	MenuScreen_Render(screen, delta);

	x = Window_Width / 2; y = Window_Height / 2;
	Gfx_Draw2DFlat(x - 250, y - 65, 500, 2, grey);
	Gfx_Draw2DFlat(x - 250, y + 45, 500, 2, grey);
}

static void EditHotkeyScreen_Free(void* screen) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	s->selectedI = -1;
	MenuScreen_Free(s);
}

static bool EditHotkeyScreen_KeyPress(void* screen, char keyChar) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	if (s->supressNextPress) {
		s->supressNextPress = false;
	} else {
		InputWidget_Append(&s->input.base, keyChar);
	}
	return true;
}

static bool EditHotkeyScreen_KeyDown(void* screen, Key key) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	if (s->selectedI >= 0) {
		if (s->selectedI == 0) {
			s->curHotkey.Trigger = key;
			EditHotkeyScreen_MakeBaseKey(s, EditHotkeyScreen_BaseKey);
		} else if (s->selectedI == 1) {
			if      (key == KEY_LCTRL  || key == KEY_RCTRL)  s->curHotkey.Flags |= HOTKEY_MOD_CTRL;
			else if (key == KEY_LSHIFT || key == KEY_RSHIFT) s->curHotkey.Flags |= HOTKEY_MOD_SHIFT;
			else if (key == KEY_LALT   || key == KEY_RALT)   s->curHotkey.Flags |= HOTKEY_MOD_ALT;
			else s->curHotkey.Flags = 0;

			EditHotkeyScreen_MakeModifiers(s, EditHotkeyScreen_Modifiers);
		}

		s->supressNextPress = true;
		s->selectedI = -1;
		return true;
	}
	return Elem_HandlesKeyDown(&s->input.base, key) || MenuScreen_KeyDown(s, key);
}

static bool EditHotkeyScreen_KeyUp(void* screen, Key key) {
	struct EditHotkeyScreen* s = (struct EditHotkeyScreen*)screen;
	return Elem_HandlesKeyUp(&s->input.base, key);
}

static void EditHotkeyScreen_ContextRecreated(void* screen) {
	static const String saveHK = String_FromConst("Save changes");
	static const String addHK  = String_FromConst("Add hotkey");
	static const String remHK  = String_FromConst("Remove hotkey");
	static const String cancel = String_FromConst("Cancel");

	struct EditHotkeyScreen* s  = (struct EditHotkeyScreen*)screen;
	struct MenuInputDesc desc;
	String text; bool existed;

	MenuInput_String(desc);
	existed = s->origHotkey.Trigger != KEY_NONE;
	if (existed) {
		text = StringsBuffer_UNSAFE_Get(&HotkeysText, s->origHotkey.TextIndex);
	} else { text = String_Empty; }

	EditHotkeyScreen_MakeBaseKey(s,   EditHotkeyScreen_BaseKey);
	EditHotkeyScreen_MakeModifiers(s, EditHotkeyScreen_Modifiers);
	EditHotkeyScreen_MakeLeaveOpen(s, EditHotkeyScreen_LeaveOpen);

	EditHotkeyScreen_Make(s, 3, 0,  80, existed ? &saveHK : &addHK, 
		EditHotkeyScreen_SaveChanges);
	EditHotkeyScreen_Make(s, 4, 0, 130, existed ? &remHK : &cancel, 
		EditHotkeyScreen_RemoveHotkey);

	Menu_Back(s,  5, &s->buttons[5], "Cancel", &s->titleFont, Menu_SwitchHotkeys);
	Menu_Input(s, 6, &s->input, 500, &text,    &s->textFont, &desc,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, -35);
}

static struct ScreenVTABLE EditHotkeyScreen_VTABLE = {
	MenuScreen_Init,          EditHotkeyScreen_Render, EditHotkeyScreen_Free,
	EditHotkeyScreen_KeyDown, EditHotkeyScreen_KeyUp,  EditHotkeyScreen_KeyPress,
	Menu_MouseDown,           Menu_MouseUp,            Menu_MouseMove,            MenuScreen_MouseScroll,
	Menu_OnResize,            Menu_ContextLost,        EditHotkeyScreen_ContextRecreated
};
void EditHotkeyScreen_Show(struct HotkeyData original) {
	static struct Widget* widgets[7];
	struct EditHotkeyScreen* s = &EditHotkeyScreen_Instance;

	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);

	s->VTABLE     = &EditHotkeyScreen_VTABLE;
	s->selectedI  = -1;
	s->origHotkey = original;
	s->curHotkey  = original;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_MENU);
}


/*########################################################################################################################*
*-----------------------------------------------------GenLevelScreen------------------------------------------------------*
*#########################################################################################################################*/
static struct GenLevelScreen {
	MenuScreen_Layout
	struct ButtonWidget buttons[3];
	struct MenuInputWidget* selected;
	struct MenuInputWidget inputs[4];
	struct TextWidget labels[5];
} GenLevelScreen_Instance;

CC_NOINLINE static int GenLevelScreen_GetInt(struct GenLevelScreen* s, int index) {
	struct MenuInputWidget* input = &s->inputs[index];
	struct MenuInputDesc* desc;
	String text = input->base.text;
	int value;

	desc = &input->desc;
	if (!desc->VTABLE->IsValidValue(desc, &text)) return 0;
	Convert_ParseInt(&text, &value); return value;
}

CC_NOINLINE static int GenLevelScreen_GetSeedInt(struct GenLevelScreen* s, int index) {
	struct MenuInputWidget* input = &s->inputs[index];
	RNGState rnd;

	if (!input->base.text.length) {
		Random_SeedFromCurrentTime(&rnd);
		return Random_Next(&rnd, Int32_MaxValue);
	}
	return GenLevelScreen_GetInt(s, index);
}

static void GenLevelScreen_Begin(int width, int height, int length) {
	World_Reset();
	World_SetDimensions(width, height, length);
	Gui_FreeActive(); // TODO: fix for classicgen
	GeneratingScreen_Show();
}

static void GenLevelScreen_Gen(void* screen, bool vanilla) {
	struct GenLevelScreen* s = (struct GenLevelScreen*)screen;
	int width  = GenLevelScreen_GetInt(s, 0);
	int height = GenLevelScreen_GetInt(s, 1);
	int length = GenLevelScreen_GetInt(s, 2);
	int seed   = GenLevelScreen_GetSeedInt(s, 3);

	uint64_t volume = (uint64_t)width * height * length;
	if (volume > Int32_MaxValue) {
		Chat_AddRaw("&cThe generated map's volume is too big.");
	} else if (!width || !height || !length) {
		Chat_AddRaw("&cOne of the map dimensions is invalid.");
	} else {
		Gen_Vanilla = vanilla; Gen_Seed = seed;
		GenLevelScreen_Begin(width, height, length);
	}
}

static void GenLevelScreen_Flatgrass(void* a, void* b) { GenLevelScreen_Gen(a, false); }
static void GenLevelScreen_Notchy(void* a, void* b)    { GenLevelScreen_Gen(a, true);  }

static void GenLevelScreen_InputClick(void* screen, void* input) {
	struct GenLevelScreen* s = (struct GenLevelScreen*)screen;
	if (s->selected) s->selected->base.showCaret = false;

	s->selected = (struct MenuInputWidget*)input;
	Elem_HandlesMouseDown(&s->selected->base, Mouse_X, Mouse_Y, MOUSE_LEFT);
	s->selected->base.showCaret = true;
}

static void GenLevelScreen_Input(struct GenLevelScreen* s, int i, int y, bool seed, int def) {
	String tmp; char tmpBuffer[STRING_SIZE];
	struct MenuInputWidget* input = &s->inputs[i];
	struct MenuInputDesc desc;

	if (seed) {
		MenuInput_Seed(desc);
	} else {
		MenuInput_Int(desc, 1, 8192, def);
	}

	String_InitArray(tmp, tmpBuffer);
	desc.VTABLE->GetDefault(&desc, &tmp);

	Menu_Input(s, i, input, 200, &tmp, &s->textFont, &desc,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, y);

	input->base.showCaret = false;
	input->base.MenuClick = GenLevelScreen_InputClick;
}

static void GenLevelScreen_Label(struct GenLevelScreen* s, int i, int y, const char* title) {
	struct TextWidget* label = &s->labels[i];
	PackedCol col = PACKEDCOL_CONST(224, 224, 224, 255);

	String text = String_FromReadonly(title);
	Menu_Label(s, i + 4, label, &text, &s->textFont,
		ANCHOR_CENTRE_MAX, ANCHOR_CENTRE, 110, y);

	label->col = col; 
}

static bool GenLevelScreen_KeyDown(void* screen, Key key) {
	struct GenLevelScreen* s = (struct GenLevelScreen*)screen;
	if (s->selected && Elem_HandlesKeyDown(&s->selected->base, key)) return true;
	return MenuScreen_KeyDown(s, key);
}

static bool GenLevelScreen_KeyUp(void* screen, Key key) {
	struct GenLevelScreen* s = (struct GenLevelScreen*)screen;
	return !s->selected || Elem_HandlesKeyUp(&s->selected->base, key);
}

static bool GenLevelScreen_KeyPress(void* screen, char keyChar) {
	struct GenLevelScreen* s = (struct GenLevelScreen*)screen;
	if (s->selected) InputWidget_Append(&s->selected->base, keyChar);
	return true;
}

static void GenLevelScreen_ContextRecreated(void* screen) {
	static const String title = String_FromConst("Generate new level");
	static const String flat  = String_FromConst("Flatgrass");
	static const String norm  = String_FromConst("Vanilla");
	struct GenLevelScreen* s  = (struct GenLevelScreen*)screen;

	GenLevelScreen_Input(s, 0, -80, false, World.Width);
	GenLevelScreen_Input(s, 1, -40, false, World.Height);
	GenLevelScreen_Input(s, 2,   0, false, World.Length);
	GenLevelScreen_Input(s, 3,  40, true,  0);

	GenLevelScreen_Label(s, 0, -80, "Width:");
	GenLevelScreen_Label(s, 1, -40, "Height:");
	GenLevelScreen_Label(s, 2,   0, "Length:");
	GenLevelScreen_Label(s, 3,  40, "Seed:");
	
	Menu_Label(s,   8, &s->labels[4], &title,      &s->textFont,
		ANCHOR_CENTRE, ANCHOR_CENTRE,    0, -130);
	Menu_Button(s,  9, &s->buttons[0], 200, &flat, &s->titleFont, GenLevelScreen_Flatgrass,
		ANCHOR_CENTRE, ANCHOR_CENTRE, -120,  100);
	Menu_Button(s, 10, &s->buttons[1], 200, &norm, &s->titleFont, GenLevelScreen_Notchy,
		ANCHOR_CENTRE, ANCHOR_CENTRE,  120,  100);
	Menu_Back(s,   11, &s->buttons[2], "Cancel",   &s->titleFont, Menu_SwitchPause);
}

static struct ScreenVTABLE GenLevelScreen_VTABLE = {
	MenuScreen_Init,        MenuScreen_Render,    MenuScreen_Free,
	GenLevelScreen_KeyDown, GenLevelScreen_KeyUp, GenLevelScreen_KeyPress,
	Menu_MouseDown,         Menu_MouseUp,         Menu_MouseMove,          MenuScreen_MouseScroll,
	Menu_OnResize,          Menu_ContextLost,     GenLevelScreen_ContextRecreated
};
void GenLevelScreen_Show(void) {
	static struct Widget* widgets[12];
	struct GenLevelScreen* s = &GenLevelScreen_Instance;

	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);

	s->VTABLE = &GenLevelScreen_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_MENU);
}


/*########################################################################################################################*
*----------------------------------------------------ClassicGenScreen-----------------------------------------------------*
*#########################################################################################################################*/
static struct ClassicGenScreen {
	MenuScreen_Layout
	struct ButtonWidget buttons[4];
} ClassicGenScreen_Instance;

static void ClassicGenScreen_Gen(int size) {
	RNGState rnd; Random_SeedFromCurrentTime(&rnd);
	Gen_Vanilla = true;
	Gen_Seed    = Random_Next(&rnd, Int32_MaxValue);

	GenLevelScreen_Begin(size, 64, size);
}

static void ClassicGenScreen_Small(void* a, void* b)  { ClassicGenScreen_Gen(128); }
static void ClassicGenScreen_Medium(void* a, void* b) { ClassicGenScreen_Gen(256); }
static void ClassicGenScreen_Huge(void* a, void* b)   { ClassicGenScreen_Gen(512); }

static void ClassicGenScreen_Make(struct ClassicGenScreen* s, int i, int y, const char* title, Widget_LeftClick onClick) {
	String text = String_FromReadonly(title);
	Menu_Button(s, i, &s->buttons[i], 400, &text, &s->titleFont, onClick,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, y);
}

static void ClassicGenScreen_ContextRecreated(void* screen) {
	struct ClassicGenScreen* s = (struct ClassicGenScreen*)screen;
	ClassicGenScreen_Make(s, 0, -100, "Small",  ClassicGenScreen_Small);
	ClassicGenScreen_Make(s, 1,  -50, "Normal", ClassicGenScreen_Medium);
	ClassicGenScreen_Make(s, 2,    0, "Huge",   ClassicGenScreen_Huge);

	Menu_Back(s, 3, &s->buttons[3], "Cancel", &s->titleFont, Menu_SwitchPause);
}

static struct ScreenVTABLE ClassicGenScreen_VTABLE = {
	MenuScreen_Init,    MenuScreen_Render,  MenuScreen_Free,
	MenuScreen_KeyDown, Menu_KeyUp,         Menu_KeyPress,
	Menu_MouseDown,     Menu_MouseUp,       Menu_MouseMove,  MenuScreen_MouseScroll,
	Menu_OnResize,      Menu_ContextLost,   ClassicGenScreen_ContextRecreated
};
void ClassicGenScreen_Show(void) {
	static struct Widget* widgets[4];
	struct ClassicGenScreen* s = &ClassicGenScreen_Instance;

	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);

	s->VTABLE = &ClassicGenScreen_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_MENU);
}


/*########################################################################################################################*
*----------------------------------------------------SaveLevelScreen------------------------------------------------------*
*#########################################################################################################################*/
static struct SaveLevelScreen {
	MenuScreen_Layout
	struct ButtonWidget buttons[3];
	struct MenuInputWidget input;
	struct TextWidget mcEdit, desc;
} SaveLevelScreen_Instance;

static void SaveLevelScreen_RemoveOverwrites(struct SaveLevelScreen* s) {
	struct ButtonWidget* btn;
	btn = &s->buttons[0];
	if (btn->optName) {
		btn->optName = NULL; 
		ButtonWidget_SetConst(btn, "Save", &s->titleFont);
	}

	btn = &s->buttons[1];
	if (btn->optName) {
		btn->optName = NULL;
		ButtonWidget_SetConst(btn, "Save schematic", &s->titleFont);
	}
}

static void SaveLevelScreen_MakeDesc(struct SaveLevelScreen* s, const String* text) {
	if (s->widgets[5]) { Elem_TryFree(s->widgets[5]); }

	Menu_Label(s, 5, &s->desc, text, &s->textFont,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 65);
}

static void SaveLevelScreen_SaveMap(struct SaveLevelScreen* s, const String* path) {
	static const String cw = String_FromConst(".cw");
	struct Stream stream, compStream;
	struct GZipState state;
	ReturnCode res;

	res = Stream_CreateFile(&stream, path);
	if (res) { Logger_Warn2(res, "creating", path); return; }
	GZip_MakeStream(&compStream, &state, &stream);

	if (String_CaselessEnds(path, &cw)) {
		res = Cw_Save(&compStream);
	} else {
		res = Schematic_Save(&compStream);
	}

	if (res) {
		stream.Close(&stream);
		Logger_Warn2(res, "encoding", path); return;
	}

	if ((res = compStream.Close(&compStream))) {
		stream.Close(&stream);
		Logger_Warn2(res, "closing", path); return;
	}

	res = stream.Close(&stream);
	if (res) { Logger_Warn2(res, "closing", path); return; }

	Chat_Add1("&eSaved map to: %s", path);
	PauseScreen_Show();
}

static void SaveLevelScreen_Save(void* screen, void* widget, const char* ext) {
	static const String fileMsg = String_FromConst("&ePlease enter a filename");
	String path; char pathBuffer[FILENAME_SIZE];

	struct SaveLevelScreen* s = (struct SaveLevelScreen*)screen;
	struct ButtonWidget* btn  = (struct ButtonWidget*)widget;
	String file = s->input.base.text;

	if (!file.length) {
		SaveLevelScreen_MakeDesc(s, &fileMsg); return;
	}
	String_InitArray(path, pathBuffer);
	String_Format2(&path, "maps/%s%c", &file, ext);

	if (File_Exists(&path) && !btn->optName) {
		ButtonWidget_SetConst(btn, "&cOverwrite existing?", &s->titleFont);
		btn->optName = "O";
	} else {
		SaveLevelScreen_RemoveOverwrites(s);
		SaveLevelScreen_SaveMap(s, &path);
	}
}
static void SaveLevelScreen_Classic(void* a, void* b)   { SaveLevelScreen_Save(a, b, ".cw"); }
static void SaveLevelScreen_Schematic(void* a, void* b) { SaveLevelScreen_Save(a, b, ".schematic"); }

static void SaveLevelScreen_Render(void* screen, double delta) {
	PackedCol grey = PACKEDCOL_CONST(150, 150, 150, 255);
	int x, y;
	MenuScreen_Render(screen, delta);

	x = Window_Width / 2; y = Window_Height / 2;
	Gfx_Draw2DFlat(x - 250, y + 90, 500, 2, grey);
}

static bool SaveLevelScreen_KeyPress(void* screen, char keyChar) {
	struct SaveLevelScreen* s = (struct SaveLevelScreen*)screen;
	SaveLevelScreen_RemoveOverwrites(s);
	InputWidget_Append(&s->input.base, keyChar);
	return true;
}

static bool SaveLevelScreen_KeyDown(void* screen, Key key) {
	struct SaveLevelScreen* s = (struct SaveLevelScreen*)screen;
	SaveLevelScreen_RemoveOverwrites(s);
	if (Elem_HandlesKeyDown(&s->input.base, key)) return true;
	return MenuScreen_KeyDown(s, key);
}

static bool SaveLevelScreen_KeyUp(void* screen, Key key) {
	struct SaveLevelScreen* s = (struct SaveLevelScreen*)screen;
	return Elem_HandlesKeyUp(&s->input.base, key);
}

static void SaveLevelScreen_ContextRecreated(void* screen) {
	static const String save   = String_FromConst("Save");
	static const String schem  = String_FromConst("Save schematic");
	static const String mcEdit = String_FromConst("&eCan be imported into MCEdit");

	struct SaveLevelScreen* s = (struct SaveLevelScreen*)screen;
	struct MenuInputDesc desc;
	MenuInput_Path(desc);
	
	Menu_Button(s, 0, &s->buttons[0], 300, &save,  &s->titleFont, SaveLevelScreen_Classic,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 20);
	Menu_Button(s, 1, &s->buttons[1], 200, &schem, &s->titleFont, SaveLevelScreen_Schematic,
		ANCHOR_CENTRE, ANCHOR_CENTRE, -150, 120);
	Menu_Label(s,  2, &s->mcEdit, &mcEdit,         &s->textFont,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 110, 120);

	Menu_Back(s,   3, &s->buttons[2], "Cancel",      &s->titleFont, Menu_SwitchPause);
	Menu_Input(s,  4, &s->input, 500, &String_Empty, &s->textFont,  &desc, 
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, -30);
	s->widgets[5] = NULL; /* description widget placeholder */
}

static struct ScreenVTABLE SaveLevelScreen_VTABLE = {
	MenuScreen_Init,         SaveLevelScreen_Render, MenuScreen_Free,
	SaveLevelScreen_KeyDown, SaveLevelScreen_KeyUp,  SaveLevelScreen_KeyPress,
	Menu_MouseDown,          Menu_MouseUp,           Menu_MouseMove,           MenuScreen_MouseScroll,
	Menu_OnResize,           Menu_ContextLost,       SaveLevelScreen_ContextRecreated
};
void SaveLevelScreen_Show(void) {
	static struct Widget* widgets[6];
	struct SaveLevelScreen* s = &SaveLevelScreen_Instance;
	
	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);

	s->VTABLE = &SaveLevelScreen_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_MENU);
}


/*########################################################################################################################*
*---------------------------------------------------TexturePackScreen-----------------------------------------------------*
*#########################################################################################################################*/
static void TexturePackScreen_EntryClick(void* screen, void* widget) {
	String path; char pathBuffer[FILENAME_SIZE];
	struct ListScreen* s = (struct ListScreen*)screen;
	String filename;
	
	filename = ListScreen_UNSAFE_GetCur(s, widget);
	String_InitArray(path, pathBuffer);
	String_Format1(&path, "texpacks/%s", &filename);
	if (!File_Exists(&path)) return;
	
	Game_SetDefaultTexturePack(&filename);
	World_TextureUrl.length = 0;
	TexturePack_ExtractCurrent(true);
}

static void TexturePackScreen_FilterFiles(const String* path, void* obj) {
	static const String zip = String_FromConst(".zip");
	String file = *path;
	if (!String_CaselessEnds(path, &zip)) return;

	Utils_UNSAFE_GetFilename(&file);
	StringsBuffer_Add((StringsBuffer*)obj, &file);
}

static void TexturePackScreen_LoadEntries(struct ListScreen* s) {
	static const String path = String_FromConst("texpacks");
	Directory_Enum(&path, &s->entries, TexturePackScreen_FilterFiles);
	ListScreen_Sort(s);
}

void TexturePackScreen_Show(void) {
	struct ListScreen* s = &ListScreen;
	s->titleText   = "Select a texture pack zip";
	s->LoadEntries = TexturePackScreen_LoadEntries;
	s->EntryClick  = TexturePackScreen_EntryClick;
	s->UpdateEntry = ListScreen_UpdateEntry;
	ListScreen_Show();
}


/*########################################################################################################################*
*----------------------------------------------------FontListScreen-------------------------------------------------------*
*#########################################################################################################################*/
static void FontListScreen_EntryClick(void* screen, void* widget) {
	struct ListScreen* s = (struct ListScreen*)screen;
	String fontName = ListScreen_UNSAFE_GetCur(s, widget);
	int cur = s->currentIndex;

	if (String_CaselessEqualsConst(&fontName, LIST_SCREEN_EMPTY)) return;
	String_Copy(&Drawer2D_FontName, &fontName);
	Options_Set(OPT_FONT_NAME,      &fontName);

	/* changing font recreates list menu */
	Menu_HandleFontChange((struct Screen*)s);
	ListScreen_SetCurrentIndex(s, cur);
}

static void FontListScreen_UpdateEntry(struct ListScreen* s, struct ButtonWidget* button, const String* text) {
	FontDesc font;
	ReturnCode res;

	if (String_CaselessEqualsConst(text, LIST_SCREEN_EMPTY)) {
		ButtonWidget_Set(button, text, &s->font); return;
	}

	res = Font_Make(&font, text, 16, FONT_STYLE_NORMAL);
	if (!res) {
		ButtonWidget_Set(button, text, &font);
	} else {
		Logger_SimpleWarn2(res, "making font", text);
		ButtonWidget_Set(button, text, &s->font);
	}
	Font_Free(&font);
}

static void FontListScreen_LoadEntries(struct ListScreen* s) {
	Font_GetNames(&s->entries);
	ListScreen_Sort(s);
	ListScreen_Select(s, &Drawer2D_FontName);
}

void FontListScreen_Show(void) {
	struct ListScreen* s = &ListScreen;
	s->titleText   = "Select a font";
	s->LoadEntries = FontListScreen_LoadEntries;
	s->EntryClick  = FontListScreen_EntryClick;
	s->UpdateEntry = FontListScreen_UpdateEntry;
	ListScreen_Show();
}


/*########################################################################################################################*
*---------------------------------------------------HotkeyListScreen------------------------------------------------------*
*#########################################################################################################################*/
/* TODO: Hotkey added event for CPE */
static void HotkeyListScreen_EntryClick(void* screen, void* widget) {
	static const String ctrl  = String_FromConst("Ctrl");
	static const String shift = String_FromConst("Shift");
	static const String alt   = String_FromConst("Alt");

	struct ListScreen* s = (struct ListScreen*)screen;
	struct HotkeyData h, original = { 0 };
	String text, key, value;
	Key trigger;
	int i, flags = 0;

	text = ListScreen_UNSAFE_GetCur(s, widget);
	if (String_CaselessEqualsConst(&text, LIST_SCREEN_EMPTY)) {
		EditHotkeyScreen_Show(original); 
		return;
	}

	String_UNSAFE_Separate(&text, '+', &key, &value);
	if (String_ContainsString(&value, &ctrl))  flags |= HOTKEY_MOD_CTRL;
	if (String_ContainsString(&value, &shift)) flags |= HOTKEY_MOD_SHIFT;
	if (String_ContainsString(&value, &alt))   flags |= HOTKEY_MOD_ALT;

	trigger = Utils_ParseEnum(&key, KEY_NONE, Key_Names, KEY_COUNT);
	for (i = 0; i < HotkeysText.count; i++) {
		h = HotkeysList[i];
		if (h.Trigger == trigger && h.Flags == flags) { original = h; break; }
	}

	EditHotkeyScreen_Show(original);
}

static void HotkeyListScreen_MakeFlags(int flags, String* str) {
	if (flags & HOTKEY_MOD_CTRL)  String_AppendConst(str, " Ctrl");
	if (flags & HOTKEY_MOD_SHIFT) String_AppendConst(str, " Shift");
	if (flags & HOTKEY_MOD_ALT)   String_AppendConst(str, " Alt");
}

static void HotkeyListScreen_LoadEntries(struct ListScreen* s) {
	static const String empty = String_FromConst(LIST_SCREEN_EMPTY);
	String text; char textBuffer[STRING_SIZE];
	struct HotkeyData hKey;
	int i;
	String_InitArray(text, textBuffer);

	for (i = 0; i < HotkeysText.count; i++) {
		hKey = HotkeysList[i];
		text.length = 0;
		String_AppendConst(&text, Key_Names[hKey.Trigger]);

		if (hKey.Flags) {
			String_AppendConst(&text, " +");
			HotkeyListScreen_MakeFlags(hKey.Flags, &text);
		}
		StringsBuffer_Add(&s->entries, &text);
	}

	for (i = 0; i < LIST_SCREEN_ITEMS; i++) {
		StringsBuffer_Add(&s->entries, &empty);
	}
}

void HotkeyListScreen_Show(void) {
	struct ListScreen* s = &ListScreen;
	s->titleText   = "Modify hotkeys";
	s->LoadEntries = HotkeyListScreen_LoadEntries;
	s->EntryClick  = HotkeyListScreen_EntryClick;
	s->UpdateEntry = ListScreen_UpdateEntry;
	ListScreen_Show();
}


/*########################################################################################################################*
*----------------------------------------------------LoadLevelScreen------------------------------------------------------*
*#########################################################################################################################*/
static void LoadLevelScreen_EntryClick(void* screen, void* widget) {
	String path; char pathBuffer[FILENAME_SIZE];
	struct ListScreen* s = (struct ListScreen*)screen;
	String filename;

	filename = ListScreen_UNSAFE_GetCur(s, widget);
	String_InitArray(path, pathBuffer);
	String_Format1(&path, "maps/%s", &filename);

	if (!File_Exists(&path)) return;
	Map_LoadFrom(&path);
}

static void LoadLevelScreen_FilterFiles(const String* path, void* obj) {
	IMapImporter importer = Map_FindImporter(path);
	String file = *path;
	if (!importer) return;

	Utils_UNSAFE_GetFilename(&file);
	StringsBuffer_Add((StringsBuffer*)obj, &file);
}

static void LoadLevelScreen_LoadEntries(struct ListScreen* s) {
	static const String path = String_FromConst("maps");
	Directory_Enum(&path, &s->entries, LoadLevelScreen_FilterFiles);
	ListScreen_Sort(s);
}

void LoadLevelScreen_Show(void) {
	struct ListScreen* s = &ListScreen;
	s->titleText   = "Select a level";
	s->LoadEntries = LoadLevelScreen_LoadEntries;
	s->EntryClick  = LoadLevelScreen_EntryClick;
	s->UpdateEntry = ListScreen_UpdateEntry;
	ListScreen_Show();
}


/*########################################################################################################################*
*---------------------------------------------------KeyBindingsScreen-----------------------------------------------------*
*#########################################################################################################################*/
static struct KeyBindingsScreen {
	MenuScreen_Layout	
	int curI, bindsCount;
	const char** descs;
	uint8_t* binds;
	Widget_LeftClick leftPage, rightPage;
	struct TextWidget title;
	struct ButtonWidget back, left, right;
	struct ButtonWidget buttons[12];
} KeyBindingsScreen_Instance;

static void KeyBindingsScreen_GetText(struct KeyBindingsScreen* s, int i, String* text) {
	Key key = KeyBinds[s->binds[i]];
	String_Format2(text, "%c: %c", s->descs[i], Key_Names[key]);
}

static void KeyBindingsScreen_OnBindingClick(void* screen, void* widget) {
	String text; char textBuffer[STRING_SIZE];
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	struct ButtonWidget* btn    = (struct ButtonWidget*)widget;
	struct ButtonWidget* cur;

	String_InitArray(text, textBuffer);
	/* previously selected a different button for binding */
	if (s->curI >= 0) {
		KeyBindingsScreen_GetText(s, s->curI, &text);
		cur = (struct ButtonWidget*)s->widgets[s->curI];
		ButtonWidget_Set(cur, &text, &s->titleFont);
	}
	s->curI     = Menu_Index(s, btn);
	s->closable = false;

	text.length = 0;
	String_AppendConst(&text, "> ");
	KeyBindingsScreen_GetText(s, s->curI, &text);
	String_AppendConst(&text, " <");
	ButtonWidget_Set(btn, &text, &s->titleFont);
}

static int KeyBindingsScreen_MakeWidgets(struct KeyBindingsScreen* s, int y, int arrowsY, int leftLength, const char* title, int btnWidth) {
	static const String lArrow = String_FromConst("<");
	static const String rArrow = String_FromConst(">");
	String text; char textBuffer[STRING_SIZE];
	String titleText;
	Widget_LeftClick backClick;
	int origin, xOffset;
	int i, xDir;

	origin  = y;
	xOffset = btnWidth / 2 + 5;
	s->curI = -1;
	String_InitArray(text, textBuffer);

	for (i = 0; i < s->bindsCount; i++) {
		if (i == leftLength) y = origin; /* reset y for next column */
		xDir = leftLength == -1 ? 0 : (i < leftLength ? -1 : 1);

		text.length = 0;
		KeyBindingsScreen_GetText(s, i, &text);

		Menu_Button(s, i, &s->buttons[i], btnWidth, &text, &s->titleFont, KeyBindingsScreen_OnBindingClick,
			ANCHOR_CENTRE, ANCHOR_CENTRE, xDir * xOffset, y);
		y += 50; /* distance between buttons */
	}

	titleText = String_FromReadonly(title);
	Menu_Label(s, i, &s->title, &titleText, &s->titleFont,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, -180); i++;

	backClick = Gui_ClassicMenu ? Menu_SwitchClassicOptions : Menu_SwitchOptions;
	Menu_Back(s, i, &s->back, "Done", &s->titleFont, backClick); i++;
	if (!s->leftPage && !s->rightPage) return i;
	
	Menu_Button(s, i, &s->left,  40, &lArrow, &s->titleFont, s->leftPage,
		ANCHOR_CENTRE, ANCHOR_CENTRE, -btnWidth - 35, arrowsY); i++;
	Menu_Button(s, i, &s->right, 40, &rArrow, &s->titleFont, s->rightPage,
		ANCHOR_CENTRE, ANCHOR_CENTRE,  btnWidth + 35, arrowsY); i++;

	s->left.disabled  = !s->leftPage;
	s->right.disabled = !s->rightPage;
	return i;
}

static bool KeyBindingsScreen_KeyDown(void* screen, Key key) {
	String text; char textBuffer[STRING_SIZE];
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	struct ButtonWidget* cur;
	KeyBind bind;

	if (s->curI == -1) return MenuScreen_KeyDown(s, key);
	bind = s->binds[s->curI];
	if (key == KEY_ESCAPE) key = KeyBind_Defaults[bind];

	KeyBind_Set(bind, key);
	String_InitArray(text, textBuffer);
	KeyBindingsScreen_GetText(s, s->curI, &text);

	cur = (struct ButtonWidget*)s->widgets[s->curI];
	ButtonWidget_Set(cur, &text, &s->titleFont);
	s->curI     = -1;
	s->closable = true;
	return true;
}

static bool KeyBindingsScreen_MouseDown(void* screen, int x, int y, MouseButton btn) {
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	int i;

	if (btn != MOUSE_RIGHT) { return Menu_MouseDown(s, x, y, btn); }
	i = Menu_DoMouseDown(s, x, y, btn);
	if (i == -1) return false;

	/* Reset a key binding by right clicking */
	if ((s->curI == -1 || s->curI == i) && i < s->bindsCount) {
		s->curI = i;
		Elem_HandlesKeyDown(s, KeyBind_Defaults[s->binds[i]]);
	}
	return true;
}

static struct ScreenVTABLE KeyBindingsScreen_VTABLE = {
	MenuScreen_Init,             MenuScreen_Render,  MenuScreen_Free,
	KeyBindingsScreen_KeyDown,   Menu_KeyUp,         Menu_KeyPress,
	KeyBindingsScreen_MouseDown, Menu_MouseUp,       Menu_MouseMove,  MenuScreen_MouseScroll,
	Menu_OnResize,               Menu_ContextLost,   NULL
};
static struct KeyBindingsScreen* KeyBindingsScreen_Make(int bindsCount, uint8_t* binds, const char** descs, Event_Void_Callback contextRecreated) {
	static struct Widget* widgets[12 + 4]; /* 12 buttons + </> buttons + 2 widgets used by MouseKeyBindings */
	struct KeyBindingsScreen* s = &KeyBindingsScreen_Instance;

	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = bindsCount + 4;

	s->VTABLE = &KeyBindingsScreen_VTABLE;
	s->VTABLE->ContextRecreated = contextRecreated;

	s->bindsCount = bindsCount;
	s->binds      = binds;
	s->descs      = descs;

	s->curI      = -1;
	s->leftPage  = NULL;
	s->rightPage = NULL;
	return s;
}


/*########################################################################################################################*
*-----------------------------------------------ClassicKeyBindingsScreen--------------------------------------------------*
*#########################################################################################################################*/
static void ClassicKeyBindingsScreen_ContextRecreated(void* screen) {
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	if (Game_ClassicHacks) {
		KeyBindingsScreen_MakeWidgets(s, -140, -40, 5, "Normal controls", 260);
	} else {
		KeyBindingsScreen_MakeWidgets(s, -140, -40, 5, "Controls", 300);
	}
}

struct Screen* ClassicKeyBindingsScreen_MakeInstance(void) {
	static uint8_t binds[10] = { KEYBIND_FORWARD, KEYBIND_BACK, KEYBIND_JUMP, KEYBIND_CHAT, KEYBIND_SET_SPAWN, KEYBIND_LEFT, KEYBIND_RIGHT, KEYBIND_INVENTORY, KEYBIND_FOG, KEYBIND_RESPAWN };
	static const char* descs[10] = { "Forward", "Back", "Jump", "Chat", "Save loc", "Left", "Right", "Build", "Toggle fog", "Load loc" };

	struct KeyBindingsScreen* s = KeyBindingsScreen_Make(Array_Elems(binds), binds, descs, ClassicKeyBindingsScreen_ContextRecreated);
	if (Game_ClassicHacks) s->rightPage = Menu_SwitchKeysClassicHacks;
	return (struct Screen*)s;
}


/*########################################################################################################################*
*--------------------------------------------ClassicHacksKeyBindingsScreen------------------------------------------------*
*#########################################################################################################################*/
static void ClassicHacksKeyBindingsScreen_ContextRecreated(void* screen) {
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	KeyBindingsScreen_MakeWidgets(s, -90, -40, 3, "Hacks controls", 260);
}

struct Screen* ClassicHacksKeyBindingsScreen_MakeInstance(void) {
	static uint8_t binds[6] = { KEYBIND_SPEED, KEYBIND_NOCLIP, KEYBIND_HALF_SPEED, KEYBIND_FLY, KEYBIND_FLY_UP, KEYBIND_FLY_DOWN };
	static const char* descs[6] = { "Speed", "Noclip", "Half speed", "Fly", "Fly up", "Fly down" };

	struct KeyBindingsScreen* s = KeyBindingsScreen_Make(Array_Elems(binds), binds, descs, ClassicHacksKeyBindingsScreen_ContextRecreated);
	s->leftPage = Menu_SwitchKeysClassic;
	return (struct Screen*)s;
}


/*########################################################################################################################*
*-----------------------------------------------NormalKeyBindingsScreen---------------------------------------------------*
*#########################################################################################################################*/
static void NormalKeyBindingsScreen_ContextRecreated(void* screen) {
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	KeyBindingsScreen_MakeWidgets(s, -140, 10, 6, "Normal controls", 260);
}

struct Screen* NormalKeyBindingsScreen_MakeInstance(void) {
	static uint8_t binds[12] = { KEYBIND_FORWARD, KEYBIND_BACK, KEYBIND_JUMP, KEYBIND_CHAT, KEYBIND_SET_SPAWN, KEYBIND_PLAYER_LIST, KEYBIND_LEFT, KEYBIND_RIGHT, KEYBIND_INVENTORY, KEYBIND_FOG, KEYBIND_RESPAWN, KEYBIND_SEND_CHAT };
	static const char* descs[12] = { "Forward", "Back", "Jump", "Chat", "Set spawn", "Player list", "Left", "Right", "Inventory", "Toggle fog", "Respawn", "Send chat" };

	struct KeyBindingsScreen* s = KeyBindingsScreen_Make(Array_Elems(binds), binds, descs, NormalKeyBindingsScreen_ContextRecreated);
	s->rightPage = Menu_SwitchKeysHacks;
	return (struct Screen*)s;
}


/*########################################################################################################################*
*------------------------------------------------HacksKeyBindingsScreen---------------------------------------------------*
*#########################################################################################################################*/
static void HacksKeyBindingsScreen_ContextRecreated(void* screen) {
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	KeyBindingsScreen_MakeWidgets(s, -40, 10, 4, "Hacks controls", 260);
}

struct Screen* HacksKeyBindingsScreen_MakeInstance(void) {
	static uint8_t binds[8] = { KEYBIND_SPEED, KEYBIND_NOCLIP, KEYBIND_HALF_SPEED, KEYBIND_ZOOM_SCROLL, KEYBIND_FLY, KEYBIND_FLY_UP, KEYBIND_FLY_DOWN, KEYBIND_THIRD_PERSON };
	static const char* descs[8] = { "Speed", "Noclip", "Half speed", "Scroll zoom", "Fly", "Fly up", "Fly down", "Third person" };

	struct KeyBindingsScreen* s = KeyBindingsScreen_Make(Array_Elems(binds), binds, descs, HacksKeyBindingsScreen_ContextRecreated);
	s->leftPage  = Menu_SwitchKeysNormal;
	s->rightPage = Menu_SwitchKeysOther;
	return (struct Screen*)s;
}


/*########################################################################################################################*
*------------------------------------------------OtherKeyBindingsScreen---------------------------------------------------*
*#########################################################################################################################*/
static void OtherKeyBindingsScreen_ContextRecreated(void* screen) {
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	KeyBindingsScreen_MakeWidgets(s, -140, 10, 6, "Other controls", 260);
}

struct Screen* OtherKeyBindingsScreen_MakeInstance(void) {
	static uint8_t binds[12] = { KEYBIND_EXT_INPUT, KEYBIND_HIDE_FPS, KEYBIND_HIDE_GUI, KEYBIND_HOTBAR_SWITCH, KEYBIND_DROP_BLOCK,KEYBIND_SCREENSHOT, KEYBIND_FULLSCREEN, KEYBIND_AXIS_LINES, KEYBIND_AUTOROTATE, KEYBIND_SMOOTH_CAMERA, KEYBIND_IDOVERLAY, KEYBIND_BREAK_LIQUIDS };
	static const char* descs[12] = { "Show ext input", "Hide FPS", "Hide gui", "Hotbar switching", "Drop block", "Screenshot", "Fullscreen", "Show axis lines", "Auto-rotate", "Smooth camera", "ID overlay", "Breakable liquids" };

	struct KeyBindingsScreen* s = KeyBindingsScreen_Make(Array_Elems(binds), binds, descs, OtherKeyBindingsScreen_ContextRecreated);
	s->leftPage  = Menu_SwitchKeysHacks;
	s->rightPage = Menu_SwitchKeysMouse;
	return (struct Screen*)s;
}


/*########################################################################################################################*
*------------------------------------------------MouseKeyBindingsScreen---------------------------------------------------*
*#########################################################################################################################*/
static void MouseKeyBindingsScreen_ContextRecreated(void* screen) {
	static const String msg = String_FromConst("&eRight click to remove the key binding");
	struct KeyBindingsScreen* s = (struct KeyBindingsScreen*)screen;
	static struct TextWidget text;

	int i = KeyBindingsScreen_MakeWidgets(s, -40, 10, -1, "Mouse key bindings", 260);
	Menu_Label(s, i, &text, &msg, &s->textFont,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 100);
}

struct Screen* MouseKeyBindingsScreen_MakeInstance(void) {
	static uint8_t binds[3] = { KEYBIND_MOUSE_LEFT, KEYBIND_MOUSE_MIDDLE, KEYBIND_MOUSE_RIGHT };
	static const char* descs[3] = { "Left", "Middle", "Right" };

	struct KeyBindingsScreen* s = KeyBindingsScreen_Make(Array_Elems(binds), binds, descs, MouseKeyBindingsScreen_ContextRecreated);
	s->leftPage = Menu_SwitchKeysOther;
	s->numWidgets++; /* Extra text widget for 'right click' message */
	return (struct Screen*)s;
}


/*########################################################################################################################*
*--------------------------------------------------MenuOptionsScreen------------------------------------------------------*
*#########################################################################################################################*/
#define MENUOPTIONS_MAX_DESC 5

static struct MenuOptionsScreen {
	MenuScreen_Layout
	struct MenuInputDesc* descs;
	const char** descriptions;
	int activeI, selectedI, descriptionsCount;
	struct ButtonWidget ok, Default;
	struct MenuInputWidget input;
	struct TextGroupWidget extHelp;
	struct Texture extHelpTextures[MENUOPTIONS_MAX_DESC];
	struct ButtonWidget buttons[11]; /* max buttons used is 11 */
} MenuOptionsScreen_Instance;

static void Menu_GetBool(String* raw, bool v) {
	String_AppendConst(raw, v ? "ON" : "OFF");
}
static bool Menu_SetBool(const String* raw, const char* key) {
	bool isOn = String_CaselessEqualsConst(raw, "ON");
	Options_SetBool(key, isOn); 
	return isOn;
}

static void MenuOptionsScreen_GetFPS(String* raw) {
	String_AppendConst(raw, FpsLimit_Names[Game_FpsLimit]);
}
static void MenuOptionsScreen_SetFPS(const String* v) {
	int method = Utils_ParseEnum(v, FPS_LIMIT_VSYNC, FpsLimit_Names, Array_Elems(FpsLimit_Names));
	Options_Set(OPT_FPS_LIMIT, v);
	Game_SetFpsLimit(method);
}

static void MenuOptionsScreen_Set(struct MenuOptionsScreen* s, int i, const String* text) {
	String title; char titleBuffer[STRING_SIZE];

	s->buttons[i].SetValue(text);
	String_InitArray(title, titleBuffer);

	/* need to get btn again here (e.g. changing FPS invalidates all widgets) */
	String_AppendConst(&title, s->buttons[i].optName);
	String_AppendConst(&title, ": ");
	s->buttons[i].GetValue(&title);
	ButtonWidget_Set(&s->buttons[i], &title, &s->titleFont);
}

static void MenuOptionsScreen_FreeExtHelp(struct MenuOptionsScreen* s) {
	if (!s->extHelp.lines) return;
	Elem_TryFree(&s->extHelp);
	s->extHelp.lines = 0;
}

static void MenuOptionsScreen_RepositionExtHelp(struct MenuOptionsScreen* s) {
	s->extHelp.xOffset = Window_Width  / 2 - s->extHelp.width / 2;
	s->extHelp.yOffset = Window_Height / 2 + 100;
	Widget_Reposition(&s->extHelp);
}

static String MenuOptionsScreen_GetDesc(void* obj, int i) {
	const char* desc = (const char*)obj;
	String descRaw, descLines[5];

	descRaw = String_FromReadonly(desc);
	String_UNSAFE_Split(&descRaw, '\n', descLines, Array_Elems(descLines));
	return descLines[i];
}

static void MenuOptionsScreen_SelectExtHelp(struct MenuOptionsScreen* s, int idx) {
	const char* desc;
	String descRaw, descLines[5];
	int count;

	MenuOptionsScreen_FreeExtHelp(s);
	if (!s->descriptions || s->activeI >= 0) return;
	desc = s->descriptions[idx];
	if (!desc) return;

	descRaw = String_FromReadonly(desc);
	count   = String_UNSAFE_Split(&descRaw, '\n', descLines, Array_Elems(descLines));

	TextGroupWidget_Create(&s->extHelp, count, &s->textFont, s->extHelpTextures, MenuOptionsScreen_GetDesc);
	Widget_SetLocation((struct Widget*)&s->extHelp, ANCHOR_MIN, ANCHOR_MIN, 0, 0);
	Elem_Init(&s->extHelp);
	
	s->extHelp.getLineObj = desc;
	TextGroupWidget_RedrawAll(&s->extHelp);
	MenuOptionsScreen_RepositionExtHelp(s);
}

static void MenuOptionsScreen_FreeInput(struct MenuOptionsScreen* s) {
	int i;
	if (s->activeI == -1) return;
	
	for (i = s->numWidgets - 3; i < s->numWidgets; i++) {
		if (!s->widgets[i]) continue;
		Elem_TryFree(s->widgets[i]);
		s->widgets[i] = NULL;
	}
}

static void MenuOptionsScreen_EnterInput(struct MenuOptionsScreen* s) {
	struct MenuInputDesc* desc = &s->input.desc;
	String text = s->input.base.text;

	if (desc->VTABLE->IsValidValue(desc, &text)) {
		MenuOptionsScreen_Set(s, s->activeI, &text);
	}

	MenuOptionsScreen_SelectExtHelp(s, s->activeI);
	MenuOptionsScreen_FreeInput(s);
	s->activeI = -1;
}

static void MenuOptionsScreen_Init(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	MenuScreen_Init(s);
	s->selectedI = -1;
}
	
#define EXTHELP_PAD 5 /* padding around extended help box */
static void MenuOptionsScreen_Render(void* screen, double delta) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct TextGroupWidget* w;
	PackedCol tableCol = PACKEDCOL_CONST(20, 20, 20, 200);

	MenuScreen_Render(s, delta);
	if (!s->extHelp.lines) return;

	w = &s->extHelp;
	Gfx_Draw2DFlat(w->x - EXTHELP_PAD, w->y - EXTHELP_PAD, 
		w->width + EXTHELP_PAD * 2, w->height + EXTHELP_PAD * 2, tableCol);

	Gfx_SetTexturing(true);
	Elem_Render(&s->extHelp, delta);
	Gfx_SetTexturing(false);
}

static void MenuOptionsScreen_OnResize(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	Menu_OnResize(s);
	if (!s->extHelp.lines) return;
	MenuOptionsScreen_RepositionExtHelp(s);
}

static void MenuOptionsScreen_ContextLost(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	Menu_ContextLost(s);
	s->activeI = -1;
	MenuOptionsScreen_FreeExtHelp(s);
}

static bool MenuOptionsScreen_KeyPress(void* screen, char keyChar) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	if (s->activeI >= 0) InputWidget_Append(&s->input.base, keyChar);
	return true;
}

static bool MenuOptionsScreen_KeyDown(void* screen, Key key) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	if (s->activeI >= 0) {
		if (Elem_HandlesKeyDown(&s->input.base, key)) return true;

		if (key == KEY_ENTER || key == KEY_KP_ENTER) {
			MenuOptionsScreen_EnterInput(s); return true;
		}
	}
	return MenuScreen_KeyDown(s, key);
}

static bool MenuOptionsScreen_KeyUp(void* screen, Key key) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	if (s->activeI == -1) return true;
	return Elem_HandlesKeyUp(&s->input.base, key);
}

static bool MenuOptionsScreen_MouseMove(void* screen, int x, int y) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	int i = Menu_DoMouseMove(s, x, y);
	if (i == -1 || i == s->selectedI) return true;
	if (!s->descriptions || i >= s->descriptionsCount) return true;

	s->selectedI = i;
	if (s->activeI == -1) MenuOptionsScreen_SelectExtHelp(s, i);
	return true;
}

static void MenuOptionsScreen_MakeButtons(struct MenuOptionsScreen* s, const struct MenuOptionDesc* btns, int count) {
	String title; char titleBuffer[STRING_SIZE];
	struct ButtonWidget* btn;
	int i;
	
	for (i = 0; i < count; i++) {
		String_InitArray(title, titleBuffer);
		String_AppendConst(&title, btns[i].name);

		if (btns[i].GetValue) {
			String_AppendConst(&title, ": ");
			btns[i].GetValue(&title);
		}

		btn = &s->buttons[i];
		Menu_Button(s, i, btn, 300, &title, &s->titleFont, btns[i].OnClick,
			ANCHOR_CENTRE, ANCHOR_CENTRE, btns[i].dir * 160, btns[i].y);

		btn->optName  = btns[i].name;
		btn->GetValue = btns[i].GetValue;
		btn->SetValue = btns[i].SetValue;
	}
}

static void MenuOptionsScreen_OK(void* screen, void* widget) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	MenuOptionsScreen_EnterInput(s);
}

static void MenuOptionsScreen_Default(void* screen, void* widget) {
	String value; char valueBuffer[STRING_SIZE];
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct MenuInputDesc* desc;

	desc = &s->descs[s->activeI];
	String_InitArray(value, valueBuffer);
	desc->VTABLE->GetDefault(desc, &value);

	InputWidget_Clear(&s->input.base);
	InputWidget_AppendString(&s->input.base, &value);
}

static void MenuOptionsScreen_Bool(void* screen, void* widget) {
	String value; char valueBuffer[STRING_SIZE];
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct ButtonWidget* btn    = (struct ButtonWidget*)widget;
	int index;
	bool isOn;

	index = Menu_Index(s, btn);
	MenuOptionsScreen_SelectExtHelp(s, index);
	String_InitArray(value, valueBuffer);
	btn->GetValue(&value);

	isOn  = String_CaselessEqualsConst(&value, "ON");
	value = String_FromReadonly(isOn ? "OFF" : "ON");
	MenuOptionsScreen_Set(s, index, &value);
}

static void MenuOptionsScreen_Enum(void* screen, void* widget) {
	String value; char valueBuffer[STRING_SIZE];
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct ButtonWidget* btn    = (struct ButtonWidget*)widget;
	int index;
	struct MenuInputDesc* desc;
	const char** names;
	int raw, count;
	
	index = Menu_Index(s, btn);
	MenuOptionsScreen_SelectExtHelp(s, index);
	String_InitArray(value, valueBuffer);
	btn->GetValue(&value);

	desc  = &s->descs[index];
	names = desc->meta.e.Names;
	count = desc->meta.e.Count;	

	raw   = (Utils_ParseEnum(&value, 0, names, count) + 1) % count;
	value = String_FromReadonly(names[raw]);
	MenuOptionsScreen_Set(s, index, &value);
}

static void MenuOptionsScreen_Input(void* screen, void* widget) {
	static const String okay = String_FromConst("OK");
	static const String def  = String_FromConst("Default value");
	String value; char valueBuffer[STRING_SIZE];

	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct ButtonWidget* btn    = (struct ButtonWidget*)widget;
	int i;

	s->activeI = Menu_Index(s, btn);
	MenuOptionsScreen_FreeExtHelp(s);
	MenuOptionsScreen_FreeInput(s);

	String_InitArray(value, valueBuffer);
	btn->GetValue(&value);
	i = s->numWidgets;

	Menu_Input(s,  i - 1, &s->input,   400, &value, &s->textFont,  &s->descs[s->activeI],
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 110);
	Menu_Button(s, i - 2, &s->ok,       40, &okay,  &s->titleFont, MenuOptionsScreen_OK,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 240, 110);
	Menu_Button(s, i - 3, &s->Default, 200, &def,   &s->titleFont, MenuOptionsScreen_Default,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 150);
}

static struct ScreenVTABLE MenuOptionsScreen_VTABLE = {
	MenuOptionsScreen_Init,     MenuOptionsScreen_Render, MenuScreen_Free,
	MenuOptionsScreen_KeyDown,  MenuOptionsScreen_KeyUp,  MenuOptionsScreen_KeyPress,
	Menu_MouseDown,             Menu_MouseUp,             MenuOptionsScreen_MouseMove, MenuScreen_MouseScroll,
	MenuOptionsScreen_OnResize, MenuOptionsScreen_ContextLost, NULL
};
struct Screen* MenuOptionsScreen_MakeInstance(int count, Event_Void_Callback contextRecreated, struct MenuInputDesc* descs, const char** descriptions, int descsCount) {
	static struct Widget* widgets[11 + 3];  /* max buttons + 3 widgets for input */

	struct MenuOptionsScreen* s = &MenuOptionsScreen_Instance;
	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = count;

	s->extHelp.lines = 0;
	s->VTABLE = &MenuOptionsScreen_VTABLE;
	s->VTABLE->ContextLost      = MenuOptionsScreen_ContextLost;
	s->VTABLE->ContextRecreated = contextRecreated;

	s->descs             = descs;
	s->descriptions      = descriptions;
	s->descriptionsCount = descsCount;

	s->activeI   = -1;
	s->selectedI = -1;
	return (struct Screen*)s;
}


/*########################################################################################################################*
*---------------------------------------------------ClassicOptionsScreen--------------------------------------------------*
*#########################################################################################################################*/
enum ViewDist { VIEW_TINY, VIEW_SHORT, VIEW_NORMAL, VIEW_FAR, VIEW_COUNT };
const char* ViewDist_Names[VIEW_COUNT] = { "TINY", "SHORT", "NORMAL", "FAR" };

static void ClassicOptionsScreen_GetMusic(String* v) { Menu_GetBool(v, Audio_MusicVolume > 0); }
static void ClassicOptionsScreen_SetMusic(const String* v) {
	Audio_SetMusic(String_CaselessEqualsConst(v, "ON") ? 100 : 0);
	Options_SetInt(OPT_MUSIC_VOLUME, Audio_MusicVolume);
}

static void ClassicOptionsScreen_GetInvert(String* v) { Menu_GetBool(v, Camera.Invert); }
static void ClassicOptionsScreen_SetInvert(const String* v) { Camera.Invert = Menu_SetBool(v, OPT_INVERT_MOUSE); }

static void ClassicOptionsScreen_GetViewDist(String* v) {
	if (Game_ViewDistance >= 512) {
		String_AppendConst(v, ViewDist_Names[VIEW_FAR]);
	} else if (Game_ViewDistance >= 128) {
		String_AppendConst(v, ViewDist_Names[VIEW_NORMAL]);
	} else if (Game_ViewDistance >= 32) {
		String_AppendConst(v, ViewDist_Names[VIEW_SHORT]);
	} else {
		String_AppendConst(v, ViewDist_Names[VIEW_TINY]);
	}
}
static void ClassicOptionsScreen_SetViewDist(const String* v) {
	int raw  = Utils_ParseEnum(v, 0, ViewDist_Names, VIEW_COUNT);
	int dist = raw == VIEW_FAR ? 512 : (raw == VIEW_NORMAL ? 128 : (raw == VIEW_SHORT ? 32 : 8));
	Game_UserSetViewDistance(dist);
}

static void ClassicOptionsScreen_GetPhysics(String* v) { Menu_GetBool(v, Physics.Enabled); }
static void ClassicOptionsScreen_SetPhysics(const String* v) {
	Physics_SetEnabled(Menu_SetBool(v, OPT_BLOCK_PHYSICS));
}

static void ClassicOptionsScreen_GetSounds(String* v) { Menu_GetBool(v, Audio_SoundsVolume > 0); }
static void ClassicOptionsScreen_SetSounds(const String* v) {
	Audio_SetSounds(String_CaselessEqualsConst(v, "ON") ? 100 : 0);
	Options_SetInt(OPT_SOUND_VOLUME, Audio_SoundsVolume);
}

static void ClassicOptionsScreen_GetShowFPS(String* v) { Menu_GetBool(v, Gui_ShowFPS); }
static void ClassicOptionsScreen_SetShowFPS(const String* v) { Gui_ShowFPS = Menu_SetBool(v, OPT_SHOW_FPS); }

static void ClassicOptionsScreen_GetViewBob(String* v) { Menu_GetBool(v, Game_ViewBobbing); }
static void ClassicOptionsScreen_SetViewBob(const String* v) { Game_ViewBobbing = Menu_SetBool(v, OPT_VIEW_BOBBING); }

static void ClassicOptionsScreen_GetHacks(String* v) { Menu_GetBool(v, LocalPlayer_Instance.Hacks.Enabled); }
static void ClassicOptionsScreen_SetHacks(const String* v) {
	LocalPlayer_Instance.Hacks.Enabled = Menu_SetBool(v, OPT_HACKS_ENABLED);
	HacksComp_Update(&LocalPlayer_Instance.Hacks);
}

static void ClassicOptionsScreen_ContextRecreated(void* screen) {
	static const struct MenuOptionDesc buttons[9] = {
		{ -1, -150, "Music",           MenuOptionsScreen_Bool,
			ClassicOptionsScreen_GetMusic,    ClassicOptionsScreen_SetMusic },
		{ -1, -100, "Invert mouse",    MenuOptionsScreen_Bool,
			ClassicOptionsScreen_GetInvert,   ClassicOptionsScreen_SetInvert },
		{ -1,  -50, "Render distance", MenuOptionsScreen_Enum,
			ClassicOptionsScreen_GetViewDist, ClassicOptionsScreen_SetViewDist },
		{ -1,    0, "Block physics",   MenuOptionsScreen_Bool,
			ClassicOptionsScreen_GetPhysics,  ClassicOptionsScreen_SetPhysics },

		{ 1, -150, "Sound",         MenuOptionsScreen_Bool,
			ClassicOptionsScreen_GetSounds,  ClassicOptionsScreen_SetSounds },
		{ 1, -100, "Show FPS",      MenuOptionsScreen_Bool,
			ClassicOptionsScreen_GetShowFPS, ClassicOptionsScreen_SetShowFPS },
		{ 1,  -50, "View bobbing",  MenuOptionsScreen_Bool,
			ClassicOptionsScreen_GetViewBob, ClassicOptionsScreen_SetViewBob },
		{ 1,    0, "FPS mode",      MenuOptionsScreen_Enum,
			MenuOptionsScreen_GetFPS,        MenuOptionsScreen_SetFPS },
		{ 0,   60, "Hacks enabled", MenuOptionsScreen_Bool,
			ClassicOptionsScreen_GetHacks,   ClassicOptionsScreen_SetHacks }
	};
	static const String title   = String_FromConst("Controls...");
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;

	MenuOptionsScreen_MakeButtons(s, buttons, Array_Elems(buttons));
	Menu_Button(s, 9, &s->buttons[9], 400, &title, &s->titleFont, Menu_SwitchKeysClassic,
		ANCHOR_CENTRE, ANCHOR_MAX, 0, 95);
	Menu_Back(s,  10, &s->buttons[10], "Done",     &s->titleFont, Menu_SwitchPause);

	/* Disable certain options */
	if (!Server.IsSinglePlayer) Menu_Remove(s, 3);
	if (!Game_ClassicHacks)     Menu_Remove(s, 8);
}

struct Screen* ClassicOptionsScreen_MakeInstance(void) {
	static struct MenuInputDesc descs[11];
	MenuInput_Enum(descs[2], ViewDist_Names, VIEW_COUNT);
	MenuInput_Enum(descs[7], FpsLimit_Names, FPS_LIMIT_COUNT);

	return MenuOptionsScreen_MakeInstance(11, 
		ClassicOptionsScreen_ContextRecreated, descs, NULL, 0);
}


/*########################################################################################################################*
*----------------------------------------------------EnvSettingsScreen----------------------------------------------------*
*#########################################################################################################################*/
static void EnvSettingsScreen_GetCloudsCol(String* v) { PackedCol_ToHex(v, Env.CloudsCol); }
static void EnvSettingsScreen_SetCloudsCol(const String* v) { Env_SetCloudsCol(Menu_HexCol(v)); }

static void EnvSettingsScreen_GetSkyCol(String* v) { PackedCol_ToHex(v, Env.SkyCol); }
static void EnvSettingsScreen_SetSkyCol(const String* v) { Env_SetSkyCol(Menu_HexCol(v)); }

static void EnvSettingsScreen_GetFogCol(String* v) { PackedCol_ToHex(v, Env.FogCol); }
static void EnvSettingsScreen_SetFogCol(const String* v) { Env_SetFogCol(Menu_HexCol(v)); }

static void EnvSettingsScreen_GetCloudsSpeed(String* v) { String_AppendFloat(v, Env.CloudsSpeed, 2); }
static void EnvSettingsScreen_SetCloudsSpeed(const String* v) { Env_SetCloudsSpeed(Menu_Float(v)); }

static void EnvSettingsScreen_GetCloudsHeight(String* v) { String_AppendInt(v, Env.CloudsHeight); }
static void EnvSettingsScreen_SetCloudsHeight(const String* v) { Env_SetCloudsHeight(Menu_Int(v)); }

static void EnvSettingsScreen_GetSunCol(String* v) { PackedCol_ToHex(v, Env.SunCol); }
static void EnvSettingsScreen_SetSunCol(const String* v) { Env_SetSunCol(Menu_HexCol(v)); }

static void EnvSettingsScreen_GetShadowCol(String* v) { PackedCol_ToHex(v, Env.ShadowCol); }
static void EnvSettingsScreen_SetShadowCol(const String* v) { Env_SetShadowCol(Menu_HexCol(v)); }

static void EnvSettingsScreen_GetWeather(String* v) { String_AppendConst(v, Weather_Names[Env.Weather]); }
static void EnvSettingsScreen_SetWeather(const String* v) {
	int raw = Utils_ParseEnum(v, 0, Weather_Names, Array_Elems(Weather_Names));
	Env_SetWeather(raw); 
}

static void EnvSettingsScreen_GetWeatherSpeed(String* v) { String_AppendFloat(v, Env.WeatherSpeed, 2); }
static void EnvSettingsScreen_SetWeatherSpeed(const String* v) { Env_SetWeatherSpeed(Menu_Float(v)); }

static void EnvSettingsScreen_GetEdgeHeight(String* v) { String_AppendInt(v, Env.EdgeHeight); }
static void EnvSettingsScreen_SetEdgeHeight(const String* v) { Env_SetEdgeHeight(Menu_Int(v)); }

static void EnvSettingsScreen_ContextRecreated(void* screen) {
	static const struct MenuOptionDesc buttons[10] = {
		{ -1, -150, "Clouds col",    MenuOptionsScreen_Input,
			EnvSettingsScreen_GetCloudsCol,    EnvSettingsScreen_SetCloudsCol },
		{ -1, -100, "Sky col",       MenuOptionsScreen_Input,
			EnvSettingsScreen_GetSkyCol,       EnvSettingsScreen_SetSkyCol },
		{ -1,  -50, "Fog col",       MenuOptionsScreen_Input,
			EnvSettingsScreen_GetFogCol,       EnvSettingsScreen_SetFogCol },
		{ -1,    0, "Clouds speed",  MenuOptionsScreen_Input,
			EnvSettingsScreen_GetCloudsSpeed,  EnvSettingsScreen_SetCloudsSpeed },
		{ -1,   50, "Clouds height", MenuOptionsScreen_Input,
			EnvSettingsScreen_GetCloudsHeight, EnvSettingsScreen_SetCloudsHeight },

		{ 1, -150, "Sunlight col",    MenuOptionsScreen_Input,
			EnvSettingsScreen_GetSunCol,       EnvSettingsScreen_SetSunCol },
		{ 1, -100, "Shadow col",      MenuOptionsScreen_Input,
			EnvSettingsScreen_GetShadowCol,    EnvSettingsScreen_SetShadowCol },
		{ 1,  -50, "Weather",         MenuOptionsScreen_Enum,
			EnvSettingsScreen_GetWeather,      EnvSettingsScreen_SetWeather },
		{ 1,    0, "Rain/Snow speed", MenuOptionsScreen_Input,
			EnvSettingsScreen_GetWeatherSpeed, EnvSettingsScreen_SetWeatherSpeed },
		{ 1,   50, "Water level",     MenuOptionsScreen_Input,
			EnvSettingsScreen_GetEdgeHeight,   EnvSettingsScreen_SetEdgeHeight }
	};
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct Widget** widgets     = s->widgets;

	MenuOptionsScreen_MakeButtons(s, buttons, Array_Elems(buttons));
	Menu_Back(s, 10, &s->buttons[10], "Done", &s->titleFont, Menu_SwitchOptions);
	widgets[11] = NULL; widgets[12] = NULL; widgets[13] = NULL;
}

static String String_InitAndClear(STRING_REF char* buffer, int capacity) {
	String str = String_Init(buffer, 0, capacity);
	int i;
	for (i = 0; i < capacity; i++) { buffer[i] = '\0'; }
	return str;
}

struct Screen* EnvSettingsScreen_MakeInstance(void) {
	static struct MenuInputDesc descs[1];
	MenuInput_Hex(descs[0],   Env_DefaultCloudsCol);
	MenuInput_Hex(descs[1],   Env_DefaultSkyCol);
	MenuInput_Hex(descs[2],   Env_DefaultFogCol);
	MenuInput_Float(descs[3],      0,  1000, 1);
	MenuInput_Int(descs[4],   -10000, 10000, World.Height + 2);

	MenuInput_Hex(descs[5],   Env_DefaultSunCol);
	MenuInput_Hex(descs[6],   Env_DefaultShadowCol);
	MenuInput_Enum(descs[7],  Weather_Names, Array_Elems(Weather_Names));
	MenuInput_Float(descs[8],  -100,  100, 1);
	MenuInput_Int(descs[9],   -2048, 2048, World.Height / 2);

	return MenuOptionsScreen_MakeInstance(4,
		EnvSettingsScreen_ContextRecreated, descs, NULL, 0);
}


/*########################################################################################################################*
*--------------------------------------------------GraphicsOptionsScreen--------------------------------------------------*
*#########################################################################################################################*/
static void GraphicsOptionsScreen_GetViewDist(String* v) { String_AppendInt(v, Game_ViewDistance); }
static void GraphicsOptionsScreen_SetViewDist(const String* v) { Game_UserSetViewDistance(Menu_Int(v)); }

static void GraphicsOptionsScreen_GetSmooth(String* v) { Menu_GetBool(v, Builder_SmoothLighting); }
static void GraphicsOptionsScreen_SetSmooth(const String* v) {
	Builder_SmoothLighting = Menu_SetBool(v, OPT_SMOOTH_LIGHTING);
	Builder_ApplyActive();
	MapRenderer_Refresh();
}

static void GraphicsOptionsScreen_GetNames(String* v) { String_AppendConst(v, NameMode_Names[Entities.NamesMode]); }
static void GraphicsOptionsScreen_SetNames(const String* v) {
	Entities.NamesMode = Utils_ParseEnum(v, 0, NameMode_Names, NAME_MODE_COUNT);
	Options_Set(OPT_NAMES_MODE, v);
}

static void GraphicsOptionsScreen_GetShadows(String* v) { String_AppendConst(v, ShadowMode_Names[Entities.ShadowsMode]); }
static void GraphicsOptionsScreen_SetShadows(const String* v) {
	Entities.ShadowsMode = Utils_ParseEnum(v, 0, ShadowMode_Names, SHADOW_MODE_COUNT);
	Options_Set(OPT_ENTITY_SHADOW, v);
}

static void GraphicsOptionsScreen_GetMipmaps(String* v) { Menu_GetBool(v, Gfx.Mipmaps); }
static void GraphicsOptionsScreen_SetMipmaps(const String* v) {
	Gfx.Mipmaps = Menu_SetBool(v, OPT_MIPMAPS);
	TexturePack_ExtractCurrent(true);
}

static void GraphicsOptionsScreen_ContextRecreated(void* screen) {
	static const struct MenuOptionDesc buttons[6] = {
		{ -1, -50, "FPS mode",          MenuOptionsScreen_Enum,
			MenuOptionsScreen_GetFPS,          MenuOptionsScreen_SetFPS },
		{ -1,   0, "View distance",     MenuOptionsScreen_Input,
			GraphicsOptionsScreen_GetViewDist, GraphicsOptionsScreen_SetViewDist },
		{ -1,  50, "Advanced lighting", MenuOptionsScreen_Bool,
			GraphicsOptionsScreen_GetSmooth,   GraphicsOptionsScreen_SetSmooth },

		{ 1, -50, "Names",   MenuOptionsScreen_Enum,
			GraphicsOptionsScreen_GetNames,   GraphicsOptionsScreen_SetNames },
		{ 1,   0, "Shadows", MenuOptionsScreen_Enum,
			GraphicsOptionsScreen_GetShadows, GraphicsOptionsScreen_SetShadows },
		{ 1,  50, "Mipmaps", MenuOptionsScreen_Bool,
			GraphicsOptionsScreen_GetMipmaps, GraphicsOptionsScreen_SetMipmaps }
	};
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct Widget** widgets     = s->widgets;

	MenuOptionsScreen_MakeButtons(s, buttons, Array_Elems(buttons));
	Menu_Back(s, 6, &s->buttons[6], "Done", &s->titleFont, Menu_SwitchOptions);
	widgets[7] = NULL; widgets[8] = NULL; widgets[9] = NULL;
}

struct Screen* GraphicsOptionsScreen_MakeInstance(void) {
	static struct MenuInputDesc descs[7];
	static const char* extDescs[Array_Elems(descs)];

	extDescs[0] = \
		"&eVSync: &fNumber of frames rendered is at most the monitor's refresh rate.\n" \
		"&e30/60/120/144 FPS: &fRenders 30/60/120/144 frames at most each second.\n" \
		"&eNoLimit: &fRenders as many frames as possible each second.\n" \
		"&cUsing NoLimit mode is discouraged.";
	extDescs[2] = "&cNote: &eSmooth lighting is still experimental and can heavily reduce performance.";
	extDescs[3] = \
		"&eNone: &fNo names of players are drawn.\n" \
		"&eHovered: &fName of the targeted player is drawn see-through.\n" \
		"&eAll: &fNames of all other players are drawn normally.\n" \
		"&eAllHovered: &fAll names of players are drawn see-through.\n" \
		"&eAllUnscaled: &fAll names of players are drawn see-through without scaling.";
	extDescs[4] = \
		"&eNone: &fNo entity shadows are drawn.\n" \
		"&eSnapToBlock: &fA square shadow is shown on block you are directly above.\n" \
		"&eCircle: &fA circular shadow is shown across the blocks you are above.\n" \
		"&eCircleAll: &fA circular shadow is shown underneath all entities.";
	
	MenuInput_Enum(descs[0], FpsLimit_Names, FPS_LIMIT_COUNT);
	MenuInput_Int(descs[1],  8, 4096, 512);
	MenuInput_Enum(descs[3], NameMode_Names,   NAME_MODE_COUNT);
	MenuInput_Enum(descs[4], ShadowMode_Names, SHADOW_MODE_COUNT);

	return MenuOptionsScreen_MakeInstance(10,
		GraphicsOptionsScreen_ContextRecreated, descs, extDescs, Array_Elems(extDescs));
}


/*########################################################################################################################*
*----------------------------------------------------GuiOptionsScreen-----------------------------------------------------*
*#########################################################################################################################*/
static void GuiOptionsScreen_GetShadows(String* v) { Menu_GetBool(v, Drawer2D_BlackTextShadows); }
static void GuiOptionsScreen_SetShadows(const String* v) {
	Drawer2D_BlackTextShadows = Menu_SetBool(v, OPT_BLACK_TEXT);
	Menu_HandleFontChange((struct Screen*)&MenuOptionsScreen_Instance);
}

static void GuiOptionsScreen_GetShowFPS(String* v) { Menu_GetBool(v, Gui_ShowFPS); }
static void GuiOptionsScreen_SetShowFPS(const String* v) { Gui_ShowFPS = Menu_SetBool(v, OPT_SHOW_FPS); }

static void GuiOptionsScreen_SetScale(const String* v, float* target, const char* optKey) {
	*target = Menu_Float(v);
	Options_Set(optKey, v);
	Gui_RefreshHud();
}

static void GuiOptionsScreen_GetHotbar(String* v) { String_AppendFloat(v, Game_RawHotbarScale, 1); }
static void GuiOptionsScreen_SetHotbar(const String* v) { GuiOptionsScreen_SetScale(v, &Game_RawHotbarScale, OPT_HOTBAR_SCALE); }

static void GuiOptionsScreen_GetInventory(String* v) { String_AppendFloat(v, Game_RawInventoryScale, 1); }
static void GuiOptionsScreen_SetInventory(const String* v) { GuiOptionsScreen_SetScale(v, &Game_RawInventoryScale, OPT_INVENTORY_SCALE); }

static void GuiOptionsScreen_GetTabAuto(String* v) { Menu_GetBool(v, Gui_TabAutocomplete); }
static void GuiOptionsScreen_SetTabAuto(const String* v) { Gui_TabAutocomplete = Menu_SetBool(v, OPT_TAB_AUTOCOMPLETE); }

static void GuiOptionsScreen_GetClickable(String* v) { Menu_GetBool(v, Gui_ClickableChat); }
static void GuiOptionsScreen_SetClickable(const String* v) { Gui_ClickableChat = Menu_SetBool(v, OPT_CLICKABLE_CHAT); }

static void GuiOptionsScreen_GetChatScale(String* v) { String_AppendFloat(v, Game_RawChatScale, 1); }
static void GuiOptionsScreen_SetChatScale(const String* v) { GuiOptionsScreen_SetScale(v, &Game_RawChatScale, OPT_CHAT_SCALE); }

static void GuiOptionsScreen_GetChatlines(String* v) { String_AppendInt(v, Gui_Chatlines); }
static void GuiOptionsScreen_SetChatlines(const String* v) {
	Gui_Chatlines = Menu_Int(v);
	Options_Set(OPT_CHATLINES, v);
	Gui_RefreshHud();
}

static void GuiOptionsScreen_GetUseFont(String* v) { Menu_GetBool(v, !Drawer2D_BitmappedText); }
static void GuiOptionsScreen_SetUseFont(const String* v) {
	Drawer2D_BitmappedText = !Menu_SetBool(v, OPT_USE_CHAT_FONT);
	Menu_HandleFontChange((struct Screen*)&MenuOptionsScreen_Instance);
}

static void GuiOptionsScreen_ContextRecreated(void* screen) {
	static const struct MenuOptionDesc buttons[10] = {
		{ -1, -150, "Black text shadows", MenuOptionsScreen_Bool,
			GuiOptionsScreen_GetShadows,   GuiOptionsScreen_SetShadows },
		{ -1, -100, "Show FPS",           MenuOptionsScreen_Bool,
			GuiOptionsScreen_GetShowFPS,   GuiOptionsScreen_SetShowFPS },
		{ -1,  -50, "Hotbar scale",       MenuOptionsScreen_Input,
			GuiOptionsScreen_GetHotbar,    GuiOptionsScreen_SetHotbar },
		{ -1,    0, "Inventory scale",    MenuOptionsScreen_Input,
			GuiOptionsScreen_GetInventory, GuiOptionsScreen_SetInventory },
		{ -1,   50, "Tab auto-complete",  MenuOptionsScreen_Bool,
			GuiOptionsScreen_GetTabAuto,   GuiOptionsScreen_SetTabAuto },
	
		{ 1, -150, "Clickable chat",     MenuOptionsScreen_Bool,
			GuiOptionsScreen_GetClickable, GuiOptionsScreen_SetClickable },
		{ 1, -100, "Chat scale",         MenuOptionsScreen_Input,
			GuiOptionsScreen_GetChatScale, GuiOptionsScreen_SetChatScale },
		{ 1,  -50, "Chat lines",         MenuOptionsScreen_Input,
			GuiOptionsScreen_GetChatlines, GuiOptionsScreen_SetChatlines },
		{ 1,    0, "Use system font",    MenuOptionsScreen_Bool,
			GuiOptionsScreen_GetUseFont,   GuiOptionsScreen_SetUseFont },
		{ 1,   50, "Select system font", Menu_SwitchFont,
			NULL,                          NULL }
	};
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct Widget** widgets     = s->widgets;

	MenuOptionsScreen_MakeButtons(s, buttons, Array_Elems(buttons));
	Menu_Back(s, 10, &s->buttons[10], "Done", &s->titleFont, Menu_SwitchOptions);
	widgets[11] = NULL; widgets[12] = NULL; widgets[13] = NULL;
}

struct Screen* GuiOptionsScreen_MakeInstance(void) {
	static struct MenuInputDesc descs[11];
	MenuInput_Float(descs[2], 0.25f, 4.00f,  1);
	MenuInput_Float(descs[3], 0.25f, 4.00f,  1);
	MenuInput_Float(descs[6], 0.25f, 4.00f,  1);
	MenuInput_Int(descs[7],       0,    30, 10);

	return MenuOptionsScreen_MakeInstance(14,
		GuiOptionsScreen_ContextRecreated, descs,  NULL, 0);
}


/*########################################################################################################################*
*---------------------------------------------------HacksSettingsScreen---------------------------------------------------*
*#########################################################################################################################*/
static void HacksSettingsScreen_GetHacks(String* v) { Menu_GetBool(v, LocalPlayer_Instance.Hacks.Enabled); }
static void HacksSettingsScreen_SetHacks(const String* v) {
	LocalPlayer_Instance.Hacks.Enabled = Menu_SetBool(v,OPT_HACKS_ENABLED);
	HacksComp_Update(&LocalPlayer_Instance.Hacks);
}

static void HacksSettingsScreen_GetSpeed(String* v) { String_AppendFloat(v, LocalPlayer_Instance.Hacks.SpeedMultiplier, 2); }
static void HacksSettingsScreen_SetSpeed(const String* v) {
	LocalPlayer_Instance.Hacks.SpeedMultiplier = Menu_Float(v);
	Options_Set(OPT_SPEED_FACTOR, v);
}

static void HacksSettingsScreen_GetClipping(String* v) { Menu_GetBool(v, Camera.Clipping); }
static void HacksSettingsScreen_SetClipping(const String* v) {
	Camera.Clipping = Menu_SetBool(v, OPT_CAMERA_CLIPPING);
}

static void HacksSettingsScreen_GetJump(String* v) { String_AppendFloat(v, LocalPlayer_JumpHeight(), 3); }
static void HacksSettingsScreen_SetJump(const String* v) {
	String str; char strBuffer[STRING_SIZE];
	struct PhysicsComp* physics;

	physics = &LocalPlayer_Instance.Physics;
	physics->JumpVel     = PhysicsComp_CalcJumpVelocity(Menu_Float(v));
	physics->UserJumpVel = physics->JumpVel;
	
	String_InitArray(str, strBuffer);
	String_AppendFloat(&str, physics->JumpVel, 8);
	Options_Set(OPT_JUMP_VELOCITY, &str);
}

static void HacksSettingsScreen_GetWOMHacks(String* v) { Menu_GetBool(v, LocalPlayer_Instance.Hacks.WOMStyleHacks); }
static void HacksSettingsScreen_SetWOMHacks(const String* v) {
	LocalPlayer_Instance.Hacks.WOMStyleHacks = Menu_SetBool(v, OPT_WOM_STYLE_HACKS);
}

static void HacksSettingsScreen_GetFullStep(String* v) { Menu_GetBool(v, LocalPlayer_Instance.Hacks.FullBlockStep); }
static void HacksSettingsScreen_SetFullStep(const String* v) {
	LocalPlayer_Instance.Hacks.FullBlockStep = Menu_SetBool(v, OPT_FULL_BLOCK_STEP);
}

static void HacksSettingsScreen_GetPushback(String* v) { Menu_GetBool(v, LocalPlayer_Instance.Hacks.PushbackPlacing); }
static void HacksSettingsScreen_SetPushback(const String* v) {
	LocalPlayer_Instance.Hacks.PushbackPlacing = Menu_SetBool(v, OPT_PUSHBACK_PLACING);
}

static void HacksSettingsScreen_GetLiquids(String* v) { Menu_GetBool(v, Game_BreakableLiquids); }
static void HacksSettingsScreen_SetLiquids(const String* v) {
	Game_BreakableLiquids = Menu_SetBool(v, OPT_MODIFIABLE_LIQUIDS);
}

static void HacksSettingsScreen_GetSlide(String* v) { Menu_GetBool(v, LocalPlayer_Instance.Hacks.NoclipSlide); }
static void HacksSettingsScreen_SetSlide(const String* v) {
	LocalPlayer_Instance.Hacks.NoclipSlide = Menu_SetBool(v, OPT_NOCLIP_SLIDE);
}

static void HacksSettingsScreen_GetFOV(String* v) { String_AppendInt(v, Game_Fov); }
static void HacksSettingsScreen_SetFOV(const String* v) {
	int fov = Menu_Int(v);
	if (Game_ZoomFov > fov) Game_ZoomFov = fov;
	Game_DefaultFov = fov;

	Options_Set(OPT_FIELD_OF_VIEW, v);
	Game_SetFov(fov);
}

static void HacksSettingsScreen_CheckHacksAllowed(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct Widget** widgets = s->widgets;
	struct LocalPlayer* p;
	bool disabled;
	int i;

	for (i = 0; i < s->numWidgets; i++) {
		if (!widgets[i]) continue;
		widgets[i]->disabled = false;
	}
	p = &LocalPlayer_Instance;

	disabled = !p->Hacks.Enabled;
	widgets[3]->disabled = disabled || !p->Hacks.CanSpeed;
	widgets[4]->disabled = disabled || !p->Hacks.CanSpeed;
	widgets[5]->disabled = disabled || !p->Hacks.CanSpeed;
	widgets[7]->disabled = disabled || !p->Hacks.CanPushbackBlocks;
}

static void HacksSettingsScreen_ContextLost(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	MenuOptionsScreen_ContextLost(s);
	Event_UnregisterVoid(&UserEvents.HackPermissionsChanged, s, HacksSettingsScreen_CheckHacksAllowed);
}

static void HacksSettingsScreen_ContextRecreated(void* screen) {
	static const struct MenuOptionDesc buttons[10] = {
		{ -1, -150, "Hacks enabled",    MenuOptionsScreen_Bool,
			HacksSettingsScreen_GetHacks,    HacksSettingsScreen_SetHacks },
		{ -1, -100, "Speed multiplier", MenuOptionsScreen_Input,
			HacksSettingsScreen_GetSpeed,    HacksSettingsScreen_SetSpeed },
		{ -1,  -50, "Camera clipping",  MenuOptionsScreen_Bool,
			HacksSettingsScreen_GetClipping, HacksSettingsScreen_SetClipping },
		{ -1,    0, "Jump height",      MenuOptionsScreen_Input,
			HacksSettingsScreen_GetJump,     HacksSettingsScreen_SetJump },
		{ -1,   50, "WOM style hacks",  MenuOptionsScreen_Bool,
			HacksSettingsScreen_GetWOMHacks, HacksSettingsScreen_SetWOMHacks },
	
		{ 1, -150, "Full block stepping", MenuOptionsScreen_Bool,
			HacksSettingsScreen_GetFullStep, HacksSettingsScreen_SetFullStep },
		{ 1, -100, "Breakable liquids",   MenuOptionsScreen_Bool,
			HacksSettingsScreen_GetLiquids,  HacksSettingsScreen_SetLiquids },
		{ 1,  -50, "Pushback placing",    MenuOptionsScreen_Bool,
			HacksSettingsScreen_GetPushback, HacksSettingsScreen_SetPushback },
		{ 1,    0, "Noclip slide",        MenuOptionsScreen_Bool,
			HacksSettingsScreen_GetSlide,    HacksSettingsScreen_SetSlide },
		{ 1,   50, "Field of view",       MenuOptionsScreen_Input,
			HacksSettingsScreen_GetFOV,      HacksSettingsScreen_SetFOV },
	};
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct Widget** widgets     = s->widgets;
	Event_RegisterVoid(&UserEvents.HackPermissionsChanged, s, HacksSettingsScreen_CheckHacksAllowed);

	MenuOptionsScreen_MakeButtons(s, buttons, Array_Elems(buttons));
	Menu_Back(s, 10, &s->buttons[10], "Done", &s->titleFont, Menu_SwitchOptions);
	widgets[11] = NULL; widgets[12] = NULL; widgets[13] = NULL;
	HacksSettingsScreen_CheckHacksAllowed(screen);
}

struct Screen* HacksSettingsScreen_MakeInstance(void) {
	static struct MenuInputDesc descs[11];
	static const char* extDescs[Array_Elems(descs)];

	extDescs[2] = "&eIf &fON&e, then the third person cameras will limit\n&etheir zoom distance if they hit a solid block.";
	extDescs[3] = "&eSets how many blocks high you can jump up.\n&eNote: You jump much higher when holding down the Speed key binding.";
	extDescs[7] = \
		"&eIf &fON&e, placing blocks that intersect your own position cause\n" \
		"&ethe block to be placed, and you to be moved out of the way.\n" \
		"&fThis is mainly useful for quick pillaring/towering.";
	extDescs[8] = "&eIf &fOFF&e, you will immediately stop when in noclip\n&emode and no movement keys are held down.";

	MenuInput_Float(descs[1], 0.1f,   50, 10);
	MenuInput_Float(descs[3], 0.1f, 2048, 1.233f);
	MenuInput_Int(descs[9],      1,  179, 70);

	struct Screen* s = MenuOptionsScreen_MakeInstance(14,
		HacksSettingsScreen_ContextRecreated, descs, extDescs, Array_Elems(extDescs));
	s->VTABLE->ContextLost = HacksSettingsScreen_ContextLost;
	return s;
}


/*########################################################################################################################*
*----------------------------------------------------MiscOptionsScreen----------------------------------------------------*
*#########################################################################################################################*/
static void MiscOptionsScreen_GetReach(String* v) { String_AppendFloat(v, LocalPlayer_Instance.ReachDistance, 2); }
static void MiscOptionsScreen_SetReach(const String* v) { LocalPlayer_Instance.ReachDistance = Menu_Float(v); }

static void MiscOptionsScreen_GetMusic(String* v) { String_AppendInt(v, Audio_MusicVolume); }
static void MiscOptionsScreen_SetMusic(const String* v) {
	Options_Set(OPT_MUSIC_VOLUME, v);
	Audio_SetMusic(Menu_Int(v));
}

static void MiscOptionsScreen_GetSounds(String* v) { String_AppendInt(v, Audio_SoundsVolume); }
static void MiscOptionsScreen_SetSounds(const String* v) {
	Options_Set(OPT_SOUND_VOLUME, v);
	Audio_SetSounds(Menu_Int(v));
}

static void MiscOptionsScreen_GetViewBob(String* v) { Menu_GetBool(v, Game_ViewBobbing); }
static void MiscOptionsScreen_SetViewBob(const String* v) { Game_ViewBobbing = Menu_SetBool(v, OPT_VIEW_BOBBING); }

static void MiscOptionsScreen_GetPhysics(String* v) { Menu_GetBool(v, Physics.Enabled); }
static void MiscOptionsScreen_SetPhysics(const String* v) {
	Physics_SetEnabled(Menu_SetBool(v, OPT_BLOCK_PHYSICS));
}

static void MiscOptionsScreen_GetAutoClose(String* v) { Menu_GetBool(v, Options_GetBool(OPT_AUTO_CLOSE_LAUNCHER, false)); }
static void MiscOptionsScreen_SetAutoClose(const String* v) { Menu_SetBool(v, OPT_AUTO_CLOSE_LAUNCHER); }

static void MiscOptionsScreen_GetInvert(String* v) { Menu_GetBool(v, Camera.Invert); }
static void MiscOptionsScreen_SetInvert(const String* v) { Camera.Invert = Menu_SetBool(v, OPT_INVERT_MOUSE); }

static void MiscOptionsScreen_GetSensitivity(String* v) { String_AppendInt(v, Camera.Sensitivity); }
static void MiscOptionsScreen_SetSensitivity(const String* v) {
	Camera.Sensitivity = Menu_Int(v);
	Options_Set(OPT_SENSITIVITY, v);
}

static void MiscOptionsScreen_ContextRecreated(void* screen) {
	static const struct MenuOptionDesc buttons[8] = {
		{ -1, -100, "Reach distance", MenuOptionsScreen_Input,
			MiscOptionsScreen_GetReach,       MiscOptionsScreen_SetReach },
		{ -1,  -50, "Music volume",   MenuOptionsScreen_Input,
			MiscOptionsScreen_GetMusic,       MiscOptionsScreen_SetMusic },
		{ -1,    0, "Sounds volume",  MenuOptionsScreen_Input,
			MiscOptionsScreen_GetSounds,      MiscOptionsScreen_SetSounds },
		{ -1,   50, "View bobbing",   MenuOptionsScreen_Bool,
			MiscOptionsScreen_GetViewBob,     MiscOptionsScreen_SetViewBob },
	
		{ 1, -100, "Block physics",       MenuOptionsScreen_Bool,
			MiscOptionsScreen_GetPhysics,     MiscOptionsScreen_SetPhysics },
		{ 1,  -50, "Auto close launcher", MenuOptionsScreen_Bool,
			MiscOptionsScreen_GetAutoClose,   MiscOptionsScreen_SetAutoClose },
		{ 1,    0, "Invert mouse",        MenuOptionsScreen_Bool,
			MiscOptionsScreen_GetInvert,      MiscOptionsScreen_SetInvert },
		{ 1,   50, "Mouse sensitivity",   MenuOptionsScreen_Input,
			MiscOptionsScreen_GetSensitivity, MiscOptionsScreen_SetSensitivity }
	};
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct Widget** widgets     = s->widgets;

	MenuOptionsScreen_MakeButtons(s, buttons, Array_Elems(buttons));
	Menu_Back(s, 8, &s->buttons[8], "Done", &s->titleFont, Menu_SwitchOptions);
	widgets[9] = NULL; widgets[10] = NULL; widgets[11] = NULL;

	/* Disable certain options */
	if (!Server.IsSinglePlayer) Menu_Remove(s, 0);
	if (!Server.IsSinglePlayer) Menu_Remove(s, 4);
}

struct Screen* MiscOptionsScreen_MakeInstance(void) {
	static struct MenuInputDesc descs[9];
	MenuInput_Float(descs[0], 1, 1024, 5);
	MenuInput_Int(descs[1],   0, 100,  0);
	MenuInput_Int(descs[2],   0, 100,  0);
#ifdef CC_BUILD_WIN
	MenuInput_Int(descs[7],   1, 200, 40);
#else
	MenuInput_Int(descs[7],   1, 200, 30);
#endif

	return MenuOptionsScreen_MakeInstance(12,
		MiscOptionsScreen_ContextRecreated, descs, NULL, 0);
}


/*########################################################################################################################*
*-----------------------------------------------------NostalgiaScreen-----------------------------------------------------*
*#########################################################################################################################*/
static void NostalgiaScreen_GetHand(String* v) { Menu_GetBool(v, Models.ClassicArms); }
static void NostalgiaScreen_SetHand(const String* v) { Models.ClassicArms = Menu_SetBool(v, OPT_CLASSIC_ARM_MODEL); }

static void NostalgiaScreen_GetAnim(String* v) { Menu_GetBool(v, !Game_SimpleArmsAnim); }
static void NostalgiaScreen_SetAnim(const String* v) {
	Game_SimpleArmsAnim = String_CaselessEqualsConst(v, "OFF");
	Options_SetBool(OPT_SIMPLE_ARMS_ANIM, Game_SimpleArmsAnim);
}

static void NostalgiaScreen_GetGui(String* v) { Menu_GetBool(v, Gui_ClassicTexture); }
static void NostalgiaScreen_SetGui(const String* v) { Gui_ClassicTexture = Menu_SetBool(v, OPT_CLASSIC_GUI); }

static void NostalgiaScreen_GetList(String* v) { Menu_GetBool(v, Gui_ClassicTabList); }
static void NostalgiaScreen_SetList(const String* v) { Gui_ClassicTabList = Menu_SetBool(v, OPT_CLASSIC_TABLIST); }

static void NostalgiaScreen_GetOpts(String* v) { Menu_GetBool(v, Gui_ClassicMenu); }
static void NostalgiaScreen_SetOpts(const String* v) { Gui_ClassicMenu = Menu_SetBool(v, OPT_CLASSIC_OPTIONS); }

static void NostalgiaScreen_GetCustom(String* v) { Menu_GetBool(v, Game_AllowCustomBlocks); }
static void NostalgiaScreen_SetCustom(const String* v) { Game_AllowCustomBlocks = Menu_SetBool(v, OPT_CUSTOM_BLOCKS); }

static void NostalgiaScreen_GetCPE(String* v) { Menu_GetBool(v, Game_UseCPE); }
static void NostalgiaScreen_SetCPE(const String* v) { Game_UseCPE = Menu_SetBool(v, OPT_CPE); }

static void NostalgiaScreen_GetTexs(String* v) { Menu_GetBool(v, Game_AllowServerTextures); }
static void NostalgiaScreen_SetTexs(const String* v) { Game_AllowServerTextures = Menu_SetBool(v, OPT_SERVER_TEXTURES); }

static void NostalgiaScreen_SwitchBack(void* a, void* b) {
	if (Gui_ClassicMenu) { Menu_SwitchPause(a, b); } else { Menu_SwitchOptions(a, b); }
}

static void NostalgiaScreen_ContextRecreated(void* screen) {
	static const struct MenuOptionDesc buttons[8] = {
		{ -1, -150, "Classic hand model",   MenuOptionsScreen_Bool,
			NostalgiaScreen_GetHand,   NostalgiaScreen_SetHand },
		{ -1, -100, "Classic walk anim",    MenuOptionsScreen_Bool,
			NostalgiaScreen_GetAnim,   NostalgiaScreen_SetAnim },
		{ -1,  -50, "Classic gui textures", MenuOptionsScreen_Bool,
			NostalgiaScreen_GetGui,    NostalgiaScreen_SetGui },
		{ -1,    0, "Classic player list",  MenuOptionsScreen_Bool,
			NostalgiaScreen_GetList,   NostalgiaScreen_SetList },
		{ -1,   50, "Classic options",      MenuOptionsScreen_Bool,
			NostalgiaScreen_GetOpts,   NostalgiaScreen_SetOpts },
	
		{ 1, -150, "Allow custom blocks", MenuOptionsScreen_Bool,
			NostalgiaScreen_GetCustom, NostalgiaScreen_SetCustom },
		{ 1, -100, "Use CPE",             MenuOptionsScreen_Bool,
			NostalgiaScreen_GetCPE,    NostalgiaScreen_SetCPE },
		{ 1,  -50, "Use server textures", MenuOptionsScreen_Bool,
			NostalgiaScreen_GetTexs,   NostalgiaScreen_SetTexs },
	};
	static const String descText = String_FromConst("&eButtons on the right require restarting game");
	struct MenuOptionsScreen* s  = (struct MenuOptionsScreen*)screen;
	static struct TextWidget desc;

	MenuOptionsScreen_MakeButtons(s, buttons, Array_Elems(buttons));
	Menu_Back(s,  8, &s->buttons[8], "Done", &s->titleFont, NostalgiaScreen_SwitchBack);
	Menu_Label(s, 9, &desc, &descText, &s->textFont, 
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 100);
}

struct Screen* NostalgiaScreen_MakeInstance(void) {
	return MenuOptionsScreen_MakeInstance(10,
		NostalgiaScreen_ContextRecreated, NULL, NULL, 0);
}


/*########################################################################################################################*
*---------------------------------------------------------Overlay---------------------------------------------------------*
*#########################################################################################################################*/
static void Overlay_Free(void* screen) {
	MenuScreen_Free(screen);
	Gui_RemoveOverlay(screen);
}

static bool Overlay_KeyDown(void* screen, Key key) { return true; }

static void Overlay_MakeLabels(void* menu, struct TextWidget* labels, const String* lines) {
	struct MenuScreen* s = (struct MenuScreen*)menu;
	PackedCol col = PACKEDCOL_CONST(224, 224, 224, 255);
	int i;
	Menu_Label(s, 0, &labels[0], &lines[0], &s->titleFont,
		ANCHOR_CENTRE, ANCHOR_CENTRE, 0, -120);
	
	for (i = 1; i < 4; i++) {
		if (!lines[i].length) continue;

		Menu_Label(s, i, &labels[i], &lines[i], &s->textFont,
			ANCHOR_CENTRE, ANCHOR_CENTRE, 0, -70 + 20 * i);
		labels[i].col = col;
	}
}

static void WarningOverlay_MakeButtons(void* menu, struct ButtonWidget* btns, bool always, Widget_LeftClick yesClick, Widget_LeftClick noClick) {
	static const String yes = String_FromConst("Yes");
	static const String no  = String_FromConst("No");
	static const String alwaysYes = String_FromConst("Always yes");
	static const String alwaysNo  = String_FromConst("Always no");

	struct MenuScreen* s = (struct MenuScreen*)menu;
	Menu_Button(s, 4, &btns[0], 160, &yes, &s->titleFont, yesClick, 
		ANCHOR_CENTRE, ANCHOR_CENTRE, -110, 30);
	Menu_Button(s, 5, &btns[1], 160, &no,  &s->titleFont, noClick, 
		ANCHOR_CENTRE, ANCHOR_CENTRE,  110, 30);

	if (!always) return;
	Menu_Button(s, 6, &btns[2], 160, &alwaysYes, &s->titleFont, yesClick, 
		ANCHOR_CENTRE, ANCHOR_CENTRE, -110, 85);
	Menu_Button(s, 7, &btns[3], 160, &alwaysNo,  &s->titleFont, noClick, 
		ANCHOR_CENTRE, ANCHOR_CENTRE,  110, 85);
}
static bool WarningOverlay_IsAlways(void* screen, void* w) { return Menu_Index(screen, w) >= 6; }


/*########################################################################################################################*
*------------------------------------------------------TexIdsOverlay------------------------------------------------------*
*#########################################################################################################################*/
#define TEXID_OVERLAY_MAX_PER_PAGE (ATLAS2D_TILES_PER_ROW * ATLAS2D_TILES_PER_ROW)
#define TEXID_OVERLAY_VERTICES_COUNT (TEXID_OVERLAY_MAX_PER_PAGE * 4)

static struct TexIdsOverlay {
	MenuScreen_Layout	
	GfxResourceID dynamicVb;
	int xOffset, yOffset, tileSize, baseTexLoc;
	struct TextAtlas idAtlas;
	struct TextWidget title;
} TexIdsOverlay_Instance;

static void TexIdsOverlay_ContextLost(void* screen) {
	struct TexIdsOverlay* s = (struct TexIdsOverlay*)screen;
	Menu_ContextLost(s);
	Gfx_DeleteVb(&s->dynamicVb);
	TextAtlas_Free(&s->idAtlas);
}

static void TexIdsOverlay_ContextRecreated(void* screen) {
	static const String chars  = String_FromConst("0123456789");
	static const String prefix = String_FromConst("f");
	static const String title  = String_FromConst("Texture ID reference sheet");
	struct TexIdsOverlay* s = (struct TexIdsOverlay*)screen;
	int size;

	size = Window_Height / ATLAS2D_TILES_PER_ROW;
	size = (size / 8) * 8;
	Math_Clamp(size, 8, 40);

	s->dynamicVb = Gfx_CreateDynamicVb(VERTEX_FORMAT_P3FT2FC4B, TEXID_OVERLAY_VERTICES_COUNT);
	TextAtlas_Make(&s->idAtlas, &chars, &s->textFont, &prefix);

	s->xOffset  = Gui_CalcPos(ANCHOR_CENTRE, 0, size * Atlas2D.RowsCount,     Window_Width);
	s->yOffset  = Gui_CalcPos(ANCHOR_CENTRE, 0, size * ATLAS2D_TILES_PER_ROW, Window_Height);
	s->tileSize = size;
	
	Menu_Label(s, 0, &s->title, &title, &s->titleFont,
		ANCHOR_CENTRE, ANCHOR_MIN, 0, s->yOffset - 30);
}

static void TexIdsOverlay_RenderTerrain(struct TexIdsOverlay* s) {
	PackedCol col = PACKEDCOL_WHITE;
	VertexP3fT2fC4b vertices[TEXID_OVERLAY_VERTICES_COUNT];
	VertexP3fT2fC4b* ptr;
	struct Texture tex;
	int size, count;
	int i, idx, end;

	size = s->tileSize;
	tex.uv.U1 = 0.0f; tex.uv.U2 = UV2_Scale;
	tex.Width = size; tex.Height = size;

	for (i = 0; i < TEXID_OVERLAY_MAX_PER_PAGE;) {
		ptr = vertices;
		idx = Atlas1D_Index(i + s->baseTexLoc);
		end = min(i + Atlas1D.TilesPerAtlas, TEXID_OVERLAY_MAX_PER_PAGE);

		for (; i < end; i++) {
			tex.X = s->xOffset + Atlas2D_TileX(i) * size;
			tex.Y = s->yOffset + Atlas2D_TileY(i) * size;

			tex.uv.V1 = Atlas1D_RowId(i + s->baseTexLoc) * Atlas1D.InvTileSize;
			tex.uv.V2 = tex.uv.V1            + UV2_Scale * Atlas1D.InvTileSize;
		
			Gfx_Make2DQuad(&tex, col, &ptr);
		}

		Gfx_BindTexture(Atlas1D.TexIds[idx]);
		count = (int)(ptr - vertices);
		Gfx_UpdateDynamicVb_IndexedTris(s->dynamicVb, vertices, count);
	}
}

static void TexIdsOverlay_RenderTextOverlay(struct TexIdsOverlay* s) {
	VertexP3fT2fC4b vertices[TEXID_OVERLAY_VERTICES_COUNT];
	VertexP3fT2fC4b* ptr = vertices;
	struct TextAtlas* idAtlas;
	int size, count;
	int x, y, id;

	size    = s->tileSize;
	idAtlas = &s->idAtlas;
	idAtlas->tex.Y = s->yOffset + (size - idAtlas->tex.Height);

	for (y = 0; y < ATLAS2D_TILES_PER_ROW; y++) {
		for (x = 0; x < ATLAS2D_TILES_PER_ROW; x++) {
			idAtlas->curX = s->xOffset + size * x + 3; /* offset text by 3 pixels */
			id = x + y * ATLAS2D_TILES_PER_ROW;
			TextAtlas_AddInt(idAtlas, id + s->baseTexLoc, &ptr);
		}

		idAtlas->tex.Y += size;
		if ((y % 4) != 3) continue;
		Gfx_BindTexture(idAtlas->tex.ID);

		count = (int)(ptr - vertices);
		Gfx_UpdateDynamicVb_IndexedTris(s->dynamicVb, vertices, count);
		ptr = vertices;
	}
}

static void TexIdsOverlay_Init(void* screen) {
	struct TexIdsOverlay* s = (struct TexIdsOverlay*)screen;
	Drawer2D_MakeFont(&s->textFont, 8, FONT_STYLE_NORMAL);
	MenuScreen_Init(s);
}

static void TexIdsOverlay_Render(void* screen, double delta) {
	struct TexIdsOverlay* s = (struct TexIdsOverlay*)screen;
	int rows, origXOffset;

	Menu_RenderBounds();
	Gfx_SetTexturing(true);
	Gfx_SetVertexFormat(VERTEX_FORMAT_P3FT2FC4B);
	Menu_RenderWidgets(s, delta);

	origXOffset = s->xOffset;
	s->baseTexLoc = 0;

	for (rows = Atlas2D.RowsCount; rows > 0; rows -= ATLAS2D_TILES_PER_ROW) {
		TexIdsOverlay_RenderTerrain(s);
		TexIdsOverlay_RenderTextOverlay(s);

		s->xOffset    += s->tileSize           * ATLAS2D_TILES_PER_ROW;
		s->baseTexLoc += ATLAS2D_TILES_PER_ROW * ATLAS2D_TILES_PER_ROW;
	}

	s->xOffset = origXOffset;
	Gfx_SetTexturing(false);
}

static bool TexIdsOverlay_KeyDown(void* screen, Key key) {
	struct Screen* s = (struct Screen*)screen;
	if (key == KeyBinds[KEYBIND_IDOVERLAY]) { Gui_Remove(s); return true; }
	return false;
}

static bool TexIdsOverlay_KeyPress(void* screen, char keyChar) { return false; }
static bool TexIdsOverlay_KeyUp(void* screen, Key key) { return false; }

static struct ScreenVTABLE TexIdsOverlay_VTABLE = {
	TexIdsOverlay_Init,    TexIdsOverlay_Render, Overlay_Free,
	TexIdsOverlay_KeyDown, TexIdsOverlay_KeyUp,  TexIdsOverlay_KeyPress,
	Menu_MouseDown,        Menu_MouseUp,         Menu_MouseMove,         MenuScreen_MouseScroll,
	Menu_OnResize,         TexIdsOverlay_ContextLost, TexIdsOverlay_ContextRecreated
};
void TexIdsOverlay_Show(void) {
	static struct Widget* widgets[1];
	struct TexIdsOverlay* s = &TexIdsOverlay_Instance;
	
	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);

	s->VTABLE = &TexIdsOverlay_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_TEXIDS);
}


/*########################################################################################################################*
*----------------------------------------------------UrlWarningOverlay----------------------------------------------------*
*#########################################################################################################################*/
static struct UrlWarningOverlay {
	MenuScreen_Layout
	bool openingUrl;
	String url;
	struct ButtonWidget buttons[2];
	struct TextWidget   labels[4];
	char _urlBuffer[STRING_SIZE * 4];
} UrlWarningOverlay_Instance;

static void UrlWarningOverlay_OpenUrl(void* screen, void* b) {
	struct UrlWarningOverlay* s = (struct UrlWarningOverlay*)screen;
	if (s->openingUrl) return;
	/* On windows, Process_StartOpen may end up calling our window procedure. */
	/* If a mouse click message is delivered (e.g. user spam clicking), then */
	/* UrlWarningOverlay_OpenUrl ends up getting called multiple times. */
	/* This will cause a crash as Elem_Free gets called multiple times. */
	/* (which attempts to unregister event handlers multiple times) */

	s->openingUrl = true;
	Process_StartOpen(&s->url);
	s->openingUrl = false;
	Elem_Free(s);
}

static void UrlWarningOverlay_AppendUrl(void* screen, void* b) {
	struct UrlWarningOverlay* s = (struct UrlWarningOverlay*)screen;
	if (Gui_ClickableChat) HUDScreen_AppendInput(&s->url);
	Elem_Free(s);
}

static void UrlWarningOverlay_ContextRecreated(void* screen) {
	static String lines[4] = {
		String_FromConst("&eAre you sure you want to open this link?"),
		String_FromConst(""),
		String_FromConst("Be careful - links from strangers may be websites that"),
		String_FromConst(" have viruses, or things you may not want to open/see."),
	};

	struct UrlWarningOverlay* s = (struct UrlWarningOverlay*)screen;
	lines[1] = s->url;
	Overlay_MakeLabels(s, s->labels, lines);

	WarningOverlay_MakeButtons((struct MenuScreen*)s, s->buttons, false,
		UrlWarningOverlay_OpenUrl, UrlWarningOverlay_AppendUrl);
}

static struct ScreenVTABLE UrlWarningOverlay_VTABLE = {
	MenuScreen_Init, MenuScreen_Render,  Overlay_Free,
	Overlay_KeyDown, Menu_KeyUp,         Menu_KeyPress,
	Menu_MouseDown,  Menu_MouseUp,       Menu_MouseMove,  MenuScreen_MouseScroll,
	Menu_OnResize,   Menu_ContextLost,   UrlWarningOverlay_ContextRecreated
};
void UrlWarningOverlay_Show(const String* url) {
	static struct Widget* widgets[6];
	struct UrlWarningOverlay* s = &UrlWarningOverlay_Instance;

	s->grabsInput = true;
	s->closable   = true;
	s->widgets    = widgets;
	s->numWidgets = Array_Elems(widgets);

	String_InitArray(s->url, s->_urlBuffer);
	String_Copy(&s->url, url);

	s->VTABLE = &UrlWarningOverlay_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_URLWARNING);
}


/*########################################################################################################################*
*-----------------------------------------------------TexPackOverlay------------------------------------------------------*
*#########################################################################################################################*/
static struct TexPackOverlay {
	MenuScreen_Layout
	bool showingDeny, alwaysDeny;
	uint32_t contentLength;
	String identifier;
	struct ButtonWidget buttons[4];
	struct TextWidget   labels[4];
	char _identifierBuffer[STRING_SIZE + 4];
} TexPackOverlay_Instance;

static void TexPackOverlay_YesClick(void* screen, void* widget) {
	struct TexPackOverlay* s = (struct TexPackOverlay*)screen;
	String url = String_UNSAFE_SubstringAt(&s->identifier, 3);

	World_ApplyTexturePack(&url);
	if (WarningOverlay_IsAlways(s, widget)) TextureCache_Accept(&url);
	Elem_Free(s);
}

static void TexPackOverlay_NoClick(void* screen, void* widget) {
	struct TexPackOverlay* s = (struct TexPackOverlay*)screen;
	s->alwaysDeny  = WarningOverlay_IsAlways(s, widget);
	s->showingDeny = true;

	s->VTABLE->ContextLost(s);
	s->VTABLE->ContextRecreated(s);
}

static void TexPackOverlay_ConfirmNoClick(void* screen, void* b) {
	struct TexPackOverlay* s = (struct TexPackOverlay*)screen;
	String url;

	url = String_UNSAFE_SubstringAt(&s->identifier, 3);
	if (s->alwaysDeny) TextureCache_Deny(&url);
	Elem_Free(s);
}

static void TexPackOverlay_GoBackClick(void* screen, void* b) {
	struct TexPackOverlay* s = (struct TexPackOverlay*)screen;
	s->showingDeny = false;

	s->VTABLE->ContextLost(s);
	s->VTABLE->ContextRecreated(s);
}

static void TexPackOverlay_Render(void* screen, double delta) {
	struct TexPackOverlay* s = (struct TexPackOverlay*)screen;
	struct HttpRequest item;

	MenuScreen_Render(s, delta);
	if (!Http_GetResult(&s->identifier, &item)) return;
	s->contentLength = item.ContentLength;

	if (!s->contentLength || Gfx.LostContext) return;
	s->VTABLE->ContextLost(s);
	s->VTABLE->ContextRecreated(s);
}

static void TexPackOverlay_MakeNormalElements(struct TexPackOverlay* s) {
	static String lines[4] = {
		String_FromConst("Do you want to download the server's texture pack?"),
		String_FromConst("Texture pack url:"),
		String_FromConst(""),
		String_FromConst(""),
	};
	static const String defCL = String_FromConst("Download size: Determining...");
	static const String https = String_FromConst("https://");
	static const String http  = String_FromConst("http://");
	String contents; char contentsBuffer[STRING_SIZE];
	float contentLengthMB;
	String url;

	url = String_UNSAFE_SubstringAt(&s->identifier, 3);
	if (String_CaselessStarts(&url, &https)) {
		url = String_UNSAFE_SubstringAt(&url, https.length);
	}
	if (String_CaselessStarts(&url, &http)) {
		url = String_UNSAFE_SubstringAt(&url, http.length);
	}

	lines[2] = url;
	if (s->contentLength) {
		String_InitArray(contents, contentsBuffer);
		contentLengthMB = s->contentLength / (1024.0f * 1024.0f);
		String_Format1(&contents, "Download size: %f3 MB", &contentLengthMB);
		lines[3] = contents;
	} else { lines[3] = defCL; }

	Overlay_MakeLabels(s, s->labels, lines);
	WarningOverlay_MakeButtons((struct MenuScreen*)s, s->buttons, true,
		TexPackOverlay_YesClick, TexPackOverlay_NoClick);
}

static void TexPackOverlay_MakeDenyElements(struct TexPackOverlay* s) {
	static String lines[4] = {
		String_FromConst("&eYou might be missing out."),
		String_FromConst("Texture packs can play a vital role in the look and feel of maps."),
		String_FromConst(""),
		String_FromConst("Sure you don't want to download the texture pack?")
	};
	static const String imSure = String_FromConst("I'm sure");
	static const String goBack = String_FromConst("Go back");
	Overlay_MakeLabels(s, s->labels, lines);

	Menu_Button(s, 4, &s->buttons[0], 160, &imSure, &s->titleFont, TexPackOverlay_ConfirmNoClick,
		ANCHOR_CENTRE, ANCHOR_CENTRE, -110, 30);
	Menu_Button(s, 5, &s->buttons[1], 160, &goBack, &s->titleFont, TexPackOverlay_GoBackClick,
		ANCHOR_CENTRE, ANCHOR_CENTRE,  110, 30);
}

static void TexPackOverlay_ContextRecreated(void* screen) {
	struct TexPackOverlay* s = (struct TexPackOverlay*)screen;
	if (s->showingDeny) {
		TexPackOverlay_MakeDenyElements(s);
	} else {
		TexPackOverlay_MakeNormalElements(s);
	}
	s->numWidgets = s->showingDeny ? 6 : 8;
}

static struct ScreenVTABLE TexPackOverlay_VTABLE = {
	MenuScreen_Init, TexPackOverlay_Render, Overlay_Free,
	Overlay_KeyDown, Menu_KeyUp,            Menu_KeyPress,
	Menu_MouseDown,  Menu_MouseUp,          Menu_MouseMove, MenuScreen_MouseScroll,
	Menu_OnResize,   Menu_ContextLost,      TexPackOverlay_ContextRecreated
};
void TexPackOverlay_Show(const String* url) {
	static struct Widget* widgets[8];
	struct TexPackOverlay* s = &TexPackOverlay_Instance;

	s->showingDeny = false;
	s->grabsInput  = true;
	s->closable    = true;
	s->widgets     = widgets;
	s->numWidgets  = Array_Elems(widgets);

	String_InitArray(s->identifier, s->_identifierBuffer);
	String_Format1(&s->identifier, "CL_%s", url);
	s->contentLength = 0;

	Http_AsyncGetHeaders(url, true, &s->identifier);
	s->VTABLE = &TexPackOverlay_VTABLE;
	Gui_Replace((struct Screen*)s, GUI_PRIORITY_TEXPACK);
}
