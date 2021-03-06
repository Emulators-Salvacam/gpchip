//*
//* Chip-8 emulator for GP2X and others systems, run with minimal lib (gp2x) and SDL (others)
//*

#ifndef GP2X
 #include <SDL/SDL.h>
 #include <SDL/SDL_mixer.h>
 #include <SDL/SDL_ttf.h>
#else
 #include "minimal.h"
 #include "bmp.h"
#endif
#include <sys/types.h>
#include <dirent.h>

// sdl
#ifndef GP2X
SDL_Surface *screen = NULL, *debugger_txt = NULL, *background = NULL;
SDL_Rect dst;
SDL_Rect rect;
SDL_Event event;
Mix_Chunk *beep = NULL;
TTF_Font *debugger_font = NULL;
SDL_Color dTxtColor = { 255, 255, 255, 0 }; // text color
Uint32 color;
#else
BMP background;
unsigned short white, black;
#endif
int loop = 0;
int sound = 1;
// debugger
int fx = 90, fy = 0;
char buf[255];

// emulator
unsigned char *ram; // memory
unsigned char V[16]; // registers
unsigned short Stack[16]; // stack
unsigned char key[16]; // keys
unsigned char delay_timer = 0, sound_timer = 0; // timer
int i, e, hl, wl, keypress = 0, size_w = 5, size_h = 5; // miscelaneous
int count = 15;
unsigned short PC, OpCode, I = 0; // program counter/opcode/adress register
unsigned short display[64*32]; // virtual screen
unsigned char pixel, x, y, h; // used for drawing

void (*OpCodeMathTBL[16])();
static inline void Cls();

void CCORE()
{
    switch(OpCode&0x00FF)
    {
        case 0x00E0: // Clears the screen.
            Cls();
            //memset(&display, 0, 64*32);
            for(i=0;i<2048;i++)
                display[i] = 0;
        break;

        case 0x00EE: // Returns from a subroutine.
            count -= 1;
            PC = Stack[count];
            PC += 2;
        break;

        case 0x00FF: // Set SCHIP graphic mode. (128x64)
            printf("Mode SCHIP non émulé.");
        break;

        case 0x00FE: // Set CHIP-8 graphic mode.
        break;

        default:
            #ifdef DEBUG
                printf("OpCode non reconnu: 0x%04X\n", OpCode);
            #endif
        break;
    }
}

// JMP
void CJMP(){
    PC = OpCode&0x0FFF;}

void CCALL(){
    Stack[count] = PC-2;
    count += 1;
    PC = OpCode&0x0FFF;}

void CSKEQ(){
    if(V[(OpCode&0x0F00)>>8] == (OpCode&0x00FF))
        PC += 2;}

void CSKNE(){
    if(V[(OpCode&0x0F00)>>8] != (OpCode&0x00FF))
        PC += 2;}

void CSKEQRR(){
    if(V[(OpCode&0x0F00)>>8] == V[(OpCode&0x00F0)>>4])
        PC += 2;}

void CMCR(){
    V[(OpCode&0x0F00)>>8] = OpCode&0x00FF;}

void CADD(){
    V[(OpCode&0x0F00)>>8] += OpCode&0x00FF;}

void CMATH(){
    OpCodeMathTBL[OpCode&0xF]();}

void CSKNER(){
    if(V[(OpCode&0x0F00)>>8] != V[(OpCode&0x00F0)>>4])
        PC += 2;}

void CMVI(){
    I = (OpCode&0x0FFF);}

void CJMI(){
    PC = (OpCode&0x0FFF) + V[0];}

void CRND(){
    srand(time(NULL));
    V[(OpCode&0x0F00)>>8] = rand()&(OpCode&0x00FF);}

