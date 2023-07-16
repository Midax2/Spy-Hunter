#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<windows.h>
#include<time.h>


extern "C" {
#include"./SDL2-2.0.10/include/SDL.h"
#include"./SDL2-2.0.10/include/SDL_main.h"
}

#define SCREEN_WIDTH	1280 // rozmiar ekranu w poziomie
#define SCREEN_HEIGHT	720 // rozmiar ekranu w pionie
#define DL_TRAWA SCREEN_WIDTH / 16 // dlugość jednego bloku trawy
#define ODS_TEXT_SAVE SCREEN_WIDTH / 16 // odstęp tekstowy pomiędzy save'ami
#define MNOZNIK_TRAWA 10 // prędkość przesuwania się trawy trawy
#define ODSTEP_TRAWA SCREEN_WIDTH / 2 // odstęp pomiędzy kolejnymi blokami trawy
#define POLA_POZA_GRA 40 // pola na informacje dodatkowe
#define ROZMIAR_CAR 32 // rozmiar samochodu
#define NALICZANE_PKT 50 // ilość pubktów naliczanych graczowi co sekundę
#define PUSTE_PIKSELE 0 // oznaczenie pustych pikseli bez koloru
#define POZOSTALY_CZAS 100 - czas->worldTime // pozostały graczowi czas gry
#define DL_TEXT 128 // dlugość zmiennej do przechowywania tekstu
#define DL_DATA 20 // dlugość zmiennej do przechowywania daty
#define DL_SAVE 50 // dlugość zmiennej do przechowywania save'u
#define ODS_SAVE SCREEN_WIDTH / 9 // odstęp pomiędzy wyrysowaniem save'u

// główbe elementy gry
typedef struct {
	SDL_Event event;
	SDL_Surface* screen, * charset;
	SDL_Texture* scrtex;
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Surface* car, *game_over;
	int **pixels;
	int wyb_save;
	int x, y;
	bool pauza, quit, koniec, load;
	int pkt;
	int sort;
}gra_t;

// elementy do obliczania czasu gry
typedef struct {
	int t1, t2, t3, frames;
	double delta, worldTime, fpsTimer, fps;
}czas_t;

// kolory używane w grze
typedef struct {
	int czarny;
	int zielony;
	int czerwony;
	int niebieski;
}kolory_t;

// elementy do zapisywania/wczytywania save'ów
typedef struct {
	int ile_saves;
	char** save;
}saves_t;

// elementy do zapisywania/wczytywania wyników gry
typedef struct {
	int ile_results;
	char** czas;
	char** pkt;
}results_t;

// narysowanie napisu txt na powierzchni screen, zaczynaj№c od punktu (x, y)
// charset to bitmapa 128x128 zawieraj№ca znaki
// draw a text txt on surface screen, starting from the point (x, y)
// charset is a 128x128 bitmap containing character images
void DrawString(SDL_Surface *screen, int x, int y, const char *text,
                SDL_Surface *charset) {
	int px, py, c;
	SDL_Rect s, d;
	s.w = 8;
	s.h = 8;
	d.w = 8;
	d.h = 8;
	while(*text) {
		c = *text & 255;
		px = (c % 16) * 8;
		py = (c / 16) * 8;
		s.x = px;
		s.y = py;
		d.x = x;
		d.y = y;
		SDL_BlitSurface(charset, &s, screen, &d);
		x += 8;
		text++;
		};
	};


// narysowanie na ekranie screen powierzchni sprite w punkcie (x, y)
// (x, y) to punkt њrodka obrazka sprite na ekranie
// draw a surface sprite on a surface screen in point (x, y)
// (x, y) is the center of sprite on screen
void DrawSurface(SDL_Surface *screen, SDL_Surface *sprite, int x, int y) {
	SDL_Rect dest;
	dest.x = x - sprite->w / 2;
	dest.y = y - sprite->h / 2;
	dest.w = sprite->w;
	dest.h = sprite->h;
	SDL_BlitSurface(sprite, NULL, screen, &dest);
	};


