/* pgviewc - compile a SwiftUI-style .view (read from stdin, variables already
 * resolved by the shell) into pguid draw commands (written to stdout). The
 * renderer stays a dumb canvas; this adds the auto-layout on top.
 *
 *   cat hunt.view | pgviewc  ->  clear / rect / frame / text / present lines
 *
 * Freestanding MIPS (no libc, integer-only). Used by ui.sh's UI_RENDER.
 */
#define SYS_read 4003
#define SYS_write 4004
#define SYS_exit 4246
static long sys3(long n,long a,long b,long c){ register long r2 asm("$2")=n,r4 asm("$4")=a,r5 asm("$5")=b,r6 asm("$6")=c,r7 asm("$7")=0;
  asm volatile("syscall":"+r"(r2),"+r"(r7):"r"(r4),"r"(r5),"r"(r6):"$1","$3","$8","$9","$10","$11","$12","$13","$14","$15","$24","$25","memory");
  return r7?-r2:r2; }
asm(".text\n.globl _start\n.ent _start\n_start:\n jal main\n nop\n move $4,$2\n li $2,4246\n syscall\n nop\n.end _start\n");
void *memset(void*d,int c,unsigned n){ unsigned char*p=d; while(n--)*p++=(unsigned char)c; return d; }
static int slen(const char*s){ int n=0; while(s[n])n++; return n; }
static int seq(const char*a,const char*b){ while(*a&&*a==*b){a++;b++;} return *a==*b; }
static void scpy(char*d,const char*s,int max){ int i=0; while(s[i]&&i<max-1){d[i]=s[i];i++;} d[i]=0; }
static int atoi_(const char*s){ int v=0,n=0; while(*s==' ')s++; if(*s=='-'){n=1;s++;} while(*s>='0'&&*s<='9')v=v*10+(*s++-'0'); return n?-v:v; }

#define W 480
#define H 222

/* ---- output ---- */
static char ob[8192]; static int on;
static void flush(){ if(on){ sys3(SYS_write,1,(long)ob,on); on=0; } }
static void es(const char*s){ while(*s){ if(on>=(int)sizeof(ob))flush(); ob[on++]=*s++; } }
static void ei(int v){ char t[16],o[16]; int i=0,j=0; if(v<0){o[j++]='-';v=-v;} if(!v)t[i++]='0'; while(v){t[i++]=(char)('0'+v%10);v/=10;} while(i)o[j++]=t[--i]; o[j]=0; es(o); }
static void CLEAR(const char*c){ es("clear ");es(c);es("\n"); }
static void RECT(int x,int y,int w,int h,const char*c){ es("rect ");ei(x);es(" ");ei(y);es(" ");ei(w);es(" ");ei(h);es(" ");es(c);es("\n"); }
static void FRAME(int x,int y,int w,int h,const char*c){ es("frame ");ei(x);es(" ");ei(y);es(" ");ei(w);es(" ");ei(h);es(" ");es(c);es("\n"); }
static void TEXT(int x,int y,int s,const char*c,const char*t){ es("text ");ei(x);es(" ");ei(y);es(" ");ei(s);es(" ");es(c);es(" ");es(t);es("\n"); }

/* ---- palette & named colors (as hex strings) ---- */
static const char *C_BG="0e1016",*C_BAR="1a2234",*C_LINE="2b3a52",*C_TEXT="e8ecf3",*C_DIM="7f8aa0",*C_ACCENT="5aa0dc";
static const char* namedhex(const char*n){
  if(seq(n,"red"))return "e0483f"; if(seq(n,"green"))return "2fae6a"; if(seq(n,"blue"))return "3a86c8";
  if(seq(n,"cyan"))return "3ac8c8"; if(seq(n,"yellow"))return "e0c23f"; if(seq(n,"amber"))return "e0a53f";
  if(seq(n,"orange"))return "e07b3f"; if(seq(n,"purple"))return "9b7ede"; if(seq(n,"pink"))return "e06ba0";
  if(seq(n,"white"))return "ffffff"; if(seq(n,"dim")||seq(n,"grey")||seq(n,"gray"))return C_DIM;
  if(seq(n,"accent"))return C_ACCENT; if(seq(n,"text"))return C_TEXT;
  /* a raw RRGGBB passes through */ int ok=1; for(int i=0;i<6;i++){ char d=n[i]; if(!((d>='0'&&d<='9')||(d>='a'&&d<='f')||(d>='A'&&d<='F'))){ok=0;break;} } if(ok&&!n[6])return n;
  return C_TEXT; }

/* ---- node tree ---- */
enum{ N_V,N_H,N_Z,N_CARD,N_TEXT,N_HEAD,N_FOOT,N_METER,N_GRAPH,N_BAR,N_IMG,N_SPACE };
typedef struct Node{ int type; char text[160],text2[64],col[16],vals[160]; int size,pct,fixW,fixH,spacing,pad;
  struct Node*kid[24]; int nk; int x,y,w,h; } Node;