void CDRAW(){
    x = V[(OpCode&0x0F00)>>8];
    y = V[(OpCode&0x00F0)>>4];
    h = OpCode&0x000F;

    if(h == 0) h = 16;

    //ram[0xF] = 0;
    V[15] = 0;

    for(e = 0;e < h;e++)
    {
        pixel = ram[I+e];
        for(i = 0; i < 8; i++)
        {
            if((pixel&(0x80>>i)))
            {
                //if(display[(i + x + ((y+e)*64))] > 0x2080)
                //    return;
                if(display[(i + x + ((y+e)<<6))] == 1)
                    V[15] = 1;
                display[(i + x + ((y+e)<<6))] ^= 0x1;

            }
        }
    }

#ifdef GP2X
    unsigned short color;
    memset(gp2x_video_RGB[0].screen16, 0, 102400); // 320*160*2
#endif

    for(e=0; e<32; e++)
    {
        for(i=0; i<64; i++)
        {
#ifndef GP2X
            if(display[i+(e<<6)])
                color = SDL_MapRGB(screen->format, 255, 255, 255);
            else
                color = SDL_MapRGB(screen->format, 0, 0, 0);

            dst.x = i*size_w;//+(320/2);
            dst.y = e*size_h;//+(240/2);
            dst.w = 8*size_w;
            dst.h = h*size_h;

            SDL_FillRect(screen, &dst, color);
#else
            if(display[i+(e<<6)])
                color = white;
            else
                color = black;

            for(hl=0;hl<h;hl++)
                for(wl=0;wl<8;wl++)
                    //gp2x_video_RGB[0].screen16[((i*size_w)+wl)+(((e*size_h)+hl)*320)] = color;
                    // optimized display using bit shifts
                    gp2x_video_RGB[0].screen16[(((e*size_h)+hl) << 8) + (((e*size_h)+hl) << 6) + ((i*size_w)+wl)] = color;
#endif
        }
    }}

void CSK(){
    if((OpCode&0xFF) == 0x9E)
    {
        if(key[V[(OpCode&0x0F00)>>8]] == 1)
            PC += 2;
    }
    else
    {
        if(key[V[(OpCode&0x0F00)>>8]] != 1)
            PC += 2;
    }}

void CS(){
    switch(OpCode&0xFF)
    {
        case 0x07:
            V[(OpCode&0x0F00)>>8] = delay_timer;
        break;

        case 0x0A:
            keypress = 0;
            for(i=0;i<16;i++)
            {
                if(key[i] == 1)
                {
                    V[(OpCode&0x0F00)>>8] = i;
                    keypress = 1;
                }
            }

            if(keypress == 0)
                PC -= 2;
        break;

        case 0x15:
            delay_timer = V[(OpCode&0x0F00)>>8];
        break;

        case 0x18:
            sound_timer = V[(OpCode&0x0F00)>>8];
            if(sound_timer != 0 && sound == 1)
                #ifndef GP2X
                    Mix_PlayChannel(-1, beep, 0);
                #else
                    gp2x_sound_pause(0);
                #endif
        break;

        case 0x1E: // Adds VX to I.
            I += V[(OpCode&0x0F00)>>8];
        break;

        case 0x29: // Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font.
            I = V[(OpCode&0x0F00)>>8]*5;
        break;

        case 0x30: // Sets I to the location of the sprite for the character in VX. Characters 0-F (in hexadecimal) are represented by a 4x5 font.
            I = V[(OpCode&0x0F00)>>8]*10;
        break;

        case 0x33: // Stores the Binary-coded decimal representation of VX at the addresses I, I plus 1, and I plus 2.
            ram[I] = V[((OpCode&0x0F00)>>8)]/100;
            ram[I+1] = (V[((OpCode&0x0F00)>>8)]/10)%10;
            ram[I+2] = V[((OpCode&0x0F00)>>8)]%10;
        break;

        case 0x55: // Stores V0 to VX in memory starting at address I.
            for(i=0; i<=((OpCode&0x0F00)>>8); i++)
                ram[I+i] = V[i];
        break;

        case 0x65: // Fills V0 to VX with values from memory starting at address I.
            for(i=0; i<=((OpCode&0x0F00)>>8); i++ )
                V[i] = ram[I+i];
        break;

        default:
            #ifdef DEBUG
                printf("OpCode non reconnu: 0x%04X\n", OpCode);
            #endif
        break;
    }}

