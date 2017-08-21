#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <cstring>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include "LTexture.h"

struct my_msgbuf {
	long mtype;
	char mtext[200];
};

class App {
	public:
		void run();
		void timeFreq(float, float);
		void octave(int);
		void press_ctrl(int);
		void unpress_ctrl(int);
		void set_switch(int);
		void unset_switch(int);
		void finish();

	private:
		void get_message(char *);
		const int SCREEN_WIDTH = 640;
		const int SCREEN_HEIGHT = 400;

		float tempo = 0.0f, freq = 0.0f;
		int oitava = 4;
		bool key_pressed[12] = { false, false, false, false, false, false, false, false, false, false, false, false};
		bool switch_turned[5] = { false, false, false, false, false };
		int kp_w[7] = {0, 2, 4, 5, 7, 9, 11};
		int kp_b[5] = {1, 3, 6, 8, 10};
		const char *notes[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
		bool quit = false;

		SDL_Window *gWindow = NULL;

		SDL_Renderer *gRenderer = NULL;

		SDL_Surface *gScreenSurface = NULL;

		LTexture gBackgroundTexture;
		LTexture gBackgroundAltTexture;

		LTexture gOctave[4];

		LTexture gInactiveChord[12];
		LTexture gActiveChord[12];

		LTexture gSwitches[4];

		LTexture gWhite[7];
		LTexture gBlack[5];
		LTexture gWhitePressed[7];
		LTexture gBlackPressed[5];

		LTexture gTextTempo;
		LTexture gTextFreq;
		LTexture gTextNote;

		TTF_Font *gFont = NULL;

		int switches_x[3] = { 167, 206, 243} ;
		int switches_y[3] = { 152, 152, 152};
		int bk_x[5] = { 64, 101, 175, 212, 249 };

		bool init();
		bool loadMedia();
		void close();
};

inline void App::run() {
	if (!init()) {
		printf("Failed to initialize!\n");
	} else {
		if (!loadMedia()) {
			printf("Failed to load media!\n");
		} else {
			struct my_msgbuf buf;
			int msqid;
			key_t key;

			if ((key = ftok("app.cpp", 'B')) == -1) {
				perror("ftok");
				exit(1);
			}

			if ((msqid = msgget(key, 0644)) == -1) {
				perror("ftok");
				exit(1);
			}

			SDL_Event e;

			int offset = 0;
			SDL_Rect switch_button, white_key, black_key;
			switch_button.x = 0;
			switch_button.y = 0;
			switch_button.w = 10;
			switch_button.h = 10;

			white_key.x = 0;
			white_key.y = 0;
			white_key.w = 37;
			white_key.h = 180;

			black_key.x = 0;
			black_key.y = 0;
			black_key.w = 27;
			black_key.h = 90;

			while (!quit) {
				while (SDL_PollEvent(&e) != 0) {
					if (e.type == SDL_QUIT) {
						this->finish();
					} else if (e.type == SDL_KEYDOWN) {
						 
					}
				}
				SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);
				SDL_RenderClear(gRenderer);
				if (this->oitava < 4)
				gBackgroundTexture.render(0, 0, NULL, gRenderer);
				else
				gBackgroundAltTexture.render(0, 0, NULL, gRenderer);

				gTextFreq.render(360, 210, NULL, gRenderer);
				gTextTempo.render(360, 294, NULL, gRenderer);
				gTextNote.render(360, 252, NULL, gRenderer);

				for (int i = 0; i < 4; i++) {
					if (i < 3) {
						if (switch_turned[i]) {
							offset = 5;
						}
						gSwitches[i].render(switches_x[i], switches_y[i] - offset, &switch_button, gRenderer);
						offset = 0;

					}
					gOctave[i].render(40 + (i*23), 148, NULL, gRenderer);
				}

				for (int i = 0; i < 7; i++) {
					if (!key_pressed[kp_w[i]]) 
						gWhite[i].render((i*white_key.w)+40, 180, &white_key, gRenderer);
					else
						gWhitePressed[i].render((i*white_key.w)+40, 180, &white_key, gRenderer);

				}

				for (int i = 0; i < 5; i++) {
					if (!key_pressed[kp_b[i]]) 
						gBlack[i].render(bk_x[i], 180, &black_key, gRenderer);
					else
						gBlackPressed[i].render(bk_x[i], 180, &black_key, gRenderer);
				}

				SDL_RenderPresent(gRenderer);

				if (msgrcv(msqid, &buf, sizeof buf.mtext, 0, IPC_NOWAIT) == -1) {
					if (errno == ENOMSG) {
						continue;
					}
					perror("msgrcv");
					exit(1);
				}
				get_message(buf.mtext);
			}
		}
	}

	close();
}

inline bool App::init() {
	//Initialization flag
	bool success = true;

	//Start SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		success = false;
	} else {
		//Set rendering to linear
		if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
			printf("Warning: Linear texture filtering not enabled!");
		}

		//Create window
		gWindow = SDL_CreateWindow("IHS SoundMaker", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
		if (!gWindow) {
			printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
			success = false;
		} else {
			//Create vsynced renderer for window
			gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_SOFTWARE);
			if (!gRenderer) {
				printf( "Renderer could not be created! SDL Error: %s\n", SDL_GetError());
				success = false;
			} else {
				//Initialize renderer color
				SDL_SetRenderDrawColor(gRenderer, 0xFF, 0xFF, 0xFF, 0xFF);

				//Initialize PNG loading
				int imgFlags = IMG_INIT_PNG;
				if (!(IMG_Init(imgFlags) & imgFlags)) {
					printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
					success = false;
				}

				if (TTF_Init() == -1) {
					printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
					success = false;
				}
			}
		}
	}

	return success;
}

