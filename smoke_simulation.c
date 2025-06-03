#include <SDL.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define GRID_WIDTH 100
#define GRID_HEIGHT 75
#define CELL_SIZE 8
#define MOUSE_FORCE 0.5f
#define MOUSE_RADIUS 50

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
        grid[x][y].density += 0.5f;
        if (grid[x][y].density > 1.0f) grid[x][y].density = 1.0f;
        grid[x][y].temperature = 1.0f;
        grid[x][y].velocity_y = -0.5f + random_float(-0.1f, 0.1f);
        grid[x][y].velocity_x = random_float(-0.1f, 0.1f);
    }
}

void apply_mouse_force() {
    int grid_mouse_x = mouse_x / CELL_SIZE;
    int grid_mouse_y = mouse_y / CELL_SIZE;
    
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            if (grid[x][y].density > 0.1f) {
                float dx = grid_mouse_x - x;
                float dy = grid_mouse_y - y;
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

// Function to enforce boundary conditions
// b = 0 for scalar (density, temp), 1 for vx, 2 for vy
void set_bnd(int b, Cell field[GRID_WIDTH][GRID_HEIGHT]) {
    for (int i = 1; i < GRID_WIDTH - 1; i++) {
        if (b == 1) { // vx
            field[0][i].velocity_x = -field[1][i].velocity_x;
            field[GRID_WIDTH - 1][i].velocity_x = -field[GRID_WIDTH - 2][i].velocity_x;
            field[i][0].velocity_x = field[i][1].velocity_x; // Tangential velocity copied
            field[i][GRID_HEIGHT - 1].velocity_x = field[i][GRID_HEIGHT - 2].velocity_x; // Tangential velocity copied
        } else if (b == 2) { // vy
            field[i][0].velocity_y = -field[i][1].velocity_y;
            field[i][GRID_HEIGHT - 1].velocity_y = -field[i][GRID_HEIGHT - 2].velocity_y;
            field[0][i].velocity_y = field[1][i].velocity_y; // Tangential velocity copied
            field[GRID_WIDTH - 1][i].velocity_y = field[GRID_WIDTH - 2][i].velocity_y; // Tangential velocity copied
        } else { // b == 0 for scalar (density, temp)
            field[i][0] = field[i][1];
            field[i][GRID_HEIGHT - 1] = field[i][GRID_HEIGHT - 2];
            field[0][i] = field[1][i];
            field[GRID_WIDTH - 1][i] = field[GRID_WIDTH - 2][i];
        }
    }

    // Corners (average adjacent values)
    if (b == 0) { // Scalar (density, temp)
        field[0][0].density = (field[1][0].density + field[0][1].density) * 0.5f;
        field[0][GRID_HEIGHT - 1].density = (field[1][GRID_HEIGHT - 1].density + field[0][GRID_HEIGHT - 2].density) * 0.5f;
        field[GRID_WIDTH - 1][0].density = (field[GRID_WIDTH - 2][0].density + field[GRID_WIDTH - 1][1].density) * 0.5f;
        field[GRID_WIDTH - 1][GRID_HEIGHT - 1].density = (field[GRID_WIDTH - 2][GRID_HEIGHT - 1].density + field[GRID_WIDTH - 1][GRID_HEIGHT - 2].density) * 0.5f;

        field[0][0].temperature = (field[1][0].temperature + field[0][1].temperature) * 0.5f;
        field[0][GRID_HEIGHT - 1].temperature = (field[1][GRID_HEIGHT - 1].temperature + field[0][GRID_HEIGHT - 2].temperature) * 0.5f;
        field[GRID_WIDTH - 1][0].temperature = (field[GRID_WIDTH - 2][0].temperature + field[GRID_WIDTH - 1][1].temperature) * 0.5f;
        field[GRID_WIDTH - 1][GRID_HEIGHT - 1].temperature = (field[GRID_WIDTH - 2][GRID_HEIGHT - 1].temperature + field[GRID_WIDTH - 1][GRID_HEIGHT - 2].temperature) * 0.5f;

    } else if (b == 1) { // vx
        field[0][0].velocity_x = (field[1][0].velocity_x + field[0][1].velocity_x) * 0.5f;
        field[0][GRID_HEIGHT - 1].velocity_x = (field[1][GRID_HEIGHT - 1].velocity_x + field[0][GRID_HEIGHT - 2].velocity_x) * 0.5f;
        field[GRID_WIDTH - 1][0].velocity_x = (field[GRID_WIDTH - 2][0].velocity_x + field[GRID_WIDTH - 1][1].velocity_x) * 0.5f;
        field[GRID_WIDTH - 1][GRID_HEIGHT - 1].velocity_x = (field[GRID_WIDTH - 2][GRID_HEIGHT - 1].velocity_x + field[GRID_WIDTH - 1][GRID_HEIGHT - 2].velocity_x) * 0.5f;

    } else if (b == 2) { // vy
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
    // Simple iterative solver (Jacobi or similar)
    for (int iter = 0; iter < 40; iter++) { // Increased iterations for pressure solve
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

// Function to add turbulence (simple noise)
void add_turbulence(float amount) {
    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            // Simple noise based on position and a changing factor
            float noise_x = (float)(rand() % 201 - 100) / 100.0f; // Range -1 to 1
            float noise_y = (float)(rand() % 201 - 100) / 100.0f; // Range -1 to 1

            grid[x][y].velocity_x += noise_x * amount;
            grid[x][y].velocity_y += noise_y * amount;
        }
    }
}

// Function to apply viscosity
void add_viscosity(float amount) {
    // A simple diffusion step for velocity
    // This is a simplified approach and a proper viscous solve is more complex
    for (int iter = 0; iter < 4; iter++) { // Number of iterations for viscosity diffusion
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
    set_bnd(1, grid); // Apply boundary conditions after viscosity for vx
    set_bnd(2, grid); // Apply boundary conditions after viscosity for vy
}

void update_simulation() {
    // --- Fluid Simulation Update Cycle ---

    // 1. Advection: Move quantities (density, velocity, temperature) based on current velocity
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

            if (prev_x < 0.5f) prev_x = 0.5f;
            if (prev_x > GRID_WIDTH - 1.5f) prev_x = GRID_WIDTH - 1.5f;
            if (prev_y < 0.5f) prev_y = 0.5f;
            if (prev_y > GRID_HEIGHT - 1.5f) prev_y = GRID_HEIGHT - 1.5f;

            int x0 = floor(prev_x);
            int y0 = floor(prev_y);
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
    set_bnd(0, grid); // Apply boundary conditions for density and temp after advection
    set_bnd(1, grid); // Apply boundary conditions for vx after advection
    set_bnd(2, grid); // Apply boundary conditions for vy after advection

    // 2. Add External Forces (buoyancy, mouse)
    for (int x = 1; x < GRID_WIDTH - 1; x++) {
        for (int y = 1; y < GRID_HEIGHT - 1; y++) {
            // Buoyancy force based on temperature and density
            grid[x][y].velocity_y -= grid[x][y].density * grid[x][y].temperature * 0.2f;
        }
    }
    apply_mouse_force();

    // 3. Add Turbulence
    add_turbulence(0.05f);

    set_bnd(1, grid); // Apply boundary conditions for vx after forces and turbulence
    set_bnd(2, grid); // Apply boundary conditions for vy after forces and turbulence

    // 4. Apply Viscosity
     for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            temp_grid[x][y] = grid[x][y];
        }
    }
    for (int iter = 0; iter < 4; iter++) { // Iterations for viscosity diffusion
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            for (int y = 1; y < GRID_HEIGHT - 1; y++) {
                grid[x][y].velocity_x = (
                    temp_grid[x][y].velocity_x + 
                    0.01f * (grid[x-1][y].velocity_x + grid[x+1][y].velocity_x +
                              grid[x][y-1].velocity_x + grid[x][y+1].velocity_x))
                    / (1 + 4 * 0.01f);

                grid[x][y].velocity_y = (
                    temp_grid[x][y].velocity_y + 
                    0.01f * (grid[x-1][y].velocity_y + grid[x+1][y].velocity_y +
                              grid[x][y-1].velocity_y + grid[x][y+1].velocity_y))
                    / (1 + 4 * 0.01f);
            }
        }
        set_bnd(1, grid); // Apply boundary conditions after viscosity iteration
        set_bnd(2, grid); // Apply boundary conditions after viscosity iteration
    }

    // 5. Project (make velocity divergence-free using pressure)
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            pressure_grid[x][y].density = 0.0f; // Initialize pressure grid to zero
        }
    }
    calculate_divergence();

    for (int iter = 0; iter < 40; iter++) { // Iterations for pressure solve
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            for (int y = 1; y < GRID_HEIGHT - 1; y++) {
                pressure_grid[x][y].density = 
                    (pressure_grid[x-1][y].density + pressure_grid[x+1][y].density +
                     pressure_grid[x][y-1].density + pressure_grid[x][y+1].density -
                     divergence_grid[x][y].density) * 0.25f;
            }
        }
        set_bnd(0, pressure_grid); // Apply boundary conditions for pressure after iteration
    }

    apply_pressure();
    set_bnd(1, grid); // Apply boundary conditions for vx after pressure solve
    set_bnd(2, grid); // Apply boundary conditions for vy after pressure solve

    // 6. Dissipation
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            grid[x][y].density *= 0.998f;
            grid[x][y].temperature *= 0.998f;
        }
    }

    add_viscosity(0.05f); // Further increased viscosity amount
}

void render_simulation(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            if (grid[x][y].density > 0.01f) {
                float temp = grid[x][y].temperature;
                int alpha = (int)(grid[x][y].density * 255);
                if (alpha > 255) alpha = 255;
                
                // Color based on temperature
                int r = (int)(200 + temp * 55);
                int g = (int)(200 + temp * 55);
                int b = (int)(200 + temp * 55);
                
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

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL initialization failed: %s\n", SDL_GetError());
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
            }
        }
        
        // Add smoke from multiple points for a more natural look
        add_smoke(GRID_WIDTH / 2, GRID_HEIGHT - 2);
        add_smoke(GRID_WIDTH / 2 - 1, GRID_HEIGHT - 2);
        add_smoke(GRID_WIDTH / 2 + 1, GRID_HEIGHT - 2);
        
        update_simulation();
        render_simulation(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
} 