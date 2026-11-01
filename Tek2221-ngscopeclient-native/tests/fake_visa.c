/* Test double: native VISA entry points backed by fixture bytes, no hardware. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char command[512];
static void log_command(const char* text)
{
    FILE* f=fopen(getenv("TEK_TEST_LOG"),"a");
    if(f) { fprintf(f,"%s\n",text); fclose(f); }
}
int viOpenDefaultRM(uint32_t* out) { *out=1; return 0; }
int viOpen(uint32_t rm,const char* address,uint32_t access,uint32_t timeout,uint32_t* out)
{ (void)rm;(void)access;(void)timeout;log_command(address); *out=2;return 0; }
int viSetAttribute(uint32_t session,uint32_t attr,uint32_t value)
{ (void)session;(void)attr;(void)value;return 0; }
int viClose(uint32_t session) { (void)session;return 0; }
int viWrite(uint32_t session,const uint8_t* bytes,uint32_t size,uint32_t* written)
{
    (void)session;
    if(size>=sizeof(command)) return (int)0xBFFF0000;
    memcpy(command,bytes,size);command[size]=0;
    while(size && (command[size-1]=='\n' || command[size-1]=='\r')) command[--size]=0;
    log_command(command);*written=(uint32_t)strlen(command)+1;return 0;
}
int viRead(uint32_t session,uint8_t* bytes,uint32_t capacity,uint32_t* count)
{
    (void)session;
    const char* text=NULL;
    if(!strcmp(command,"ID?")) text="ID TEK/2221,V81.1,VERS:15;";
    else if(!strcmp(command,"DATA?")) text="DATA SOURCE:ACQ,TARGET:REF4,CHANNEL:CH2,ENCDG:ASCII;";
    if(text) { *count=(uint32_t)strlen(text);memcpy(bytes,text,*count);return 0; }
    const char* name=!strcmp(command,"WFMPRE?")?"preamble.txt":!strcmp(command,"CURVE?")?"curve-response.bin":NULL;
    if(!name) return (int)0xBFFF0000;
    char path[4096];snprintf(path,sizeof(path),"%s/%s",getenv("TEK_TEST_FIXTURE"),name);
    FILE* f=fopen(path,"rb");if(!f) return (int)0xBFFF0000;
    *count=(uint32_t)fread(bytes,1,capacity,f);fclose(f);return 0;
}
