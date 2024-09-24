#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#define POINT_ZERO (SDL_Point){0,0}

#undef main

//Screen size
const uint16_t SCREEN_W = 640;
const uint16_t SCREEN_H = 640;

//Size constants
const uint8_t GRID_SIZE = 64;
const uint8_t CHUNK_SIZE = 120;
const uint8_t CHUNK_COLUMNS = 5;

//SDL rendering variables
SDL_Window* window;
SDL_Surface* windowSurface;
SDL_Renderer* windowRenderer;

//Event handler
SDL_Event event;

// structs and unions
typedef struct {
	char* src;
	SDL_Texture* texture;
	SDL_Point size;
} Texture;
typedef struct {
	uint8_t length;
	uint16_t interval;
	uint8_t* frames;
} Animation;
typedef struct {
	uint8_t frame;
	Uint32 lastFrame;
	Animation currAnim;
	float x;
	float y;
	uint8_t chunk;
	float velX;
	float velY;
} Entity;
typedef struct {
	uint8_t texture;
	uint8_t pos;
	uint8_t chunk;
	bool collision;
} Block;
union GameElement {
	Block block;
	Entity entity;
};
typedef struct {
	union GameElement elem;
	uint8_t type;
} MapElement;

// all textures
Texture gTextures[5] = {
	{"assets/player0.png",NULL,POINT_ZERO}, 	//0
	{"assets/player01.png",NULL,POINT_ZERO}, 	//1
	{"assets/player1.png",NULL,POINT_ZERO},		//2
	{"assets/player2.png",NULL,POINT_ZERO},		//3
	{"assets/player3.png",NULL,POINT_ZERO}		//4
};

//animations
const Animation PLAYERIDLEANIM = (Animation){2,750,(uint8_t[2]){0,1}};
const Animation PLAYERWALKANIM = (Animation){4,100,(uint8_t[5]){2,3,2,4,0}};

//global variables
uint32_t mapElements_i = 0;
MapElement* mapElements = NULL;
Entity player = {0,0,(Animation){0,0,(uint8_t[1]){0}},0.0,0.0,0,0.0,0.0};
SDL_Point camera;

SDL_Point normalizeVector(float x, float y){
	float magnitude = (float)sqrt((double)(x*x+y*y));
	return (SDL_Point){x/magnitude,y/magnitude};
}

//called to initialize all important variables and systems
bool init(){
	//try to initialize SDL
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0){
		printf("SDL init failed, SDL error: %s\n",SDL_GetError());
		return false;
	}else{
		//initialize window
		window = SDL_CreateWindow("RPG Game",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,SCREEN_W,SCREEN_H,SDL_WINDOW_SHOWN);
		if(window == NULL){
			printf("SDL create window failed, SDL error: %s\n",SDL_GetError());
			return false;
		}else{
			//create renderer
			windowRenderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
			if(windowRenderer == NULL){
				printf("SDL create renderer failed, SDL error: %s\n",SDL_GetError());
				return false;
			}else{SDL_SetRenderDrawColor(windowRenderer,0x00,0x00,0x00,0xFF);}
			
			//get window surface
			windowSurface = SDL_GetWindowSurface(window);
			if(windowSurface == NULL){
				printf("SDL get window surface failed, SDL error: %s",SDL_GetError());
			}
		}
	}
	return true;
}

// called to quit systems and free memory
void stopPrgrm(){
	SDL_DestroyRenderer(windowRenderer); // destroy renderer and free memory
	windowRenderer = NULL;
	
	SDL_FreeSurface(windowSurface); // free memory used by the surface
	windowSurface = NULL;
	
	SDL_DestroyWindow(window); // destroy window and free memory
	window = NULL;
	
	TTF_Quit(); // quit all systems
	IMG_Quit();
	SDL_Quit();
}

// load texture with a path
SDL_Texture* loadTexture(char* filePath){
	SDL_Texture* newTexture;
	SDL_Surface* loadedSurface = IMG_Load(filePath);
	if(loadedSurface == NULL){
		printf("IMG load on %s failed, IMG error: %s\n",filePath,IMG_GetError());
	}else{
		newTexture = SDL_CreateTextureFromSurface(windowRenderer,loadedSurface);
		if(newTexture == NULL){
			printf("SDL create texture from %s failed, SDL error: %s\n",filePath,SDL_GetError());
		}
		SDL_FreeSurface(loadedSurface);
	}
	return newTexture;
}// Debug version of loadTexture (prints if loading is successful)
SDL_Texture* loadTextureDebug(char* filePath){SDL_Texture* resultTexture = loadTexture(filePath);if(resultTexture!=NULL)printf("Loaded %s!\n",filePath);return resultTexture;}

// rendering functions
void renderTexture(SDL_Texture* textureToRender, SDL_Rect* clipRect, SDL_Rect* renderRect){
	SDL_RenderCopy(windowRenderer,textureToRender,clipRect,renderRect);
}void renderTextureEx(SDL_Texture* textureToRender, SDL_Rect* clipRect, SDL_Rect* renderRect, double angle, SDL_Point* center, SDL_RendererFlip flipFlag){
	SDL_RenderCopyEx(windowRenderer,textureToRender,clipRect,renderRect,angle,center,flipFlag);
}SDL_Point getTextureSize(SDL_Texture* textureToMeasure){
	SDL_Point textureSize;
	SDL_QueryTexture(textureToMeasure, NULL, NULL, &textureSize.x, &textureSize.y);
	return textureSize;
}

// geometric rendering functions
void setRenderDrawColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a){SDL_SetRenderDrawColor(windowRenderer,r,g,b,a);}
void renderFillRect(SDL_Rect targetRect){SDL_RenderFillRect(windowRenderer,&targetRect);}

