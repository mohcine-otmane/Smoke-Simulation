#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define WINDOW_WIDTH 100*4
#define WINDOW_HEIGHT 75*4
#define GRID_WIDTH 100*2
#define GRID_HEIGHT 75*2
#define CELL_SIZE 2
#define MOUSE_FORCE 0.5f
#define MOUSE_RADIUS 50
#define PRESSURE_ITERATIONS 100
#define VISCOSITY_ITERATIONS 2
#define TURBULENCE_AMOUNT 0.08f
#define VORTICITY_STRENGTH 0.015f
#define DENSITY_DECAY 0.998f
#define TEMPERATURE_DECAY 0.998f

// UI Constants
#define BUTTON_WIDTH 120
#define BUTTON_HEIGHT 40
#define SLIDER_WIDTH 200
#define SLIDER_HEIGHT 20
#define UI_PADDING 10
#define FONT_SIZE 20

typedef struct {
    float density;
    float velocity_x;
    float velocity_y;
    float temperature;
} Cell;

Cell grid[GRID_WIDTH][GRID_HEIGHT];
Cell temp_grid[GRID_WIDTH][GRID_HEIGHT];
Cell divergence_grid[GRID_WIDTH][GRID_HEIGHT];
Cell pressure_grid[GRID_WIDTH][GRID_HEIGHT];
int mouse_x = 0;
int mouse_y = 0;
int mouse_clicked = 0;
int window_dragging = 0;
float emission_density_amount = 0.25f;

typedef struct {
    SDL_Rect rect;
    SDL_Color color;
    SDL_Color hover_color;
    SDL_Color text_color;
    char* label;
    SDL_Texture* texture;
    int is_hovered;
    int is_pressed;
} Button;

typedef struct {
    SDL_Rect rect;
    SDL_Rect handle;
    SDL_Color color;
    SDL_Color handle_color;
    float value;
    float min_value;
    float max_value;
    int is_dragging;
} Slider;

// Global UI elements
Button emission_button;
Button reset_button;
Slider emission_slider;
TTF_Font* font;
int emission_enabled = 1;

float random_float(float min, float max) {
    return min + ((float)rand() / RAND_MAX) * (max - min);
}

void init_grid() {
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            grid[x][y].density = 0.0f;
            grid[x][y].velocity_x = 0.0f;
            grid[x][y].velocity_y = 0.0f;
            grid[x][y].temperature = 0.0f;
        }
    }
}

void add_smoke(int x, int y) {
    if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
        grid[x][y].density += emission_density_amount;
        grid[x][y].temperature = 1.0f + random_float(-0.2f, 0.2f);
        if (grid[x][y].temperature < 0.5f) grid[x][y].temperature = 0.5f;
        
        // Add more dynamic initial velocity
        float angle = random_float(0, 2 * 3.14159f);
        float speed = random_float(0.3f, 0.7f);
        grid[x][y].velocity_y = -0.5f + speed * sinf(angle);
        grid[x][y].velocity_x = speed * cosf(angle);
    }
}

void apply_mouse_force() {
    if (!mouse_clicked && !window_dragging) return;
    
    int grid_mouse_x = mouse_x / CELL_SIZE;
    int grid_mouse_y = mouse_y / CELL_SIZE;
    
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            if (grid[x][y].density > 0.1f) {
                float dx = x - grid_mouse_x;
                float dy = y - grid_mouse_y;
                float distance = sqrtf(dx * dx + dy * dy);
                
                if (distance < MOUSE_RADIUS) {
                    if (distance < 1e-6) distance = 1e-6;

                    float force = (1.0f - distance / MOUSE_RADIUS) * MOUSE_FORCE;
                    grid[x][y].velocity_x += (dx / distance) * force;
                    grid[x][y].velocity_y += (dy / distance) * force;
                }
            }
        }
    }
}

