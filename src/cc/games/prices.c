
#include "prices.h"
#include <time.h>
#include <unistd.h>
#ifdef BUILD_GAMESCC
#include "../rogue/cursesd.h"
#else
#include <curses.h>
#endif

#define SATOSHIDEN ((uint64_t)100000000L)
#define issue_curl(cmdstr) bitcoind_RPC(0,(char *)"prices",cmdstr,0,0,0)
extern int64_t Net_change,Betsize;

int random_tetromino(struct games_state *rs)
{
    rs->seed = _games_rngnext(rs->seed);
    return(rs->seed % NUM_TETROMINOS);
}

int32_t pricesdata(struct games_player *P,void *ptr)
{
    tetris_game *tg = (tetris_game *)ptr;
    P->gold = tg->points;
    P->dungeonlevel = tg->level;
    //fprintf(stderr,"score.%d level.%d\n",tg->points,tg->level);
    return(0);
}

void sleep_milli(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = milliseconds * 1000 * 1000;
    nanosleep(&ts, NULL);
}

struct games_state globalR;
extern char Gametxidstr[];
int32_t issue_games_events(struct games_state *rs,char *gametxidstr,uint32_t eventid,gamesevent c);
uint64_t get_btcusd();
int32_t issue_bet(struct games_state *rs,int64_t x,int64_t betsize);

void *gamesiterate(struct games_state *rs)
{
    bool running = true; uint32_t eventid = 0; int64_t price;
    if ( rs->guiflag != 0 || rs->sleeptime != 0 )
    {
        initscr();             // initialize curses
        cbreak();              // pass key presses to program, but not signals
        noecho();              // don't echo key presses to screen
        timeout(0); 
    }
    while ( running != 0 )
    {
        //running = tg_tick(rs,tg,move);
        if ( rs->guiflag != 0 || rs->sleeptime != 0 )
        {
        }
        if ( rs->guiflag != 0 )
        {
#ifdef STANDALONE
            price = get_btcusd();
            //fprintf(stderr,"%llu -> t%u %.4f\n",(long long)price,(uint32_t)(price >> 32),(double)(price & 0xffffffff)/10000);
            //issue_games_events(rs,Gametxidstr,eventid,price);
            issue_bet(rs,price,Betsize);
            eventid++;
            doupdate();
            sleep(10);
            switch ( getch() )
            {
                case '+': Net_change++; break;
                case '-': Net_change--; break;
                case '0': Net_change = 0; break;
                case '$': Betsize = SATOSHIDEN; break;
                case '^': Betsize += (Betsize >> 3); break;
                case '/': Betsize -= (Betsize >> 3); break;
            }
            /*if ( (counter++ % 10) == 0 )
                doupdate();
            c = games_readevent(rs);
            if ( c <= 0x7f || skipcount == 0x3fff )
            {
                if ( skipcount > 0 )
                    issue_games_events(rs,Gametxidstr,eventid-skipcount,skipcount | 0x4000);
                if ( c <= 0x7f )
                    issue_games_events(rs,Gametxidstr,eventid,c);
                if ( tg->level != prevlevel )
                {
                    flushkeystrokes(rs,0);
                    prevlevel = tg->level;
                }
                skipcount = 0;
            } else skipcount++;*/
#endif
        }
        else
        {
            if ( rs->replaydone != 0 )
                break;
            if ( rs->sleeptime != 0 )
            {
                sleep_milli(1);
            }
            /*if ( skipcount == 0 )
            {
                c = games_readevent(rs);
                //fprintf(stderr,"%04x score.%d level.%d\n",c,tg->points,tg->level);
                if ( (c & 0x4000) == 0x4000 )
                {
                    skipcount = (c & 0x3fff);
                    c = 'S';
                }
            }
            if ( skipcount > 0 )
                skipcount--;*/
        }
        eventid++;
    }
    return(0);
}

#ifdef STANDALONE
#include <ncurses.h>
#include "dapps/dappstd.c"
int64_t Net_change,Betsize = SATOSHIDEN;

char *send_curl(char *url,char *fname)
{
    char *retstr;
    retstr = issue_curl(url);
    return(retstr);
}

cJSON *get_urljson(char *url,char *fname)
{
    char *jsonstr; cJSON *json = 0;
    if ( (jsonstr= send_curl(url,fname)) != 0 )
    {
        //printf("(%s) -> (%s)\n",url,jsonstr);
        json = cJSON_Parse(jsonstr);
        free(jsonstr);
    }
    return(json);
}

//////////////////////////////////////////////
// start of dapp
//////////////////////////////////////////////