void CEQ(){
    V[(OpCode&0x0F00)>>8] = V[(OpCode&0x00F0)>>4];}

void COR(){
    V[(OpCode&0x0F00)>>8] |= V[(OpCode&0x00F0)>>4];}

void CAND(){
    V[(OpCode&0x0F00)>>8] &= V[(OpCode&0x00F0)>>4];}

void CXOR(){
    V[(OpCode&0x0F00)>>8] ^= V[(OpCode&0x00F0)>>4];}

void CADDC(){
    if((V[(OpCode&0x00F0)>>4] + V[(OpCode&0x0F00)>>8]) > 0xFF)
        V[15] = 1;
    else
        V[15] = 0;

    V[(OpCode&0x0F00)>>8] += V[(OpCode&0x00F0)>>4];}

void CSHR(){
    if(V[(OpCode&0x0F00)>>8]&0x1)
        V[15] = 1;
    else
        V[15] = 0;

    V[(OpCode&0x0F00)>>8] >>= 1;}

void CSUBXY(){
    if(V[(OpCode&0x0F00)>>8] <= V[((OpCode&0x00F0)>>4)])
        V[15] = 0;
    else
        V[15] = 1;

    V[(OpCode&0x0F00)>>8] -= V[(OpCode&0x00F0)>>4];}

void CSUBYX(){
    if(V[((OpCode&0x00F0)>>4)] <= V[(OpCode&0x0F00)>>8])
        V[15] = 0;
    else
        V[15] = 1;

    V[(OpCode&0x0F00)>>8] = V[(OpCode&0x00F0)>>4] - V[(OpCode&0x0F00)>>8];}

void CN(){
    #ifdef DEBUG
        printf("OpCode non reconnu: 0x%04X\n", OpCode);
    #endif
}

void CSHL(){
    if(V[(OpCode&0x0F00)>>8]&0x80)
        V[15] = 1;
    else
        V[15] = 0;

    V[(OpCode&0x0F00)>>8] <<= 1;}

void (*OpCodeTBL[16])() =
{
	CCORE, CJMP, CCALL, CSKEQ, CSKNE, CSKEQRR, CMCR, CADD, CMATH, CSKNER,
	CMVI, CJMI, CRND, CDRAW, CSK, CS
};

void (*OpCodeMathTBL[16])() =
{
	CEQ, COR, CAND, CXOR, CADDC, CSUBXY, CSHR, CSUBYX, CN, CN,
	CN, CN, CN, CN, CSHL, CN
};

// bytecoded font
char font[80]=
        { 0xF0, 0x90, 0x90, 0x90, 0xF0,
		  0x20, 0x60, 0x20, 0x20, 0x70,
		  0xF0, 0x10, 0xF0, 0x80, 0xF0,
		  0xF0, 0x10, 0xF0, 0x10, 0xF0,
		  0x90, 0x90, 0xF0, 0x10, 0x10,
		  0xF0, 0x80, 0xF0, 0x10, 0xF0,
		  0xF0, 0x80, 0xF0, 0x90, 0xF0,
		  0xF0, 0x10, 0x20, 0x40, 0x40,
		  0xF0, 0x90, 0xF0, 0x90, 0xF0,
		  0xF0, 0x90, 0xF0, 0x10, 0xF0,
		  0xF0, 0x90, 0xF0, 0x90, 0x90,
		  0xE0, 0x90, 0xE0, 0x90, 0xE0,
		  0xF0, 0x80, 0x80, 0x80, 0xF0,
		  0xE0, 0x90, 0x90, 0x90, 0xE0,
		  0xF0, 0x80, 0xF0, 0x80, 0xF0,
		  0xF0, 0x80, 0xF0, 0x80, 0x80
		};