// rysowanie pojedynczego pixela
// draw a single pixel
void DrawPixel(SDL_Surface* surface, int x, int y, Uint32 color, gra_t *gra) {
	if (x <= SCREEN_WIDTH && x >= 0 && y <= SCREEN_HEIGHT && y >= 0) {
		int bpp = surface->format->BytesPerPixel;
		Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;
		*(Uint32*)p = color;
		gra->pixels[y][x] = color;
	}
}


// rysowanie linii o dіugoњci l w pionie (gdy dx = 0, dy = 1) 
// b№dџ poziomie (gdy dx = 1, dy = 0)
// draw a vertical (when dx = 0, dy = 1) or horizontal (when dx = 1, dy = 0) line
void DrawLine(SDL_Surface *screen, int x, int y, int l, int dx, int dy, Uint32 color, gra_t *gra) {
	for(int i = 0; i < l; i++) {
		DrawPixel(screen, x, y, color, gra);
		x += dx;
		y += dy;
		};
	};


// rysowanie prostok№ta o dіugoњci bokуw l i k
// draw a rectangle of size l by k
void DrawRectangle(SDL_Surface *screen, int x, int y, int l, int k,
                   Uint32 outlineColor, Uint32 fillColor, gra_t *gra) {
	int i;
	DrawLine(screen, x, y, k, 0, 1, outlineColor, gra);
	DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor, gra);
	DrawLine(screen, x, y, l, 1, 0, outlineColor, gra);
	DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor, gra);
	for(i = y + 1; i < y + k - 1; i++)
		DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor, gra);
	};

// wyczyszczanie gry
void clear(gra_t *gra) {
	for (int i = 0; i < SCREEN_HEIGHT; i++) {
		free(gra->pixels[i]);
	}
	free(gra->pixels);
	SDL_FreeSurface(gra->screen);
	SDL_DestroyTexture(gra->scrtex);
	SDL_DestroyWindow(gra->window);
	SDL_DestroyRenderer(gra->renderer);
	SDL_Quit();
}

// sprawdzenie czy podany plik istnieje
bool czy_plik_istnieje(const char file[]) {
	FILE* plik;
	if (plik = fopen(file, "r")) {
		fclose(plik);
		return true;
	}
	return false;
}

// pobranie z pliku poprzednich rezultatów gry
void laduj_res(FILE* res, results_t *rezultaty) {
	rezultaty->czas = (char**)malloc((rezultaty->ile_results) * sizeof(char*));
	for (int i = 0; i < rezultaty->ile_results; i++)
		rezultaty->czas[i] = (char*)malloc(DL_TEXT * sizeof(char));

	rezultaty->pkt = (char**)malloc((rezultaty->ile_results) * sizeof(char*));
	for (int i = 0; i < rezultaty->ile_results; i++)
		rezultaty->pkt[i] = (char*)malloc(DL_TEXT * sizeof(char));

	for (int i = 0; i < rezultaty->ile_results - 1; i++) {
		for (int j = 0; rezultaty->czas[i][j - 1] != '\0'; j++) {
			fread(&rezultaty->czas[i][j], sizeof(char), 1, res);
		}
	}
	for (int i = 0; i < rezultaty->ile_results - 1; i++) {
		for (int j = 0; rezultaty->pkt[i][j - 1] != '\0'; j++) {
			fread(&rezultaty->pkt[i][j], sizeof(char), 1, res);
		}
	}
}

