#include <SDL2/SDL.h>
#include <SDL_FontCache.h>
#include <coreinit/memory.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <romfs-wiiu.h>
#include <string>

#define SCREEN_WIDTH        1920
#define SCREEN_HEIGHT       1080
#define FONT_SIZE           36
#define SCREEN_COLOR_BLACK  ((SDL_Color){.r = 0x00, .g = 0x00, .b = 0x00, .a = 0xFF})
#define SCREEN_COLOR_WHITE  ((SDL_Color){.r = 0xFF, .g = 0xFF, .b = 0xFF, .a = 0xFF})
#define SCREEN_COLOR_YELLOW ((SDL_Color){.r = 0xFF, .g = 0xFF, .b = 0x00, .a = 0xFF})
#define SCREEN_COLOR_D_RED  ((SDL_Color){.r = 0x7F, .g = 0x00, .b = 0x00, .a = 0xFF})
#define SCREEN_COLOR_GRAY   ((SDL_Color){.r = 0x6A, .g = 0x6A, .b = 0x6A, .a = 0xFF})
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
bool retval = false;
bool done = false;
bool set_LED = false;
SDL_Texture *background_front, *background_back, *button, *axis;
SDL_GameController *gamecontroller;
SDL_GameController **gamecontrollers;
int num_controllers = 0;
SDL_Rect controllerRect = {SCREEN_WIDTH / 2 - 256, SCREEN_HEIGHT / 2 - 160, 512, 320};
SDL_JoystickID currentController = -1;

FC_Font *font = nullptr;

static const struct {
    int x;
    int y;
} button_positions[] = {
        {controllerRect.x + 431, controllerRect.y + 132}, /* SDL_CONTROLLER_BUTTON_B */
        {controllerRect.x + 387, controllerRect.y + 167}, /* SDL_CONTROLLER_BUTTON_A */
        {controllerRect.x + 389, controllerRect.y + 101}, /* SDL_CONTROLLER_BUTTON_Y */
        {controllerRect.x + 342, controllerRect.y + 132}, /* SDL_CONTROLLER_BUTTON_X */
        {controllerRect.x + 174, controllerRect.y + 132}, /* SDL_CONTROLLER_BUTTON_BACK */
        {controllerRect.x + 232, controllerRect.y + 128}, /* SDL_CONTROLLER_BUTTON_GUIDE */
        {controllerRect.x + 289, controllerRect.y + 132}, /* SDL_CONTROLLER_BUTTON_START */
        {controllerRect.x + 75, controllerRect.y + 154},  /* SDL_CONTROLLER_BUTTON_LEFTSTICK */
        {controllerRect.x + 305, controllerRect.y + 230}, /* SDL_CONTROLLER_BUTTON_RIGHTSTICK */
        {controllerRect.x + 77, controllerRect.y + 40},   /* SDL_CONTROLLER_BUTTON_LEFTSHOULDER */
        {controllerRect.x + 396, controllerRect.y + 36},  /* SDL_CONTROLLER_BUTTON_RIGHTSHOULDER */
        {controllerRect.x + 154, controllerRect.y + 188}, /* SDL_CONTROLLER_BUTTON_DPAD_UP */
        {controllerRect.x + 154, controllerRect.y + 249}, /* SDL_CONTROLLER_BUTTON_DPAD_DOWN */
        {controllerRect.x + 116, controllerRect.y + 217}, /* SDL_CONTROLLER_BUTTON_DPAD_LEFT */
        {controllerRect.x + 186, controllerRect.y + 217}, /* SDL_CONTROLLER_BUTTON_DPAD_RIGHT */
        {controllerRect.x + 232, controllerRect.y + 174}, /* SDL_CONTROLLER_BUTTON_MISC1 */
        {controllerRect.x + 132, controllerRect.y + 135}, /* SDL_CONTROLLER_BUTTON_PADDLE1 */
        {controllerRect.x + 330, controllerRect.y + 135}, /* SDL_CONTROLLER_BUTTON_PADDLE2 */
        {controllerRect.x + 132, controllerRect.y + 175}, /* SDL_CONTROLLER_BUTTON_PADDLE3 */
        {controllerRect.x + 330, controllerRect.y + 175}, /* SDL_CONTROLLER_BUTTON_PADDLE4 */
};
/* This is indexed by SDL_GameControllerAxis. */
static const struct {
    int x;
    int y;
    double angle;
} axis_positions[] = {
        {controllerRect.x + 74, controllerRect.y + 153, 270.0},  /* LEFTX */
        {controllerRect.x + 74, controllerRect.y + 153, 0.0},    /* LEFTY */
        {controllerRect.x + 306, controllerRect.y + 231, 270.0}, /* RIGHTX */
        {controllerRect.x + 306, controllerRect.y + 231, 0.0},   /* RIGHTY */
        {controllerRect.x + 91, controllerRect.y - 20, 0.0},     /* TRIGGERLEFT */
        {controllerRect.x + 375, controllerRect.y - 20, 0.0},    /* TRIGGERRIGHT */
};