static Node pool[256]; static int np;
static Node* nnew(int t){ Node*n=&pool[np++]; memset(n,0,sizeof*n); n->type=t; n->size=2; n->fixW=-1;n->fixH=-1;n->spacing=6;n->pad=8;
  scpy(n->col,C_TEXT,16); return n; }
static int typeFor(const char*s){ if(seq(s,"VStack"))return N_V; if(seq(s,"HStack"))return N_H; if(seq(s,"ZStack"))return N_Z;
  if(seq(s,"Card"))return N_CARD; if(seq(s,"Text"))return N_TEXT; if(seq(s,"Header"))return N_HEAD; if(seq(s,"Footer"))return N_FOOT;
  if(seq(s,"Meter"))return N_METER; if(seq(s,"Graph"))return N_GRAPH; if(seq(s,"Bar"))return N_BAR; if(seq(s,"Image"))return N_IMG;
  if(seq(s,"Spacer"))return N_SPACE; return -1; }

/* ---- parser ---- */
static const char*P;
static void sws(){ for(;;){ while(*P==' '||*P=='\t'||*P=='\n'||*P=='\r'||*P==',')P++; if(P[0]=='/'&&P[1]=='/'){ while(*P&&*P!='\n')P++; continue;} break; } }
static int isid(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'; }
static int rid(char*b,int m){ sws(); int i=0; while(isid(*P)&&i<m-1)b[i++]=*P++; b[i]=0; return i; }
static int rstr(char*b,int m){ sws(); if(*P!='"')return 0; P++; int i=0; while(*P&&*P!='"'&&i<m-1){ if(*P=='\\'&&P[1])P++; b[i++]=*P++; } if(*P=='"')P++; b[i]=0; return 1; }
static void applyDot(Node*n,const char*m){ if(seq(m,"big"))n->size=3; else if(seq(m,"med"))n->size=2; else if(seq(m,"small"))n->size=1;
  else scpy(n->col,namedhex(m),16); }
static void pArgs(Node*n){ int first=1; for(;;){ sws(); if(*P==')'||!*P)break;
  if(*P=='"'){ char s[160]; rstr(s,160); if(n->type==N_FOOT){ if(first)scpy(n->text,s,160); else scpy(n->text2,s,64);} else if(n->type==N_GRAPH)scpy(n->vals,s,160); else scpy(n->text,s,160); }
  else if(*P=='.'){ P++; char m[24]; rid(m,24); applyDot(n,m); }
  else { char id[40]; rid(id,40); sws(); if(*P==':'){ P++; sws(); if(*P=='"'){ char v[160]; rstr(v,160); scpy(n->text,v,160);} else if(*P=='.'){P++;char m[24];rid(m,24);applyDot(n,m);} else { char v[32]; rid(v,32); if(seq(id,"value"))n->pct=atoi_(v); else if(seq(id,"spacing"))n->spacing=atoi_(v); } } else { if(!n->text[0])scpy(n->text,id,160); } }
  first=0; sws(); if(*P==',')P++; } }
static void pMods(Node*n){ for(;;){ sws(); if(*P!='.')break; P++; char m[24]; rid(m,24); sws(); int a=0; if(*P=='('){ P++; char v[24]; rid(v,24); a=atoi_(v); sws(); if(*P==')')P++; }
  if(seq(m,"h"))n->fixH=a; else if(seq(m,"w"))n->fixW=a; else if(seq(m,"pad"))n->pad=a; else if(seq(m,"size"))n->size=a; else applyDot(n,m); } }
static Node* pNode(){ sws(); char name[32]; if(!rid(name,32))return 0; int t=typeFor(name); if(t<0)return 0; Node*n=nnew(t);
  if(t==N_METER)scpy(n->col,namedhex("green"),16); if(t==N_GRAPH||t==N_BAR)scpy(n->col,C_ACCENT,16);
  sws(); if(*P=='('){ P++; pArgs(n); sws(); if(*P==')')P++; } pMods(n); sws();
  if(*P=='{'){ P++; for(;;){ sws(); if(*P=='}'||!*P)break; Node*k=pNode(); if(!k){ if(*P)P++; else break; continue;} n->kid[n->nk++]=k; } sws(); if(*P=='}')P++; }
  pMods(n);   /* SwiftUI-style modifiers can also come after the { } block */
  return n; }

/* ---- layout ---- */
static int tw(const char*s,int sz){ int n=slen(s); return n? n*6*sz-sz:0; }
static int mainIntr(Node*k,int parent){ if(parent==N_V){ if(k->fixH>=0)return k->fixH; if(k->type==N_HEAD)return 26; if(k->type==N_FOOT)return 16; if(k->type==N_TEXT)return 7*k->size+6; return -1; }
  else { if(k->fixW>=0)return k->fixW; if(k->type==N_TEXT)return tw(k->text,k->size); return -1; } }
static void layout(Node*n,int x,int y,int w,int h){ n->x=x;n->y=y;n->w=w;n->h=h;
  if(n->type==N_V||n->type==N_H){ int vert=(n->type==N_V); int main=vert?h:w,cross=vert?w:h,sp=n->spacing; int gap=sp*(n->nk>1?n->nk-1:0),fixed=0,flex=0;
    for(int i=0;i<n->nk;i++){ int m=mainIntr(n->kid[i],n->type); if(m>=0)fixed+=m; else flex++; }
    int remain=main-fixed-gap; if(remain<0)remain=0; int each=flex?remain/flex:0; int pos=vert?y:x;
    for(int i=0;i<n->nk;i++){ int m=mainIntr(n->kid[i],n->type); int sz=m>=0?m:each; if(vert)layout(n->kid[i],x,pos,cross,sz); else layout(n->kid[i],pos,y,sz,cross); pos+=sz+sp; } }
  else if(n->type==N_CARD){ if(n->nk){ int p=n->pad,th=n->text[0]?18:0; int cy=y+p+th,ch=h-p-th-p; Node*k=n->kid[0]; int kh=k->type==N_TEXT?7*k->size:ch; int oy=cy+(ch-kh)/2; if(oy<cy)oy=cy; layout(k,x+p,oy,w-2*p,kh); } }
  else if(n->type==N_Z){ for(int i=0;i<n->nk;i++)layout(n->kid[i],x,y,w,h); } }

/* ---- emit draw commands ---- */
static void draw(Node*n){ switch(n->type){
  case N_V: case N_H: case N_Z: for(int i=0;i<n->nk;i++)draw(n->kid[i]); break;
  case N_HEAD: RECT(0,n->y,W,26,C_BAR); RECT(0,n->y+26,W,1,C_LINE); TEXT(10,n->y+6,2,C_ACCENT,n->text); break;
  case N_FOOT: RECT(n->x,n->y,n->w,1,C_LINE); if(n->text[0])TEXT(n->x+8,n->y+4,1,C_DIM,n->text);
    if(n->text2[0])TEXT(n->x+n->w-8-tw(n->text2,1),n->y+4,1,C_DIM,n->text2); break;
  case N_CARD: RECT(n->x,n->y,n->w,n->h,C_BAR); FRAME(n->x,n->y,n->w,n->h,C_LINE); if(n->text[0])TEXT(n->x+10,n->y+8,2,C_TEXT,n->text); for(int i=0;i<n->nk;i++)draw(n->kid[i]); break;
  case N_TEXT: TEXT(n->x,n->y,n->size,n->col,n->text); break;
  case N_BAR: RECT(n->x,n->y,n->w,n->h,n->col); break;
  case N_METER:{ FRAME(n->x,n->y,n->w,n->h,C_LINE); int p=n->pct; if(p<0)p=0;if(p>100)p=100; int fw=(n->w-2)*p/100; if(fw>0)RECT(n->x+1,n->y+1,fw,n->h-2,n->col); break; }
  case N_IMG: RECT(n->x,n->y,n->w,n->h,"1b2432"); FRAME(n->x,n->y,n->w,n->h,C_ACCENT); break;
  case N_GRAPH:{ /* parse space-separated values */ const char*v=n->vals; int vals[80],nv=0;
    while(*v&&nv<80){ while(*v==' ')v++; if(!*v)break; int neg=0; if(*v=='-'){neg=1;v++;} int x=0,any=0; while(*v>='0'&&*v<='9'){x=x*10+(*v++-'0');any=1;} if(any)vals[nv++]=neg?-x:x; while(*v&&*v!=' ')v++; }
    if(!nv){ FRAME(n->x,n->y,n->w,n->h,C_LINE); break; } int mx=1; for(int i=0;i<nv;i++)if(vals[i]>mx)mx=vals[i];
    int bw=n->w/nv; if(bw<1)bw=1; int gap=1; if(bw>2)bw-=gap; for(int i=0;i<nv;i++){ int bh=vals[i]*(n->h-1)/mx; if(bh<1)bh=1; RECT(n->x+i*(bw+gap),n->y+n->h-bh,bw,bh,n->col); } break; }
  case N_SPACE: break; } }

int main(void){ static char src[8192]; int n=0; long r;
  while((r=sys3(SYS_read,0,(long)(src+n),sizeof src-1-n))>0){ n+=r; if(n>=(int)sizeof(src)-1)break; } src[n]=0;
  P=src; np=0; Node*root=pNode(); if(!root){ es("clear 0e1016\npresent\n"); flush(); return 1; }
  CLEAR(C_BG); layout(root,0,0,W,H); draw(root); es("present\n"); flush(); return 0; }