//function to load all assets
bool loadMedia(){
	bool success = true;
	for(uint8_t i = 0; i < sizeof(gTextures)/sizeof(Texture); i++){
		gTextures[i].texture = loadTexture(gTextures[i].src);
		if(gTextures[i].texture == NULL){
			success = false;
		}else{
			gTextures[i].size = getTextureSize(gTextures[i].texture);
			printf("Successfully loaded asset from %s, Size: %d, %d\n",gTextures[i].src,gTextures[i].size.x,gTextures[i].size.y);
		}
	}return success;
}

//returns SDL_Rect with camera offset applied to it
SDL_Rect getTransformedSDLRect(float x, float y, float w, float h){
	return (SDL_Rect){(x-w/2)-camera.x,(y-h/2)-camera.y,w,h};
}

//struct specific functions
void Entity_Render(Entity targetEnt){
	SDL_Rect entTransformedRect = getTransformedSDLRect(targetEnt.x,targetEnt.y,GRID_SIZE,GRID_SIZE);
	SDL_RendererFlip entTextureFlipping = SDL_FLIP_NONE;
	if(targetEnt.velX < 0) entTextureFlipping = SDL_FLIP_HORIZONTAL;
	renderTextureEx(gTextures[targetEnt.currAnim.frames[targetEnt.frame]].texture,NULL,&entTransformedRect,0.0,NULL,entTextureFlipping);
}
void Entity_UpdateAnim(Entity* targetEnt, Uint32 currTicks){
	uint8_t currAnimLength = targetEnt->currAnim.length;
	if(currAnimLength <= 1){return;}
	if(currTicks-targetEnt->lastFrame >= targetEnt->currAnim.interval){
		targetEnt->lastFrame = currTicks;
		targetEnt->frame++;
		if(targetEnt->frame >= currAnimLength){targetEnt->frame = 0;}
	}
}
void Entity_SetAnim(Entity* targetEnt, Animation targetAnim){targetEnt->currAnim = targetAnim; targetEnt->frame = 0; targetEnt->lastFrame = SDL_GetTicks();}
bool Animation_Compare(Animation firstAnim, const Animation secondAnim){
	if(firstAnim.length == secondAnim.length && firstAnim.interval == secondAnim.interval){
		for(uint8_t i = 0; i < firstAnim.length; i++){
			if(firstAnim.frames[i] != secondAnim.frames[i]){return false;}
		}
	}else{return false;}
	return true;
}

// render all map items (maybe)
void render(double delta){
	setRenderDrawColor(255,255,255,255);
	renderFillRect(getTransformedSDLRect(-20,-20,40,40));
	Entity_Render(player);
}

void updateCamera(){camera.x = -SCREEN_W/2+player.x;camera.y = -SCREEN_H/2+player.y;}
void update(double delta){
	if(player.velX != 0 || player.velY != 0){
		if(!Animation_Compare(player.currAnim,PLAYERWALKANIM)) Entity_SetAnim(&player,PLAYERWALKANIM);
		player.x += player.velX * delta * 160;
		player.y += player.velY * delta * 160;
	}else{if(!Animation_Compare(player.currAnim,PLAYERIDLEANIM)) Entity_SetAnim(&player,PLAYERIDLEANIM);}
	
	Uint32 currTicks = SDL_GetTicks();
	Entity_UpdateAnim(&player,currTicks);
	
	updateCamera(delta);
}
// main function
int main(int argv, char* args[]){
	//call initializing function
	if(!init()){printf("failed to initialize something... quitting program.\n");return -1;}
	printf("successfully initialized SDL and other systems!\n\n"); // alert that init() was successfull
	
	//load all assets
	loadMedia();
	
	//setup player
	Entity_SetAnim(&player,PLAYERIDLEANIM);
	camera.x = -SCREEN_W/2+player.x;
	camera.y = -SCREEN_H/2+player.y;
	
	//setup map
	// -- malloc memory according to the map size
	// -- load map from json
	
	//deltaTime calculation variables
	Uint64 NOW = SDL_GetPerformanceCounter();
	Uint64 LAST = 0;
	double deltaTime = 0.0;
	
	//game loop
	bool running = true;
	while(running){
		//calculate deltaTime
		LAST = NOW;
		NOW = SDL_GetPerformanceCounter();
		deltaTime = (double)((NOW-LAST)*1000 / (double)SDL_GetPerformanceFrequency());
		deltaTime *= 0.001;
		
		//handle all events
		while(SDL_PollEvent(&event) != 0){
			switch(event.type){
				case SDL_QUIT:
					running = false;
					break;
				case SDL_KEYDOWN:
					switch(event.key.keysym.sym){
						case SDLK_w:
							player.velY = -1;
							break;
						case SDLK_s:
							player.velY = 1;
							break;
						case SDLK_a:
							player.velX = -1;
							break;
						case SDLK_d:
							player.velX = 1;
							break;
					}break;
				case SDL_KEYUP:
					switch(event.key.keysym.sym){
						case SDLK_w:
							if(player.velY < 0) player.velY = 0;
							break;
						case SDLK_s:
							if(player.velY > 0) player.velY = 0;
							break;
						case SDLK_a:
							if(player.velX < 0) player.velX = 0;
							break;
						case SDLK_d:
							if(player.velX > 0) player.velX = 0;
							break;
					}break;
			}
		}
		
		//fill screen with color
		setRenderDrawColor(100,100,100,255);
		SDL_RenderClear(windowRenderer);
		
		//update stuff
		update(deltaTime);
		
		//render stuff on screen
		render(deltaTime);
		
		//update stuff on screen
		SDL_RenderPresent(windowRenderer);
	}
	
	return 0;
}