static int findController(SDL_JoystickID controller_id) {
    for (int i = 0; i < num_controllers; ++i) {
        if (controller_id == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamecontrollers[i]))) {
            return i;
        }
    }
    return -1;
}

static void AddController(int device_index) {
    SDL_JoystickID controller_id = SDL_JoystickGetDeviceInstanceID(device_index);
    SDL_GameController *controller;
    SDL_GameController **controllers;
    controller_id = SDL_JoystickGetDeviceInstanceID(device_index);
    if (controller_id < 0) {
        return;
    }
    if (findController(controller_id) >= 0) {
        return;
    }
    controller = SDL_GameControllerOpen(device_index);
    if (!controller) {
        return;
    }
    controllers = (SDL_GameController **) SDL_realloc(gamecontrollers, (num_controllers + 1) * sizeof(*controllers));
    if (!controllers) {
        SDL_GameControllerClose(controller);
        return;
    }
    controllers[num_controllers++] = controller;
    gamecontrollers = controllers;
    gamecontroller = controller;
    if (SDL_GameControllerHasSensor(gamecontroller, SDL_SENSOR_ACCEL)) {
        SDL_GameControllerSetSensorEnabled(gamecontroller, SDL_SENSOR_ACCEL, SDL_TRUE);
    }
    if (SDL_GameControllerHasSensor(gamecontroller, SDL_SENSOR_GYRO)) {
        SDL_GameControllerSetSensorEnabled(gamecontroller, SDL_SENSOR_GYRO, SDL_TRUE);
    }
}

static void SetController(SDL_JoystickID controller) {
    int i = findController(controller);
    if (i < 0) {
        return;
    }
    if (gamecontroller != gamecontrollers[i]) {
        gamecontroller = gamecontrollers[i];
    }
}

static void DelController(SDL_JoystickID controller) {
    int i = findController(controller);
    if (i < 0) {
        return;
    }
    SDL_GameControllerClose(gamecontrollers[i]);
    --num_controllers;
    if (i < num_controllers) {
        SDL_memcpy(&gamecontrollers[i], &gamecontrollers[i + 1], (num_controllers - i) * sizeof(*gamecontrollers));
    }
    if (num_controllers > 0) {
        gamecontroller = gamecontrollers[0];
    } else {
        gamecontroller = nullptr;
    }
}

static SDL_Texture *LoadTexture(SDL_Renderer *renderer, const char *file, bool transparent) {
    SDL_Surface *temp = nullptr;
    SDL_Texture *texture = nullptr;
    temp = SDL_LoadBMP(file);
    if (temp) {
        /* Set transparent pixel as the pixel at (0,0) */
        if (transparent) {
            if (temp->format->BytesPerPixel == 1) {
                SDL_SetColorKey(temp, SDL_TRUE, *(Uint8 *) temp->pixels);
            }
        }
        texture = SDL_CreateTextureFromSurface(renderer, temp);
    }
    if (temp) {
        SDL_FreeSurface(temp);
    }
    return texture;
}

static uint16_t ConvertAxisToRumble(int16_t axis) {
    /* Only start rumbling if the axis is past the halfway point */
    const int16_t half_axis = (int16_t) SDL_ceil(SDL_JOYSTICK_AXIS_MAX / 2.0f);
    if (axis > half_axis) {
        return (int16_t) (axis - half_axis) * 4;
    } else {
        return 0;
    }
}