void set_bnd(int b, Cell field[GRID_WIDTH][GRID_HEIGHT]) {
    for (int i = 1; i < GRID_WIDTH - 1; i++) {
        if (b == 1) {
            field[0][i].velocity_x = -field[1][i].velocity_x;
            field[GRID_WIDTH - 1][i].velocity_x = -field[GRID_WIDTH - 2][i].velocity_x;
            field[i][0].velocity_x = field[i][1].velocity_x;
            field[i][GRID_HEIGHT - 1].velocity_x = field[i][GRID_HEIGHT - 2].velocity_x;
        } else if (b == 2) {
            field[i][0].velocity_y = -field[i][1].velocity_y;
            field[i][GRID_HEIGHT - 1].velocity_y = -field[i][GRID_HEIGHT - 2].velocity_y;
            field[0][i].velocity_y = field[1][i].velocity_y;
            field[GRID_WIDTH - 1][i].velocity_y = field[GRID_WIDTH - 2][i].velocity_y;
        } else {
            field[i][0] = field[i][1];
            field[i][GRID_HEIGHT - 1] = field[i][GRID_HEIGHT - 2];
            field[0][i] = field[1][i];
            field[GRID_WIDTH - 1][i] = field[GRID_WIDTH - 2][i];
        }
    }

    if (b == 0) {
        field[0][0].density = (field[1][0].density + field[0][1].density) * 0.5f;
        field[0][GRID_HEIGHT - 1].density = (field[1][GRID_HEIGHT - 1].density + field[0][GRID_HEIGHT - 2].density) * 0.5f;
        field[GRID_WIDTH - 1][0].density = (field[GRID_WIDTH - 2][0].density + field[GRID_WIDTH - 1][1].density) * 0.5f;
        field[GRID_WIDTH - 1][GRID_HEIGHT - 1].density = (field[GRID_WIDTH - 2][GRID_HEIGHT - 1].density + field[GRID_WIDTH - 1][GRID_HEIGHT - 2].density) * 0.5f;

        field[0][0].temperature = (field[1][0].temperature + field[0][1].temperature) * 0.5f;
        field[0][GRID_HEIGHT - 1].temperature = (field[1][GRID_HEIGHT - 1].temperature + field[0][GRID_HEIGHT - 2].temperature) * 0.5f;
        field[GRID_WIDTH - 1][0].temperature = (field[GRID_WIDTH - 2][0].temperature + field[GRID_WIDTH - 1][1].temperature) * 0.5f;
        field[GRID_WIDTH - 1][GRID_HEIGHT - 1].temperature = (field[GRID_WIDTH - 2][GRID_HEIGHT - 1].temperature + field[GRID_WIDTH - 1][GRID_HEIGHT - 2].temperature) * 0.5f;

    } else if (b == 1) {
        field[0][0].velocity_x = (field[1][0].velocity_x + field[0][1].velocity_x) * 0.5f;
        field[0][GRID_HEIGHT - 1].velocity_x = (field[1][GRID_HEIGHT - 1].velocity_x + field[0][GRID_HEIGHT - 2].velocity_x) * 0.5f;
        field[GRID_WIDTH - 1][0].velocity_x = (field[GRID_WIDTH - 2][0].velocity_x + field[GRID_WIDTH - 1][1].velocity_x) * 0.5f;
        field[GRID_WIDTH - 1][GRID_HEIGHT - 1].velocity_x = (field[GRID_WIDTH - 2][GRID_HEIGHT - 1].velocity_x + field[GRID_WIDTH - 1][GRID_HEIGHT - 2].velocity_x) * 0.5f;

    } else if (b == 2) {
        field[0][0].velocity_y = (field[1][0].velocity_y + field[0][1].velocity_y) * 0.5f;
        field[0][GRID_HEIGHT - 1].velocity_y = (field[1][GRID_HEIGHT - 1].velocity_y + field[0][GRID_HEIGHT - 2].velocity_y) * 0.5f;
        field[GRID_WIDTH - 1][0].velocity_y = (field[GRID_WIDTH - 2][0].velocity_y + field[GRID_WIDTH - 1][1].velocity_y) * 0.5f;
        field[GRID_WIDTH - 1][GRID_HEIGHT - 1].velocity_y = (field[GRID_WIDTH - 2][GRID_HEIGHT - 1].velocity_y + field[GRID_WIDTH - 1][GRID_HEIGHT - 2].velocity_y) * 0.5f;
    }
}