// roms selector stuff
typedef struct romsList
{
    char file[255];
}RomsList;
RomsList *romslist;
int romslist_size = 0, ym = 0, list_start = 0, yc = 0;
int by = 40;

static inline void DrawStr(const char *str, int x, int y, int style, int aa);

// Clear every frame the debugger zone
static inline void ClearDebugger()
{
    #ifndef GP2X
        rect.x = 0;
        rect.y = 160;
        rect.w = 320;
        rect.h = 240;
        SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 0, 0, 0));
    #else
        memset(&gp2x_video_RGB[0].screen16[54400], 0, 44800);//(170 << 8) + (170 << 6) or 320*70*2
    #endif
}

// Clear the entire screen
static inline void Cls()
{
    #ifndef GP2X
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
    #else
        memset(gp2x_video_RGB[0].screen16, 0, 153600);//320*240*2
    #endif
}

// Load a rom into the memory
static inline int LoadRom(char *rom)
{
    if(romslist_size != 0)
    {
        FILE *file;
        file = fopen(rom, "rb");

        if(file != NULL)
        {
            fread(&ram[0x200], 0xFFF, 1, file);
            fclose(file);
        }
        else
            return 1;

        return 0;
    }
    else
        return 1;
}

static inline void GetRoms()
{
    int roms = 1;
    i = 0;
    char *pch;

    struct dirent *read;
    DIR *dir;
    dir = opendir("./roms"); // check the roms directory
    if(dir == NULL) // doesn't exist ?
    {
        dir = opendir("./"); // check program directory
        roms = 0;
    }

    romslist = (RomsList *)malloc(0);

    // read the directory
    while((read = readdir(dir)))
    {
        romslist = (RomsList *)realloc(romslist, (i+1)*sizeof(RomsList));
        if(strcmp("roms",read->d_name) > 0)
        {
            // find chip8 roms by checking if the file has extension
            pch = strchr(read->d_name,".c8");
            if(pch == NULL)
            {
                if(roms == 1)
                {
                    strncpy(romslist[i].file, "./roms/", 255);
                    strncat(romslist[i].file, read->d_name, 255-8);
                }
                else
                    strncpy(romslist[i].file, read->d_name, 255);
                romslist_size++;
                i++;
            }
        }
    }
    closedir(dir);
}

// display the rom browser
static inline void DisplayBrowser()
{
    by = 40;
    Cls();

#ifndef GP2X
    SDL_BlitSurface(background, NULL, screen, NULL);
#else
    DrawBMP(&background);
#endif

    sprintf(buf,"Roms available (%i) :",romslist_size);
    DrawStr(buf, 20, 20, 0, 0);

    if(romslist_size != 0)
    {
        for(i=list_start;i<list_start+12;i++)
        {
            if(i < romslist_size)
            {
                DrawStr(romslist[i].file, 20, by, 1, 0);
                by += 12;
            }
        }

        DrawStr(">>", 6, 40+(12*yc), 1, 0);
    }
    else
        DrawStr(">> NO ROMS FOUND <<", 20, 40, 2, 1);
}

// used for displaying text quickly
static inline void DrawStr(const char *str, int sx, int sy, int style, int aa)
{
    #ifndef GP2X
        switch(style)
        {
            case 0:
                TTF_SetFontStyle(debugger_font, TTF_STYLE_UNDERLINE);
            break;

            case 1:
                TTF_SetFontStyle(debugger_font, TTF_STYLE_NORMAL);
            break;

            case 2:
                TTF_SetFontStyle(debugger_font, TTF_STYLE_BOLD);
            break;

            case 3:
                TTF_SetFontStyle(debugger_font, TTF_STYLE_UNDERLINE|TTF_STYLE_BOLD);
            break;

            default:
            break;
        }

        // anti aliased text
        if(aa == 0)
            debugger_txt = TTF_RenderText_Solid(debugger_font, str, dTxtColor);
        else
            debugger_txt = TTF_RenderText_Blended(debugger_font, str, dTxtColor);

        rect.x = sx;
        rect.y = sy;
        rect.w = debugger_txt->w;
        rect.h = debugger_txt->h;

        SDL_BlitSurface(debugger_txt, NULL, screen, &rect);

        if(debugger_txt != NULL)
            SDL_FreeSurface(debugger_txt);

        debugger_txt = NULL;
    #else
        gp2x_printf(NULL, sx, sy, str);
    #endif
}