// zapisanie obecnego rezultatu
void save_res(results_t *rezultaty, gra_t *gra, czas_t *czas) {
	char text[DL_TEXT];
	if (rezultaty->ile_results != 1) {
		FILE* res = fopen("results", "r+");
		laduj_res(res, rezultaty);
		fclose(res);
	}
	sprintf(text, "%.0lf", czas->worldTime);
	strcpy(rezultaty->czas[rezultaty->ile_results - 1], text);
	sprintf(text, "%d", gra->pkt);
	strcpy(rezultaty->pkt[rezultaty->ile_results - 1], text);
	FILE* res = fopen("results", "w+");
	for (int i = 0; i < rezultaty->ile_results; i++) {
		for (int j = 0; rezultaty->czas[i][j - 1]; j++) {
			fwrite(&rezultaty->czas[i][j], sizeof(char), 1, res);
		}
	}
	for (int i = 0; i < rezultaty->ile_results; i++) {
		for (int j = 0; rezultaty->pkt[i][j - 1]; j++) {
			fwrite(&rezultaty->pkt[i][j], sizeof(char), 1, res);
		}
	}
	fclose(res);
	for (int i = 0; i < rezultaty->ile_results; i++) {
		free(rezultaty->czas[i]);
	}
	free(rezultaty->czas);
	for (int i = 0; i < rezultaty->ile_results; i++) {
		free(rezultaty->pkt[i]);
	}
	free(rezultaty->pkt);
	rezultaty->ile_results++;
}