void calculate_divergence() {
    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            divergence_grid[x][y].density = 
                (grid[x+1][y].velocity_x - grid[x-1][y].velocity_x + 
                 grid[x][y+1].velocity_y - grid[x][y-1].velocity_y) * 0.5f;
        }
    }
}

void solve_pressure() {
    for (int iter = 0; iter < 40; iter++) {
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            for (int y = 1; y < GRID_HEIGHT - 1; y++) {
                pressure_grid[x][y].density = 
                    (pressure_grid[x-1][y].density + pressure_grid[x+1][y].density +
                     pressure_grid[x][y-1].density + pressure_grid[x][y+1].density -
                     divergence_grid[x][y].density) * 0.25f;
            }
        }
    }
}

void apply_pressure() {
    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            grid[x][y].velocity_x -= (pressure_grid[x+1][y].density - pressure_grid[x-1][y].density) * 0.5f;
            grid[x][y].velocity_y -= (pressure_grid[x][y+1].density - pressure_grid[x][y-1].density) * 0.5f;
        }
    }
    set_bnd(1, grid);
    set_bnd(2, grid);
}

void add_turbulence(float amount) {
    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            float noise_x = (float)(rand() % 201 - 100) / 100.0f;
            float noise_y = (float)(rand() % 201 - 100) / 100.0f;

            grid[x][y].velocity_x += noise_x * amount;
            grid[x][y].velocity_y += noise_y * amount;
        }
    }
}

void add_viscosity(float amount) {
    for (int iter = 0; iter < 4; iter++) {
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            for (int y = 1; y < GRID_HEIGHT - 1; y++) {
                grid[x][y].velocity_x = (
                    grid[x][y].velocity_x + 
                    amount * (grid[x-1][y].velocity_x + grid[x+1][y].velocity_x +
                              grid[x][y-1].velocity_x + grid[x][y+1].velocity_x))
                    / (1 + 4 * amount);

                grid[x][y].velocity_y = (
                    grid[x][y].velocity_y + 
                    amount * (grid[x-1][y].velocity_y + grid[x+1][y].velocity_y +
                              grid[x][y-1].velocity_y + grid[x][y+1].velocity_y))
                    / (1 + 4 * amount);
            }
        }
    }
    set_bnd(1, grid);
    set_bnd(2, grid);
}

void calculate_vorticity() {
    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            divergence_grid[x][y].density = 
                (grid[x+1][y].velocity_y - grid[x-1][y].velocity_y) * 0.5f -
                (grid[x][y+1].velocity_x - grid[x][y-1].velocity_x) * 0.5f;
        }
    }
}

void apply_vorticity_confinement(float strength) {
    calculate_vorticity();

    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            float omega = divergence_grid[x][y].density;

            float grad_omega_x = (fabs(divergence_grid[x+1][y].density) - fabs(divergence_grid[x-1][y].density)) * 0.5f;
            float grad_omega_y = (fabs(divergence_grid[x][y+1].density) - fabs(divergence_grid[x][y-1].density)) * 0.5f;

            float grad_omega_mag = sqrtf(grad_omega_x * grad_omega_x + grad_omega_y * grad_omega_y);

            if (grad_omega_mag > 1e-6) {
                float Nx = grad_omega_x / grad_omega_mag;
                float Ny = grad_omega_y / grad_omega_mag;

                float force_x = Ny * omega;
                float force_y = -Nx * omega;

                grid[x][y].velocity_x += force_x * strength;
                grid[x][y].velocity_y += force_y * strength;
            }
        }
    }
}

