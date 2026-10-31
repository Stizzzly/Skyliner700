#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
static void Check(bool ok,const char* s){if(!ok){fprintf(stderr,"FAIL: %s\n",s);exit(1);}}
static Pixel pixels[(SKY_DISPLAY_W+8)*SKY_DISPLAY_H+32];
static Pixel background[SKY_W*SKY_H];
int main(){
    MenuState m;Check(m.Visible()&&m.mode==MenuState::Main&&m.Count()==2,"starts in main menu");
    m.Move(-1);Check(m.selection==1&&m.Select()==MenuState::Exit,"main exit and wrap");
    m.Move(1);Check(m.Select()==MenuState::Start&&!m.Visible(),"start flight");
    m.Pause();Check(m.Count()==3&&m.Select()==MenuState::Resume,"pause/resume");
    m.Pause();m.Move(1);Check(m.Select()==MenuState::Return&&m.mode==MenuState::Main,"return to main menu");
    m.Select();m.Pause();m.Move(-1);Check(m.Select()==MenuState::Exit,"pause exit");
    for(int mode=0;mode<2;++mode){m.mode=mode?MenuState::Paused:MenuState::Main;
        for(int choice=0;choice<m.Count();++choice){m.selection=choice;
            for(unsigned i=0;i<sizeof(pixels)/sizeof(pixels[0]);++i)pixels[i]=0xA55A;
            SkyDrawMenu(pixels+16,SKY_DISPLAY_W+8,m,background,SKY_W);
            for(int g=0;g<16;++g){Check(pixels[g]==0xA55A,"menu leading guard");Check(pixels[16+(SKY_DISPLAY_W+8)*SKY_DISPLAY_H+g]==0xA55A,"menu trailing guard");}
            for(int y=0;y<SKY_DISPLAY_H;++y)for(int x=SKY_DISPLAY_W;x<SKY_DISPLAY_W+8;++x)Check(pixels[16+y*(SKY_DISPLAY_W+8)+x]==0xA55A,"menu stride guard");
        }
    }
    puts("PASS: main/pause navigation, resume/return/exit and all menu framebuffer guards.");return 0;
}