// pobranie z systemu obecnej daty i czasu
void data(char *text) {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	sprintf(text, "%d-%02d-%02d_%02d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// pobranie save'ów z pliku i wczytanie ich do programu
void laduj_save(FILE* saved, saves_t *savea) {
	savea->save = (char**)malloc((savea->ile_saves) * sizeof(char*));
	for (int i = 0; i < savea->ile_saves; i++)
		savea->save[i] = (char*)malloc(DL_DATA * sizeof(char));

	for (int i = 0; i < savea->ile_saves; i++) {
		for (int j = 0; j < DL_DATA; j++) {
			fread(&savea->save[i][j], sizeof(char), 1, saved);
		}
	}
}

// zapisanie stanu gry
void save(gra_t *gra, czas_t *czas, saves_t *savea) {
	if (savea->ile_saves != 1) {
		FILE* saved = fopen("saves", "r+");
		laduj_save(saved, savea);
		fclose(saved);
	}
	
	char date[DL_DATA];
	data(date);
	FILE* save = fopen(date, "w+");
	fwrite(&czas->worldTime, sizeof(double), 1, save);
	fwrite(&gra->x, sizeof(int), 1, save);
	fwrite(&gra->y, sizeof(int), 1, save);
	fwrite(&gra->pkt, sizeof(int), 1, save);
	fwrite(&gra->koniec, sizeof(bool), 1, save);
	for (int i = 0; i <= SCREEN_HEIGHT; i++) {
		for (int j = 0; j <= SCREEN_WIDTH; j++) {
			fwrite(&gra->pixels[i][j], sizeof(int), 1, save);
		}
	}
	fclose(save);
	strcpy(savea->save[savea->ile_saves - 1], date);
	FILE* savee = fopen("saves", "w+");
	for (int i = 0; i < savea->ile_saves; i++) {
		for (int j = 0; j < DL_DATA; j++) {
			fwrite(&savea->save[i][j], sizeof(char), 1, savee);
		}
	}
	fclose(savee);
	for (int i = 0; i < savea->ile_saves; i++) {
		free(savea->save[i]);
	}
	free(savea->save);
	savea->ile_saves++;
}

// podświetlenie obecnie wybranego save'a do wczytania
int kolor_save(int poziom, int teraz, kolory_t *kolory) {
	if (poziom == teraz)
		return kolory->zielony;
	else if (poziom == 5 && teraz > 5)
		return kolory->zielony;
	else
		return kolory->niebieski;
}

// wybór save'a do wczytania przez gracza
void wybor_save(gra_t *gra, kolory_t *kolory, saves_t *saves) {
	SDL_FillRect(gra->screen, NULL, kolory->czarny);
	for (int i = 0; i < 6; i++) {
		DrawRectangle(gra->screen, SCREEN_WIDTH / 3, SCREEN_HEIGHT / 3 + (i * DL_SAVE), SCREEN_WIDTH / 3, DL_SAVE, kolory->czerwony, kolor_save(i, gra->wyb_save, kolory), gra);
		if (i == 0)
			DrawString(gra->screen, SCREEN_WIDTH / 3 + ODS_TEXT_SAVE, SCREEN_HEIGHT / 3 + 5, "Wybierz save. Kliknij 'e', by potwierdzic", gra->charset);
		if (gra->wyb_save > 5 && i != 0) {
			DrawString(gra->screen, SCREEN_WIDTH / 3 + ODS_SAVE, SCREEN_HEIGHT / 3 + (i * DL_SAVE) + 5, saves->save[i + (gra->wyb_save - 6)], gra->charset);
		}
		else {
			if (i != 0 && i < saves->ile_saves) {
				DrawString(gra->screen, SCREEN_WIDTH / 3 + ODS_SAVE, SCREEN_HEIGHT / 3 + (i * DL_SAVE) + 5, saves->save[i - 1], gra->charset);
			}
		}
	}
	SDL_UpdateTexture(gra->scrtex, NULL, gra->screen->pixels, gra->screen->pitch);
	SDL_RenderClear(gra->renderer);
	SDL_RenderCopy(gra->renderer, gra->scrtex, NULL, NULL);
	SDL_RenderPresent(gra->renderer);
}

// wczytanie save wybranego przez gracza
void load(gra_t* gra, czas_t* czas, saves_t *saves) {
	if (czy_plik_istnieje(saves->save[gra->wyb_save - 1])) {
		FILE* load = fopen(saves->save[gra->wyb_save - 1], "r+");
		fread(&czas->worldTime, sizeof(double), 1, load);
		fread(&gra->x, sizeof(int), 1, load);
		fread(&gra->x, sizeof(int), 1, load);
		fread(&gra->pkt, sizeof(int), 1, load);
		fread(&gra->koniec, sizeof(bool), 1, load);
		for (int i = 0; i <= SCREEN_HEIGHT; i++) {
			for (int j = 0; j <= SCREEN_WIDTH; j++) {
				fread(&gra->pixels[i][j], sizeof(int), 1, load);
			}
		}
		fclose(load);
	}
}

// sprawdzenie czy plik bmp został wczytany poprawnie
void blad_bmp(gra_t* gra, SDL_Surface* name) {
	if (name == NULL) {
		printf("SDL_LoadBMP error: %s\n", SDL_GetError());
		clear(gra);
		exit(false);
	}
}

// ustawianie ekranu przed rozpoczęciem rozrywki
void ustawianie_ekranu(gra_t *gra, int *rc) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("SDL_Init error: %s\n", SDL_GetError());
		exit(false);
	}
	// tryb peіnoekranowy / fullscreen mode
//	rc = SDL_CreateWindowAndRenderer(0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP,
//	                                 &window, &renderer);
	*rc = SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0,
		&gra->window, &gra->renderer);
	if (*rc != 0) {
		SDL_Quit();
		printf("SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
		exit(0);
	};

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(gra->renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
	SDL_SetRenderDrawColor(gra->renderer, 0, 0, 0, 255);

	SDL_SetWindowTitle(gra->window, "Spy Hunter");


	gra->screen = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

	gra->scrtex = SDL_CreateTexture(gra->renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		SCREEN_WIDTH, SCREEN_HEIGHT);


	// wyі№czenie widocznoњci kursora myszy
	SDL_ShowCursor(SDL_DISABLE);
}

// ustawianie kolorów użytych w trakcie gry
void ustawianie_kolorow(kolory_t *kolory, gra_t *gra) {
	kolory->czarny = SDL_MapRGB(gra->screen->format, 0x00, 0x00, 0x00);
	kolory->zielony = SDL_MapRGB(gra->screen->format, 0x00, 0xFF, 0x00);
	kolory->czerwony = SDL_MapRGB(gra->screen->format, 0xFF, 0x00, 0x00);
	kolory->niebieski = SDL_MapRGB(gra->screen->format, 0x11, 0x11, 0xCC);
}

// pobranie z pliku ilości zapisanych elementów
int ile_elementow(FILE* file) {
	char znak;
	int ile = 1;
	while ((znak = fgetc(file)) != EOF) {
		if (znak == '\0') {
			ile++;
		}
	}
	return ile;
}

// ustawianie początkowych wartości gry przed rozpoczęciem rozrywki lub w przypadku nowej gry
void poczatkowe_wartosci(czas_t* czas, gra_t *gra, saves_t *saves, results_t *rezultaty) {
	gra->x = SCREEN_WIDTH / 2;
	gra->y = SCREEN_HEIGHT * 2 / 3;
	gra->wyb_save = 1;
	gra->pauza = false;
	gra->quit = false;
	gra->koniec = false;
	gra->load = false;
	czas->t1 = SDL_GetTicks();
	czas->t3 = 0;
	czas->t2 = 0;
	czas->frames = 0;
	czas->worldTime = 0;
	czas->fpsTimer = 0;
	gra->pkt = 0;
	if (!czy_plik_istnieje("saves")) {
		saves->ile_saves = 1;
	}
	else {
		FILE* savey = fopen("saves", "r+");
		saves->ile_saves = ile_elementow(savey);
		fclose(savey);
	}
	if (!czy_plik_istnieje("results")) {
		rezultaty->ile_results = 1;
	}
	else {
		FILE* res = fopen("results", "r+");
		rezultaty->ile_results = ((ile_elementow(res)) / 2) + 1;
		fclose(res);
	}
}

// ustawianie czasu podczas gry
void ustawianie_czasu(czas_t *czas, gra_t *gra, kolory_t *kolory) {
	czas->t2 = SDL_GetTicks() - czas->t3;
	// w tym momencie t2-t1 to czas w milisekundach,
	// jaki uplynaі od ostatniego narysowania ekranu
	// delta to ten sam czas w sekundach
	// here t2-t1 is the time in milliseconds since
	// the last screen was drawn
	// delta is the same time in seconds
	czas->delta = (czas->t2 - czas->t1) * 0.001;
	czas->t1 = czas->t2;
	czas->worldTime += czas->delta;
	if (POZOSTALY_CZAS <= 0) {
		gra->koniec = true;
	}
}

// obliczanie wartości y wyrysowania siętrawy na ekranie
int granica_y(gra_t *gra, czas_t *czas) {
		double test = czas->worldTime + 1;
		int y = MNOZNIK_TRAWA * SCREEN_HEIGHT * cos(czas->worldTime / MNOZNIK_TRAWA / 2);
		int y_test = MNOZNIK_TRAWA * SCREEN_HEIGHT * cos(test / MNOZNIK_TRAWA / 2);

		if (y_test < y)
			return y;
		else
			return -100000;
}

// rysowanie trawy na ekranie
void trawa(gra_t* gra, kolory_t* kolory, czas_t* czas) {
	DrawRectangle(gra->screen, 0, POLA_POZA_GRA, DL_TRAWA, SCREEN_HEIGHT - POLA_POZA_GRA, kolory->zielony, kolory->zielony, gra);
	DrawRectangle(gra->screen, SCREEN_WIDTH - DL_TRAWA, 40, DL_TRAWA, SCREEN_HEIGHT - POLA_POZA_GRA, kolory->zielony, kolory->zielony, gra);
	for (int i = 0; i < 4; i++) {
		DrawRectangle(gra->screen, (i * DL_TRAWA) + DL_TRAWA, (i * ODSTEP_TRAWA) - granica_y(gra, czas), DL_TRAWA, (i * -1 * SCREEN_HEIGHT * 3 / 2) + (MNOZNIK_TRAWA * SCREEN_HEIGHT), kolory->zielony, kolory->zielony, gra);
	}
	for (int i = 0; i < 4; i++) {
		DrawRectangle(gra->screen, (i * -DL_TRAWA) + SCREEN_WIDTH - (ODSTEP_TRAWA / 4), (i * ODSTEP_TRAWA) - granica_y(gra, czas), DL_TRAWA, (i * -SCREEN_HEIGHT * 3 / 2) + (MNOZNIK_TRAWA * SCREEN_HEIGHT), kolory->zielony, kolory->zielony, gra);
	}
	DrawRectangle(gra->screen, SCREEN_WIDTH / 2, -1 * MNOZNIK_TRAWA * SCREEN_HEIGHT - granica_y(gra, czas), DL_TRAWA, MNOZNIK_TRAWA / 2 * SCREEN_HEIGHT, kolory->zielony, kolory->zielony, gra);
}

// rysowanie wszystkich elementów gry na ekranie
void rysowanie(gra_t *gra, kolory_t *kolory, czas_t *czas, char *text) {
	SDL_FillRect(gra->screen, NULL, kolory->czarny);
	DrawSurface(gra->screen, gra->car,
		gra->x,
		gra->y);
	trawa(gra, kolory, czas);
	// tekst informacyjny / info text
	DrawRectangle(gra->screen, 0, 0, SCREEN_WIDTH, POLA_POZA_GRA, kolory->czerwony, kolory->niebieski, gra);
	DrawRectangle(gra->screen, 0, SCREEN_HEIGHT - POLA_POZA_GRA, SCREEN_WIDTH, POLA_POZA_GRA, kolory->czerwony, kolory->niebieski, gra);
	//            "template for the second project, elapsed time = %.1lf s  %.0lf frames / s"
	sprintf(text, "Dmytro Dzhusov nr. 196751");
	DrawString(gra->screen, gra->screen->w / 2 - strlen(text) * 8 / 2, 10, text, gra->charset);
	//	      "Esc - exit, \030 - faster, \031 - slower"
	sprintf(text, "Elementy: a, b, c, d, e, f, g, h, i, o");
	DrawString(gra->screen, SCREEN_WIDTH - 300 , SCREEN_HEIGHT - 24, text, gra->charset);
	sprintf(text, "Punkty: %d Czas: %.0lf s", gra->pkt, POZOSTALY_CZAS);
	DrawString(gra->screen, gra->screen->w / 2 - strlen(text) * 8 / 2, 26, text, gra->charset);
	SDL_UpdateTexture(gra->scrtex, NULL, gra->screen->pixels, gra->screen->pitch);
	SDL_RenderClear(gra->renderer);
	SDL_RenderCopy(gra->renderer, gra->scrtex, NULL, NULL);
	SDL_RenderPresent(gra->renderer);
}

// sterowanie grą przez gracza
void sterowanie(gra_t *gra, czas_t *czas, kolory_t *kolory, saves_t *saves, results_t *rezultaty) {
	while (SDL_PollEvent(&gra->event)) {
		switch (gra->event.type) {
		case SDL_KEYDOWN:
			if (gra->koniec != true) {
				if (gra->pauza != true) {
					if (gra->event.key.keysym.sym == SDLK_LEFT && gra->x > ROZMIAR_CAR + DL_TRAWA && gra->pixels[gra->y][gra->x - ROZMIAR_CAR] != kolory->zielony) gra->x -= SCREEN_WIDTH / 100;
					if (gra->event.key.keysym.sym == SDLK_RIGHT && gra->x < SCREEN_WIDTH - ROZMIAR_CAR - DL_TRAWA && gra->pixels[gra->y][gra->x + ROZMIAR_CAR] != kolory->zielony) gra->x += SCREEN_WIDTH / 100;
				}
				if (gra->event.key.keysym.sym == SDLK_f) {
					gra->koniec = true;
					save_res(rezultaty, gra, czas);
					FILE* res = fopen("results", "r+");
					laduj_res(res, rezultaty);
					fclose(res);
				}
				if (gra->event.key.keysym.sym == SDLK_p) gra->pauza = !(gra->pauza);
				if (gra->event.key.keysym.sym == SDLK_s) save(gra, czas, saves);
			}
			else {
				if (gra->event.key.keysym.sym == SDLK_UP && gra->wyb_save > 1) gra->wyb_save--;
				if (gra->event.key.keysym.sym == SDLK_DOWN && gra->wyb_save < rezultaty->ile_results - 1) gra->wyb_save++;
			}
			if (gra->load == true) {
				if (gra->event.key.keysym.sym == SDLK_e) {
					gra->load = false;
					load(gra, czas, saves);
				}
				if (gra->event.key.keysym.sym == SDLK_UP && gra->wyb_save > 1) gra->wyb_save--;
				if (gra->event.key.keysym.sym == SDLK_DOWN && gra->wyb_save < saves->ile_saves - 1) gra->wyb_save++;
			}
			if (gra->event.key.keysym.sym == SDLK_ESCAPE) gra->quit = true;
			if (gra->event.key.keysym.sym == SDLK_n) poczatkowe_wartosci(czas, gra, saves, rezultaty);
			if (gra->event.key.keysym.sym == SDLK_l) {
				if (saves->ile_saves != 1) {
					FILE* saved = fopen("saves", "r+");
					laduj_save(saved, saves);
					fclose(saved);
				} 
				gra->load = true; 
				gra->koniec = false;
				gra->wyb_save = 1; 
			}
			break;
		case SDL_QUIT:
			gra->quit = true;
			break;
		};
	};
	czas->frames++;
}

// liczenie punktów poczas gry
void licz_pkt(gra_t *gra, czas_t *czas) {
	if (czas->worldTime - floor(czas->worldTime) < 0.03)
		gra->pkt += NALICZANE_PKT;
}

// wyczyszczenie tabeli z kolorami na planszy po każdym odświetleniu ekranu
void zerowanie_tabeli(gra_t* gra) {
	for (int i = 1; i <= SCREEN_HEIGHT; i++) {
		for (int j = 1; j <= SCREEN_WIDTH; j++) {
			gra->pixels[i][j] = PUSTE_PIKSELE;
		}
	}
}

// pobranie potrzebnych do gry plików bmp 
void load_bmp(gra_t *gra) {
	gra->charset = SDL_LoadBMP("./cs8x8.bmp");
	blad_bmp(gra, gra->charset);
	SDL_SetColorKey(gra->charset, true, 0x000000);

	gra->car = SDL_LoadBMP("./car.bmp");
	blad_bmp(gra, gra->car);

	gra->game_over = SDL_LoadBMP("./game_over.bmp");
	blad_bmp(gra, gra->game_over);
}

// ekran końca gry
// wyświetlenie obecnego wyniku i poprzednich
void titles(gra_t *gra, kolory_t *kolory, results_t *rezultaty) {
	SDL_FillRect(gra->screen, NULL, kolory->czarny);
	DrawSurface(gra->screen, gra->game_over, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
	char text[DL_TEXT];
	sprintf(text, "%d", gra->pkt);
	for (int i = 0; i < 2; i++) {
		DrawRectangle(gra->screen, SCREEN_WIDTH / 6, SCREEN_HEIGHT / 3 + (i * DL_SAVE), SCREEN_WIDTH / 5, DL_SAVE, kolory->czerwony, kolory->niebieski, gra);
		if (i == 0)
			DrawString(gra->screen, SCREEN_WIDTH / 6 + 5, SCREEN_HEIGHT / 3 + 5, "Obecny wynik", gra->charset);
		else
			DrawString(gra->screen, SCREEN_WIDTH / 6 + ODS_SAVE, SCREEN_HEIGHT / 3 + (i * DL_SAVE) + 5, text, gra->charset);
	}
	for (int i = 0; i < 6; i++) {
		DrawRectangle(gra->screen, SCREEN_WIDTH / 6 * 4, SCREEN_HEIGHT / 3 + (i * DL_SAVE), SCREEN_WIDTH / 5, DL_SAVE, kolory->czerwony, kolor_save(i, gra->wyb_save, kolory), gra);
		if (i == 0)
			sprintf(text, "Najleprze wyniki. Wedlug daty");
			DrawString(gra->screen, SCREEN_WIDTH / 6 * 4 + 5, SCREEN_HEIGHT / 3 + 5, text, gra->charset);
		if (gra->wyb_save > 5 && i != 0) {
			sprintf(text, "pkt: %s   czas: %s", rezultaty->pkt[i + (gra->wyb_save - 6)], rezultaty->czas[i + (gra->wyb_save - 6)]);
			DrawString(gra->screen, SCREEN_WIDTH / 8 * 5 + ODS_SAVE, SCREEN_HEIGHT / 3 + (i * DL_SAVE) + 5, text, gra->charset);
		}
		else {
			if (i != 0 && i < rezultaty->ile_results) {
				sprintf(text, "pkt: %s   czas: %s", rezultaty->pkt[i - 1], rezultaty->czas[i - 1]);
				DrawString(gra->screen, SCREEN_WIDTH / 8 * 5 + ODS_SAVE, SCREEN_HEIGHT / 3 + (i * DL_SAVE) + 5, text, gra->charset);
			}
		}
	}
	SDL_UpdateTexture(gra->scrtex, NULL, gra->screen->pixels, gra->screen->pitch);
	SDL_RenderClear(gra->renderer);
	SDL_RenderCopy(gra->renderer, gra->scrtex, NULL, NULL);
	SDL_RenderPresent(gra->renderer);
}

// przydzielenie pamięci na przechowanie wyników i save'ów
void przydzielenie_pamieci(gra_t *gra, results_t *rezultaty, saves_t *saves) {
	if (saves->ile_saves == 1) {
		saves->save = (char**)malloc((saves->ile_saves) * sizeof(char*));
		for (int i = 0; i < saves->ile_saves; i++)
			saves->save[i] = (char*)malloc(DL_DATA * sizeof(char));
	}
	if (rezultaty->ile_results == 1) {
		rezultaty->pkt = (char**)malloc((rezultaty->ile_results) * sizeof(char*));
		rezultaty->czas = (char**)malloc((rezultaty->ile_results) * sizeof(char*));
		for (int i = 0; i < rezultaty->ile_results; i++) {
			rezultaty->pkt[i] = (char*)malloc(DL_TEXT * sizeof(char));
			rezultaty->czas[i] = (char*)malloc(DL_TEXT * sizeof(char));
		}
	}
	gra->pixels = (int**)malloc((SCREEN_HEIGHT + 1) * sizeof(int*));
	for (int i = 0; i < SCREEN_HEIGHT + 1; i++)
		gra->pixels[i] = (int*)malloc((SCREEN_WIDTH + 1) * sizeof(int));
}

// main
#ifdef __cplusplus
extern "C"
#endif
int main(int argc, char **argv) {
	int rc;
	czas_t czas;
	gra_t gra;
	saves_t saves;
	kolory_t kolory;
	results_t rezultaty;
	poczatkowe_wartosci(&czas, &gra, &saves, &rezultaty);
	przydzielenie_pamieci(&gra, &rezultaty, &saves);
	ustawianie_ekranu(&gra, &rc);
	
	// wczytanie bmp obrazów
	load_bmp(&gra);

	char text[DL_TEXT];
	ustawianie_kolorow(&kolory, &gra);

	while (!gra.quit) {
		licz_pkt(&gra, &czas);
		rysowanie(&gra, &kolory, &czas, text);
		// stopowanie gry
		if (gra.pixels[gra.y - ROZMIAR_CAR][gra.x] != kolory.zielony && gra.pauza != true) {
			ustawianie_czasu(&czas, &gra, &kolory);
		}
		else
			czas.t3 = SDL_GetTicks() - czas.t2;
		// obsіuga zdarzeс (o ile jakieњ zaszіy) / handling of events (if there were any)
		sterowanie(&gra, &czas, &kolory, &saves, &rezultaty);
		zerowanie_tabeli(&gra);
		// ekran końca gry
		while (gra.koniec == true) {
			titles(&gra, &kolory, &rezultaty);
			sterowanie(&gra, &czas, &kolory, &saves, &rezultaty);
		}
		// ekran wybierania save'a do wczytania
		while (gra.load == true) {
			wybor_save(&gra, &kolory, &saves);
			sterowanie(&gra, &czas, &kolory, &saves, &rezultaty);
		}
	}
	// zwolnienie powierzchni / freeing all surfaces
	clear(&gra);
	return 0;
	};