void update_simulation() {
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            temp_grid[x][y] = grid[x][y];
        }
    }

    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            float vx = temp_grid[x][y].velocity_x;
            float vy = temp_grid[x][y].velocity_y;

            float prev_x = x - vx;
            float prev_y = y - vy;

            prev_x = fmaxf(0.5f, fminf(GRID_WIDTH - 1.5f, prev_x));
            prev_y = fmaxf(0.5f, fminf(GRID_HEIGHT - 1.5f, prev_y));

            int x0 = (int)prev_x;
            int y0 = (int)prev_y;
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            float s1 = prev_x - x0;
            float s0 = 1.0f - s1;
            float t1 = prev_y - y0;
            float t0 = 1.0f - t1;

            grid[x][y].density = s0 * (t0 * temp_grid[x0][y0].density + t1 * temp_grid[x0][y1].density) +
                                s1 * (t0 * temp_grid[x1][y0].density + t1 * temp_grid[x1][y1].density);

            grid[x][y].temperature = s0 * (t0 * temp_grid[x0][y0].temperature + t1 * temp_grid[x0][y1].temperature) +
                                    s1 * (t0 * temp_grid[x1][y0].temperature + t1 * temp_grid[x1][y1].temperature);

            grid[x][y].velocity_x = s0 * (t0 * temp_grid[x0][y0].velocity_x + t1 * temp_grid[x0][y1].velocity_x) +
                                  s1 * (t0 * temp_grid[x1][y0].velocity_x + t1 * temp_grid[x1][y1].velocity_x);

            grid[x][y].velocity_y = s0 * (t0 * temp_grid[x0][y0].velocity_y + t1 * temp_grid[x0][y1].velocity_y) +
                                  s1 * (t0 * temp_grid[x1][y0].velocity_y + t1 * temp_grid[x1][y1].velocity_y);
        }
    }
    set_bnd(0, grid);
    set_bnd(1, grid);
    set_bnd(2, grid);

    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            grid[x][y].velocity_y -= grid[x][y].density * grid[x][y].temperature * 0.15f;
        }
    }
    apply_mouse_force();

    add_turbulence(TURBULENCE_AMOUNT);

    apply_vorticity_confinement(VORTICITY_STRENGTH);

    set_bnd(1, grid);
    set_bnd(2, grid);

    for (int iter = 0; iter < VISCOSITY_ITERATIONS; iter++) {
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            for (int y = 1; y < GRID_HEIGHT - 1; y++) {
                grid[x][y].velocity_x = (
                    grid[x][y].velocity_x + 
                    0.008f * (grid[x-1][y].velocity_x + grid[x+1][y].velocity_x +
                              grid[x][y-1].velocity_x + grid[x][y+1].velocity_x))
                    / (1 + 4 * 0.008f);

                grid[x][y].velocity_y = (
                    grid[x][y].velocity_y + 
                    0.008f * (grid[x-1][y].velocity_y + grid[x+1][y].velocity_y +
                              grid[x][y-1].velocity_y + grid[x][y+1].velocity_y))
                    / (1 + 4 * 0.008f);
            }
        }
        set_bnd(1, grid);
        set_bnd(2, grid);
    }

    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            pressure_grid[x][y].density = 0.0f;
        }
    }
    calculate_divergence();

    for (int iter = 0; iter < PRESSURE_ITERATIONS; iter++) {
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            for (int y = 1; y < GRID_HEIGHT - 1; y++) {
                pressure_grid[x][y].density = 
                    (pressure_grid[x-1][y].density + pressure_grid[x+1][y].density +
                     pressure_grid[x][y-1].density + pressure_grid[x][y+1].density -
                     divergence_grid[x][y].density) * 0.25f;
            }
        }
        set_bnd(0, pressure_grid);
    }

    apply_pressure();
    set_bnd(1, grid);
    set_bnd(2, grid);

    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            grid[x][y].density *= DENSITY_DECAY;
            grid[x][y].temperature *= TEMPERATURE_DECAY;
        }
    }

    add_viscosity(0.05f);
}