void loop() {
    SDL_Event event;
    int i;
    bool showing_front = true;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_CONTROLLERDEVICEADDED:
                AddController(event.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                DelController(event.cdevice.which);
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (event.caxis.value <= (-SDL_JOYSTICK_AXIS_MAX / 2) || event.caxis.value >= (SDL_JOYSTICK_AXIS_MAX / 2)) {
                    SetController(event.caxis.which);
                    currentController = event.caxis.which;
                }
                break;
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP:
                if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    SetController(event.cbutton.which);
                    currentController = event.cbutton.which;
                }
                break;
            case SDL_QUIT:
                done = SDL_TRUE;
                break;
            default:
                break;
        }
    }
    if (gamecontroller) {
        /* Show the back of the controller if the paddles are being held */
        for (i = SDL_CONTROLLER_BUTTON_PADDLE1; i <= SDL_CONTROLLER_BUTTON_PADDLE4; ++i) {
            if (SDL_GameControllerGetButton(gamecontroller, (SDL_GameControllerButton) i) == SDL_PRESSED) {
                showing_front = false;
                break;
            }
        }
    }
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, showing_front ? background_front : background_back, nullptr, &controllerRect);
    const char *currentControllerName = SDL_GameControllerNameForIndex(currentController);
    int textWidth = FC_GetWidth(font, currentControllerName);
    FC_Draw(font, renderer, (SCREEN_WIDTH / 2) - (textWidth / 2), 0, currentControllerName);
    if (gamecontroller) {
        /* Update visual controller state */
        for (i = 0; i < SDL_CONTROLLER_BUTTON_TOUCHPAD; ++i) {
            if (SDL_GameControllerGetButton(gamecontroller, (SDL_GameControllerButton) i) == SDL_PRESSED) {
                bool on_front = (i < SDL_CONTROLLER_BUTTON_PADDLE1 || i > SDL_CONTROLLER_BUTTON_PADDLE4);
                if (on_front == showing_front) {
                    const SDL_Rect dst = {button_positions[i].x, button_positions[i].y, 50, 50};
                    SDL_RenderCopyEx(renderer, button, nullptr, &dst, 0, nullptr, SDL_FLIP_NONE);
                }
            }
        }
        if (showing_front) {
            for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) {
                const int16_t deadzone = 8000; /* !!! FIXME: real deadzone */
                const int16_t value = SDL_GameControllerGetAxis(gamecontroller, (SDL_GameControllerAxis) (i));
                if (value < -deadzone) {
                    const SDL_Rect dst = {axis_positions[i].x, axis_positions[i].y, 50, 50};
                    const double angle = axis_positions[i].angle;
                    SDL_RenderCopyEx(renderer, axis, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
                } else if (value > deadzone) {
                    const SDL_Rect dst = {axis_positions[i].x, axis_positions[i].y, 50, 50};
                    const double angle = axis_positions[i].angle + 180.0;
                    SDL_RenderCopyEx(renderer, axis, nullptr, &dst, angle, nullptr, SDL_FLIP_NONE);
                }
            }
        }
        /* Update LED based on left thumbstick position */
        int16_t x = SDL_GameControllerGetAxis(gamecontroller, SDL_CONTROLLER_AXIS_LEFTX);
        int16_t y = SDL_GameControllerGetAxis(gamecontroller, SDL_CONTROLLER_AXIS_LEFTY);
        if (!set_LED) {
            set_LED = (x < -8000 || x > 8000 || y > 8000);
        }
        if (set_LED) {
            Uint8 r, g, b;
            if (x < 0) {
                r = (Uint8) (((int) (~x) * 255) / 32767);
                b = 0;
            } else {
                r = 0;
                b = (Uint8) (((int) (x) *255) / 32767);
            }
            if (y > 0) {
                g = (Uint8) (((int) (y) *255) / 32767);
            } else {
                g = 0;
            }
            SDL_GameControllerSetLED(gamecontroller, r, g, b);
        }

        /* Update rumble based on trigger state */
        int16_t left = SDL_GameControllerGetAxis(gamecontroller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        int16_t right = SDL_GameControllerGetAxis(gamecontroller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        uint16_t low_frequency_rumble = ConvertAxisToRumble(left);
        uint16_t high_frequency_rumble = ConvertAxisToRumble(right);
        SDL_GameControllerRumble(gamecontroller, low_frequency_rumble, high_frequency_rumble, 250);
        /* Update trigger rumble based on thumbstick state */
        left = SDL_GameControllerGetAxis(gamecontroller, SDL_CONTROLLER_AXIS_LEFTY);
        right = SDL_GameControllerGetAxis(gamecontroller, SDL_CONTROLLER_AXIS_RIGHTY);
        uint16_t left_rumble = ConvertAxisToRumble(~left);
        uint16_t right_rumble = ConvertAxisToRumble(~right);
        SDL_GameControllerRumbleTriggers(gamecontroller, left_rumble, right_rumble, 250);
    }
    SDL_RenderPresent(renderer);
}

int main() {
    romfsInit();
    int controller_index = 0;
    /* Enable standard application logging */
    /* Initialize SDL (Note: video is required to start event loop) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        romfsExit();
        return 1;
    }

    /* Create a window to display controller state */
    window = SDL_CreateWindow("Game Controller Test", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                              SCREEN_HEIGHT, 0);
    if (window == nullptr) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
        SDL_Quit();

        romfsExit();
        return 2;
    }
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == nullptr) {
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
        SDL_Quit();

        romfsExit();
        return 2;
    }

    void *ttf;
    size_t size;
    OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &ttf, &size);
    font = FC_CreateFont();
    if (font == nullptr) {
        SDL_Quit();
        return 1;
    }
    SDL_RWops *rw = SDL_RWFromConstMem(ttf, size);
    if (!FC_LoadFont_RW(font, renderer, rw, 1, FONT_SIZE, SCREEN_COLOR_WHITE, TTF_STYLE_NORMAL)) {
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    /* scale for platforms that don't give you the window size you asked for. */
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    background_front = LoadTexture(renderer, "romfs:/controllermap.bmp", false);
    background_back = LoadTexture(renderer, "romfs:/controllermap_back.bmp", false);
    button = LoadTexture(renderer, "romfs:/button.bmp", SDL_TRUE);
    axis = LoadTexture(renderer, "romfs:/axis.bmp", SDL_TRUE);
    if (!background_front || !background_back || !button || !axis) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return 2;
    }
    SDL_SetTextureColorMod(button, 10, 255, 21);
    SDL_SetTextureColorMod(axis, 10, 255, 21);
    if (controller_index < num_controllers) {
        gamecontroller = gamecontrollers[controller_index];
    } else {
        gamecontroller = nullptr;
    }
    /* Loop, getting controller events! */
    while (!done) {
        loop();
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    SDL_Quit();

    romfsExit();
    return 0;
}
