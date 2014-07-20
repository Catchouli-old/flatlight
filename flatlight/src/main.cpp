#include <string.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <glm/glm.hpp>

#include <SDL2/SDL.h>
#undef main

struct colour_t
{
	uint32_t r : 8;
	uint32_t g : 8;
	uint32_t b : 8;
	uint32_t a : 8;
};

struct light_t
{
	glm::vec3 colour;
	glm::vec2 pos;
};

void pause();
void createWindow(int width, int height, SDL_Window** window, SDL_Renderer** renderer);
void loadLevel(const char* filename, char** level, int* level_width, int* level_height);
void setTile(uint32_t* pixels, int x, int y, int colour);
bool raycast(int startx, int starty, int endx, int endy);

// Constants
const char* LEVEL_FILENAME = "level.txt";

const int TILE_WIDTH = 16;
const int TILE_HEIGHT = 16;

const int INITIAL_WIDTH = 800;
const int INITIAL_HEIGHT = 600;

// Window width and height
int width = INITIAL_WIDTH;
int height = INITIAL_HEIGHT;

// Colour buffer
uint32_t* pixels;

// Level width, height, and buffer
int level_width;
int level_height;
char* level;

// Default light settings
light_t lights[] =
{
	light_t{ glm::vec3(1.0f, 0.0f, 0.0f), glm::ivec2(28, 9) },
	light_t{ glm::vec3(0.0f, 1.0f, 0.0f), glm::ivec2(53, 10) },
	light_t{ glm::vec3(0.0f, 0.0f, 1.0f), glm::ivec2(14, 34) },
	light_t{ glm::vec3(1.0f, 1.0f, 1.0f), glm::ivec2(34, 31) }
};

const int LIGHT_COUNT = sizeof(lights) / sizeof(light_t);

int main(int argc, char** argv)
{
	// State
	bool running = true;
	int cur_light = -1;

	// Window references
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;

	// Load level
	loadLevel(LEVEL_FILENAME, &level, &level_width, &level_height);

	// Set window size based on level size
	width = level_width * TILE_WIDTH;
	height = level_height * TILE_HEIGHT;

	// Initialise SDL
	SDL_Init(SDL_INIT_VIDEO);

	// Create window
	createWindow(width, height, &window, &renderer);

	// Create render texture
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_STATIC, level_width, level_height);

	// Create colour buffer
	pixels = new uint32_t[level_width * level_height];

	// Main loop
	while (running)
	{
		// Handle events
		SDL_Event e;

		while (SDL_PollEvent(&e))
		{
			switch (e.type)
			{
			case SDL_KEYDOWN:
				if (e.key.state == SDL_PRESSED && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
				{
					running = false;
				}
				break;
			case SDL_MOUSEBUTTONDOWN:
				if (e.button.button == SDL_BUTTON_LEFT)
				{
					int tile_x = e.button.x / TILE_WIDTH;
					int tile_y = e.button.y / TILE_HEIGHT;

					char tile = level[tile_y * level_width + tile_x];

					if (tile == '#')
						level[tile_y * level_width + tile_x] = '%';
					else
						level[tile_y * level_width + tile_x] = '#';
				}
				else if (e.button.button == SDL_BUTTON_RIGHT)
				{
					int tile_x = e.button.x / TILE_WIDTH;
					int tile_y = e.button.y / TILE_HEIGHT;

					cur_light = -1;

					for (int i = 0; i < LIGHT_COUNT; ++i)
					{
						if (tile_x == lights[i].pos.x && tile_y == lights[i].pos.y)
							cur_light = i;
					}
				}
				break;
			case SDL_MOUSEMOTION:
				if (cur_light != -1)
				{
					int tile_x = e.motion.x / TILE_WIDTH;
					int tile_y = e.motion.y / TILE_HEIGHT;

					lights[cur_light].pos = glm::ivec2(tile_x, tile_y);
				}
			case SDL_MOUSEBUTTONUP:
				if (e.button.button == SDL_BUTTON_RIGHT)
				{
					printf("Light dropped: (%f, %f)\n", lights[cur_light].pos.x, lights[cur_light].pos.y);
					
					cur_light = -1;
				}
				break;
			case SDL_QUIT:
				running = false;
				break;
			}
		}

		// Clear screen to 0 (black)
		memset(pixels, 0, level_width * level_height * sizeof(uint32_t));

		// Render
		for (int y = 0; y < level_height; ++y)
		{
			for (int x = 0; x < level_width; ++x)
			{
				switch (level[y * level_width + x])
				{
				case '#':
					// Wall
					setTile(pixels, x, y, 0x808080);
					break;
				default:
					// Air
					glm::vec3 tile_col;

					// Check lighting
					// For each light
					for (int i = 0; i < LIGHT_COUNT; ++i)
					{
						const light_t& light = lights[i];

						if (!raycast(light.pos.x, light.pos.y, x, y))
						{
							float diffx = x - light.pos.x;
							float diffy = y - light.pos.y;

							float dist = sqrt(diffx * diffx + diffy * diffy);

							const float a = 0.1f;
							const float b = 0.1f;

							float att = 1.0f / (1.0f + a*dist + b*dist*dist);

							tile_col += light.colour * att;
						}
					}

					// Clamp and convert to colour
					tile_col = glm::clamp(tile_col, 0.0f, 1.0f);

					glm::vec3 tile_col_255 = tile_col * 255.0f;

					colour_t final_tile_col{(uint8_t)tile_col_255.r,
											(uint8_t)tile_col_255.g,
											(uint8_t)tile_col_255.b,
											1.0f };

					setTile(pixels, x, y, *(uint32_t*)&final_tile_col);

					break;
				}
			}
		}

		// Update render texture from colour buffer
		SDL_UpdateTexture(texture, nullptr, pixels, level_width * sizeof(uint32_t));

		SDL_Rect src_rect;
		SDL_Rect dest_rect;

		src_rect.x = 0;
		src_rect.y = 0;
		src_rect.w = level_width;
		src_rect.h = level_height;

		dest_rect.x = 0;
		dest_rect.y = 0;
		dest_rect.w = width;
		dest_rect.h = height;

		// Render to window
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, &src_rect, &dest_rect);
		SDL_RenderPresent(renderer);
	}

	// Clean up SDL and exit program
	delete[] pixels;
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}