// display debugger
static inline void DisplayDebugger()
{
    ClearDebugger();

    fx = 90; fy = 0;

    DrawStr("Debugger:", 0, 170, 0, 0);

    /* Draw debugger stuff */
    sprintf(buf,"OpCode: 0x%04X",OpCode);
    DrawStr(buf, 0, 184, 1, 0);

    sprintf(buf,"PC: 0x%03X",PC);
    DrawStr(buf, 0, 196, 1, 0);

    sprintf(buf,"I: 0x%03X",I);
    DrawStr(buf, 0, 206, 1, 0);

    sprintf(buf,"Stack: 0x%03X",Stack[count]);
    DrawStr(buf, 0, 216, 1, 0);

    sprintf(buf,"DTimer: %X",delay_timer);
    DrawStr(buf, 0, 226, 1, 0);

    for(i=0;i<16;i++)
    {
        sprintf(buf,"V[%d]=%d",i+1,V[i]);
        DrawStr(buf, fx, 180+fy, 1, 0);

        fx += 56;
        if(fx >= 260)
        {
            fx = 90;
            fy += 12;
        }
    }
}

// Initialize the memory and registers
static inline void InitRegisters()
{
    for(i=0; i<16; i++)
    {
        V[i] = 0; // all registers set to 0
        key[i] = 0;
    }

for(i=0;i<80;i++) // load the byte coded font of the chip8
{
ram[0x000 + i] = font[i];
display[i] = 0;
}

for(i=80;i<2048;i++) // initialize the virtual screen
display[i] = 0;

    PC = 0x200; // set the PC to the beginning of the memory
    count = 0;

    delay_timer = 0;
    sound_timer = 0;
}

// Emulate the CPU!
static inline void Emulate()
{
	// update delay timer
    if(delay_timer > 0)
    {
        delay_timer--;
        return;
    }

    OpCode = (ram[PC]<<8) + ram[PC+1]; // get the actual op code // |
    PC += 2;
    OpCodeTBL[(OpCode>>12)&0xF]();

    #ifdef DEBUG
        printf("OpCode: %X %X %X\n", OpCode,PC, PC+1);
    #endif
}

void quit()
{
    #ifdef GP2X
    gp2x_deinit();
    #else
    SDL_Quit();
    #endif
}