void render_simulation(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            if (grid[x][y].density > 0.005f) {
                float density = grid[x][y].density;
                float temp = grid[x][y].temperature;
                
                // Enhanced color calculation
                float heat = temp * temp;
                float intensity = density * density;
                
                int r = (int)((heat * 255 + intensity * 150) * (1.0f + sinf(temp * 3.14159f) * 0.2f));
                int g = (int)((heat * 200 + intensity * 100) * (1.0f + sinf(temp * 2.0f) * 0.2f));
                int b = (int)((heat * 100 + intensity * 50) * (1.0f + sinf(temp * 1.5f) * 0.2f));

                r = fminf(255, fmaxf(0, r));
                g = fminf(255, fmaxf(0, g));
                b = fminf(255, fmaxf(0, b));

                int alpha = (int)(pow(density, 0.6f) * (200 + temp * 150));
                alpha = fminf(255, fmaxf(0, alpha));
                
                SDL_SetRenderDrawColor(renderer, r, g, b, alpha);
                SDL_Rect rect = {
                    x * CELL_SIZE,
                    y * CELL_SIZE,
                    CELL_SIZE,
                    CELL_SIZE
                };
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

Button create_button(int x, int y, int w, int h, SDL_Color color, SDL_Color hover_color, SDL_Color text_color, char* label, SDL_Renderer* renderer) {
    Button button;
    button.rect = (SDL_Rect){x, y, w, h};
    button.color = color;
    button.hover_color = hover_color;
    button.text_color = text_color;
    button.label = label;
    button.is_hovered = 0;
    button.is_pressed = 0;

    SDL_Surface* surface = TTF_RenderText_Solid(font, label, text_color);
    button.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    return button;
}

Slider create_slider(int x, int y, int w, int h, float min_val, float max_val, float initial_val, SDL_Color color, SDL_Color handle_color, SDL_Renderer* renderer) {
    Slider slider;
    slider.rect = (SDL_Rect){x, y, w, h};
    slider.handle = (SDL_Rect){x, y, h, h};
    slider.color = color;
    slider.handle_color = handle_color;
    slider.value = initial_val;
    slider.min_value = min_val;
    slider.max_value = max_val;
    slider.is_dragging = 0;

    // Set initial handle position
    float normalized_value = (initial_val - min_val) / (max_val - min_val);
    slider.handle.x = x + (int)(normalized_value * (w - h));

    return slider;
}

void render_button(Button* button, SDL_Renderer* renderer) {
    SDL_Color color = button->is_hovered ? button->hover_color : button->color;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &button->rect);

    // Draw button border
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &button->rect);

    // Center the text
    int text_w, text_h;
    SDL_QueryTexture(button->texture, NULL, NULL, &text_w, &text_h);
    SDL_Rect text_rect = {
        button->rect.x + (button->rect.w - text_w) / 2,
        button->rect.y + (button->rect.h - text_h) / 2,
        text_w,
        text_h
    };
    SDL_RenderCopy(renderer, button->texture, NULL, &text_rect);
}

void render_slider(Slider* slider, SDL_Renderer* renderer) {
    // Draw slider track
    SDL_SetRenderDrawColor(renderer, slider->color.r, slider->color.g, slider->color.b, slider->color.a);
    SDL_RenderFillRect(renderer, &slider->rect);

    // Draw slider handle
    SDL_SetRenderDrawColor(renderer, slider->handle_color.r, slider->handle_color.g, slider->handle_color.b, slider->handle_color.a);
    SDL_RenderFillRect(renderer, &slider->handle);
}

