#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
static Pixel canvas[SKY_DISPLAY_W*SKY_DISPLAY_H];
static Pixel scene[SKY_W*SKY_H];
static void Save(const char* path,int w,int h){
    FILE* f=fopen(path,"wb");if(!f)exit(1);fprintf(f,"P6\n%d %d\n255\n",w,h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){unsigned p=canvas[y*w+x];unsigned char rgb[3]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};fwrite(rgb,1,3,f);}
    fclose(f);
}
int main(){
    MenuState menu;SkyDrawMenu(canvas,SKY_DISPLAY_W,menu,0,0);Save("main-menu.ppm",176,208);
    menu.Select();menu.Pause();menu.selection=1;
    FlightModel flight;FlightModel_Reset(&flight);Soft3D renderer;renderer.Render(scene,SKY_W,flight.state);
    SkyDrawMenu(canvas,SKY_DISPLAY_W,menu,scene,SKY_W);Save("pause-menu.ppm",176,208);
    SkyDrawIcon(canvas,44,44,44);Save("icon44.ppm",44,44);
    SkyDrawIcon(canvas,42,42,29);Save("icon42.ppm",42,29);return 0;
}