int main(int argc, char** argv)
{
    argc = 0; argv = NULL;
    int debug_sbs = 0, step = 0, debugger = 0, menu = 1, delay = 100;

    #ifndef GP2X
        /* SDL initialization */
        if(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO ) < 0)
        {
            printf("Impossible d'initialiser SDL: %s\n", SDL_GetError());
            exit(1);
        }
        //SDL_JoystickOpen(0);

        screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
        if(screen == NULL)
        {
            printf("Impossible d'initialiser le mode vidéo 320x240x16bpp: %s\n", SDL_GetError());
            exit(1);
        }
    #else // GP2X init
        gp2x_init(1000, 16, 22050, 16, 1, 60, 0);
        gp2x_video_logo_enable(1);
        if(LoadBMP("gpchip8x.bmp", &background) != 0)
        {
            gp2x_deinit();
            return 1;
        }
        // pre-generate colors (instead of calculate every frames)
        white = gp2x_video_RGB_color16(255, 255, 255);
        black = gp2x_video_RGB_color16(0, 0, 0);
    #endif

    atexit(quit);

    #ifndef GP2X
        if(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096))
        {
            printf("Impossible d'initialiser l'audio: %s\n", Mix_GetError());
            exit(1);
        }

        beep = Mix_LoadWAV("beep.wav");
        if(beep == NULL)
        {
            printf("Impossible de charger 'beep.wav' : %s\n", Mix_GetError());
            exit(1);
        }

        if(TTF_Init() == -1)
        {
            printf("Impossible d'initialiser SDL_ttf: %s\n", TTF_GetError());
            Mix_FreeChunk(beep);
            Mix_CloseAudio();
            exit(1);
        }

        debugger_font = TTF_OpenFont("font.ttf", 10);

        if(debugger_font == NULL)
        {
            printf("Impossible de charger 'font.ttf': %s\n", TTF_GetError());
            Mix_FreeChunk(beep);
            Mix_CloseAudio();
            TTF_Quit();
            exit(1);
        }

        // load background image
        SDL_Surface *tmp = SDL_LoadBMP("gpchip8x.bmp");
        if(tmp == NULL)
        {
            printf("Impossible de charger 'gpchip8x.png': %s\n", SDL_GetError());
            TTF_CloseFont(debugger_font);
            Mix_FreeChunk(beep);
            Mix_CloseAudio();
            TTF_Quit();
            exit(1);
        }

        // optimize the background image
        background = SDL_DisplayFormat(tmp);
        SDL_FreeSurface(tmp);

        SDL_WM_SetCaption("GpChip-8x", NULL);

        SDL_ShowCursor(SDL_DISABLE);
	#endif

    // Allocate virtual memory
    ram = malloc(0xFFF);
    if(ram == NULL)
    {
		printf("Allocation impossible.\n");
		#ifndef GP2X
            Mix_FreeChunk(beep);
            Mix_CloseAudio();
            SDL_FreeSurface(background);
            TTF_CloseFont(debugger_font);
            TTF_Quit();
        #endif
		exit(1);
    }

	GetRoms();

    // reset registers
    InitRegisters();

    while(loop == 0)
    {
        /* GP2X pad events */
        #ifdef GP2X
            //Cls();

            //gp2x_timer_delay(delay);
            unsigned long  pad = gp2x_joystick_read();
        #else
            // SDL events
            while(SDL_PollEvent(&event))
        #endif
        {
            // reset pressed keys
            for(i=15;i--;)
                key[i] = 0;

            #ifdef GP2X
                if(pad&GP2X_START)
                {
                    if(menu == 1)
                        loop = 1;
                    else
                        menu = 1;
                }

                if(pad&GP2X_UP)
                {
                    if(ym >= 1)
                        ym--;
                }

                if(pad&GP2X_DOWN)
                {
                    if(ym < romslist_size-1)
                        ym++;
                }

                if(pad&GP2X_B)
                {
                    if(menu == 1)
                    {
                        if(LoadRom(romslist[ym].file) != 1)
                        {
                            // reset
                            InitRegisters();
                            menu = 0;
                            Cls();
                        }
                    }
                    else
                    {
                        if(debugger == 0)
                            debugger = 1;
                        else
                        {
                            debugger = 0;
                            ClearDebugger();
                        }
                    }
                }

                if(pad&GP2X_L)
                {
                    delay-=10;
                    if(delay <= 0)
                        delay = 0;
                }

                if(pad&GP2X_R)
                    delay+=10;
            #else
            switch(event.type)
            {
                case SDL_QUIT:
                    loop = 1;
                break;

                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym)
                    {
                        case SDLK_ESCAPE:
                            if(menu == 1)
                                loop = 1;
                            else
                                menu = 1;
                        break;

                        /* Chip-8 Keys */
                        case SDLK_LALT:
                            key[0x0] = 1;
                        break;

                        case SDLK_LSHIFT:
                            key[0x1] = 1;
                        break;

                        case SDLK_DOWN:
                            key[0x2] = 1;
                        break;

                        case SDLK_LCTRL:
                            key[0x3] = 1;
                        break;

                        case SDLK_LEFT:
                            key[0x4] = 1;
                        break;

                        case SDLK_SPACE:
                            key[0x5] = 1;
                        break;

                        case SDLK_RIGHT:
                            key[0x6] = 1;
                        break;

                        case SDLK_TAB:
                            key[0x7] = 1;
                        break;

                        case SDLK_UP:
                            key[0x8] = 1;
                        break;

                        case SDLK_BACKSPACE:
                            key[0x9] = 1;
                        break;

                        case SDLK_KP_PERIOD:
                            key[0xA] = 1;
                        break;

                        case SDLK_KP_ENTER:
                            key[0xB] = 1;
                        break;

                        case SDLK_KP_PLUS:
                            key[0xC] = 1;
                        break;

                        case SDLK_KP_MINUS:
                            key[0xD] = 1;
                        break;

                        case SDLK_KP_MULTIPLY:
                            key[0xE] = 1;
                        break;

                        case SDLK_KP_DIVIDE:
                            key[0xF] = 1;
                        break;

                        case SDLK_F1:
                            size_w++;
                            size_h++;
                            Cls();
                        break;

                        case SDLK_F2:
                            if(size_w > 1 && size_h > 1)
                            {
                                size_w--;
                                size_h--;
                                Cls();
                            }
                        break;

                        default:
                        break;
                    }
                break;

                case SDL_KEYUP:
                    switch(event.key.keysym.sym)
                    {
                        case SDLK_UP:
                            if(ym >= 1)
                            {
                                ym--;
                                yc--;
                            }
                            if(ym < list_start)
                            {
                                yc = 11;
                                list_start-=12;
                            }
                        break;

                        case SDLK_DOWN:
                            if(ym < romslist_size-1)
                            {
                                ym++;
                                yc++;
                            }
                            if(ym >= list_start+12)
                            {
                                list_start+=12;
                                yc = 0;
                            }
                        break;

                        case SDLK_RETURN:
                            if(menu == 1)
                            {
                                if(LoadRom(romslist[ym].file) != 1)
                                {
                                    // reset
                                    InitRegisters();
                                    Cls();
                                    menu = 0;
                                }
                            }
                            else
                            {
                                if(debug_sbs == 1)
                                    debug_sbs = 0;
                                else
                                    debug_sbs = 1;
                            }
                        break;

                        case SDLK_F4:
                            if(sound == 1) sound = 0; else sound = 1;
                        break;

                        case SDLK_F5:
                            step = 1;
                        break;

                        case SDLK_F6:
                            if(debugger == 0)
                                debugger = 1;
                            else
                            {
                                ClearDebugger();
                                debugger = 0;
                            }
                        break;

                        default:
                        break;
                    }
                break;

                default:
                break;
            }
            #endif
        }

        if(menu == 1)
            DisplayBrowser();
        else
        {
            #ifndef GP2X
            long Timer = SDL_GetTicks();
            static double lTime = 0;
            double cTime = ((Timer * 1000) / 60) * 0.001f;
            double eTime = cTime - lTime;

            if(eTime > (1.0f / 60) )
            {
                lTime = cTime;
            #endif
                if(debug_sbs == 0 || step == 1)
                {
                    Emulate();
                    if(step == 1)
                        step = 0;
                }

                if(debugger)
                    DisplayDebugger();
                else
                    ClearDebugger();
            #ifndef GP2X
            }
            #endif
        }
        #ifndef GP2X
        SDL_UpdateRect(screen, 0, 0, 0, 0);
        #else
        gp2x_video_waitvsync();
        //gp2x_video_waithsync();
        gp2x_video_RGB_flip(0);
        #endif
    }

    free(romslist);
    free(ram);

    #ifndef GP2X
        TTF_CloseFont(debugger_font);
        TTF_Quit();

        if(debugger_txt != NULL)
            SDL_FreeSurface(debugger_txt);
        if(background != NULL)
            SDL_FreeSurface(background);

        Mix_FreeChunk(beep);
        Mix_CloseAudio();
    #endif

    return 0;
}

#ifdef GP2X
void gp2x_sound_frame(void *blah, void *buff, int samples)
{
}
#endif
