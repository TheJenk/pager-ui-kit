/* pgview - host-side prototype of the Pager view engine.
 * Reads a SwiftUI-style .view file (already variable-resolved) and renders it to
 * a 480x222 RGB565 raw buffer using a VStack/HStack layout. This is where the
 * layout logic is developed; it then ports into the pguid renderer.
 *
 *   cc -O2 -o pgview pgview.c && ./pgview file.view out.raw
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define W 480
#define H 222
static uint16_t fb[W*H];

/* ---- font (5x7) ---- */
static const unsigned char FONT[64][7]={
{0,0,0,0,0,0,0},{4,4,4,4,4,0,4},{10,10,0,0,0,0,0},{10,31,10,31,10,0,0},{31,17,17,17,17,17,31},{17,2,4,8,17,0,0},{12,18,12,21,19,13,0},{4,4,0,0,0,0,0},{2,4,4,4,4,4,2},{8,4,4,4,4,4,8},{0,10,4,31,4,10,0},{0,4,4,31,4,4,0},{0,0,0,0,4,4,8},{0,0,0,31,0,0,0},{0,0,0,0,0,4,4},{1,1,2,4,8,16,16},{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{31,2,4,2,1,17,14},{2,6,10,18,31,2,2},{31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,2,12},{0,4,4,0,4,4,0},{31,17,17,17,17,17,31},{2,4,8,16,8,4,2},{0,0,31,0,31,0,0},{8,4,2,1,2,4,8},{14,17,1,2,4,0,4},{14,17,23,21,23,16,14},{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{28,18,17,17,17,18,28},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,14},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,17,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},{14,8,8,8,8,8,14},{31,17,17,17,17,17,31},{14,2,2,2,2,2,14},{31,17,17,17,17,17,31},{0,0,0,0,0,0,31}};

