#include "menu.h"
#include "ui_font.inc"
static Pixel Color(int r,int g,int b){return (Pixel)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));}
class Canvas {
public:
    Pixel* pixels;int stride,width,height;
    Canvas(Pixel* p,int s,int w,int h):pixels(p),stride(s),width(w),height(h){}
    void Point(int x,int y,Pixel c){if(x>=0&&x<width&&y>=0&&y<height)pixels[y*stride+x]=c;}
    void Rect(int x,int y,int w,int h,Pixel c){for(int py=y;py<y+h;++py)for(int px=x;px<x+w;++px)Point(px,py,c);}
    void Line(int x,int y,int ex,int ey,Pixel c){
        int dx=ex>x?ex-x:x-ex,sx=x<ex?1:-1,dy=ey>y?y-ey:ey-y,sy=y<ey?1:-1,error=dx+dy;
        for(;;){Point(x,y,c);if(x==ex&&y==ey)break;int twice=error*2;if(twice>=dy){error+=dy;x+=sx;}if(twice<=dx){error+=dx;y+=sy;}}
    }
    void Tri(int ax,int ay,int bx,int by,int cx,int cy,Pixel c){
        int minx=ax<bx?ax:bx;if(cx<minx)minx=cx;int maxx=ax>bx?ax:bx;if(cx>maxx)maxx=cx;
        int miny=ay<by?ay:by;if(cy<miny)miny=cy;int maxy=ay>by?ay:by;if(cy>maxy)maxy=cy;
        for(int y=miny;y<=maxy;++y)for(int x=minx;x<=maxx;++x){
            int a=(bx-ax)*(y-ay)-(by-ay)*(x-ax),b=(cx-bx)*(y-by)-(cy-by)*(x-bx),d=(ax-cx)*(y-cy)-(ay-cy)*(x-cx);
            if((a>=0&&b>=0&&d>=0)||(a<=0&&b<=0&&d<=0))Point(x,y,c);
        }
    }
    void Text(int x,int y,const char* text,int scale,Pixel c){
        for(int letter=0;text[letter];++letter){unsigned ch=(unsigned char)text[letter];const unsigned char* g=0;
            if(ch>='A'&&ch<='Z')g=letters[ch-'A'];else if(ch>='0'&&ch<='9')g=digits[ch-'0'];
            if(g)for(int row=0;row<7;++row)for(int col=0;col<5;++col)if(g[row]&(1<<(4-col)))Rect(x+letter*6*scale+col*scale,y+row*scale,scale,scale,c);
        }
    }
    void Center(int y,const char* text,int scale,Pixel c){int n=0;while(text[n])++n;Text((width-(n*6-1)*scale)/2,y,text,scale,c);}
};
static void Plane(Canvas& c,int x,int y,int size){
    Pixel white=Color(231,247,255),cyan=Color(80,184,230),gold=Color(255,231,122);
    int cx=x+size/2;
    c.Tri(cx,y+size/9,x+size*43/100,y+size*82/100,x+size*57/100,y+size*82/100,white);
    c.Tri(x+size*46/100,y+size*38/100,x+size/9,y+size*65/100,x+size*48/100,y+size*58/100,white);
    c.Tri(x+size*54/100,y+size*38/100,x+size*89/100,y+size*65/100,x+size*52/100,y+size*58/100,cyan);
    c.Tri(cx,y+size*70/100,x+size*30/100,y+size*88/100,x+size*70/100,y+size*88/100,white);
    c.Line(cx,y+size/5,cx,y+size*68/100,gold);
}
void SkyDrawIcon(Pixel* pixels,int stride,int width,int height){
    Canvas c(pixels,stride,width,height);
    int radius=height/6;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        int dx=x<radius?radius-x:(x>=width-radius?x-(width-radius-1):0);
        int dy=y<radius?radius-y:(y>=height-radius?y-(height-radius-1):0);
        Pixel col=Color(9,25+y*18/height,43+y*25/height);
        if(dx*dx+dy*dy>radius*radius)col=0;
        c.Point(x,y,col);
    }
    c.Line(radius,1,width-radius-1,1,Color(80,184,230));
    c.Line(width-2,radius,width-2,height-radius-1,Color(30,86,115));
    c.Line(3,height*2/3,width-4,height/3,Color(32,88,115));
    int size=height-2;Plane(c,(width-size)/2,1,size);
}
void SkyDrawMenu(Pixel* pixels,int stride,const MenuState& state,const Pixel* background,int backgroundStride){
    Canvas c(pixels,stride,SKY_DISPLAY_W,SKY_DISPLAY_H);
    Pixel cyan=Color(80,184,230),gold=Color(255,231,122),regular=Color(220,239,250);
    for(int y=0;y<c.height;++y)for(int x=0;x<c.width;++x){
        Pixel p=Color(7,20+y/16,34+y/8);
        if(background&&state.mode==MenuState::Paused)p=(Pixel)((background[(y/2)*backgroundStride+x/2]&0xE79C)>>2);
        c.Point(x,y,p);
    }
    if(state.mode==MenuState::Main){
        c.Line(12,12,163,12,Color(29,78,102));c.Rect(12,12,30,2,cyan);
        c.Center(25,"SKYLINER",2,cyan);c.Center(46,"700",3,gold);
        c.Line(38,91,138,91,Color(30,76,98));Plane(c,65,70,46);
    }else{
        c.Center(23,"PAUSED",2,cyan);c.Center(45,"SKYLINER 700",1,regular);
        c.Line(46,64,130,64,Color(29,78,102));
    }
    int top=state.mode==MenuState::Main?116:84;
    int count=state.Count(),panelHeight=count*28+10;
    c.Rect(11,top-3,154,panelHeight+2,Color(30,78,100));
    c.Rect(12,top-2,152,panelHeight,Color(7,20,34));c.Rect(12,top-2,152,2,cyan);
    const char* mainItems[2]={"START FLIGHT","EXIT GAME"};
    const char* pauseItems[3]={"RESUME","RETURN MENU","EXIT GAME"};
    for(int item=0;item<count;++item){int y=top+7+item*28;
        if(item==state.selection){c.Rect(18,y-3,140,22,Color(25,61,79));c.Rect(18,y-3,3,22,gold);}
        c.Center(y+3,state.mode==MenuState::Main?mainItems[item]:pauseItems[item],1,item==state.selection?gold:regular);
    }
    c.Center(187,"UP DOWN  5 SELECT",1,Color(123,165,184));
    c.Text(9,200,"SELECT",1,regular);c.Text(144,200,"EXIT",1,regular);
}