uint64_t get_btcusd()
{
    cJSON *pjson,*bpi,*usd; char str[512]; uint64_t x,newprice,mult,btcusd = 0;
    if ( (pjson= get_urljson((char *)"http://api.coindesk.com/v1/bpi/currentprice.json",(char *)"/tmp/oraclefeed.json")) != 0 )
    {
        if ( (bpi= jobj(pjson,(char *)"bpi")) != 0 && (usd= jobj(bpi,(char *)"USD")) != 0 )
        {
            btcusd = jdouble(usd,(char *)"rate_float") * SATOSHIDEN;
            mult = 10000 + Net_change*10;
            newprice = (btcusd * mult) / 10000;
            x = ((uint64_t)time(NULL) << 32) | ((newprice / 10000) & 0xffffffff);
            sprintf(str,"BTC/USD %.4f -> Betsize %.8f (^ / to change) && %.4f Net %.1f%% [+ - to change]\n",dstr(btcusd),dstr(Betsize),dstr(newprice),(double)100*(mult-10000)/10000);
            mvaddstr(0, 0, str);
            clrtoeol();
            doupdate();
        }
        free_json(pjson);
    }
    return(x);
}

char *clonestr(char *str)
{
    char *clone; int32_t len;
    if ( str == 0 || str[0] == 0 )
    {
        printf("warning cloning nullstr.%p\n",str);
#ifdef __APPLE__
        while ( 1 ) sleep(1);
#endif
        str = (char *)"<nullstr>";
    }
    len = strlen(str);
    clone = (char *)calloc(1,len+16);
    strcpy(clone,str);
    return(clone);
}

int32_t issue_games_events(struct games_state *rs,char *gametxidstr,uint32_t eventid,gamesevent c)
{
    static FILE *fp;
    char params[512],*retstr; cJSON *retjson,*resobj; int32_t retval = -1;
    if ( fp == 0 )
        fp = fopen("events.log","wb");
    rs->buffered[rs->num++] = c;
    if ( 1 )
    {
        if ( sizeof(c) == 1 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%02x%%22,%%22%s%%22,%u]\"]",(uint8_t)c&0xff,gametxidstr,eventid);
        else if ( sizeof(c) == 2 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%04x%%22,%%22%s%%22,%u]\"]",(uint16_t)c&0xffff,gametxidstr,eventid);
        else if ( sizeof(c) == 4 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%08x%%22,%%22%s%%22,%u]\"]",(uint32_t)c&0xffffffff,gametxidstr,eventid);
        else if ( sizeof(c) == 8 )
            sprintf(params,"[\"events\",\"17\",\"[%%22%016llx%%22,%%22%s%%22,%u]\"]",(long long)c,gametxidstr,eventid);
        if ( (retstr= komodo_issuemethod(USERPASS,(char *)"cclib",params,GAMES_PORT)) != 0 )
        {
            if ( (retjson= cJSON_Parse(retstr)) != 0 )
            {
                if ( (resobj= jobj(retjson,(char *)"result")) != 0 )
                {
                    retval = 0;
                    if ( fp != 0 )
                    {
                        fprintf(fp,"%s\n",jprint(resobj,0));
                        fflush(fp);
                    }
                }
                free_json(retjson);
            } else fprintf(fp,"error parsing %s\n",retstr);
            free(retstr);
        } else fprintf(fp,"error issuing method %s\n",params);
        return(retval);
    } else return(0);
}

int32_t issue_bet(struct games_state *rs,int64_t x,int64_t betsize)
{
    char params[512],hexstr[64],*retstr; cJSON *retjson,*resobj; int32_t i,retval = -1;
    memset(hexstr,0,sizeof(hexstr));
    for (i=0; i<8; i++)
    {
        sprintf(&hexstr[i<<1],"%02x",(uint8_t)(x & 0xff));
        x >>= 8;
    }
    sprintf(params,"[\"bet\",\"17\",\"[%.8f,%%22%s%%22]\"]",dstr(betsize),hexstr);
    if ( (retstr= komodo_issuemethod(USERPASS,(char *)"cclib",params,GAMES_PORT)) != 0 )
    {
        if ( (retjson= cJSON_Parse(retstr)) != 0 )
        {
            if ( (resobj= jobj(retjson,(char *)"result")) != 0 )
            {
                retval = 0;
                //fprintf(stderr,"%s\n",jprint(resobj,0));
            }
            free_json(retjson);
        }
        free(retstr);
    }
    return(retval);
}

int prices(int argc, char **argv)
{
    struct games_state *rs = &globalR;
    int32_t c,skipcount=0; uint32_t eventid = 0;
    memset(rs,0,sizeof(*rs));
    rs->guiflag = 1;
    rs->sleeptime = 1; // non-zero to allow refresh()
    if ( argc >= 2 && strlen(argv[2]) == 64 )
    {
#ifdef _WIN32
#ifdef _MSC_VER
        rs->origseed = _strtoui64(argv[1], NULL, 10);
#else
        rs->origseed = atol(argv[1]); // windows, but not MSVC
#endif // _MSC_VER
#else
        rs->origseed = atol(argv[1]); // non-windows
#endif // _WIN32
        rs->seed = rs->origseed;
        if ( argc >= 3 )
        {
            strcpy(Gametxidstr,argv[2]);
            fprintf(stderr,"setplayerdata %s\n",Gametxidstr);
            if ( games_setplayerdata(rs,Gametxidstr) < 0 )
            {
                fprintf(stderr,"invalid gametxid, or already started\n");
                return(-1);
            }
        }
    } else rs->seed = 777;
    gamesiterate(rs);
    //gamesbailout(rs);
    return 0;
}

#endif