static uint16_t rgb(int r,int g,int b){ return (uint16_t)(((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3)); }
static uint16_t hx(const char*h){ int v=(int)strtol(h,0,16); return rgb((v>>16)&255,(v>>8)&255,v&255); }
static void px(int x,int y,uint16_t c){ if((unsigned)x<W&&(unsigned)y<H) fb[y*W+x]=c; }
static void fill(int x,int y,int w,int h,uint16_t c){ for(int i=0;i<w;i++)for(int j=0;j<h;j++)px(x+i,y+j,c); }
static void frame(int x,int y,int w,int h,uint16_t c){ fill(x,y,w,1,c);fill(x,y+h-1,w,1,c);fill(x,y,1,h,c);fill(x+w-1,y,1,h,c); }
static int tw(const char*s,int sz){ int n=strlen(s); return n? n*6*sz-sz:0; }
static void dtext(int x,int y,const char*s,uint16_t c,int sz){ for(;*s;s++){ char ch=*s; if(ch>='a'&&ch<='z')ch-=32;
  if(ch>=0x20&&ch<0x60){ const unsigned char*g=FONT[ch-0x20]; for(int r=0;r<7;r++){unsigned b=g[r];
    for(int col=0;col<5;col++) if(b&(1<<(4-col))) fill(x+col*sz,y+r*sz,sz,sz,c);} } x+=6*sz; } }

/* ---- palette & named colors ---- */
static uint16_t C_BG,C_BAR,C_LINE,C_TEXT,C_DIM,C_ACCENT;
static uint16_t namedcol(const char*n){
  if(!strcmp(n,"red"))return hx("e0483f"); if(!strcmp(n,"green"))return hx("2fae6a");
  if(!strcmp(n,"blue"))return hx("3a86c8"); if(!strcmp(n,"cyan"))return hx("3ac8c8");
  if(!strcmp(n,"yellow"))return hx("e0c23f"); if(!strcmp(n,"amber"))return hx("e0a53f");
  if(!strcmp(n,"orange"))return hx("e07b3f"); if(!strcmp(n,"purple"))return hx("9b7ede");
  if(!strcmp(n,"pink"))return hx("e06ba0"); if(!strcmp(n,"white"))return hx("ffffff");
  if(!strcmp(n,"dim")||!strcmp(n,"grey")||!strcmp(n,"gray"))return C_DIM;
  if(!strcmp(n,"accent"))return C_ACCENT; return C_TEXT;
}

/* ---- node tree ---- */
enum{ N_VSTACK,N_HSTACK,N_ZSTACK,N_CARD,N_TEXT,N_HEADER,N_FOOTER,N_METER,N_GRAPH,N_BAR,N_SPACER };
typedef struct Node{ int type; char text[160],text2[64]; uint16_t color; int size,pct;
  int fixW,fixH,spacing,pad; struct Node*kid[24]; int nk; int x,y,w,h; } Node;
static Node pool[256]; static int np;
static Node* nnew(int t){ Node*n=&pool[np++]; memset(n,0,sizeof*n); n->type=t; n->color=C_TEXT; n->size=2;
  n->fixW=-1;n->fixH=-1;n->spacing=6;n->pad=8;n->pct=0; return n; }
static int typeFor(const char*s){
  if(!strcmp(s,"VStack"))return N_VSTACK; if(!strcmp(s,"HStack"))return N_HSTACK;
  if(!strcmp(s,"ZStack"))return N_ZSTACK; if(!strcmp(s,"Card"))return N_CARD;
  if(!strcmp(s,"Text"))return N_TEXT; if(!strcmp(s,"Header"))return N_HEADER;
  if(!strcmp(s,"Footer"))return N_FOOTER; if(!strcmp(s,"Meter"))return N_METER;
  if(!strcmp(s,"Graph"))return N_GRAPH; if(!strcmp(s,"Bar"))return N_BAR;
  if(!strcmp(s,"Spacer"))return N_SPACER; return -1; }

/* ---- parser ---- */
static const char*P;
static void sws(){ for(;;){ while(*P==' '||*P=='\t'||*P=='\n'||*P=='\r'||*P==',')P++;
  if(P[0]=='/'&&P[1]=='/'){ while(*P&&*P!='\n')P++; continue;} break; } }
static int isid(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'; }
static int rid(char*b,int m){ sws(); int i=0; while(isid(*P)&&i<m-1)b[i++]=*P++; b[i]=0; return i; }
static int rstr(char*b,int m){ sws(); if(*P!='"')return 0; P++; int i=0; while(*P&&*P!='"'&&i<m-1){ if(*P=='\\'&&P[1])P++; b[i++]=*P++; } if(*P=='"')P++; b[i]=0; return 1; }
static void applySize(Node*n,const char*s){ if(!strcmp(s,"big"))n->size=3; else if(!strcmp(s,"med"))n->size=2;
  else if(!strcmp(s,"small"))n->size=1; }
static void applyDot(Node*n,const char*m){ /* .color or .size keyword in args */
  if(!strcmp(m,"big")||!strcmp(m,"med")||!strcmp(m,"small")) applySize(n,m); else n->color=namedcol(m); }
static void parseArgs(Node*n){ for(;;){ sws(); if(*P==')'||!*P)break;
  if(*P=='"'){ char s[160]; rstr(s,160); if(!n->text[0])strcpy(n->text,s); else strcpy(n->text2,s); }
  else if(*P=='.'){ P++; char m[24]; rid(m,24); applyDot(n,m); }
  else { char id[40]; rid(id,40); sws();
    if(*P==':'){ P++; sws(); char v[64];
      if(*P=='"'){ rstr(v,64); if(!n->text[0])strcpy(n->text,v); }
      else if(*P=='.'){ P++; char m[24]; rid(m,24); applyDot(n,m); }
      else { rid(v,64); if(!strcmp(id,"value"))n->pct=atoi(v);
             else if(!strcmp(id,"spacing"))n->spacing=atoi(v);
             else if(!n->text[0])strcpy(n->text,v); } }
    else { if(!n->text[0])strcpy(n->text,id); } }
  sws(); if(*P==',')P++; } }
static void parseMods(Node*n){ for(;;){ sws(); if(*P!='.')break; P++; char m[24]; rid(m,24); sws();
  int arg=0; if(*P=='('){ P++; char v[24]; rid(v,24); arg=atoi(v); sws(); if(*P==')')P++; }
  if(!strcmp(m,"h"))n->fixH=arg; else if(!strcmp(m,"w"))n->fixW=arg;
  else if(!strcmp(m,"pad"))n->pad=arg; else if(!strcmp(m,"size"))n->size=arg;
  else applyDot(n,m); } }
static Node* parseNode(){ sws(); char name[32]; if(!rid(name,32))return 0; int t=typeFor(name); if(t<0)return 0;
  Node*n=nnew(t); if(t==N_BAR)n->color=C_ACCENT; if(t==N_METER)n->color=namedcol("green"); if(t==N_GRAPH)n->color=C_ACCENT;
  sws(); if(*P=='('){ P++; parseArgs(n); sws(); if(*P==')')P++; }
  parseMods(n);
  sws(); if(*P=='{'){ P++; for(;;){ sws(); if(*P=='}'||!*P)break; Node*k=parseNode(); if(!k){ if(*P)P++; else break; continue;} n->kid[n->nk++]=k; } sws(); if(*P=='}')P++; }
  return n; }

/* ---- layout ---- */
static int mainIntr(Node*k,int parent){ /* fixed main-axis size, or -1 = flexible */
  if(parent==N_VSTACK){ if(k->fixH>=0)return k->fixH;
    if(k->type==N_HEADER)return 26; if(k->type==N_FOOTER)return 16;
    if(k->type==N_TEXT)return 7*k->size+6; return -1; }
  else { if(k->fixW>=0)return k->fixW; if(k->type==N_TEXT)return tw(k->text,k->size); return -1; }
}
static void layout(Node*n,int x,int y,int w,int h){ n->x=x;n->y=y;n->w=w;n->h=h;
  if(n->type==N_VSTACK||n->type==N_HSTACK){ int vert=(n->type==N_VSTACK);
    int main=vert?h:w, cross=vert?w:h, sp=n->spacing;
    int gap=sp*(n->nk>1?n->nk-1:0), fixed=0, flex=0;
    for(int i=0;i<n->nk;i++){ int m=mainIntr(n->kid[i],n->type); if(m>=0)fixed+=m; else flex++; }
    int remain=main-fixed-gap; if(remain<0)remain=0; int each=flex?remain/flex:0;
    int pos=vert?y:x;
    for(int i=0;i<n->nk;i++){ int m=mainIntr(n->kid[i],n->type); int sz=m>=0?m:each;
      if(vert)layout(n->kid[i],x,pos,cross,sz); else layout(n->kid[i],pos,y,sz,cross); pos+=sz+sp; } }
  else if(n->type==N_CARD){ if(n->nk){ int p=n->pad,th=n->text[0]?18:0;
    int cy=y+p+th, ch=h-p-th-p; Node*k=n->kid[0];
    int kh=(k->type==N_TEXT)?7*k->size:ch; int oy=cy+(ch-kh)/2; if(oy<cy)oy=cy;
    layout(k,x+p,oy,w-2*p,kh); } }
  else if(n->type==N_ZSTACK){ for(int i=0;i<n->nk;i++)layout(n->kid[i],x,y,w,h); }
}

/* ---- draw ---- */
static void draw(Node*n){ switch(n->type){
  case N_VSTACK: case N_HSTACK: case N_ZSTACK: for(int i=0;i<n->nk;i++)draw(n->kid[i]); break;
  case N_HEADER: fill(0,n->y,W,26,C_BAR); fill(0,n->y+26,W,1,C_LINE); dtext(10,n->y+6,n->text,C_ACCENT,2); break;
  case N_FOOTER: fill(n->x,n->y,n->w,1,C_LINE);
    if(n->text[0])dtext(n->x+8,n->y+4,n->text,C_DIM,1);
    if(n->text2[0])dtext(n->x+n->w-8-tw(n->text2,1),n->y+4,n->text2,C_DIM,1); break;
  case N_CARD: fill(n->x,n->y,n->w,n->h,C_BAR); frame(n->x,n->y,n->w,n->h,C_LINE);
    if(n->text[0])dtext(n->x+10,n->y+8,n->text,C_TEXT,2);
    for(int i=0;i<n->nk;i++)draw(n->kid[i]); break;
  case N_TEXT: dtext(n->x,n->y,n->text,n->color,n->size); break;
  case N_BAR: fill(n->x,n->y,n->w,n->h,n->color); break;
  case N_METER:{ frame(n->x,n->y,n->w,n->h,C_LINE); int p=n->pct; if(p<0)p=0;if(p>100)p=100;
    int fw=(n->w-2)*p/100; if(fw>0)fill(n->x+1,n->y+1,fw,n->h-2,n->color); break; }
  case N_GRAPH:{ char buf[160]; strncpy(buf,n->text,159); buf[159]=0; int vals[80],nv=0;
    char*t=strtok(buf," "); while(t&&nv<80){ vals[nv++]=atoi(t); t=strtok(0," "); }
    if(!nv){ frame(n->x,n->y,n->w,n->h,C_LINE); break; } int mx=1; for(int i=0;i<nv;i++)if(vals[i]>mx)mx=vals[i];
    int bw=n->w/nv; if(bw<1)bw=1; int gap=1; if(bw>2)bw-=gap;
    for(int i=0;i<nv;i++){ int bh=vals[i]*(n->h-1)/mx; if(bh<1)bh=1; fill(n->x+i*(bw+gap),n->y+n->h-bh,bw,bh,n->color); } break; }
  case N_SPACER: break; } }

int main(int argc,char**argv){ if(argc<3){ fprintf(stderr,"usage: pgview file.view out.raw\n"); return 2; }
  C_BG=hx("0e1016");C_BAR=hx("1a2234");C_LINE=hx("2b3a52");C_TEXT=hx("e8ecf3");C_DIM=hx("7f8aa0");C_ACCENT=hx("5aa0dc");
  FILE*f=fopen(argv[1],"rb"); if(!f){perror("open");return 1;}
  static char src[8192]; int nrd=fread(src,1,sizeof src-1,f); src[nrd]=0; fclose(f);
  P=src; np=0; Node*root=parseNode(); if(!root){fprintf(stderr,"parse failed\n");return 1;}
  for(int i=0;i<W*H;i++)fb[i]=C_BG;
  layout(root,0,0,W,H); draw(root);
  FILE*o=fopen(argv[2],"wb"); fwrite(fb,2,W*H,o); fclose(o);
  fprintf(stderr,"rendered %d nodes\n",np); return 0; }
