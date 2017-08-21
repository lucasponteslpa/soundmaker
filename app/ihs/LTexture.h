#pragma once
#include <string>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

//Texture wrapper class
class LTexture
{
	public:
		//Initializes variables
		LTexture();

		//Deallocates memory
		~LTexture();

		//Loads image at specified path
		bool loadFromFile(std::string path, SDL_Renderer *gRenderer);

		//Creates image from string
		bool loadFromRenderedText(std::string textureText, SDL_Color textColor, TTF_Font *gFont, SDL_Renderer *gRenderer);

		//Deallocates texture
		void free();

		//Renders texture at given point
		void render(int x, int y, SDL_Rect *clip = NULL, SDL_Renderer *gRenderer = NULL);

		//Gets image dimension
		int getWidth();
		int getHeight();

	private:
		//The actual hardware texture
		SDL_Texture *texture_;

		//Image dimensions
		int width_;
		int height_;
};