void pause()
{
	// Pause and wait for input
	printf("\nPress return to continue\n");
	getchar();
}

void createWindow(int width, int height, SDL_Window** window, SDL_Renderer** renderer)
{
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, 0);

	// Create window
	*window = SDL_CreateWindow("flatlight - place tiles with left click and drag lights with right",
							   SDL_WINDOWPOS_UNDEFINED,
							   SDL_WINDOWPOS_UNDEFINED,
							   width,
							   height,
							   SDL_WINDOW_SHOWN);

	if (window == nullptr)
	{
		fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
		SDL_Quit();

		pause();

		exit(1);
	}

	// Create renderer
	*renderer = SDL_CreateRenderer(*window, -1, 0);
}

void loadLevel(const char* filename, char** level, int* level_width, int* level_height)
{
	char buf[1024];

	int width, height;

	// Open file
	FILE* file = fopen(filename, "r");

	if (file == nullptr)
	{
		fprintf(stderr, "Failed to open file for reading: %s\n", filename);

		pause();

		exit(1);
	}

	// Get length of first line for width
	char* line = fgets(buf, 1024, file);

	if (line == nullptr)
	{
		*level = nullptr;
		*level_width = 0;
		*level_height = 0;

		return;
	}

	// Get width
	width = strlen(line);

	// Remove newline from width if existent
	if (line[width - 1] = '\n')
		width--;

	// Initialise height to 1 because we already read a line
	height = 1;

	// Read the rest of the lines to get height
	while (fgets(buf, 1024, file))
	{
		height++;
	}

	// Rewind the file for reading
	fseek(file, 0, SEEK_SET);

	// Initialise arrays
	*level = new char[width * height];
	*level_width = width;
	*level_height = height;

	// Load level
	int y = 0;
	while (line = fgets(buf, 1024, file))
	{
		// Get length of line
		int length = strlen(line);

		// Remove newline from length if existent
		if (line[length - 1] == '\n')
			length--;

		// If length isn't the same as width, quit
		if (length != width)
		{
			fprintf(stderr, "Length of line differs from width %d (l = %d, w = %d)\n", y, length, width);

			pause();

			exit(1);
		}

		// Copy to array
		memcpy(&(*level)[y * width], buf, length);

		// Increment y
		y++;
	}

	// Output stats
	printf("Level height: %d, level width: %d\n", height, width);
}

void setTile(uint32_t* pixels, int x, int y, int colour)
{
	pixels[y * level_width + x] = colour;
}

// A fast raycast that skips to the next tile along the ray in a grid
// An implementation of John Amanatides (1987) - A fast voxel traversal algorithm for ray tracing
bool raycast(int startx, int starty, int endx, int endy)
{
	// Hit if the start tile is obstructed
	if (level[starty * level_width + startx] == '#')
	{
		return true;
	}

	// No hit if the start tile is the end tile
	if (startx == endx && starty == endy)
	{
		return false;
	}

	float diffx = endx - startx;
	float diffy = endy - starty;

	float length = sqrt(diffx * diffx + diffy * diffy);

	float dx = diffx / length;
	float dy = diffy / length;

	if (abs(dx) < 0.0001f)
		dx = 0.0001f;
	if (abs(dy) < 0.0001f)
		dy = 0.0001f;

	int cur_tile_x = startx;
	int cur_tile_y = starty;

	// Calculate coefficient and bias for tx, ty calculation
	// The calculation is tx = (x - x0) / dx, ty = (y - y0) / dy
	// Which is derived from x = x0 + tdx, y = y0 + ydx
	// And can then be simplified to a multiply add
	// tx = x * (1/dx) + (- x0 / dx)
	// or tx = x * dx_coeff + dx_bias
	const float dx_coeff = 1.0f / dx;
	const float dy_coeff = 1.0f / dy;

	const float dx_bias = -(startx / dx);
	const float dy_bias = -(starty / dy);

	int dx_step = (dx > 0 ? 1 : -1);
	int dy_step = (dy > 0 ? 1 : -1);

	while (cur_tile_x != endx || cur_tile_y != endy)
	{
		int next_x = cur_tile_x + dx_step;
		int next_y = cur_tile_y + dy_step;

		// Calculate next tx and ty value
		float tx = next_x * dx_coeff + dx_bias;
		float ty = next_y * dy_coeff + dy_bias;

		if (tx < ty)
			cur_tile_x = next_x;
		else
			cur_tile_y = next_y;

		// If tile blocked, return true (hit)
		if (level[cur_tile_y * level_width + cur_tile_x] == '#')
		{
			return true;
		}
	}

	// Made it to (endx, endy), return false (hit)
	return false;
}