inline bool App::loadMedia() {
	//Initialization flag
	bool success = true;

	gBackgroundTexture.loadFromFile("Background.png", gRenderer);
	gBackgroundAltTexture.loadFromFile("Background2.png", gRenderer);

	for (int i = 0; i < 12; i++) {
		char path[30];
		sprintf(path, "%d.bmp", i);

		gActiveChord[i].loadFromFile(path, gRenderer);

		sprintf(path, "%di.bmp", i);
		gInactiveChord[i].loadFromFile(path, gRenderer);
	}

	for (int i = 0; i < 7; i++) {
		gWhite[i].loadFromFile("white_key.png", gRenderer);
		gWhitePressed[i].loadFromFile("white_key_pressed.png", gRenderer);
	}

	for (int i = 0; i < 5; i++) {
		gBlack[i].loadFromFile("black_key.png", gRenderer);
		gBlackPressed[i].loadFromFile("black_key_pressed.png", gRenderer);
	}

	for (int i = 0; i < 4; i++) {
		gOctave[i].loadFromFile("oct_unactive.png", gRenderer);
		if (i == 0)
			gOctave[i].loadFromFile("oct_active.png", gRenderer);
		if (i < 3)
		gSwitches[i].loadFromFile("black.png", gRenderer);
	}

	gFont = TTF_OpenFont("alienleaguebold.ttf", 24);
	if (!gFont) {
		printf("Failed to load font! SDL_ttf Error: %s\n", TTF_GetError());
		success = false;
	} else {
		SDL_Color textColor = { 0xFF, 0xFF, 0xFF, 0xFF };
		if (!gTextFreq.loadFromRenderedText("Frequencia: 0.0", textColor, gFont, gRenderer)) {
			printf("Failed to render texture!");
			success = false;
		}
		if (!gTextTempo.loadFromRenderedText("Tempo: 0.0", textColor, gFont, gRenderer)) {
			printf("Failed to render texture!");
			success = false;
		}
		if (!gTextNote.loadFromRenderedText("Nota: ", textColor, gFont, gRenderer)) {
			printf("Failed to render texture!");
			success = false;
		}
	}

	//Load media
	return success;
}

inline void App::close() {
	//Deallocate surface
	for (int i = 0; i < 12; i++) {
		gActiveChord[i].free();
		gInactiveChord[i].free();
	}

	gTextFreq.free();
	gTextTempo.free();
	gTextNote.free();


	//Destroy window
	SDL_DestroyWindow(gWindow);
	SDL_DestroyRenderer(gRenderer);
	gWindow = NULL;
	gRenderer = NULL;

	TTF_CloseFont(gFont);
	gFont = NULL;

	//Close SDL and subsystems
	IMG_Quit();
	SDL_Quit();
}

inline void App::finish() {
	this->quit = true;
}

inline void App::octave(int oitava) {
	gOctave[this->oitava].loadFromFile("oct_unactive.png", gRenderer);
	this->oitava = oitava;
	gOctave[this->oitava].loadFromFile("oct_active.png", gRenderer);
}

inline void App::set_switch(int tmp) {
	this->switch_turned[tmp] = true;
}

inline void App::unset_switch(int tmp) {
	this->switch_turned[tmp] = false;
}

inline void App::press_ctrl(int tmp) {
	this->key_pressed[tmp] = true;

	SDL_Color textColor = { 0xFF, 0xFF, 0xFF, 0xFF };
	std::stringstream sstm;
	sstm << "Note: " << notes[tmp];
	std::string inputNote = sstm.str();
	this->gTextNote.loadFromRenderedText(inputNote, textColor, gFont, gRenderer);

}

inline void App::unpress_ctrl(int tmp) {
	this->key_pressed[tmp] = false;

	SDL_Color textColor = { 0xFF, 0xFF, 0xFF, 0xFF };
	this->gTextNote.loadFromRenderedText("Note: ", textColor, gFont, gRenderer);

}

inline void App::timeFreq(float tempo, float freq) {
	this->tempo = tempo;
	this->freq = freq;

	SDL_Color textColor = { 0xFF, 0xFF, 0xFF, 0xFF };

	std::stringstream sstm;
	sstm << "Frequencia: " << freq;
	std::string inputFreq = sstm.str();
	this->gTextFreq.loadFromRenderedText(inputFreq, textColor, gFont, gRenderer);

	sstm.str(std::string());
	sstm << "Tempo: " << tempo;
	std::string inputTempo = sstm.str();
	this->gTextTempo.loadFromRenderedText(inputTempo, textColor, gFont, gRenderer);
}

inline void App::get_message(char *request) {
	char buf[4];
	memcpy(buf, &request[0], 3);
	buf[3] = '\0';

	if (strcmp(buf, "tfr") == 0) {
		char *pSep = strchr(request, '_');
		float f1 = strtof(&request[3], &pSep);
		float f2 = strtof(&request[pSep-request+1], NULL);
		this->timeFreq(f1, f2);
	} else if (strcmp(buf, "oct") == 0) {
		int i = atoi(&request[3]);
		this->octave(i);
	} else if (strcmp(buf, "pct") == 0) {
		int i = atoi(&request[3]);
		this->press_ctrl(i);
	} else if (strcmp(buf, "uct") == 0) {
		int i = atoi(&request[3]);
		this->unpress_ctrl(i);
	} else if (strcmp(buf, "std") == 0) {
		int i = atoi(&request[3]);
		this->set_switch(i);
	} else if (strcmp(buf, "utd") == 0) {
		int i = atoi(&request[3]);
		this->unset_switch(i);
	} else if (strcmp(buf, "fin") == 0) {
		printf("teste\n");
		this->finish();
	} 
}