void update_slider_value(Slider* slider, int mouse_x) {
    if (mouse_x < slider->rect.x) mouse_x = slider->rect.x;
    if (mouse_x > slider->rect.x + slider->rect.w - slider->handle.w) 
        mouse_x = slider->rect.x + slider->rect.w - slider->handle.w;

    slider->handle.x = mouse_x;
    float normalized_value = (float)(mouse_x - slider->rect.x) / (slider->rect.w - slider->handle.w);
    slider->value = slider->min_value + normalized_value * (slider->max_value - slider->min_value);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        printf("TTF initialization failed: %s\n", TTF_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Smoke Simulation",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Initialize font
    font = TTF_OpenFont("arial.ttf", FONT_SIZE);
    if (!font) {
        printf("Font loading failed: %s\n", TTF_GetError());
        // Continue without font, but UI won't have text
    }

    // Create UI elements
    SDL_Color button_color = {50, 50, 50, 255};
    SDL_Color button_hover_color = {70, 70, 70, 255};
    SDL_Color text_color = {255, 255, 255, 255};
    SDL_Color slider_color = {30, 30, 30, 255};
    SDL_Color handle_color = {100, 100, 100, 255};

    emission_button = create_button(
        UI_PADDING,
        UI_PADDING,
        BUTTON_WIDTH,
        BUTTON_HEIGHT,
        button_color,
        button_hover_color,
        text_color,
        "Emission: ON",
        renderer
    );

    reset_button = create_button(
        UI_PADDING,
        UI_PADDING * 2 + BUTTON_HEIGHT,
        BUTTON_WIDTH,
        BUTTON_HEIGHT,
        button_color,
        button_hover_color,
        text_color,
        "Reset",
        renderer
    );

    emission_slider = create_slider(
        UI_PADDING,
        UI_PADDING * 3 + BUTTON_HEIGHT * 2,
        SLIDER_WIDTH,
        SLIDER_HEIGHT,
        0.0f,
        1.0f,
        emission_density_amount,
        slider_color,
        handle_color,
        renderer
    );

    init_grid();
    srand(time(NULL));

    int quit = 0;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            else if (e.type == SDL_MOUSEMOTION) {
                mouse_x = e.motion.x;
                mouse_y = e.motion.y;

                // Update button hover states
                emission_button.is_hovered = (mouse_x >= emission_button.rect.x && 
                    mouse_x < emission_button.rect.x + emission_button.rect.w &&
                    mouse_y >= emission_button.rect.y && 
                    mouse_y < emission_button.rect.y + emission_button.rect.h);

                reset_button.is_hovered = (mouse_x >= reset_button.rect.x && 
                    mouse_x < reset_button.rect.x + reset_button.rect.w &&
                    mouse_y >= reset_button.rect.y && 
                    mouse_y < reset_button.rect.y + reset_button.rect.h);

                // Update slider if dragging
                if (emission_slider.is_dragging) {
                    update_slider_value(&emission_slider, mouse_x);
                    emission_density_amount = emission_slider.value;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouse_clicked = 1;

                    // Check button clicks
                    if (emission_button.is_hovered) {
                        emission_enabled = !emission_enabled;
                        SDL_DestroyTexture(emission_button.texture);
                        emission_button.texture = SDL_CreateTextureFromSurface(renderer,
                            TTF_RenderText_Solid(font, 
                                emission_enabled ? "Emission: ON" : "Emission: OFF",
                                text_color));
                    }
                    else if (reset_button.is_hovered) {
                        init_grid();
                    }

                    // Check slider drag
                    if (mouse_x >= emission_slider.handle.x && 
                        mouse_x < emission_slider.handle.x + emission_slider.handle.w &&
                        mouse_y >= emission_slider.handle.y && 
                        mouse_y < emission_slider.handle.y + emission_slider.handle.h) {
                        emission_slider.is_dragging = 1;
                    }
                }
            }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    mouse_clicked = 0;
                    window_dragging = 0;
                    emission_slider.is_dragging = 0;
                }
            }
        }
        
        // Update simulation
        if (emission_enabled) {
            // Candle base
            for (int i = -3; i <= 3; i++) {
                add_smoke(GRID_WIDTH / 2 + i, GRID_HEIGHT - 2);
            }
            
            // Candle middle
            for (int i = -2; i <= 2; i++) {
                add_smoke(GRID_WIDTH / 2 + i, GRID_HEIGHT - 3);
            }
            
            // Candle top
            for (int i = -1; i <= 1; i++) {
                add_smoke(GRID_WIDTH / 2 + i, GRID_HEIGHT - 4);
            }
            
            // Candle tip
            add_smoke(GRID_WIDTH / 2, GRID_HEIGHT - 5);
        }
        
        update_simulation();
        
        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        render_simulation(renderer);
        
        // Render UI
        render_button(&emission_button, renderer);
        render_button(&reset_button, renderer);
        render_slider(&emission_slider, renderer);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // Cleanup
    SDL_DestroyTexture(emission_button.texture);
    SDL_DestroyTexture(reset_button.texture);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
} 